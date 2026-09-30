// MAJEED Grain - AE effect definition. All pixels are produced by majeed::render_grain (core/grain.cpp).
#include "MajeedGlue.h"
using namespace majeed;

#define GRAIN_LIST(F,K,P,C,A,G,E) \
 G(GR_G_GRAIN,"Grain") \
 P(GR_TYPE,"Grain Type","Gaussian (Sensor)|Film Crystal|Tape (Scanline)|Clustered (fBm)",4,2) \
 P(GR_PRESET,"Size Preset","Custom|4 mm|8 mm|12 mm|16 mm|24 mm|32 mm|48 mm|64 mm",9,1) \
 F(GR_SIZE,"Grain Size (mm)",4,64,4,64,8,1) \
 F(GR_FRAMEW,"Print Width (mm)",50,50000,500,5000,2400,0) \
 F(GR_AMOUNT,"Grain Amount",0,400,0,200,50,1) \
 F(GR_DENSITY,"Grain Density",0,100,0,100,100,1) \
 F(GR_CONTRAST,"Grain Contrast",0,400,0,300,100,1) \
 F(GR_BRIGHT,"Grain Brightness",-100,100,-100,100,0,1) \
 F(GR_SHARP,"Grain Sharpness",0,100,0,100,30,1) \
 F(GR_SOFT,"Grain Softness",0,100,0,100,50,1) \
 F(GR_THRESH,"Threshold",0,100,0,100,0,1) \
 F(GR_DIST,"Distribution",-100,100,-100,100,0,1) \
 F(GR_CLUMP,"Clumping",0,200,0,100,0,1) \
 F(GR_FINE,"Fine Detail",0,200,0,100,0,1) \
 F(GR_COARSE,"Coarse Detail",0,200,0,100,0,1) \
 F(GR_ASPECT,"Grain Aspect (X stretch %)",20,1000,50,500,100,0) \
 F(GR_SPECKS,"Specks / Dropouts",0,100,0,100,0,1) \
 F(GR_ROWVAR,"Tape: Row Variation",0,100,0,100,40,1) \
 F(GR_ROWBLEED,"Tape: Row Bleed",0,100,0,100,0,1) \
 F(GR_SHADOWS,"Shadows Response",0,200,0,200,100,0) \
 F(GR_HIGHS,"Highlights Response",0,200,0,200,100,0) \
 E(GR_E_GRAIN) \
 G(GR_G_ANIM,"Animation") \
 F(GR_SEED,"Random Seed",0,100000,0,1000,0,0) \
 A(GR_EVO,"Evolution",0) \
 F(GR_EVOSPEED,"Evolution Speed (steps/sec)",0,1000,0,60,24,1) \
 K(GR_SMOOTH,"Smooth Evolution",0) \
 E(GR_E_ANIM) \
 G(GR_G_COLOR,"Color") \
 K(GR_MONO,"Monochrome",0) \
 F(GR_RGBGRAIN,"RGB Grain",0,100,0,100,100,1) \
 F(GR_RED,"Red Amount",0,300,0,200,100,0) \
 F(GR_GREEN,"Green Amount",0,300,0,200,100,0) \
 F(GR_BLUE,"Blue Amount",0,300,0,200,100,0) \
 F(GR_COLORVAR,"Color Variation",0,100,0,100,0,1) \
 F(GR_COLORRAND,"Color Randomness",0,100,0,100,0,1) \
 F(GR_CHANVAR,"Channel Variation",0,100,0,100,0,1) \
 F(GR_SAT,"Saturation",0,300,0,200,100,0) \
 F(GR_RGBSEP,"RGB Separation (px)",-50,50,-20,20,0,2) \
 C(GR_GCOLOR,"Grain Color (Tint)",1,1,1) \
 F(GR_COLORAMT,"Color Amount",0,100,0,100,0,1) \
 E(GR_E_COLOR) \
 G(GR_G_OUT,"Output") \
 F(GR_OPACITY,"Opacity",0,100,0,100,100,1) \
 P(GR_BLEND,"Blend Mode","Add (Signed)|Normal|Overlay|Soft Light|Hard Light|Linear Light|Multiply|Screen|Difference|Lighten|Darken|Grain Only",12,1) \
 E(GR_E_OUT)

MJ_DEFINE_PARAMS(GRAIN_LIST, GRAIN)

static void grain_render(const Image& src, const Image& dst, const mj::Vals& v, const FrameCtx& c) {
    GrainParams p;
    p.type = v.pop(GR_TYPE);
    static const double presets[] = { 0, 4, 8, 12, 16, 24, 32, 48, 64 };
    int pr = v.pop(GR_PRESET);
    p.sizeMm = pr > 0 ? presets[pr] : v[GR_SIZE];
    p.frameWidthMm = v[GR_FRAMEW];
    p.amount = v[GR_AMOUNT]; p.density = v[GR_DENSITY]; p.contrast = v[GR_CONTRAST]; p.brightness = v[GR_BRIGHT];
    p.sharpness = v[GR_SHARP]; p.softness = v[GR_SOFT]; p.threshold = v[GR_THRESH]; p.distribution = v[GR_DIST]; p.clumping = v[GR_CLUMP];
    p.fine = v[GR_FINE]; p.coarse = v[GR_COARSE]; p.aspect = v[GR_ASPECT]; p.specks = v[GR_SPECKS];
    p.rowVar = v[GR_ROWVAR]; p.rowBleed = v[GR_ROWBLEED]; p.shadows = v[GR_SHADOWS]; p.highlights = v[GR_HIGHS];
    p.seed = (int)v[GR_SEED]; p.evolutionDeg = v[GR_EVO]; p.evoSpeed = v[GR_EVOSPEED]; p.smoothEvo = v.on(GR_SMOOTH);
    p.mono = v.on(GR_MONO); p.rgbGrain = v[GR_RGBGRAIN]; p.redAmt = v[GR_RED]; p.greenAmt = v[GR_GREEN]; p.blueAmt = v[GR_BLUE];
    p.colorVar = v[GR_COLORVAR]; p.colorRand = v[GR_COLORRAND]; p.chanVar = v[GR_CHANVAR]; p.saturation = v[GR_SAT];
    p.rgbSep = v[GR_RGBSEP]; p.grainColor = v.col(GR_GCOLOR); p.colorAmt = v[GR_COLORAMT];
    p.opacity = v[GR_OPACITY]; p.blend = v.pop(GR_BLEND);
    render_grain(src, dst, p, c);
}
static const mj::EffectDef GRAIN_DEF = { "MAJEED Grain", GRAIN_specs, GRAIN_COUNT, grain_render,
    "4 procedural grain generators, 4-64 mm grain size." };
MJ_EXPORT_EFFECT(EffectMainGrain, GRAIN_DEF)
