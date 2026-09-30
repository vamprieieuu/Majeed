// ============================================================================
// MAJEED - After Effects SDK glue (shared by all effects).
// Table-driven: every effect declares its parameters once (X-macro list) and a
// render function that maps the parameter values onto the native engine in ../core.
// Uses SmartFX (8 / 16 / 32-bit float). NO native AE effects are applied here.
// ============================================================================
#pragma once
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AEFX_SuiteHelper.h"
#include <vector>
#include <cstring>
#include "../core/majeed_core.h"

#define MAJEED_MAJOR 1
#define MAJEED_MINOR 0
#define MAJEED_BUG   0
#define MAJEED_BUILD 1

namespace mj {

enum { T_FLOAT, T_CHECK, T_POPUP, T_COLOR, T_ANGLE, T_GROUP, T_GROUP_END };
struct PSpec {
    int t; const char* name;
    double mn, mx, smn, smx, def; int prec;      // float
    const char* popup; int npop;                 // popup ("a|b|c"), def = 1-based default
    float r, g, b;                               // colour default
};
static const int MAXP = 96;

// X-macro helpers:  expansion 1 -> enum,  expansion 2 -> PSpec table
#define MJ_ENUM_F(id,name,mn,mx,smn,smx,def,prec) id,
#define MJ_ENUM_K(id,name,def) id,
#define MJ_ENUM_P(id,name,list,n,def) id,
#define MJ_ENUM_C(id,name,r,g,b) id,
#define MJ_ENUM_A(id,name,def) id,
#define MJ_ENUM_G(id,name) id,
#define MJ_ENUM_E(id) id,
#define MJ_SPEC_F(id,name,mn,mx,smn,smx,def,prec) { mj::T_FLOAT, name, mn, mx, smn, smx, def, prec, 0, 0, 0, 0, 0 },
#define MJ_SPEC_K(id,name,def) { mj::T_CHECK, name, 0, 1, 0, 1, def, 0, 0, 0, 0, 0, 0 },
#define MJ_SPEC_P(id,name,list,n,def) { mj::T_POPUP, name, 1, n, 1, n, def, 0, list, n, 0, 0, 0 },
#define MJ_SPEC_C(id,name,r,g,b) { mj::T_COLOR, name, 0, 1, 0, 1, 0, 0, 0, 0, r, g, b },
#define MJ_SPEC_A(id,name,def) { mj::T_ANGLE, name, 0, 0, 0, 0, def, 0, 0, 0, 0, 0, 0 },
#define MJ_SPEC_G(id,name) { mj::T_GROUP, name, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
#define MJ_SPEC_E(id) { mj::T_GROUP_END, "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
#define MJ_DEFINE_PARAMS(LIST, PFX) \
    enum PFX##_ids { LIST(MJ_ENUM_F, MJ_ENUM_K, MJ_ENUM_P, MJ_ENUM_C, MJ_ENUM_A, MJ_ENUM_G, MJ_ENUM_E) PFX##_COUNT }; \
    static const mj::PSpec PFX##_specs[] = { LIST(MJ_SPEC_F, MJ_SPEC_K, MJ_SPEC_P, MJ_SPEC_C, MJ_SPEC_A, MJ_SPEC_G, MJ_SPEC_E) };

// Parameter values handed to the effect's render function
struct Vals {
    double f[MAXP]; float c[MAXP][3];
    double operator[](int i) const { return f[i]; }
    bool   on(int i) const { return f[i] > 0.5; }
    int    pop(int i) const { return (int)f[i]; }               // 0-based popup index
    majeed::RGB col(int i) const { return { c[i][0], c[i][1], c[i][2] }; }
};
typedef void (*RenderFn)(const majeed::Image& src, const majeed::Image& dst, const Vals& v, const majeed::FrameCtx& ctx);
struct EffectDef { const char* name; const PSpec* specs; int nspecs; RenderFn render; const char* about; };

// ------------------------------------------------------------- param setup --
inline PF_Err ParamsSetup(PF_InData* in, PF_OutData* out, const EffectDef& d) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;
    for (int i = 0; i < d.nspecs && !err; ++i) {
        const PSpec& s = d.specs[i];
        AEFX_CLR_STRUCT(def);
        const int id = i + 1;
        switch (s.t) {
        case T_FLOAT:
            PF_ADD_FLOAT_SLIDERX(s.name, s.mn, s.mx, s.smn, s.smx, s.def, s.prec, 0, 0, 0, id); break;
        case T_CHECK:
            PF_ADD_CHECKBOX(s.name, "On", s.def > 0.5 ? 1 : 0, 0, id); break;
        case T_POPUP:
            PF_ADD_POPUP(s.name, s.npop, (int)s.def, s.popup, id); break;
        case T_COLOR:
            PF_ADD_COLOR(s.name, (A_u_char)(s.r * 255.f + 0.5f), (A_u_char)(s.g * 255.f + 0.5f), (A_u_char)(s.b * 255.f + 0.5f), id); break;
        case T_ANGLE:
            PF_ADD_ANGLE(s.name, (int)s.def, id); break;
        case T_GROUP:
            PF_ADD_TOPIC(s.name, id); break;
        case T_GROUP_END:
            PF_END_TOPIC(id); break;
        }
    }
    out->num_params = d.nspecs + 1;
    return err;
}

