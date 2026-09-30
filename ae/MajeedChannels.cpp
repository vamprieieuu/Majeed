// MAJEED Channels - AE effect definition (core/channels.cpp): real per-channel resampling.
#include "MajeedGlue.h"
using namespace majeed;

#define CH_LIST(F,K,P,C,A,G,E) \
 G(CC_G_OFF,"Channel Offsets") \
 F(CC_RX,"Red X (px)",-2000,2000,-50,50,0,2) \
 F(CC_RY,"Red Y (px)",-2000,2000,-50,50,0,2) \
 F(CC_GX,"Green X (px)",-2000,2000,-50,50,0,2) \
 F(CC_GY,"Green Y (px)",-2000,2000,-50,50,0,2) \
 F(CC_BX,"Blue X (px)",-2000,2000,-50,50,0,2) \
 F(CC_BY,"Blue Y (px)",-2000,2000,-50,50,0,2) \
 E(CC_E_OFF) \
 G(CC_G_SEP,"RGB Separation") \
 F(CC_SEP,"Separation (px)",-500,500,-40,40,0,2) \
 A(CC_SEPANG,"Separation Angle",0) \
 P(CC_SEPMODE,"Separation Mode","Linear|Radial",2,1) \
 E(CC_E_SEP) \
 G(CC_G_RAND,"Randomness") \
 F(CC_RAND,"Channel Randomness (px)",0,200,0,30,0,2) \
 F(CC_RANDRATE,"Randomness Rate (fps)",0,120,0,60,24,1) \
 F(CC_SEED,"Random Seed",0,100000,0,1000,0,0) \
 E(CC_E_RAND) \
 G(CC_G_COL,"Color") \
 F(CC_RGAIN,"Red Intensity",0,400,0,200,100,0) \
 F(CC_GGAIN,"Green Intensity",0,400,0,200,100,0) \
 F(CC_BGAIN,"Blue Intensity",0,400,0,200,100,0) \
 F(CC_COLORAMT,"Color Amount (Saturation)",0,400,0,200,100,0) \
 F(CC_HUE,"Hue Shift",-180,180,-180,180,0,1) \
 C(CC_TINT,"Tint Color",1,1,1) \
 F(CC_TINTAMT,"Tint Amount",0,100,0,100,0,1) \
 F(CC_BRIGHT,"Brightness",-100,100,-100,100,0,1) \
 F(CC_CONTRAST,"Contrast",0,400,0,300,100,1) \
 K(CC_INVERT,"Invert",0) \
 E(CC_E_COL)

MJ_DEFINE_PARAMS(CH_LIST, CH)

static void ch_render(const Image& src, const Image& dst, const mj::Vals& v, const FrameCtx& c) {
    ChannelParams p;
    p.rX = v[CC_RX]; p.rY = v[CC_RY]; p.gX = v[CC_GX]; p.gY = v[CC_GY]; p.bX = v[CC_BX]; p.bY = v[CC_BY];
    p.sepAmount = v[CC_SEP]; p.sepAngle = v[CC_SEPANG]; p.sepMode = v.pop(CC_SEPMODE);
    p.random = v[CC_RAND]; p.randomRate = v[CC_RANDRATE]; p.seed = (int)v[CC_SEED];
    p.rGain = v[CC_RGAIN]; p.gGain = v[CC_GGAIN]; p.bGain = v[CC_BGAIN]; p.colorAmt = v[CC_COLORAMT]; p.hue = v[CC_HUE];
    p.tint = v.col(CC_TINT); p.tintAmt = v[CC_TINTAMT]; p.brightness = v[CC_BRIGHT]; p.contrast = v[CC_CONTRAST]; p.invert = v.on(CC_INVERT);
    render_channels(src, dst, p, c);
}
static const mj::EffectDef CH_DEF = { "MAJEED Channels", CH_specs, CH_COUNT, ch_render,
    "Per-channel sub-pixel offsets, radial/linear RGB separation, gains, hue, tint." };
MJ_EXPORT_EFFECT(EffectMainChannels, CH_DEF)
