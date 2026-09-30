// ============================================================================
// MAJEED RANDOM LINES - procedural scribble generator.
// Every line is derived from a hash of (seed, line index, generation). A line has
// its own centre, angle, length, curvature, thickness, opacity, brightness,
// lifetime, phase and motion. Lines are stroked as flattened quadratic Bezier
// polylines with an analytic anti-aliased distance coverage (no textures, no
// native AE effects). Rasterisation is tiled across threads; each tile walks the
// lines in index order, so the result is deterministic.
// ============================================================================
#include "majeed_core.h"

namespace majeed {
namespace {

struct SubSeg { float x0, y0, x1, y1; };
struct LineInfo {
    int first, count;
    float thick;          // buffer px
    float alpha;          // opacity * lifetime envelope
    float col[3];         // colour (already includes brightness)
    float ymin, ymax;
};

inline float pow_signed(float t, float e) { return t < 0 ? -std::pow(-t, e) : std::pow(t, e); }

} // namespace

void render_lines(const Image& src, const Image& dst, const LinesParams& p, const FrameCtx& c) {
    const int W = src.w, H = src.h;
    const float FW = (float)c.fullW, FH = (float)c.fullH;
    const float invSx = (float)(1.0 / c.scaleX), invSy = (float)(1.0 / c.scaleY);
    const uint32_t seedB = hash_u32((uint32_t)p.seed * 2654435761u + 0x51u);

    const int N = (int)std::max(0.0, std::min(p.amount, 60000.0));
    const float dens = (float)clampf((float)(p.density / 100.0), 0.f, 1.f);
    const int nGrid = std::max(1, (int)std::ceil(std::sqrt((double)std::max(1, N))));
    const int nClusters = std::max(2, N / 60);
    const float tEvo = (float)(c.timeSec * p.evoSpeed / 100.0 + p.evolutionDeg / 360.0 * std::max(0.5, p.lifetime));
    const int jitStep = (int)std::floor(c.timeSec * std::max(0.0, p.jitterSpeed) + 1e-4);
    const float mSpd = (float)(p.motionSpeed / 100.0);
    const float mAmt = (float)p.motionAmount;
    const int nSegs = std::max(1, std::min(8, p.segments));

    const float wH = (float)(p.horizBias / 100.0 * 3.0), wV = (float)(p.vertBias / 100.0 * 3.0), wD = (float)(p.diagBias / 100.0 * 3.0);
    const float wTot = 1.f + wH + wV + wD;

    std::vector<SubSeg> segs; segs.reserve((size_t)N * 8);
    std::vector<LineInfo> lines; lines.reserve((size_t)N);

    for (int i = 0; i < N; ++i) {
        uint32_t h = hash2(seedB, (uint32_t)i);
        if (u01(hash_u32(h ^ 0xD5u)) >= dens) continue;

        // ---- lifetime / generation -----------------------------------------
        float frac = 0.5f; uint32_t gen = 0; float env = 1.f;
        float life = (float)p.lifetime * (0.6f + 0.8f * u01(hash_u32(h + 1)));
        float phase = u01(hash_u32(h + 2));
        if (p.lifetime > 0.01) {
            float age = tEvo / life + phase;
            float g = std::floor(age);
            frac = age - g; gen = (uint32_t)(int)g;
        }
        uint32_t hg = hash2(h, gen);
        auto R = [&](uint32_t k) { return u01(hash_u32(hg + 0x1000u * (k + 1))); };

        float fadeW = (float)(p.fade / 100.0) * 0.5f * (1.f - (float)(p.fadeRand / 100.0) * R(20));
        if (p.lifetime > 0.01 && fadeW > 1e-3f)
            env = std::min(smoothstepf(0.f, fadeW, frac), smoothstepf(0.f, fadeW, 1.f - frac));

        // ---- length / thickness --------------------------------------------
        float L = (float)(p.minLen + (p.maxLen - p.minLen) * R(1)) * (float)(p.lengthScale / 100.0);
        float thick = (float)(p.minThick + (p.maxThick - p.minThick) * R(2)) * (float)(p.thickScale / 100.0);
        float mo = std::min(1.f, mAmt / 50.f) * mSpd;
        L *= 1.f + 0.18f * mo * std::sin(6.2831853f * (frac + R(3)));

        // ---- angle (uniform mixed with horizontal / vertical / diagonal) -----
        float pick = R(4) * wTot, centre = 0.f, spread = (float)p.angleRand;
        if (pick < 1.f) { centre = 0.f; }
        else if (pick < 1.f + wH) { centre = 0.f;  spread = std::min(spread, 180.f) * 0.18f; }
        else if (pick < 1.f + wH + wV) { centre = 90.f; spread = std::min(spread, 180.f) * 0.18f; }
        else { centre = (R(5) < 0.5f ? 45.f : 135.f); spread = std::min(spread, 180.f) * 0.18f; }
        float ang = (float)p.angle + centre + (R(6) * 2.f - 1.f) * spread;
        ang += 6.f * mo * std::sin(6.2831853f * (frac + R(7)));               // angle wobble
        float th = ang * 0.017453293f;

        // ---- centre position --------------------------------------------------
        float rx = -0.1f + 1.2f * R(8), ry = -0.1f + 1.2f * R(9);
        if (p.spacing > 0.f) {
            uint64_t cell = ((uint64_t)(uint32_t)i * 2654435761ull) % (uint64_t)(nGrid * nGrid);
            float sx = ((float)(cell % nGrid) + R(10)) / nGrid, sy = ((float)(cell / nGrid) + R(11)) / nGrid;
            float s = (float)clampf((float)(p.spacing / 100.0), 0.f, 1.f);
            rx = lerpf(rx, -0.1f + 1.2f * sx, s); ry = lerpf(ry, -0.1f + 1.2f * sy, s);
        }
        float cx = rx * FW, cy = ry * FH;
        if (p.clustering > 0.f && R(12) < (float)(p.clustering / 100.0)) {
            uint32_t j = hash2(seedB ^ 0xC1u, hash_u32(h + 3) % (uint32_t)nClusters);
            float ccx = u01(hash_u32(j + 1)) * FW, ccy = u01(hash_u32(j + 2)) * FH;
            cx = ccx + gauss(hash_u32(hg + 0x77u)) * (float)p.clusterSize;
            cy = ccy + gauss(hash_u32(hg + 0x78u)) * (float)p.clusterSize;
        }
        if (p.distribution != 0.0) {                                               // centre / edge weighting
            float e = std::pow(2.f, (float)(p.distribution / 50.0));
            float tx = pow_signed((cx / FW - 0.5f) * 2.f, e), ty = pow_signed((cy / FH - 0.5f) * 2.f, e);
            cx = (0.5f + 0.5f * tx) * FW; cy = (0.5f + 0.5f * ty) * FH;
        }
        // motion drift
        {
            float md = ((float)p.motionDir + (float)(p.motionRand / 100.0) * (R(13) * 2.f - 1.f) * 180.f) * 0.017453293f;
            float dist = mAmt * (frac - 0.5f) * mSpd * (0.4f + R(14));
            cx += std::cos(md) * dist; cy += std::sin(md) * dist;
        }

        // ---- appearance -------------------------------------------------------
        float alpha = (float)(p.opacity / 100.0) * (1.f - (float)(p.opacityRand / 100.0) * R(15)) * env;
        float br = (float)(p.brightness / 100.0) * (1.f - (float)(p.brightRand / 100.0) * R(16));
        float col[3] = { br * lerpf(1.f, p.color.r, (float)(p.colorAmt / 100.0)),
                         br * lerpf(1.f, p.color.g, (float)(p.colorAmt / 100.0)),
                         br * lerpf(1.f, p.color.b, (float)(p.colorAmt / 100.0)) };
        float curv = (float)(p.curvature / 100.0) * (1.f + (float)(p.curvRand / 100.0) * (R(17) * 2.f - 1.f));
        curv *= (R(18) < 0.5f ? -1.f : 1.f);
        curv *= 1.f + 0.3f * mo * std::sin(6.2831853f * (frac * 1.3f + R(19)));

        // ---- build the polyline ----------------------------------------------
        float segL = L / nSegs;
        float dx = std::cos(th), dy = std::sin(th);
        float px = cx - dx * L * 0.5f, py = cy - dy * L * 0.5f;
        int first = (int)segs.size();
        float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
        float jit = (float)p.jitter;
        for (int s = 0; s < nSegs; ++s) {
            if (s > 0) {
                float turn = (R(30 + (uint32_t)s) * 2.f - 1.f) * (float)p.kink * 0.017453293f;
                float ca = std::cos(turn), sa = std::sin(turn);
                float ndx = dx * ca - dy * sa, ndy = dx * sa + dy * ca; dx = ndx; dy = ndy;
            }
            float qx = px + dx * segL, qy = py + dy * segL;
            // jitter endpoints (deterministic per line / step / point)
            float jx0 = 0, jy0 = 0, jx1 = 0, jy1 = 0;
            if (jit > 0.f) {
                uint32_t jh = hash3((uint32_t)i, (uint32_t)jitStep, seedB ^ 0x77u);
                jx1 = s11(hash_u32(jh + (uint32_t)s * 4 + 1)) * jit; jy1 = s11(hash_u32(jh + (uint32_t)s * 4 + 2)) * jit;
                if (s == 0) { jx0 = s11(hash_u32(jh + 900)) * jit; jy0 = s11(hash_u32(jh + 901)) * jit; }
            }
            float ax = px + jx0, ay = py + jy0, bx = qx + jx1, by = qy + jy1;
            float mx = (ax + bx) * 0.5f - dy * segL * curv * 0.5f;
            float my = (ay + by) * 0.5f + dx * segL * curv * 0.5f;
            int n = (int)clampf(segL / 10.f, 2.f, 24.f);
            float lx = ax, ly = ay;
            for (int k = 1; k <= n; ++k) {
                float t = (float)k / n, u = 1.f - t;
                float x = u * u * ax + 2.f * u * t * mx + t * t * bx;
                float y = u * u * ay + 2.f * u * t * my + t * t * by;
                float X0 = (lx - (float)c.originX) * invSx, Y0 = (ly - (float)c.originY) * invSy;
                float X1 = (x - (float)c.originX) * invSx,  Y1 = (y - (float)c.originY) * invSy;
                segs.push_back({ X0, Y0, X1, Y1 });
                xmin = std::min(xmin, std::min(X0, X1)); xmax = std::max(xmax, std::max(X0, X1));
                ymin = std::min(ymin, std::min(Y0, Y1)); ymax = std::max(ymax, std::max(Y0, Y1));
                lx = x; ly = y;
            }
            px = qx; py = qy;
        }
        float rr = thick * invSx * 0.5f + 2.f;
        if (xmax + rr < 0 || xmin - rr > W || ymax + rr < 0 || ymin - rr > H || alpha <= 0.002f) {
            segs.resize(first); continue;
        }
        LineInfo li; li.first = first; li.count = (int)segs.size() - first;
        li.thick = thick * invSx; li.alpha = alpha * (float)(p.layerOpacity / 100.0);
        li.col[0] = col[0]; li.col[1] = col[1]; li.col[2] = col[2];
        li.ymin = ymin - rr; li.ymax = ymax + rr;
        lines.push_back(li);
    }

    // ---- rasterise (premultiplied accumulation, tiled by rows) ----------------
    std::vector<float> acc((size_t)W * H * 4, 0.f);
    parallel_rows(H, [&](int y0, int y1) {
        for (const LineInfo& li : lines) {
            if (li.ymax < y0 || li.ymin > y1) continue;
            const float t = li.thick;
            const float r = std::max(t * 0.5f, 0.5f) + 1.0f;
            for (int si = 0; si < li.count; ++si) {
                const SubSeg& sg = segs[li.first + si];
                int bx0 = std::max(0, (int)std::floor(std::min(sg.x0, sg.x1) - r));
                int bx1 = std::min(W - 1, (int)std::ceil(std::max(sg.x0, sg.x1) + r));
                int by0 = std::max(y0, (int)std::floor(std::min(sg.y0, sg.y1) - r));
                int by1 = std::min(y1 - 1, (int)std::ceil(std::max(sg.y0, sg.y1) + r));
                if (bx0 > bx1 || by0 > by1) continue;
                float vx = sg.x1 - sg.x0, vy = sg.y1 - sg.y0;
                float len2 = vx * vx + vy * vy, inv = len2 > 1e-8f ? 1.f / len2 : 0.f;
                for (int y = by0; y <= by1; ++y) {
                    float py = y + 0.5f - sg.y0;
                    for (int x = bx0; x <= bx1; ++x) {
                        float pxx = x + 0.5f - sg.x0;
                        float u = clampf((pxx * vx + py * vy) * inv, 0.f, 1.f);
                        float ex = pxx - u * vx, ey = py - u * vy;
                        float d = std::sqrt(ex * ex + ey * ey);
                        float cov = t < 1.f ? t * std::max(0.f, 1.f - d) : clampf(t * 0.5f + 0.5f - d, 0.f, 1.f);
                        if (cov <= 0.f) continue;
                        float a = cov * li.alpha; if (a > 1.f) a = 1.f;
                        float* q = &acc[((size_t)y * W + x) * 4];
                        q[0] = q[0] * (1.f - a) + li.col[0] * a;
                        q[1] = q[1] * (1.f - a) + li.col[1] * a;
                        q[2] = q[2] * (1.f - a) + li.col[2] * a;
                        q[3] = q[3] + a * (1.f - q[3]);
                    }
                }
            }
        }
    });

    // ---- composite ------------------------------------------------------------
    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < W; ++x) {
                const float* s = src.at(x, y);
                const float* q = &acc[((size_t)y * W + x) * 4];
                float* o = dst.at(x, y);
                float A = q[3];
                switch (p.composite) {
                case LC_OVER:
                    for (int k = 0; k < 3; ++k) o[k] = s[k] * (1.f - A) + q[k];
                    o[3] = s[3]; break;
                case LC_ADD:
                    for (int k = 0; k < 3; ++k) o[k] = s[k] + q[k];
                    o[3] = s[3]; break;
                case LC_SCREEN:
                    for (int k = 0; k < 3; ++k) o[k] = 1.f - (1.f - s[k]) * (1.f - q[k]);
                    o[3] = s[3]; break;
                case LC_MULTIPLY:
                    for (int k = 0; k < 3; ++k) o[k] = s[k] * (1.f - A + q[k]);
                    o[3] = s[3]; break;
                case LC_TRANSPARENT:
                    for (int k = 0; k < 3; ++k) o[k] = A > 1e-5f ? q[k] / A : 0.f;
                    o[3] = A; break;
                default: // LC_ON_BLACK
                    for (int k = 0; k < 3; ++k) o[k] = q[k];
                    o[3] = 1.f; break;
                }
            }
    });
}

} // namespace majeed
