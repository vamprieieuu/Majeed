// ============================================================================
// MAJEED CHANNELS - true per-channel resampling.
// R, G and B are each sampled from the source at their own sub-pixel position
// (manual offsets + linear/radial separation + per-row/per-frame random jitter),
// then gain, hue rotation (YIQ), saturation, tint, brightness/contrast, invert.
// ============================================================================
#include "majeed_core.h"

namespace majeed {

void render_channels(const Image& src, const Image& dst, const ChannelParams& p, const FrameCtx& c) {
    const int W = src.w, H = src.h;
    const float isx = (float)(1.0 / c.scaleX), isy = (float)(1.0 / c.scaleY);
    const float off[3][2] = { { (float)p.rX * isx, (float)p.rY * isy },
                              { (float)p.gX * isx, (float)p.gY * isy },
                              { (float)p.bX * isx, (float)p.bY * isy } };
    const float sepA = (float)p.sepAngle * 0.017453293f;
    const float sdx = std::cos(sepA), sdy = std::sin(sepA);
    const float sepAmt = (float)p.sepAmount;
    const float sgn[3] = { 1.f, 0.f, -1.f };
    const float cxm = W * 0.5f, cym = H * 0.5f, halfDiag = std::sqrt(cxm * cxm + cym * cym);
    const float rnd = (float)p.random * isx;
    const int rStep = (int)std::floor(c.timeSec * std::max(0.0, p.randomRate) + 1e-4);
    const uint32_t seed = hash_u32((uint32_t)p.seed + 0xC4A7u);
    const float gain[3] = { (float)(p.rGain / 100.0), (float)(p.gGain / 100.0), (float)(p.bGain / 100.0) };
    const float sat = (float)(p.colorAmt / 100.0);
    const float hue = (float)p.hue * 0.017453293f, ch = std::cos(hue), sh = std::sin(hue);
    const float tint[3] = { p.tint.r, p.tint.g, p.tint.b };
    const float tAmt = (float)clampf((float)(p.tintAmt / 100.0), 0.f, 1.f);
    const float contrast = (float)(p.contrast / 100.0), bright = (float)(p.brightness / 100.0);

    parallel_rows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float rowJ[3] = { 0, 0, 0 };
            if (rnd > 0.f) for (int k = 0; k < 3; ++k) rowJ[k] = s11(hash4((uint32_t)y, (uint32_t)rStep, seed, (uint32_t)k)) * rnd;
            for (int x = 0; x < W; ++x) {
                float v[3], tmp[4];
                for (int k = 0; k < 3; ++k) {
                    float ox = off[k][0] + rowJ[k], oy = off[k][1];
                    if (sepAmt != 0.f && sgn[k] != 0.f) {
                        if (p.sepMode == 0) { ox += sgn[k] * sepAmt * sdx * isx; oy += sgn[k] * sepAmt * sdy * isy; }
                        else { float rx = (x - cxm) / halfDiag, ry = (y - cym) / halfDiag; ox += sgn[k] * sepAmt * rx * isx; oy += sgn[k] * sepAmt * ry * isy; }
                    }
                    sample_bilinear(src, x + 0.5f - ox - 0.5f, y - oy, tmp);
                    v[k] = tmp[k] * gain[k];
                }
                if (hue != 0.f || sat != 1.f) {
                    float Y, I, Q; rgb2yiq(v[0], v[1], v[2], Y, I, Q);
                    float I2 = (I * ch - Q * sh) * sat, Q2 = (I * sh + Q * ch) * sat;
                    yiq2rgb(Y, I2, Q2, v[0], v[1], v[2]);
                }
                if (tAmt > 0.f) for (int k = 0; k < 3; ++k) v[k] = lerpf(v[k], v[k] * tint[k], tAmt);
                float* o = dst.at(x, y);
                for (int k = 0; k < 3; ++k) {
                    float t = (v[k] - 0.5f) * contrast + 0.5f + bright;
                    o[k] = p.invert ? 1.f - t : t;
                }
                o[3] = src.at(x, y)[3];
            }
        }
    });
}

} // namespace majeed
