// ============================================================================
// MAJEED GRAIN - four genuinely different grain generators.
//   GT_GAUSS   : band-limited gaussian value-noise lattice (digital sensor noise)
//   GT_CRYSTAL : jittered-point Voronoi crystals, per-crystal density (silver-halide look)
//   GT_TAPE    : scan-line grain. Independent 1-D noise per row, per-row gain and
//                offset, optional vertical bleed. Horizontally correlated (tape / video)
//   GT_CLUSTER : 4-octave fBm with a heavy-tailed transfer curve (clumped, blotchy grain)
// Grain size is defined in millimetres and converted to full-resolution pixels
// with  px = mm * (layerWidthPx / frameWidthMm).
// ============================================================================
#include "majeed_core.h"

namespace majeed {

double grain_px_per_mm(const GrainParams& p, const FrameCtx& c) {
    double fw = std::max(1.0, p.frameWidthMm);
    return c.fullW / fw;
}

namespace {

struct Field {
    int   type = GT_CRYSTAL;
    float cell = 8.f;          // grain size in full-res px
    float ax = 1.f;            // x stretch
    float soft = 0.5f;
    float dens = 1.f;
    float clump = 0.f, fine = 0.f, coarse = 0.f;
    float rowBleed = 0.f, rowVar = 0.4f;
    float norm = 1.f;

    // ---- occupied lattice helpers -----------------------------------------
    inline float latt(int ix, int iy, uint32_t seed) const {
        uint32_t h = hash3((uint32_t)ix, (uint32_t)iy, seed);
        if (dens < 0.999f && u01(hash_u32(h ^ 0x51ed270bU)) >= dens) return 0.f;
        return gauss(h);
    }
    inline float vn2(float x, float y, uint32_t seed) const {
        float fx = std::floor(x), fy = std::floor(y);
        int ix = (int)fx, iy = (int)fy;
        float tx = shape_interp(x - fx, soft), ty = shape_interp(y - fy, soft);
        float a = latt(ix, iy, seed), b = latt(ix + 1, iy, seed);
        float c = latt(ix, iy + 1, seed), d = latt(ix + 1, iy + 1, seed);
        return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), ty);
    }
    inline float row1(float x, int row, uint32_t seed) const {
        float fx = std::floor(x); int ix = (int)fx;
        float t = shape_interp(x - fx, soft);
        auto L = [&](int i) {
            uint32_t h = hash3((uint32_t)i, (uint32_t)row, seed);
            if (dens < 0.999f && u01(hash_u32(h ^ 0x51ed270bU)) >= dens) return 0.f;
            return gauss(h);
        };
        return lerpf(L(ix), L(ix + 1), t);
    }

    // ---- the four generators (raw, un-normalised) -------------------------
    float gaussF(float X, float Y, uint32_t seed, float cs) const {
        float c = cell * cs;
        return vn2(X / (c * ax), Y / c, seed);
    }