inline PF_Err GlobalSetup(PF_InData*, PF_OutData* out) {
    out->my_version = PF_VERSION(MAJEED_MAJOR, MAJEED_MINOR, MAJEED_BUG, PF_Stage_RELEASE, MAJEED_BUILD);
    out->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE;
    out->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}

inline PF_Err PreRender(PF_InData* in, PF_OutData*, PF_PreRenderExtra* extra) {
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult res;
    req.rect.left = 0; req.rect.top = 0; req.rect.right = in->width; req.rect.bottom = in->height;  // whole layer
    req.preserve_rgb_of_zero_alpha = TRUE;
    ERR(extra->cb->checkout_layer(in->effect_ref, 0, 0, &req, in->current_time, in->time_step, in->time_scale, &res));
    if (!err) {
        extra->output->result_rect = res.result_rect;
        extra->output->max_result_rect = res.max_result_rect;
        extra->output->solid = FALSE;
        extra->output->pre_render_data = 0;
    }
    return err;
}

// --------------------------------------------------------- pixel conversion --
template <class P, class C> struct PixIO;
struct P8  { static const int fmt = 8; };
inline float ch8(A_u_char v)  { return v * (1.f / 255.f); }
inline float ch16(A_u_short v){ return v * (1.f / 32768.f); }

inline void load_image(const PF_EffectWorld* w, PF_PixelFormat fmt, std::vector<float>& buf) {
    const int W = w->width, H = w->height; buf.resize((size_t)W * H * 4);
    for (int y = 0; y < H; ++y) {
        const char* row = (const char*)w->data + (size_t)y * w->rowbytes;
        float* o = &buf[(size_t)y * W * 4];
        for (int x = 0; x < W; ++x, o += 4) {
            float a, r, g, b;
            if (fmt == PF_PixelFormat_ARGB32) { const PF_Pixel8* p = (const PF_Pixel8*)row + x; a = ch8(p->alpha); r = ch8(p->red); g = ch8(p->green); b = ch8(p->blue); }
            else if (fmt == PF_PixelFormat_ARGB64) { const PF_Pixel16* p = (const PF_Pixel16*)row + x; a = ch16(p->alpha); r = ch16(p->red); g = ch16(p->green); b = ch16(p->blue); }
            else { const PF_PixelFloat* p = (const PF_PixelFloat*)row + x; a = p->alpha; r = p->red; g = p->green; b = p->blue; }
            if (a > 1e-6f) { r /= a; g /= a; b /= a; } else { r = g = b = 0.f; }      // AE buffers are premultiplied
            o[0] = r; o[1] = g; o[2] = b; o[3] = a;
        }
    }
}
inline void store_image(const std::vector<float>& buf, int bw, int bh, PF_EffectWorld* w, PF_PixelFormat fmt) {
    const int W = std::min(bw, (int)w->width), H = std::min(bh, (int)w->height);
    for (int y = 0; y < H; ++y) {
        char* row = (char*)w->data + (size_t)y * w->rowbytes;
        const float* s = &buf[(size_t)y * bw * 4];
        for (int x = 0; x < W; ++x, s += 4) {
            float a = majeed::clampf(s[3], 0.f, 1.f), r = s[0] * a, g = s[1] * a, b = s[2] * a;
            if (fmt == PF_PixelFormat_ARGB32) {
                PF_Pixel8* p = (PF_Pixel8*)row + x;
                p->alpha = (A_u_char)(a * 255.f + 0.5f);
                p->red = (A_u_char)(majeed::clampf(r, 0.f, 1.f) * 255.f + 0.5f); p->green = (A_u_char)(majeed::clampf(g, 0.f, 1.f) * 255.f + 0.5f); p->blue = (A_u_char)(majeed::clampf(b, 0.f, 1.f) * 255.f + 0.5f);
            } else if (fmt == PF_PixelFormat_ARGB64) {
                PF_Pixel16* p = (PF_Pixel16*)row + x;
                p->alpha = (A_u_short)(a * 32768.f + 0.5f);
                p->red = (A_u_short)(majeed::clampf(r, 0.f, 1.f) * 32768.f + 0.5f); p->green = (A_u_short)(majeed::clampf(g, 0.f, 1.f) * 32768.f + 0.5f); p->blue = (A_u_short)(majeed::clampf(b, 0.f, 1.f) * 32768.f + 0.5f);
            } else {
                PF_PixelFloat* p = (PF_PixelFloat*)row + x; p->alpha = a; p->red = r; p->green = g; p->blue = b;
            }
        }
    }
}

