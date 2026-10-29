// NFS Most Wanted (PC v1.3, speed.exe sha256 80774c2e...) addresses
#pragma once
#include <stdint.h>

namespace G {
// ---- globals
static uint8_t** const pFEDatabase      = (uint8_t**)0x91CF90;  // cFrontendDatabase*
static int*      const pQRMode          = (int*)0x9B9E68;
static int*      const pSimUserMode     = (int*)0x988E04;       // Sim::mUserMode (1 = split screen)
static int*      const pGameFlowState   = (int*)0x925E90;       // TheGameFlowManager state (6 = in game)
static int*      const pCurrentViewMode = (int*)0x982B3C;
static int*      const pRenderViewMode  = (int*)0x982B40;
static void**    const ppD3DDevice      = (void**)0x982BDC;     // IDirect3DDevice9*
static int*      const pCarLoaderPool2P = (int*)0x8F7EF4;       // CarLoaderPoolSizes[1] (KB)
static int*      const pCarLoaderPool1P = (int*)0x8F7EF0;       // CarLoaderPoolSizes[0] (KB)
static uint8_t*  const pIOModule        = (uint8_t*)0x920DE0;   // IOModule instance

// eViews array, sizeof(eView) == 0x70
static const uintptr_t EVIEWS = 0x9195E0;
static inline uint8_t* View(int id) { return (uint8_t*)(EVIEWS + id * 0x70); }
// eView offsets
enum {
    EV_PLAT      = 0x00, // eViewPlatInfo*
    EV_ID        = 0x04,
    EV_ACTIVE    = 0x08, // int8
    EV_CAMERA    = 0x40, // Camera*
    EV_MOVERLIST = 0x44, // bTList<CameraMover> {next, prev}
    EV_RT        = 0x60, // render target info: +0x10 active, +0x14 width, +0x18 height
    EV_SCREENFX  = 0x64,
    EV_RAIN      = 0x68,
};

// FEDatabase offsets
enum {
    FEDB_NUMPLAYERS = 0x00,  // uint8
    FEDB_JOYPORT0   = 0x0C,  // int8
    FEDB_JOYPORT1   = 0x0D,  // int8
    FEDB_GAMEMODE   = 0x12C, // uint32, 4 = quick race
};

// ---- functions
// cdecl
typedef void  (*VoidFn)();
typedef void  (*IntFn)(int);
typedef void* (*NewFn)(unsigned);
typedef unsigned (*HashFn)(const char*);
static VoidFn   const eDisplayFrameInner = (VoidFn)0x6DE300;  // ___eDisplayFrame
static VoidFn   const PostDisplayFX      = (VoidFn)0x6C3870;
static IntFn    const DisplayComposite   = (IntFn)0x6E6E40;   // (1) before scene, (0) after scene + FE
static VoidFn   const ResetRenderStates  = (VoidFn)0x6CFCE0;
static NewFn    const GameNew            = (NewFn)0x652AD0;
static HashFn   const stringhash32       = (HashFn)0x5CC240;

// call sites we patch
static const uintptr_t CALL_eDisplayFrameInner = 0x6E759C;
static const uintptr_t CALL_FEManagerRender    = 0x6E71D0;
static const uintptr_t CALL_QRMenu_AddOption2  = 0x7AA81C;
static const uintptr_t JE_PressStartSamePort   = 0x7A3F6C;
static const uintptr_t JNE_ViewModeSwitch      = 0x6CFC09;

static const uintptr_t FEManager_Render        = 0x516F70;  // thiscall
static const uintptr_t IconScrollerMenu_AddOption = 0x573960; // thiscall(IconOption*)
static const uintptr_t IconOption_ctor         = 0x586FA0;  // thiscall(tex,name,desc)
static const uintptr_t CustomRace_vtable       = 0x8B7590;  // {dtor, React}
static const uintptr_t eView_SetupWorldLight   = 0x5029F0;  // thiscall()
static const uintptr_t eView_CalcMatrices      = 0x6CF400;  // thiscall(plat)(view, f, farz, f, int) ret 0x14

// InputDevice base helpers (thiscall, use vtable[5] and fields +0x1c/+0x20)
static const uintptr_t InputDevice_HasChanged  = 0x627110;
static const uintptr_t InputDevice_AnyNonZero  = 0x627160;
static const uintptr_t InputDevice_Snapshot    = 0x6271A0;
static const uintptr_t InputDevice_Restore     = 0x6271E0;

static const char* const* const GameActionNames = (const char* const*)0x8F6D80; // stride 13 dwords
static const int NUM_GAME_ACTIONS = 76;

static const uint32_t BUTTON_PRESSED = 0x0C407210;
static const uint32_t STR_SPLITSCREEN = 0x841D518A;
static const uint32_t TEX_SPLITSCREEN = 0xF365B5F5;
}