    float crystalF(float X, float Y, uint32_t seed, float cs) const {
        float c = cell * cs;
        float gx = X / (c * ax), gy = Y / c;
        int ix = (int)std::floor(gx), iy = (int)std::floor(gy);
        float d2[9], val[9]; float dmin = 1e9f; int n = 0;
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i, ++n) {
                int cx = ix + i, cy = iy + j;
                uint32_t h = hash3((uint32_t)cx, (uint32_t)cy, seed);
                uint32_t h2 = hash_u32(h + 0x68e31da4U);
                uint32_t h3 = hash_u32(h2 + 0xb5297a4dU);
                float px = cx + 0.1f + 0.8f * u01(h2);
                float py = cy + 0.1f + 0.8f * u01(h3);
                float dx = gx - px, dy = gy - py;
                d2[n] = dx * dx + dy * dy;
                dmin = std::min(dmin, d2[n]);
                bool occ = (dens >= 0.999f) || u01(hash_u32(h ^ 0x51ed270bU)) < dens;
                val[n] = occ ? gauss(h) : 0.f;
            }
        float k = lerpf(90.f, 5.f, soft);              // hard crystal edges .. soft blend
        float sw = 0.f, sv = 0.f;
        for (int m = 0; m < 9; ++m) {
            float e = k * (d2[m] - dmin);
            if (e > 9.f) continue;
            float w = std::exp(-e);
            sw += w; sv += w * val[m];
        }
        return sv / sw;
    }

    float tapeF(float X, float Y, uint32_t seed, float cs) const {
        float c = cell * cs;
        float rh = c, lx = c * ax;
        int r = (int)std::floor(Y / rh);
        auto rowVal = [&](int rr) {
            uint32_t hg = hash3((uint32_t)rr, seed, 0x77u);
            float gain = std::max(0.25f, 1.f + rowVar * 0.5f * gauss(hg));
            float off = u01(hash3((uint32_t)rr, seed, 0x99u)) * 997.f;
            return gain * row1(X / lx + off, rr, seed);
        };
        float f = rowVal(r);
        if (rowBleed > 0.001f) {
            float nb = 0.5f * (rowVal(r - 1) + rowVal(r + 1));
            f = (1.f - rowBleed) * f + rowBleed * nb;
        }
        return f;
    }

    float clusterF(float X, float Y, uint32_t seed, float cs) const {
        float c = cell * cs * 2.f;
        float sum = 0.f, wsum = 0.f, w = 1.f, sc = 1.f;
        for (int o = 0; o < 4; ++o) {
            float v = vn2(X / (c * ax * sc), Y / (c * sc), seed + 0x101u * (uint32_t)o);
            sum += w * v; wsum += w * w;
            w *= 0.62f; sc *= 0.5f;
        }
        float f = sum / std::sqrt(wsum);
        return f * std::sqrt(std::fabs(f));                 // heavy tail
    }

    float base(float X, float Y, uint32_t seed, float cs) const {
        switch (type) {
        case GT_GAUSS:   return gaussF(X, Y, seed, cs);
        case GT_CRYSTAL: return crystalF(X, Y, seed, cs);
        case GT_TAPE:    return tapeF(X, Y, seed, cs);
        default:         return clusterF(X, Y, seed, cs);
        }
    }

    // ---- full evaluation: warp + octaves + normalisation ------------------
    float eval(float X, float Y, uint32_t seed) const {
        if (clump > 0.001f) {                                // clumping: domain warp
            float s = cell * 7.f;
            float wx = vnoise2(X / s, Y / s, seed ^ 0xA1u, 1.f);
            float wy = vnoise2(X / s + 17.f, Y / s - 9.f, seed ^ 0xB2u, 1.f);
            X += wx * clump * cell * 2.2f; Y += wy * clump * cell * 2.2f;
        }
        float f = base(X, Y, seed, 1.f);
        float ww = 1.f;
        if (fine > 0.001f)   { f += fine   * base(X + 31.f, Y - 57.f, seed ^ 0x1111u, 0.5f); ww += fine * fine; }
        if (coarse > 0.001f) { f += coarse * base(X - 91.f, Y + 13.f, seed ^ 0x2222u, 3.0f); ww += coarse * coarse; }
        f /= std::sqrt(ww);
        if (clump > 0.001f) {                                 // clumping: density modulation
            float m = vnoise2(X / (cell * 9.f), Y / (cell * 9.f), seed ^ 0xC3u, 1.f);
            f *= std::max(0.f, 1.f + clump * 0.9f * m);
        }
        return f * norm;
    }
};

// standard deviation of the raw field (dens forced to 1) -> unit-variance normalisation
float calibrate(Field f) {
    f.dens = 1.f; f.norm = 1.f;
    double s = 0, s2 = 0; const int N = 6000;
    for (int i = 0; i < N; ++i) {
        float X = u01(hash3(i, 1, 0xCA11u)) * f.cell * 70.f;
        float Y = u01(hash3(i, 2, 0xCA11u)) * f.cell * 70.f;
        float v = f.eval(X, Y, 0xBEEF);
        s += v; s2 += (double)v * v;
    }
    double m = s / N, var = s2 / N - m * m;
    return var > 1e-9 ? (float)(1.0 / std::sqrt(var)) : 1.f;
}

inline float phi_uniform(float f) {            // normal CDF approximation -> [0,1]
    float t = 0.7978845608f * (f + 0.044715f * f * f * f);
    return 0.5f * (1.f + std::tanh(t));
}

struct Shaper {
    float dist, thr, sharpK, sharpDen;
    inline float operator()(float f) const {
        if (dist > 0.f) f = lerpf(f, (phi_uniform(f) - 0.5f) * 3.4641f, dist);
        else if (dist < 0.f) f = lerpf(f, f >= 0.f ? 1.f : -1.f, -dist);
        if (thr > 0.f) { float a = std::fabs(f) - thr; f = a > 0.f ? (f > 0.f ? a : -a) : 0.f; }
        if (sharpK > 0.01f) f = std::tanh(sharpK * f) * sharpDen;
        return f;
    }
};

