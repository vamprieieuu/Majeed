// PiPL resources - one per effect. Built with the SDK's PiPLtool (see vs/MAJEED.vcxproj pre-build step).
// Flag values must equal the flags set in MajeedGlue.h GlobalSetup():
//   out_flags  = PF_OutFlag_DEEP_COLOR_AWARE                                   -> 0x02000000
//   out_flags2 = FLOAT_COLOR_AWARE | SUPPORTS_SMART_RENDER | THREADED_RENDERING -> 0x08001400
// If After Effects reports a flag mismatch, copy the values it prints into this file.
#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
    #include "AE_General.r"
#endif

#define MJ_PIPL(RESID, NAME, ENTRY, MATCH) \
resource 'PiPL' (RESID, NAME) { \
    { \
        Kind { AEEffect }, \
        Name { NAME }, \
        Category { "MAJEED" }, \
        CodeWin64X86 { ENTRY }, \
        CodeMacIntel64 { ENTRY }, \
        CodeMacARM64 { ENTRY }, \
        AE_PiPL_Version { 2, 0 }, \
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS }, \
        AE_Effect_Version { 525313 }, \
        AE_Effect_Info_Flags { 0 }, \
        AE_Effect_Global_OutFlags { 0x02000000 }, \
        AE_Effect_Global_OutFlags_2 { 0x08001400 }, \
        AE_Effect_Match_Name { MATCH }, \
        AE_Reserved_Info { 0 }, \
        AE_Effect_Support_URL { "https://example.invalid/majeed" } \
    } \
};

MJ_PIPL(16000, "MAJEED Grain",        "EffectMainGrain",    "MAJEED Grain")
MJ_PIPL(16001, "MAJEED Random Lines", "EffectMainLines",    "MAJEED Random Lines")
MJ_PIPL(16002, "MAJEED Dither",       "EffectMainDither",   "MAJEED Dither")
MJ_PIPL(16003, "MAJEED VHS",          "EffectMainVHS",      "MAJEED VHS")
MJ_PIPL(16004, "MAJEED Channels",     "EffectMainChannels", "MAJEED Channels")
