// ============================================================================
// MAJEED DITHER - real dithering algorithms.
//   * Error diffusion: every algorithm runs its own published kernel over the image
//     in scan order (optionally serpentine). Nothing is approximated by noise+threshold.
//   * Ordered dithering: Bayer 2/4/8/16 (recursive construction), blue noise
//     (void-and-cluster, generated at start-up), interleaved gradient noise, white noise.
//   * Screens: analytic halftone spot functions (dot, square, line, diagonal, cross,
//     diamond, astroid star, grid, wave, zig-zag, circuit, stitch, clock, thread, knit,
//     block). Each spot function is rank-normalised so tone maps linearly to dot area.
// ============================================================================
#include "majeed_core.h"
#include <cstring>
#include <mutex>

namespace majeed {
namespace {

// ------------------------------------------------------------ kernels -------
struct Tap { int dx, dy; float w; };
struct Kernel { std::vector<Tap> taps; };

Kernel make_kernel(std::initializer_list<Tap> t, float div) {
    Kernel k; for (auto tp : t) { tp.w /= div; k.taps.push_back(tp); } return k;
}
enum {
    K_FS, K_JJN, K_STUCKI, K_ATKINSON, K_BURKES, K_SIERRA3, K_SIERRA2, K_SIERRALITE, K_FAN, K_SHIAUFAN,
    K_SKIP1, K_SKIP2, K_SKIP3, K_COUNT
};
const Kernel& kernel(int id) {
    static std::vector<Kernel> K;
    static std::once_flag once;
    std::call_once(once, [] {
        K.resize(K_COUNT);
        K[K_FS]  = make_kernel({{1,0,7},{-1,1,3},{0,1,5},{1,1,1}}, 16);
        K[K_JJN] = make_kernel({{1,0,7},{2,0,5},{-2,1,3},{-1,1,5},{0,1,7},{1,1,5},{2,1,3},{-2,2,1},{-1,2,3},{0,2,5},{1,2,3},{2,2,1}}, 48);
        K[K_STUCKI] = make_kernel({{1,0,8},{2,0,4},{-2,1,2},{-1,1,4},{0,1,8},{1,1,4},{2,1,2},{-2,2,1},{-1,2,2},{0,2,4},{1,2,2},{2,2,1}}, 42);
        K[K_ATKINSON] = make_kernel({{1,0,1},{2,0,1},{-1,1,1},{0,1,1},{1,1,1},{0,2,1}}, 8);   // diffuses only 6/8
        K[K_BURKES] = make_kernel({{1,0,8},{2,0,4},{-2,1,2},{-1,1,4},{0,1,8},{1,1,4},{2,1,2}}, 32);
        K[K_SIERRA3] = make_kernel({{1,0,5},{2,0,3},{-2,1,2},{-1,1,4},{0,1,5},{1,1,4},{2,1,2},{-1,2,2},{0,2,3},{1,2,2}}, 32);
        K[K_SIERRA2] = make_kernel({{1,0,4},{2,0,3},{-2,1,1},{-1,1,2},{0,1,3},{1,1,2},{2,1,1}}, 16);
        K[K_SIERRALITE] = make_kernel({{1,0,2},{-1,1,1},{0,1,1}}, 4);
        K[K_FAN] = make_kernel({{1,0,7},{-2,1,1},{-1,1,3},{0,1,5}}, 16);
        K[K_SHIAUFAN] = make_kernel({{1,0,4},{-2,1,1},{-1,1,1},{0,1,2}}, 8);
        // "Skip neighbours": Floyd-Steinberg weights, but the taps are pushed (1+s) pixels away
        for (int s = 1; s <= 3; ++s) {
            int d = 1 + s;
            K[K_SKIP1 + s - 1] = make_kernel({{d,0,7},{-d,d,3},{0,d,5},{d,d,1}}, 16);
        }
    });
    return K[id];
}

// ------------------------------------------------------------ algorithm table
enum Spot { S_DOT, S_SQUARE, S_LINEH, S_LINEV, S_DIAG, S_DIAG2, S_CROSS, S_DIAMOND, S_STAR, S_GRID,
            S_WAVE, S_ZIGZAG, S_CIRCUIT, S_STITCHH, S_STITCHV, S_CLOCK, S_BITHREAD, S_KNIT, S_RECT };
const int F_SERP = 256, F_NOISE = 512;

const DitherAlgoInfo ALGOS[] = {
    {"Floyd-Steinberg",           DK_ERROR, K_FS, 0, 0, "Error diffusion 7-3-5-1 /16"},
    {"Floyd-Steinberg Serpentine",DK_ERROR, K_FS | F_SERP, 0, 0, "Floyd-Steinberg, alternating scan direction"},
    {"Jarvis-Judice-Ninke",       DK_ERROR, K_JJN, 0, 0, "3-row kernel /48"},
    {"Stucki",                    DK_ERROR, K_STUCKI, 0, 0, "3-row kernel /42"},
    {"Atkinson",                  DK_ERROR, K_ATKINSON, 0, 0, "Diffuses 6/8 of the error, 1/8 to six neighbours"},
    {"Burkes",                    DK_ERROR, K_BURKES, 0, 0, "2-row kernel /32"},
    {"Sierra",                    DK_ERROR, K_SIERRA3, 0, 0, "Sierra-3, 3-row kernel /32"},
    {"Sierra Two Row",            DK_ERROR, K_SIERRA2, 0, 0, "Sierra-2, 2-row kernel /16"},
    {"Sierra Lite",               DK_ERROR, K_SIERRALITE, 0, 0, "Sierra Lite 2-1-1 /4"},
    {"Fan",                       DK_ERROR, K_FAN, 0, 0, "Fan kernel 7 / 1-3-5 (/16)"},
    {"Shiau-Fan",                 DK_ERROR, K_SHIAUFAN, 0, 0, "Shiau-Fan kernel 4 / 1-1-2 (/8)"},
    {"Skip Neighbours",           DK_ERROR, K_SKIP1, 0, 0, "Floyd-Steinberg weights, taps 2 px away (MAJEED definition)"},
    {"Skip1 Neighbours",          DK_ERROR, K_SKIP2, 0, 0, "Floyd-Steinberg weights, taps 3 px away (MAJEED definition)"},
    {"Skip2 Neighbours",          DK_ERROR, K_SKIP3, 0, 0, "Floyd-Steinberg weights, taps 4 px away (MAJEED definition)"},
    {"Xerox Grain",               DK_ERROR, K_FS | F_SERP | F_NOISE, 0, 0, "Serpentine Floyd-Steinberg with random threshold jitter"},
    {"Bayer 2x2",                 DK_BAYER, 2, 0, 0, "Ordered dither, 2x2 Bayer matrix"},
    {"Bayer 4x4",                 DK_BAYER, 4, 0, 0, "Ordered dither, 4x4 Bayer matrix"},
    {"Bayer 8x8",                 DK_BAYER, 8, 0, 0, "Ordered dither, 8x8 Bayer matrix"},
    {"Bayer 16x16",               DK_BAYER, 16, 0, 0, "Ordered dither, 16x16 Bayer matrix"},
    {"Blue Noise",                DK_BLUE, 0, 0, 0, "Void-and-cluster blue noise mask, 64x64 tiled"},
    {"Interleaved Gradient Noise",DK_IGN, 0, 0, 0, "Jimenez interleaved gradient noise threshold"},
    {"White Noise",               DK_WHITE, 0, 0, 0, "Random threshold per pixel"},
    {"Halftone",                  DK_SCREEN, S_DOT, 8, 0, "Round-dot halftone screen, 0 deg"},
    {"Halftone 22.5",             DK_SCREEN, S_DOT, 8, 22.5, "Round-dot halftone screen, 22.5 deg"},
    {"Halftone 45",               DK_SCREEN, S_DOT, 8, 45, "Round-dot halftone screen, 45 deg (newspaper)"},
    {"Matrix",                    DK_SCREEN, S_DOT, 4, 45, "Fine dot matrix, 45 deg"},
    {"Square Halftone",           DK_SCREEN, S_SQUARE, 8, 0, "Square-dot screen"},
    {"Mosaic Halftone",           DK_SCREEN, S_SQUARE, 6, 0, "Small square-dot screen"},
    {"Rekt Block",                DK_SCREEN, S_RECT, 8, 0, "2:1 rectangular block screen"},
    {"Row Modulation",            DK_SCREEN, S_LINEH, 4, 0, "Horizontal line screen, pitch 4"},
    {"Medium Modulation",         DK_SCREEN, S_LINEH, 6, 0, "Horizontal line screen, pitch 6"},
    {"Heavy Modulation",          DK_SCREEN, S_LINEH, 8, 0, "Horizontal line screen, pitch 8"},
    {"Column Modulation",         DK_SCREEN, S_LINEV, 4, 0, "Vertical line screen"},
    {"Tilt Modulation",           DK_SCREEN, S_DIAG, 6, 0, "Diagonal line screen"},
    {"Bitslash",                  DK_SCREEN, S_DIAG2, 4, 0, "Fine slash screen"},
    {"Variable Hatch",            DK_SCREEN, S_DIAG, 8, 0, "Coarse diagonal hatch"},
    {"Grid Modulation",           DK_SCREEN, S_GRID, 6, 0, "Cell-border grid screen"},
    {"Cyber",                     DK_SCREEN, S_GRID, 4, 0, "Tight cell-border grid"},
    {"Cross Square",              DK_SCREEN, S_CROSS, 6, 0, "Plus-shaped clusters"},
    {"Diamond",                   DK_SCREEN, S_DIAMOND, 8, 0, "Diamond clusters"},
    {"Star",                      DK_SCREEN, S_STAR, 8, 0, "Astroid star clusters"},
    {"Bytewav",                   DK_SCREEN, S_WAVE, 8, 0, "Sine-wave line screen"},
    {"Z-Modulation",              DK_SCREEN, S_ZIGZAG, 8, 0, "Zig-zag line screen"},
    {"Circuit Modulation",        DK_SCREEN, S_CIRCUIT, 8, 0, "Square rings with checker-swapped radius"},
    {"Vertical Stitch",           DK_SCREEN, S_STITCHV, 6, 0, "Dashed vertical lines"},
    {"Horizontal Stitch",         DK_SCREEN, S_STITCHH, 6, 0, "Dashed horizontal lines"},
    {"Clock",                     DK_SCREEN, S_CLOCK, 8, 0, "Radial sweep clusters"},
    {"Bi-thread",                 DK_SCREEN, S_BITHREAD, 6, 0, "Two crossing diagonal threads"},
    {"Knit",                      DK_SCREEN, S_KNIT, 4, 0, "Alternating diagonal weave"},
};
const int NALGOS = (int)(sizeof(ALGOS) / sizeof(ALGOS[0]));

// ------------------------------------------------------------ threshold maps
const std::vector<float>& bayer_map(int n) {          // returns n*n thresholds in (0,1)
    static std::vector<float> maps[4]; static std::once_flag once[4];
    int idx = n == 2 ? 0 : n == 4 ? 1 : n == 8 ? 2 : 3;
    std::call_once(once[idx], [&] {
        std::vector<int> m = { 0, 2, 3, 1 }; int s = 2;
        while (s < n) {
            int ns = s * 2; std::vector<int> nm((size_t)ns * ns);
            for (int y = 0; y < s; ++y) for (int x = 0; x < s; ++x) {
                int v = 4 * m[y * s + x];
                nm[y * ns + x] = v;           nm[y * ns + x + s] = v + 2;
                nm[(y + s) * ns + x] = v + 3; nm[(y + s) * ns + x + s] = v + 1;
            }
            m.swap(nm); s = ns;
        }
        maps[idx].resize((size_t)n * n);
        for (int i = 0; i < n * n; ++i) maps[idx][i] = (m[i] + 0.5f) / (float)(n * n);
    });
    return maps[idx];
}

const int BN = 64;
const std::vector<float>& blue_noise_map() {          // void-and-cluster (Ulichney)
    static std::vector<float> out; static std::once_flag once;
    std::call_once(once, [] {
        const int N = BN * BN, R = 6; const float sigma = 1.5f;
        std::vector<float> kern((2 * R + 1) * (2 * R + 1));
        for (int j = -R; j <= R; ++j) for (int i = -R; i <= R; ++i)
            kern[(j + R) * (2 * R + 1) + (i + R)] = std::exp(-(i * i + j * j) / (2 * sigma * sigma));
        std::vector<float> E(N, 0.f); std::vector<char> bp(N, 0);
        auto splat = [&](int idx, float sgn) {
            int x = idx % BN, y = idx / BN;
            for (int j = -R; j <= R; ++j) for (int i = -R; i <= R; ++i)
                E[((y + j + BN) % BN) * BN + ((x + i + BN) % BN)] += sgn * kern[(j + R) * (2 * R + 1) + (i + R)];
        };
        auto tightest = [&]() { int b = -1; float bv = -1e30f; for (int i = 0; i < N; ++i) if (bp[i] && E[i] > bv) { bv = E[i]; b = i; } return b; };
        auto largestVoid = [&]() { int b = -1; float bv = 1e30f; for (int i = 0; i < N; ++i) if (!bp[i] && E[i] < bv) { bv = E[i]; b = i; } return b; };
        Rng rng(0xB10E);
        int n0 = N / 10, placed = 0;
        while (placed < n0) { int i = rng.next() % N; if (!bp[i]) { bp[i] = 1; splat(i, 1.f); ++placed; } }
        for (int it = 0; it < 20000; ++it) {                        // relax initial pattern
            int c = tightest(); bp[c] = 0; splat(c, -1.f);
            int v = largestVoid();
            bp[v] = 1; splat(v, 1.f);
            if (v == c) break;
        }
        std::vector<int> rank(N, 0);
        std::vector<char> bp0 = bp; std::vector<float> E0 = E;
        for (int r = n0 - 1; r >= 0; --r) { int c = tightest(); bp[c] = 0; splat(c, -1.f); rank[c] = r; }
        bp = bp0; E = E0;
        for (int r = n0; r < N; ++r) { int v = largestVoid(); bp[v] = 1; splat(v, 1.f); rank[v] = r; }
        out.resize(N);
        for (int i = 0; i < N; ++i) out[i] = (rank[i] + 0.5f) / (float)N;
    });
    return out;
}

// spot function: distance-from-dark-core style value, (fu,fv) in [0,1) cell coords, par = checker parity
float spot(int id, float fu, float fv, int par) {
    float du = fu - 0.5f, dv = fv - 0.5f;
    auto tri = [](float t) { return 1.f - std::fabs(2.f * t - 1.f); };
    auto frac1 = [](float t) { return t - std::floor(t); };
    switch (id) {
    case S_DOT:     return std::sqrt(du * du + dv * dv);
    case S_SQUARE:  return std::max(std::fabs(du), std::fabs(dv));
    case S_LINEH:   return std::fabs(dv);
    case S_LINEV:   return std::fabs(du);
    case S_DIAG:    return std::fabs(frac1(fu + fv) - 0.5f);
    case S_DIAG2:   return std::fabs(frac1(fu - fv) - 0.5f);
    case S_CROSS:   return std::min(std::fabs(du), std::fabs(dv));
    case S_DIAMOND: return std::fabs(du) + std::fabs(dv);
    case S_STAR: { float a = std::sqrt(std::fabs(du)) + std::sqrt(std::fabs(dv)); return a * a; }
    case S_GRID:    return std::min(std::min(fu, 1.f - fu), std::min(fv, 1.f - fv));
    case S_WAVE:    return std::fabs(dv - 0.25f * std::sin(6.2831853f * fu));
    case S_ZIGZAG:  return std::fabs(fv - (0.25f + 0.5f * tri(fu)));
    case S_CIRCUIT: { float r = std::max(std::fabs(du), std::fabs(dv)); return std::fabs(r - (par ? 0.14f : 0.30f)); }
    case S_STITCHH: return std::fabs(dv) + (fu > 0.7f ? 0.5f : 0.f);
    case S_STITCHV: return std::fabs(du) + (fv > 0.7f ? 0.5f : 0.f);
    case S_CLOCK:   { float a = std::atan2(dv, du) * 0.15915494f + 0.5f; return a * 0.8f + 0.2f * std::sqrt(du * du + dv * dv); }
    case S_BITHREAD:return std::min(std::fabs(frac1(fu + fv) - 0.5f), std::fabs(frac1(fu - fv) - 0.5f));
    case S_KNIT:    return par ? std::fabs(frac1(fu + fv) - 0.5f) : std::fabs(frac1(fu - fv + 0.25f) - 0.5f);
    case S_RECT:    return std::max(std::fabs(du), std::fabs(dv) * 2.f);
    }
    return 0.f;
}

// Threshold map over a 2x2 cell tile, rank-normalised so T is uniform in [0,1].
// Axis-aligned screens with an integer pitch P are sampled exactly on the pixel lattice
// (M = 2P texels, one per pixel) so that tone -> dot area is exact even for tiny cells.
// Rotated screens use a 256x256 map.
struct ScreenMap { std::vector<float> T; int M; bool exact; float pitch; };
ScreenMap build_screen_map(int id, float pitch, bool axisAligned) {
    ScreenMap sm; sm.exact = axisAligned; 
    int P = std::max(1, (int)std::floor(pitch + 0.5f));
    sm.M = axisAligned ? 2 * P : 256; sm.pitch = axisAligned ? (float)P : pitch;
    const int M = sm.M;
    std::vector<float> t((size_t)M * M);
    for (int y = 0; y < M; ++y) for (int x = 0; x < M; ++x) {
        float u2 = (x + 0.5f) * 2.f / M, v2 = (y + 0.5f) * 2.f / M;
        int pu = (int)u2, pv = (int)v2;
        t[(size_t)y * M + x] = spot(id, u2 - pu, v2 - pv, (pu + pv) & 1);
    }
    std::vector<int> order(t.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return t[a] < t[b]; });
    sm.T.resize(t.size());
    const float n = (float)t.size();
    for (size_t r = 0; r < order.size(); ++r) sm.T[order[r]] = 1.f - (r + 0.5f) / n;   // core = highest threshold
    return sm;
}

} // namespace

