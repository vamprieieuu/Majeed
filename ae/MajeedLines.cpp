// MAJEED Random Lines - AE effect definition. Lines are generated and stroked by majeed::render_lines.
#include "MajeedGlue.h"
using namespace majeed;

#define LINES_LIST(F,K,P,C,A,G,E) \
 G(LN_G_LINES,"Lines") \
 F(LN_AMOUNT,"Lines Amount",0,60000,0,10000,2300,0) \
 F(LN_DENSITY,"Line Density",0,100,0,100,100,1) \
 F(LN_MINLEN,"Minimum Length (px)",1,20000,1,1000,80,0) \
 F(LN_MAXLEN,"Maximum Length (px)",1,20000,1,2000,600,0) \
 F(LN_LENSCALE,"Line Length (%)",1,1000,10,400,100,0) \
 F(LN_MINTHK,"Minimum Thickness (px)",0.05,50,0.1,5,0.5,2) \
 F(LN_MAXTHK,"Maximum Thickness (px)",0.05,50,0.1,10,1.2,2) \
 F(LN_THKSCALE,"Line Thickness (%)",1,1000,10,500,100,0) \
 F(LN_SEGMENTS,"Segments (Scribble)",1,8,1,8,3,0) \
 F(LN_KINK,"Kink Angle",0,180,0,180,40,1) \
 F(LN_CURV,"Curvature",0,200,0,100,8,1) \
 F(LN_CURVRND,"Curvature Randomness",0,100,0,100,100,1) \
 E(LN_E_LINES) \
 G(LN_G_DIR,"Direction") \
 A(LN_ANGLE,"Angle",0) \
 F(LN_ANGRND,"Angle Randomness",0,180,0,180,180,1) \
 F(LN_HBIAS,"Horizontal Bias",0,100,0,100,30,1) \
 F(LN_VBIAS,"Vertical Bias",0,100,0,100,0,1) \
 F(LN_DBIAS,"Diagonal Bias",0,100,0,100,0,1) \
 E(LN_E_DIR) \
 G(LN_G_LOOK,"Appearance") \
 F(LN_OPACITY,"Opacity",0,100,0,100,100,1) \
 F(LN_OPRND,"Opacity Randomness",0,100,0,100,60,1) \
 F(LN_BRIGHT,"Brightness",0,200,0,100,78,1) \
 F(LN_BRRND,"Brightness Randomness",0,100,0,100,35,1) \
 C(LN_COLOR,"Line Color",1,1,1) \
 F(LN_COLORAMT,"Color Amount",0,100,0,100,0,1) \
 P(LN_COMP,"Composite","Over Source|Add|Screen|Multiply|Lines Only (Transparent)|Lines on Black",6,1) \
 F(LN_LAYEROP,"Layer Opacity",0,100,0,100,100,1) \
 E(LN_E_LOOK) \
 G(LN_G_DIST,"Distribution") \
 F(LN_SPACING,"Spacing",0,100,0,100,0,1) \
 F(LN_CLUSTER,"Clustering",0,100,0,100,30,1) \
 F(LN_CLSIZE,"Cluster Size (px)",1,5000,10,1000,220,0) \
 F(LN_DISTR,"Distribution",-100,100,-100,100,0,1) \
 E(LN_E_DIST) \
 G(LN_G_ANIM,"Animation") \
 F(LN_SEED,"Random Seed",0,100000,0,1000,1,0) \
 A(LN_EVO,"Evolution",0) \
 F(LN_EVOSPEED,"Evolution Speed (%)",0,2000,0,400,100,1) \
 F(LN_LIFE,"Lifetime (sec, 0 = static)",0,600,0,30,6,2) \
 F(LN_FADE,"Fade",0,100,0,100,35,1) \
 F(LN_FADERND,"Fade Randomness",0,100,0,100,50,1) \
 F(LN_MOTAMT,"Motion Amount (px)",0,2000,0,200,40,1) \
 F(LN_MOTSPD,"Motion Speed (%)",0,1000,0,400,100,1) \
 A(LN_MOTDIR,"Motion Direction",0) \
 F(LN_MOTRND,"Motion Randomness",0,100,0,100,100,1) \
 F(LN_JITTER,"Jitter Amount (px)",0,50,0,10,0.6,2) \
 F(LN_JITSPD,"Jitter Speed (fps)",0,120,0,60,12,1) \
 E(LN_E_ANIM)

MJ_DEFINE_PARAMS(LINES_LIST, LINES)

static void lines_render(const Image& src, const Image& dst, const mj::Vals& v, const FrameCtx& c) {
    LinesParams p;
    p.amount = v[LN_AMOUNT]; p.density = v[LN_DENSITY]; p.minLen = v[LN_MINLEN]; p.maxLen = std::max(v[LN_MINLEN], v[LN_MAXLEN]); p.lengthScale = v[LN_LENSCALE];
    p.minThick = v[LN_MINTHK]; p.maxThick = std::max(v[LN_MINTHK], v[LN_MAXTHK]); p.thickScale = v[LN_THKSCALE];
    p.segments = (int)v[LN_SEGMENTS]; p.kink = v[LN_KINK]; p.curvature = v[LN_CURV]; p.curvRand = v[LN_CURVRND];
    p.angle = v[LN_ANGLE]; p.angleRand = v[LN_ANGRND]; p.horizBias = v[LN_HBIAS]; p.vertBias = v[LN_VBIAS]; p.diagBias = v[LN_DBIAS];
    p.opacity = v[LN_OPACITY]; p.opacityRand = v[LN_OPRND]; p.brightness = v[LN_BRIGHT]; p.brightRand = v[LN_BRRND];
    p.color = v.col(LN_COLOR); p.colorAmt = v[LN_COLORAMT]; p.composite = v.pop(LN_COMP); p.layerOpacity = v[LN_LAYEROP];
    p.spacing = v[LN_SPACING]; p.clustering = v[LN_CLUSTER]; p.clusterSize = v[LN_CLSIZE]; p.distribution = v[LN_DISTR];
    p.seed = (int)v[LN_SEED]; p.evolutionDeg = v[LN_EVO]; p.evoSpeed = v[LN_EVOSPEED]; p.lifetime = v[LN_LIFE]; p.fade = v[LN_FADE]; p.fadeRand = v[LN_FADERND];
    p.motionAmount = v[LN_MOTAMT]; p.motionSpeed = v[LN_MOTSPD]; p.motionDir = v[LN_MOTDIR]; p.motionRand = v[LN_MOTRND];
    p.jitter = v[LN_JITTER]; p.jitterSpeed = v[LN_JITSPD];
    render_lines(src, dst, p, c);
}
static const mj::EffectDef LINES_DEF = { "MAJEED Random Lines", LINES_specs, LINES_COUNT, lines_render,
    "Procedural scribble: every line is generated, animated and stroked natively." };
MJ_EXPORT_EFFECT(EffectMainLines, LINES_DEF)
