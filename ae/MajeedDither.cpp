// MAJEED Dither - AE effect definition. Algorithms live in core/dither.cpp (real kernels / threshold maps).
#include "MajeedGlue.h"
#include "DitherAlgoList.h"
using namespace majeed;

#define DITHER_LIST(F,K,P,C,A,G,E) \
 G(DT_G_DITHER,"Dither") \
 P(DT_ALGO,"Algorithm",MJ_DITHER_ALGO_POPUP,MJ_DITHER_ALGO_COUNT,1) \
 P(DT_MODE,"Color Mode","Preserve Original Colors|Black & White|Two-Tone",3,1) \
 F(DT_LEVELS,"Tones per Channel",2,64,2,32,2,0) \
 F(DT_SIZE,"Dither Pixel Size (px)",1,64,1,16,1,0) \
 F(DT_AMOUNT,"Amount",0,100,0,100,100,1) \
 F(DT_STRENGTH,"Diffusion / Spread Strength",0,200,0,200,100,1) \
 K(DT_SERP,"Serpentine Scan",0) \
 F(DT_NOISE,"Threshold Noise",0,100,0,100,0,1) \
 F(DT_SEED,"Random Seed",0,100000,0,1000,0,0) \
 E(DT_E_DITHER) \
 G(DT_G_TONE,"Tone") \
 F(DT_CONTRAST,"Contrast",0,400,0,300,100,1) \
 F(DT_BRIGHT,"Brightness",-100,100,-100,100,0,1) \
 K(DT_LINEAR,"Linear-Light Dithering",0) \
 K(DT_INVERT,"Invert",0) \
 C(DT_DARK,"Two-Tone Dark",0,0,0) \
 C(DT_LIGHT,"Two-Tone Light",1,1,1) \
 E(DT_E_TONE) \
 G(DT_G_PATTERN,"Screen Pattern") \
 F(DT_PSCALE,"Pattern Scale (%)",25,800,25,400,100,0) \
 F(DT_PANGLE,"Pattern Angle Offset",-180,180,-90,90,0,1) \
 E(DT_E_PATTERN)

MJ_DEFINE_PARAMS(DITHER_LIST, DITHER)

static void dither_render(const Image& src, const Image& dst, const mj::Vals& v, const FrameCtx& c) {
    DitherParams p;
    p.algo = v.pop(DT_ALGO); p.mode = v.pop(DT_MODE); p.levels = (int)(v[DT_LEVELS] + 0.5); p.size = v[DT_SIZE];
    p.amount = v[DT_AMOUNT]; p.strength = v[DT_STRENGTH]; p.serpentine = v.on(DT_SERP); p.noise = v[DT_NOISE]; p.seed = (int)v[DT_SEED];
    p.contrast = v[DT_CONTRAST]; p.brightness = v[DT_BRIGHT]; p.linear = v.on(DT_LINEAR); p.invert = v.on(DT_INVERT);
    p.dark = v.col(DT_DARK); p.light = v.col(DT_LIGHT); p.patternScale = v[DT_PSCALE]; p.patternAngle = v[DT_PANGLE];
    render_dither(src, dst, p, c);
}
static const mj::EffectDef DITHER_DEF = { "MAJEED Dither", DITHER_specs, DITHER_COUNT, dither_render,
    "Error diffusion (FS, JJN, Stucki, Atkinson, Burkes, Sierra...), Bayer, blue noise, halftone screens." };
MJ_EXPORT_EFFECT(EffectMainDither, DITHER_DEF)