int dither_algo_count() { return NALGOS; }
const DitherAlgoInfo& dither_algo(int i) { return ALGOS[std::max(0, std::min(NALGOS - 1, i))]; }
int dither_algo_find(const char* name) {
    for (int i = 0; i < NALGOS; ++i) if (std::strcmp(ALGOS[i].name, name) == 0) return i;
    return -1;
}

namespace {
inline float quant(float v, int L) {
    v = clampf(v, 0.f, 1.f);
    return std::floor(v * (L - 1) + 0.5f) / (float)(L - 1);
}

void diffuse_plane(std::vector<float>& pl, int gw, int gh, int kid, bool serp, float strength,
                   float noise, uint32_t seed, int L) {
    const Kernel& K = kernel(kid);
    for (int y = 0; y < gh; ++y) {
        bool rev = serp && (y & 1);
        for (int i = 0; i < gw; ++i) {
            int x = rev ? gw - 1 - i : i;
            float v = pl[(size_t)y * gw + x];
            float t = v;
            if (noise > 0.f) t += (u01(hash3((uint32_t)x, (uint32_t)y, seed)) - 0.5f) * noise / (float)(L - 1);
            float q = quant(t, L);
            float err = (v - q) * strength;
            pl[(size_t)y * gw + x] = q;
            for (const Tap& tp : K.taps) {
                int nx = x + (rev ? -tp.dx : tp.dx), ny = y + tp.dy;
                if (nx < 0 || nx >= gw || ny >= gh) continue;
                pl[(size_t)ny * gw + nx] += err * tp.w;
            }
        }
    }
}
} // namespace