// sparse bright/dark dashes ("specks", tape drop-outs)
inline float speckAt(float X, float Y, uint32_t seed, float cell, float ax, float amt) {
    if (amt <= 0.f) return 0.f;
    float W = 8.f * cell * ax;
    int cx = (int)std::floor(X / W), r = (int)std::floor(Y / cell);
    uint32_t h = hash4((uint32_t)cx, (uint32_t)r, seed, 0x5be0u);
    if (u01(h) >= amt * 0.02f) return 0.f;
    uint32_t h2 = hash_u32(h + 1), h3 = hash_u32(h + 2), h4 = hash_u32(h + 3);
    float x0 = cx * W + W * 0.7f * u01(h2);
    float len = cell * ax * (1.5f + 3.f * u01(h3));
    if (X < x0 || X > x0 + len) return 0.f;
    float v = 6.f * (0.6f + 0.4f * u01(h4));
    return (u01(hash_u32(h + 4)) < 0.2f) ? -v : v;
}

} // namespace

void render_grain(const Image& src, const Image& dst, const GrainParams& p, const FrameCtx& c) {
    const int W = src.w, H = src.h;
    Field F;
    F.type = p.type;
    double ppm = grain_px_per_mm(p, c);
    F.cell = (float)std::max(0.35, p.sizeMm * ppm);
    F.ax = (float)std::max(0.05, p.aspect / 100.0);
    F.soft = (float)clampf((float)(p.softness / 100.0), 0.f, 1.f);
    F.dens = (float)clampf((float)(p.density / 100.0), 0.f, 1.f);
    F.clump = (float)clampf((float)(p.clumping / 100.0), 0.f, 2.f);
    F.fine = (float)std::max(0.0, p.fine / 100.0);
    F.coarse = (float)std::max(0.0, p.coarse / 100.0);
    F.rowBleed = (float)clampf((float)(p.rowBleed / 100.0), 0.f, 1.f);
    F.rowVar = (float)clampf((float)(p.rowVar / 100.0), 0.f, 2.f);
    F.norm = calibrate(F);

    float kSharp = (float)clampf((float)(p.sharpness / 100.0), 0.f, 1.f) * 4.f;
    Shaper shp{ (float)clampf((float)(p.distribution / 100.0), -1.f, 1.f),
                (float)clampf((float)(p.threshold / 100.0), 0.f, 1.f) * 2.5f,
                kSharp, kSharp > 0.01f ? 1.f / std::tanh(kSharp) : 1.f };

    // temporal evolution: phase in "steps"
    double P = p.evolutionDeg / 360.0 + p.evoSpeed * c.timeSec;
    double Pf = std::floor(P + 1e-4);
    int stepA = (int)Pf; float tB = (float)(P - Pf); if (tB < 0.f) tB = 0.f;
    bool smooth = p.smoothEvo && tB > 1e-4f;
    float wA = std::cos(tB * 1.5707963f), wB = std::sin(tB * 1.5707963f);
    uint32_t seedBase = hash_u32((uint32_t)p.seed * 7919u + 0x1234u);
    uint32_t sA = hash2(seedBase, (uint32_t)stepA), sB = hash2(seedBase, (uint32_t)(stepA + 1));

    const bool mono   = p.mono;
    const float rgbMix = mono ? 0.f : (float)clampf((float)(p.rgbGrain / 100.0), 0.f, 1.f);
    const float rgbNorm = 1.f / std::sqrt((1.f - rgbMix) * (1.f - rgbMix) + rgbMix * rgbMix);
    const float sep = mono ? 0.f : (float)p.rgbSep;
    const float cvar = mono ? 0.f : (float)(p.colorVar / 100.0);
    const float crand = mono ? 0.f : (float)(p.colorRand / 100.0);
    const float chvar = mono ? 0.f : (float)(p.chanVar / 100.0);
    const float sat = (float)(p.saturation / 100.0);
    const float cAmt[3] = { (float)(p.redAmt / 100.0), (float)(p.greenAmt / 100.0), (float)(p.blueAmt / 100.0) };
    const float tmax = std::max(1e-4f, std::max(p.grainColor.r, std::max(p.grainColor.g, p.grainColor.b)));
    const float T[3] = { p.grainColor.r / tmax, p.grainColor.g / tmax, p.grainColor.b / tmax };
    const float tAmt = (float)clampf((float)(p.colorAmt / 100.0), 0.f, 1.f);
    const float amount = (float)(p.amount / 100.0);
    const float cscale = (float)(p.contrast / 100.0) * 0.4f;
    const float bright = (float)(p.brightness / 100.0);
    const float opac = (float)clampf((float)(p.opacity / 100.0), 0.f, 1.f);
    const float shadowsW = (float)(p.shadows / 100.0), highW = (float)(p.highlights / 100.0);
    const float specks = (float)(p.specks / 100.0);
    const int   mode = p.blend;
    const bool  needSepEval = sep != 0.f;

    auto fieldT = [&](float X, float Y, uint32_t tag) -> float {
        if (!smooth) return shp(F.eval(X, Y, hash2(sA, tag))) + speckAt(X, Y, hash2(sA, tag), F.cell, F.ax, specks);
        float a = F.eval(X, Y, hash2(sA, tag)), b = F.eval(X, Y, hash2(sB, tag));
        return shp(wA * a + wB * b) + speckAt(X, Y, hash2(sA, tag), F.cell, F.ax, specks);
    };

    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float Y = (float)(c.originY + (y + 0.5) * c.scaleY);
            for (int x = 0; x < W; ++x) {
                float X = (float)(c.originX + (x + 0.5) * c.scaleX);
                const float* s = src.at(x, y);
                float* o = dst.at(x, y);

                float n[3];
                float fL = fieldT(X, Y, 0x100u);
                if (!needSepEval) { n[0] = n[1] = n[2] = fL; }
                else {
                    n[0] = fieldT(X - sep, Y, 0x100u);
                    n[1] = fL;
                    n[2] = fieldT(X + sep, Y, 0x100u);
                }
                if (rgbMix > 0.f) {
                    for (int k = 0; k < 3; ++k) {
                        float xs = X + (k == 0 ? -sep : (k == 2 ? sep : 0.f));
                        float fc = fieldT(xs, Y, 0x200u + (uint32_t)k);
                        n[k] = ((1.f - rgbMix) * n[k] + rgbMix * fc) * rgbNorm;
                    }
                }
                if (cvar > 0.f) {                              // chroma blotches at 2x scale
                    float fi = fieldT(X * 0.5f, Y * 0.5f, 0x300u);
                    float fq = fieldT(X * 0.5f, Y * 0.5f, 0x301u);
                    float dr, dg, db; yiq2rgb(0.f, fi * cvar * 0.7f, fq * cvar * 0.7f, dr, dg, db);
                    n[0] += dr; n[1] += dg; n[2] += db;
                }
                if (crand > 0.f || chvar > 0.f) {
                    int cx = (int)std::floor(X / F.cell), cy = (int)std::floor(Y / F.cell);
                    if (crand > 0.f) {                          // each grain cell gets a random hue
                        float th = u01(hash4((uint32_t)cx, (uint32_t)cy, sA, 0xC0u)) * 6.2831853f;
                        float dr, dg, db; yiq2rgb(0.f, std::cos(th), std::sin(th), dr, dg, db);
                        float k2 = fL * crand * 0.8f;
                        n[0] += dr * k2; n[1] += dg * k2; n[2] += db * k2;
                    }
                    if (chvar > 0.f)
                        for (int k = 0; k < 3; ++k)
                            n[k] *= std::max(0.f, 1.f + chvar * 0.7f * gauss(hash4((uint32_t)cx, (uint32_t)cy, sA, 0x31u + (uint32_t)k)));
                }
                for (int k = 0; k < 3; ++k) n[k] *= cAmt[k];
                if (sat != 1.f) {
                    float m = (n[0] + n[1] + n[2]) * (1.f / 3.f);
                    for (int k = 0; k < 3; ++k) n[k] = m + (n[k] - m) * sat;
                }
                if (tAmt > 0.f) {
                    float m = (n[0] + n[1] + n[2]) * (1.f / 3.f);
                    for (int k = 0; k < 3; ++k) n[k] = lerpf(n[k], m * T[k], tAmt);
                }
                float l = luma709(s[0], s[1], s[2]);
                float amp = (shadowsW * (1.f - l) + highW * l) * amount * cscale;
                for (int k = 0; k < 3; ++k) {
                    float d = n[k] * amp + bright;
                    float res = blend_channel(mode, s[k], 0.5f + 0.5f * d, d);
                    o[k] = lerpf(s[k], res, opac);
                }
                o[3] = s[3];
            }
        }
    });
}

} // namespace majeed
