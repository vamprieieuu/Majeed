// ============================================================================
// MAJEED VHS - analogue tape simulation with real processing stages:
//  1 geometry   : per-scanline horizontal displacement (wobble, rolling tear band,
//                 head-switching skew, per-line jitter) + R/G/B channel offsets
//  2 chroma     : YIQ; horizontal chroma bleed (box smear), chroma shift, luma softening
//  3 artefacts  : tape dashes (per-row procedural drop-outs), vertical streaks,
//                 scanline modulation with interlace phase, flicker
//  4 noise      : scan-line grain (GT_TAPE generator of the grain engine)
// ============================================================================
#include "majeed_core.h"

namespace majeed {

void render_vhs(const Image& src, const Image& dst, const VHSParams& p, const FrameCtx& c) {
    const int W = src.w, H = src.h;
    const float isx = (float)(1.0 / c.scaleX), isy = (float)(1.0 / c.scaleY);
    const float fullH = (float)c.fullH, fullW = (float)c.fullW;
    const double t = c.timeSec * p.speed / 100.0;
    const int frame = (int)std::floor(c.timeSec * c.fps * p.speed / 100.0 + 1e-4);
    const uint32_t seed = hash_u32((uint32_t)p.seed + 0x7A9Eu);
    const int jFrame = (int)std::floor(c.timeSec * c.fps * p.jitterRate / 100.0 + 1e-4);

    // ---------------- 1. per-row displacement (buffer px) ------------------
    std::vector<float> dx(H, 0.f);
    {
        float wl = std::max(4.f, fullH * (float)(p.trackSize / 100.0) * 0.12f);
        float tearY = (float)(((t * 0.13 * p.trackSpeed / 50.0) + u01(hash_u32(seed + 5))) - std::floor((t * 0.13 * p.trackSpeed / 50.0) + u01(hash_u32(seed + 5)))) * fullH;
        float tearH = fullH * 0.06f;
        float tearAmt = (float)p.trackAmount * 1.6f * s11(hash3(seed, (uint32_t)std::floor(t * 0.13 * p.trackSpeed / 50.0 + u01(hash_u32(seed + 5))), 9));
        float hsH = std::max(1.f, fullH * (float)(p.headHeight / 100.0));
        float gShake = s11(hash3(seed, (uint32_t)jFrame, 3)) * (float)p.jitterAmount * 0.05f;
        for (int y = 0; y < H; ++y) {
            float Y = (float)(c.originY + (y + 0.5) * c.scaleY);
            float d = vnoise1(Y / wl + (float)(t * p.trackSpeed / 100.0 * 1.7), seed, 1, 1.f) * (float)p.trackAmount * 0.6f;
            float dt = std::fabs(Y - tearY) / tearH;
            if (dt < 1.f) d += tearAmt * (1.f - dt * dt);
            float hy = (Y - (fullH - hsH)) / hsH;
            if (hy > 0.f) d += (float)p.headSwitch * hy * hy * (0.6f + 0.4f * s11(hash3(seed, (uint32_t)frame, 11))) + s11(hash3(seed, (uint32_t)(frame * 977 + y), 12)) * (float)p.headSwitch * 0.15f * hy;
            uint32_t jh = hash3((uint32_t)(int)(Y), (uint32_t)jFrame, seed ^ 0x1234u);
            if (u01(jh) < 0.3f) d += s11(hash_u32(jh + 1)) * (float)p.jitterAmount * 0.12f;
            d += gShake;
            dx[y] = d * isx;
        }
    }
    std::vector<float> A((size_t)W * H * 4), B((size_t)W * H * 4);
    Image tmp{ W, H, A.data() };
    const float rOff = (float)p.rSepX * isx, bOff = (float)p.bSepX * isx;
    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* row = src.at(0, y);
            auto rowSample = [&](float xx, int k) {
                xx = clampf(xx, 0.f, (float)(W - 1)); int x0 = (int)xx; int x1 = std::min(x0 + 1, W - 1); float f = xx - x0;
                return lerpf(row[x0 * 4 + k], row[x1 * 4 + k], f);
            };
            for (int x = 0; x < W; ++x) {
                float* o = tmp.at(x, y);
                float bx = x - dx[y];
                o[0] = rowSample(bx - rOff, 0);
                o[1] = rowSample(bx, 1);
                o[2] = rowSample(bx + bOff, 2);
                o[3] = row[x * 4 + 3];
            }
        }
    });

    // ---------------- 2. chroma bleed / luma soften ---------------------------
    const int rc = (int)std::ceil((float)(p.bleed / 100.0) * 12.f * isx);
    const int rl = (int)std::ceil((float)(p.lumaSoft / 100.0) * 2.5f * isx);
    const int sh = (int)std::floor((float)p.bleedShift * isx + 0.5f);
    const float sat = (float)(p.saturation / 100.0);
    Image out2{ W, H, B.data() };
    parallel_rows(H, [&](int y0, int y1) {
        std::vector<float> Yv(W), Iv(W), Qv(W), Ib(W), Qb(W), Yb(W);
        for (int y = y0; y < y1; ++y) {
            for (int x = 0; x < W; ++x) { const float* s = tmp.at(x, y); rgb2yiq(s[0], s[1], s[2], Yv[x], Iv[x], Qv[x]); }
            auto box = [&](const std::vector<float>& in, std::vector<float>& outv, int r) {
                if (r <= 0) { outv = in; return; }
                double sum = 0; int cnt = 0;
                for (int i = -r; i <= r; ++i) { int xi = std::min(W - 1, std::max(0, i)); sum += in[xi]; ++cnt; }
                for (int x = 0; x < W; ++x) {
                    outv[x] = (float)(sum / cnt);
                    int add = std::min(W - 1, x + r + 1), sub = std::max(0, x - r);
                    sum += in[add] - in[sub];
                }
            };
            box(Iv, Ib, rc); box(Qv, Qb, rc); box(Yv, Yb, rl);
            for (int x = 0; x < W; ++x) {
                int xs = std::min(W - 1, std::max(0, x - sh));
                float r, g, b; yiq2rgb(Yb[x], Ib[xs] * sat, Qb[xs] * sat, r, g, b);
                float* o = out2.at(x, y); o[0] = r; o[1] = g; o[2] = b; o[3] = tmp.at(x, y)[3];
            }
        }
    });

    // ---------------- 3. dashes, streaks, scanlines, flicker ------------------
    struct Dash { float x0, x1, v; };
    const float dashP = (float)(p.dashes / 100.0);
    const float rowH = std::max(1.f, (float)p.dashRows);
    const float per = std::max(1.5f, (float)p.scanPeriod), sDepth = (float)(p.scanlines / 100.0) * 0.8f;
    const float sExp = 1.f + (float)(p.scanSharp / 100.0) * 3.f;
    const float flick = (float)(p.flicker / 100.0);
    const float gFlick = 1.f + flick * 0.12f * gauss(hash3(seed, (uint32_t)frame, 21));
    const float stW = std::max(1.f, (float)p.streakWidth);
    const float stP = (float)(p.streaks / 100.0);
    const int slowT = (int)std::floor(t * 2.0);
    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float Y = (float)(c.originY + (y + 0.5) * c.scaleY);
            int band = (int)std::floor(Y / rowH);
            Dash ds[3]; int nd = 0;
            if (dashP > 0.f)
                for (int j = 0; j < 3; ++j) {
                    uint32_t h = hash4((uint32_t)band, (uint32_t)frame, seed, 0xDA5u + (uint32_t)j);
                    if (u01(h) >= dashP * 0.12f) continue;
                    float x0 = u01(hash_u32(h + 1)) * fullW;
                    float u2 = u01(hash_u32(h + 2));
                    float len = (10.f + 160.f * u2 * u2) * (float)(p.dashLength / 100.0);
                    float v = (u01(hash_u32(h + 3)) < 0.85f ? 1.f : -1.f) * (0.12f + 0.4f * u01(hash_u32(h + 4)));
                    ds[nd++] = { x0, x0 + len, v };
                }
            float sl = 1.f;
            if (sDepth > 0.f) {
                float ph = 0.5f * (float)(frame & 1) * (float)(p.interlace / 100.0);
                float s = 0.5f + 0.5f * std::cos(6.2831853f * (Y / per + ph));
                sl = 1.f - sDepth * (1.f - std::pow(s, sExp));
            }
            float fl = gFlick * (1.f + flick * 0.05f * std::sin(6.2831853f * (Y / fullH * 1.5f - (float)t * 0.7f)));
            float envY[1] = { 0 };
            for (int x = 0; x < W; ++x) {
                float X = (float)(c.originX + (x + 0.5) * c.scaleX);
                float* o = out2.at(x, y);
                float add = 0.f;
                for (int j = 0; j < nd; ++j)
                    if (X >= ds[j].x0 && X <= ds[j].x1) {
                        float e = std::min(smoothstepf(ds[j].x0, ds[j].x0 + 4.f, X), smoothstepf(ds[j].x1, ds[j].x1 - 4.f, X));
                        add += ds[j].v * e;
                    }
                if (stP > 0.f) {
                    int col = (int)std::floor(X / stW);
                    uint32_t h = hash3((uint32_t)col, (uint32_t)slowT, seed ^ 0x57u);
                    if (u01(h) < 0.06f * stP + 0.01f) {
                        float env = vnoise1(Y / (fullH * 0.4f) + u01(hash_u32(h + 7)) * 50.f, seed, (uint32_t)col, 1.f);
                        env = std::max(0.f, env - 0.2f);
                        add += gauss(hash_u32(h + 9)) * env * 0.22f * stP;
                    }
                }
                for (int k = 0; k < 3; ++k) o[k] = (o[k] + add) * sl * fl;
            }
            (void)envY;
        }
    });

    // ---------------- 4. tape static -----------------------------------------
    if (p.staticAmount > 0.0) {
        GrainParams g;
        g.type = GT_TAPE; g.frameWidthMm = c.fullW;                 // 1 mm == 1 px
        g.sizeMm = 1.6 * p.staticSize / 100.0; g.aspect = 220; g.amount = p.staticAmount * 1.4;
        g.density = 100; g.sharpness = 55; g.softness = 30; g.rgbGrain = 35; g.rowVar = 60; g.specks = 10;
        g.blend = BM_ADD_SIGNED; g.seed = p.seed + 991; g.evoSpeed = c.fps * p.speed / 100.0; g.evolutionDeg = 0;
        g.shadows = 100; g.highlights = 100;
        render_grain(out2, out2, g, c);
    }
    for (size_t i = 0; i < (size_t)W * H * 4; ++i) dst.px[i] = out2.px[i];
    (void)isy;
}

} // namespace majeed
