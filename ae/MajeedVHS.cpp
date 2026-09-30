// MAJEED VHS - AE effect definition (core/vhs.cpp).
#include "MajeedGlue.h"
using namespace majeed;

#define VHS_LIST(F,K,P,C,A,G,E) \
 G(VH_G_TRACK,"Tracking / Geometry") \
 F(VH_TRACK,"Tracking Wobble (px)",0,300,0,100,20,1) \
 F(VH_TRACKSIZE,"Wobble Size (%)",1,400,1,200,40,0) \
 F(VH_TRACKSPD,"Tracking Speed (%)",0,1000,0,300,50,0) \
 F(VH_HEAD,"Head-Switch Skew (px)",0,400,0,150,40,1) \
 F(VH_HEADH,"Head-Switch Height (%)",0,50,0,30,6,1) \
 F(VH_JIT,"Line Jitter (px)",0,200,0,60,15,1) \
 F(VH_JITRATE,"Jitter Rate (%)",0,1000,0,300,100,0) \
 E(VH_E_TRACK) \
 G(VH_G_COLOR,"Color / Chroma") \
 F(VH_BLEED,"Chroma Bleed",0,100,0,100,30,1) \
 F(VH_BLEEDSHIFT,"Chroma Shift (px)",-30,30,-10,10,2,1) \
 F(VH_LUMASOFT,"Luma Softness",0,100,0,100,10,1) \
 F(VH_RSEP,"Red Offset X (px)",-50,50,-20,20,0,2) \
 F(VH_BSEP,"Blue Offset X (px)",-50,50,-20,20,0,2) \
 F(VH_SAT,"Saturation",0,300,0,200,100,0) \
 E(VH_E_COLOR) \
 G(VH_G_NOISE,"Noise / Artifacts") \
 F(VH_STATIC,"Static (Tape Grain)",0,300,0,100,35,1) \
 F(VH_STATICSIZE,"Static Size (%)",10,600,25,300,100,0) \
 F(VH_DASH,"Tape Dashes",0,100,0,100,30,1) \
 F(VH_DASHLEN,"Dash Length (%)",10,1000,10,500,100,0) \
 F(VH_DASHROWS,"Dash Row Height (px)",1,16,1,8,2,0) \
 F(VH_STREAK,"Vertical Streaks",0,100,0,100,25,1) \
 F(VH_STREAKW,"Streak Width (px)",1,32,1,16,2,0) \
 E(VH_E_NOISE) \
 G(VH_G_SCAN,"Scanlines / Flicker") \
 F(VH_SCAN,"Scanlines",0,100,0,100,30,1) \
 F(VH_SCANPER,"Scanline Period (px)",1.5,64,2,16,3,1) \
 F(VH_SCANSHARP,"Scanline Sharpness",0,100,0,100,50,1) \
 F(VH_INTERLACE,"Interlace Phase",0,100,0,100,30,1) \
 F(VH_FLICKER,"Flicker",0,100,0,100,20,1) \
 E(VH_E_SCAN) \
 G(VH_G_ANIM,"Animation") \
 F(VH_SPEED,"Speed (%)",0,1000,0,400,100,0) \
 F(VH_SEED,"Random Seed",0,100000,0,1000,0,0) \
 E(VH_E_ANIM)

MJ_DEFINE_PARAMS(VHS_LIST, VHS)

static void vhs_render(const Image& src, const Image& dst, const mj::Vals& v, const FrameCtx& c) {
    VHSParams p;
    p.trackAmount = v[VH_TRACK]; p.trackSize = v[VH_TRACKSIZE]; p.trackSpeed = v[VH_TRACKSPD]; p.headSwitch = v[VH_HEAD]; p.headHeight = v[VH_HEADH];
    p.jitterAmount = v[VH_JIT]; p.jitterRate = v[VH_JITRATE];
    p.bleed = v[VH_BLEED]; p.bleedShift = v[VH_BLEEDSHIFT]; p.lumaSoft = v[VH_LUMASOFT]; p.rSepX = v[VH_RSEP]; p.bSepX = v[VH_BSEP]; p.saturation = v[VH_SAT];
    p.staticAmount = v[VH_STATIC]; p.staticSize = v[VH_STATICSIZE]; p.dashes = v[VH_DASH]; p.dashLength = v[VH_DASHLEN]; p.dashRows = v[VH_DASHROWS];
    p.streaks = v[VH_STREAK]; p.streakWidth = v[VH_STREAKW];
    p.scanlines = v[VH_SCAN]; p.scanPeriod = v[VH_SCANPER]; p.scanSharp = v[VH_SCANSHARP]; p.interlace = v[VH_INTERLACE]; p.flicker = v[VH_FLICKER];
    p.speed = v[VH_SPEED]; p.seed = (int)v[VH_SEED];
    render_vhs(src, dst, p, c);
}
static const mj::EffectDef VHS_DEF = { "MAJEED VHS", VHS_specs, VHS_COUNT, vhs_render,
    "Tracking, head-switch skew, chroma bleed, dashes, streaks, scanlines, tape static." };
MJ_EXPORT_EFFECT(EffectMainVHS, VHS_DEF)