// ------------------------------------------------------------- smart render --
inline PF_Err SmartRender(PF_InData* in, PF_OutData* out, PF_SmartRenderExtra* extra, const EffectDef& d) {
    PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
    PF_EffectWorld *inW = NULL, *outW = NULL;
    ERR(extra->cb->checkout_layer_pixels(in->effect_ref, 0, &inW));
    ERR(extra->cb->checkout_output(in->effect_ref, &outW));
    if (err || !inW || !outW) return err ? err : PF_Err_INTERNAL_STRUCT_DAMAGED;

    // ---- parameters ------------------------------------------------------
    Vals v; std::memset(&v, 0, sizeof(v));
    for (int i = 0; i < d.nspecs && !err; ++i) {
        const PSpec& s = d.specs[i];
        if (s.t == T_GROUP || s.t == T_GROUP_END) continue;
        PF_ParamDef pd; AEFX_CLR_STRUCT(pd);
        ERR(PF_CHECKOUT_PARAM(in, i + 1, in->current_time, in->time_step, in->time_scale, &pd));
        if (err) break;
        switch (s.t) {
        case T_FLOAT: v.f[i] = pd.u.fs_d.value; break;
        case T_CHECK: v.f[i] = pd.u.bd.value ? 1.0 : 0.0; break;
        case T_POPUP: v.f[i] = (double)(pd.u.pd.value - 1); break;
        case T_COLOR: v.c[i][0] = pd.u.cd.value.red / 255.f; v.c[i][1] = pd.u.cd.value.green / 255.f; v.c[i][2] = pd.u.cd.value.blue / 255.f; break;
        case T_ANGLE: v.f[i] = pd.u.ad.value / 65536.0; break;
        }
        ERR2(PF_CHECKIN_PARAM(in, &pd));
    }
    if (err) return err;

    // ---- pixel format ----------------------------------------------------
    PF_PixelFormat fmt = PF_PixelFormat_INVALID;
    AEFX_SuiteScoper<PF_WorldSuite2> ws(in, kPF_WorldSuite, kPF_WorldSuiteVersion2, out);
    ERR(ws->PF_GetPixelFormat(inW, &fmt));
    if (err) return err;

    // ---- frame context (everything in full-resolution layer pixels) --------
    majeed::FrameCtx ctx;
    ctx.timeSec = in->time_scale ? (double)in->current_time / (double)in->time_scale : 0.0;
    ctx.fps = in->time_step > 0 ? (double)in->time_scale / (double)in->time_step : 24.0;
    double sx = in->downsample_x.num ? (double)in->downsample_x.den / (double)in->downsample_x.num : 1.0;
    double sy = in->downsample_y.num ? (double)in->downsample_y.den / (double)in->downsample_y.num : 1.0;
    ctx.scaleX = sx; ctx.scaleY = sy;
    ctx.originX = 0; ctx.originY = 0;
    ctx.fullW = (double)in->width * sx; ctx.fullH = (double)in->height * sy;
    if (in->pixel_aspect_ratio.den) ctx.pixelAspect = (double)in->pixel_aspect_ratio.num / (double)in->pixel_aspect_ratio.den;

    // ---- run the native engine -------------------------------------------
    std::vector<float> srcBuf, dstBuf;
    load_image(inW, fmt, srcBuf);
    dstBuf.assign(srcBuf.size(), 0.f);
    majeed::Image src{ (int)inW->width, (int)inW->height, srcBuf.data() };
    majeed::Image dst{ (int)inW->width, (int)inW->height, dstBuf.data() };
    d.render(src, dst, v, ctx);
    store_image(dstBuf, src.w, src.h, outW, fmt);

    ERR2(extra->cb->checkin_layer_pixels(in->effect_ref, 0));
    return err;
}

// ------------------------------------------------------------ main dispatch --
inline PF_Err Dispatch(PF_Cmd cmd, PF_InData* in, PF_OutData* out, void* extra, const EffectDef& d) {
    PF_Err err = PF_Err_NONE;
    switch (cmd) {
    case PF_Cmd_ABOUT:
        PF_SPRINTF(out->return_msg, "%s\rMAJEED native engine.\r%s", d.name, d.about); break;
    case PF_Cmd_GLOBAL_SETUP:  err = GlobalSetup(in, out); break;
    case PF_Cmd_PARAMS_SETUP:  err = ParamsSetup(in, out, d); break;
    case PF_Cmd_SMART_PRE_RENDER: err = PreRender(in, out, (PF_PreRenderExtra*)extra); break;
    case PF_Cmd_SMART_RENDER:  err = SmartRender(in, out, (PF_SmartRenderExtra*)extra, d); break;
    default: break;
    }
    return err;
}

} // namespace mj

#define MJ_EXPORT_EFFECT(FUNC, DEF) \
    extern "C" DllExport PF_Err FUNC(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) { \
        (void)params; (void)output; \
        try { return mj::Dispatch(cmd, in_data, out_data, extra, DEF); } catch (PF_Err e) { return e; } catch (...) { return PF_Err_INTERNAL_STRUCT_DAMAGED; } \
    }