void render_dither(const Image& src, const Image& dst, const DitherParams& p, const FrameCtx& c) {
    const int W = src.w, H = src.h;
    const DitherAlgoInfo& A = dither_algo(p.algo);
    const int L = std::max(2, std::min(64, p.levels));
    const int block = std::max(1, (int)std::floor(p.size / c.scaleX + 0.5));
    const int gw = (W + block - 1) / block, gh = (H + block - 1) / block;
    const int nch = p.mode == 0 ? 3 : 1;
    const float contrast = (float)(p.contrast / 100.0), bright = (float)(p.brightness / 100.0);
    const float gam = 2.2f;

    // ---- 1. reduce to the dither grid (block average) + pre-process ---------
    std::vector<std::vector<float>> pl(nch, std::vector<float>((size_t)gw * gh));
    parallel_rows(gh, [&](int y0, int y1) {
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = 0; gx < gw; ++gx) {
                double acc[3] = { 0, 0, 0 }; int cnt = 0;
                int xe = std::min(W, (gx + 1) * block), ye = std::min(H, (gy + 1) * block);
                for (int y = gy * block; y < ye; ++y)
                    for (int x = gx * block; x < xe; ++x) { const float* s = src.at(x, y); acc[0] += s[0]; acc[1] += s[1]; acc[2] += s[2]; ++cnt; }
                float v[3] = { (float)(acc[0] / cnt), (float)(acc[1] / cnt), (float)(acc[2] / cnt) };
                float ch[3]; int n = nch;
                if (nch == 1) { ch[0] = luma709(v[0], v[1], v[2]); } else { ch[0] = v[0]; ch[1] = v[1]; ch[2] = v[2]; }
                for (int k = 0; k < n; ++k) {
                    float t = (ch[k] - 0.5f) * contrast + 0.5f + bright;
                    if (p.invert) t = 1.f - t;
                    t = clampf(t, 0.f, 1.f);
                    if (p.linear) t = std::pow(t, gam);
                    pl[k][(size_t)gy * gw + gx] = t;
                }
            }
    });

    // ---- 2. quantise --------------------------------------------------------
    const float spread = (float)clampf((float)(p.strength / 100.0), 0.f, 2.f);
    const uint32_t seed = hash_u32((uint32_t)p.seed + 0xD17Eu);
    if (A.kind == DK_ERROR) {
        int kid = A.param & 255; bool serp = p.serpentine || (A.param & F_SERP);
        float noise = (float)(p.noise / 100.0) + ((A.param & F_NOISE) ? 0.9f : 0.f);
        std::vector<std::thread> th;                                 // channels are independent
        for (int k = 0; k < nch; ++k)
            th.emplace_back([&, k] { diffuse_plane(pl[k], gw, gh, kid, serp, spread, noise, seed + (uint32_t)k * 77u, L); });
        for (auto& t : th) t.join();
    } else {
        ScreenMap sm; const float* tex = nullptr; int texN = 0; bool exact = false;
        float cosA = 1, sinA = 0, pitch = 1;
        if (A.kind == DK_SCREEN) {
            pitch = (float)std::max(1.0, A.cell * p.patternScale / 100.0);
            double angDeg = A.angle + p.patternAngle;
            exact = std::fabs(std::fmod(angDeg, 90.0)) < 1e-6;
            sm = build_screen_map(A.param, pitch, exact); tex = sm.T.data(); texN = sm.M;
            if (exact) pitch = sm.pitch;
            float a = (float)angDeg * 0.017453293f; cosA = std::cos(a); sinA = std::sin(a);
        }
        const float* bn = A.kind == DK_BLUE ? blue_noise_map().data() : nullptr;
        const float* bay = A.kind == DK_BAYER ? bayer_map(A.param).data() : nullptr;
        const int bayN = A.param;
        const float nz = (float)(p.noise / 100.0);
        parallel_rows(gh, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y)
                for (int x = 0; x < gw; ++x) {
                    float T = 0.5f;
                    switch (A.kind) {
                    case DK_BAYER: T = bay[(y % bayN) * bayN + (x % bayN)]; break;
                    case DK_BLUE:  T = bn[(y % BN) * BN + (x % BN)]; break;
                    case DK_IGN: { float f = 0.06711056f * x + 0.00583715f * y; f -= std::floor(f); f *= 52.9829189f; T = f - std::floor(f); break; }
                    case DK_WHITE: T = u01(hash3((uint32_t)x, (uint32_t)y, seed)); break;
                    case DK_SCREEN: {
                        if (exact) {                                   // lattice-exact lookup (angle multiple of 90 deg)
                            int q = (int)std::floor(p.patternAngle / 90.0 + A.angle / 90.0 + 0.5) & 3, xx = x, yy = y;
                            if (q == 1) { xx = y; yy = -x - 1; } else if (q == 2) { xx = -x - 1; yy = -y - 1; } else if (q == 3) { xx = -y - 1; yy = x; }
                            int M2 = texN; xx = ((xx % M2) + M2) % M2; yy = ((yy % M2) + M2) % M2;
                            T = tex[(size_t)yy * M2 + xx]; break;
                        }
                        float xs = x + 0.5f, ys = y + 0.5f;
                        float u = (xs * cosA + ys * sinA) / pitch, v = (-xs * sinA + ys * cosA) / pitch;
                        u = u * 0.5f; v = v * 0.5f; u -= std::floor(u); v -= std::floor(v);       // 2x2-cell tile
                        int ix = std::min(texN - 1, (int)(u * texN)), iy = std::min(texN - 1, (int)(v * texN));
                        T = tex[(size_t)iy * texN + ix]; break;
                    }
                    default: break;
                    }
                    if (nz > 0.f) T = clampf(T + (u01(hash3((uint32_t)x, (uint32_t)y, seed ^ 0x5a5au)) - 0.5f) * nz, 0.f, 1.f);
                    T = 0.5f + (T - 0.5f) * spread;
                    for (int k = 0; k < nch; ++k) {
                        float v = clampf(pl[k][(size_t)y * gw + x], 0.f, 1.f) * (L - 1);
                        float q = std::floor(v + T);
                        pl[k][(size_t)y * gw + x] = clampf(q / (float)(L - 1), 0.f, 1.f);
                    }
                }
        });
    }

    // ---- 3. write back ------------------------------------------------------
    const float amount = (float)clampf((float)(p.amount / 100.0), 0.f, 1.f);
    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < W; ++x) {
                size_t gi = (size_t)(y / block) * gw + (x / block);
                float v[3];
                for (int k = 0; k < nch; ++k) { float t = pl[k][gi]; if (p.linear) t = std::pow(t, 1.f / gam); v[k] = t; }
                float rgb[3];
                if (p.mode == 0) { rgb[0] = v[0]; rgb[1] = v[1]; rgb[2] = v[2]; }
                else if (p.mode == 1) { rgb[0] = rgb[1] = rgb[2] = v[0]; }
                else { rgb[0] = lerpf(p.dark.r, p.light.r, v[0]); rgb[1] = lerpf(p.dark.g, p.light.g, v[0]); rgb[2] = lerpf(p.dark.b, p.light.b, v[0]); }
                const float* s = src.at(x, y); float* o = dst.at(x, y);
                for (int k = 0; k < 3; ++k) o[k] = lerpf(s[k], rgb[k], amount);
                o[3] = s[3];
            }
    });
    (void)p.preserveAlpha;
}

} // namespace majeed
