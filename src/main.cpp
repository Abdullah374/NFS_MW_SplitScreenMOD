// NFSMW Split Screen - restores the console 2-player split screen mode on PC.
// Target: speed.exe v1.3 (English, 6,029,312 bytes)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <xinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <initializer_list>
#include "game.h"

#define THISCALL __attribute__((thiscall))
#undef FASTCALL
#define FASTCALL __attribute__((fastcall))

// ------------------------------------------------------------------ config / log
struct Config {
    int enabled = 1;
    int layout = 0;          // 0 = top/bottom, 1 = side by side
    int p2pad = 1;           // XInput index used by player 2
    int poolKB = 0;          // 2P car loader pool, 0 = same as 1P
    int divider = 2;         // divider thickness in pixels
    int debug = 1;
    int motionBlur = 0;
    float widen = 0.25f;
    int separateMinimaps = 1;
    int autoShots = 0;
    int joinScreen = 1;
    int p2MenuInput = 1;
    float minimapScale = 0.8f;
    int stickDeadzone = 7849;
    int triggerThreshold = 30;
} cfg;

static FILE* g_log = nullptr;
static char g_dir[MAX_PATH];
static char g_ini[MAX_PATH];
static char g_ctlIni[MAX_PATH];   // written by the game (player 2 device + bindings)

static void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
    fputc('\n', g_log); fflush(g_log);
}

static void LoadConfig() {
    char* ini = g_ini; snprintf(g_ini, sizeof g_ini, "%s\\NFSMWSplitScreen.ini", g_dir);
    cfg.enabled  = GetPrivateProfileIntA("SplitScreen", "Enabled", 1, ini);
    cfg.layout   = GetPrivateProfileIntA("SplitScreen", "Layout", 0, ini);
    cfg.p2pad    = GetPrivateProfileIntA("SplitScreen", "Player2ControllerIndex", 1, ini);
    cfg.poolKB   = GetPrivateProfileIntA("SplitScreen", "TwoPlayerCarMemoryKB", 0, ini);
    cfg.divider  = GetPrivateProfileIntA("SplitScreen", "DividerPixels", 2, ini);
    cfg.debug    = GetPrivateProfileIntA("SplitScreen", "DebugLog", 1, ini);
    cfg.motionBlur = GetPrivateProfileIntA("SplitScreen", "MotionBlur", 0, ini);
    cfg.p2MenuInput = GetPrivateProfileIntA("SplitScreen", "Player2MenuControl", 1, ini);
    cfg.joinScreen = GetPrivateProfileIntA("SplitScreen", "JoinScreen", 1, ini);
    cfg.autoShots = GetPrivateProfileIntA("SplitScreen", "AutoScreenshots", 0, ini);
    cfg.separateMinimaps = GetPrivateProfileIntA("SplitScreen", "SeparateMinimaps", 1, ini);
    { char b[32]; GetPrivateProfileStringA("SplitScreen", "MinimapScale", "0.8", b, sizeof b, ini); cfg.minimapScale = (float)atof(b);
      if (cfg.minimapScale < 0.3f || cfg.minimapScale > 1.5f) cfg.minimapScale = 0.8f; }
    { char b[32]; GetPrivateProfileStringA("SplitScreen", "ViewWidening", "0.25", b, sizeof b, ini); cfg.widen = (float)atof(b);
      if (cfg.widen < 0) cfg.widen = 0; if (cfg.widen > 1) cfg.widen = 1; }
    cfg.stickDeadzone = GetPrivateProfileIntA("Player2", "StickDeadzone", 7849, ini);
    cfg.triggerThreshold = GetPrivateProfileIntA("Player2", "TriggerThreshold", 30, ini);
    snprintf(g_ctlIni, sizeof g_ctlIni, "%s\\NFSMWSplitScreen_Controls.ini", g_dir);
    cfg.p2pad    = GetPrivateProfileIntA("Player2", "Device", cfg.p2pad, ini);   // -1 = keyboard
    cfg.p2pad    = GetPrivateProfileIntA("Player2", "Device", cfg.p2pad, g_ctlIni);
    if (cfg.p2pad < -1 || cfg.p2pad > 3) cfg.p2pad = 1;
}

// ------------------------------------------------------------------ patch helpers
static void WriteMem(uintptr_t addr, const void* data, size_t n) {
    DWORD old; VirtualProtect((void*)addr, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy((void*)addr, data, n);
    VirtualProtect((void*)addr, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)addr, n);
}
static bool PatchCall(uintptr_t at, void* fn, uintptr_t expectTarget) {
    uint8_t* p = (uint8_t*)at;
    if (p[0] != 0xE8) { Log("PatchCall %08X: not a call (%02X)", at, p[0]); return false; }
    uintptr_t cur = at + 5 + *(int32_t*)(p + 1);
    if (expectTarget && cur != expectTarget) { Log("PatchCall %08X: unexpected target %08X", at, cur); return false; }
    uint8_t b[5]; b[0] = 0xE8; *(int32_t*)(b + 1) = (int32_t)((uintptr_t)fn - (at + 5));
    WriteMem(at, b, 5);
    return true;
}
// Replace a call and return whatever it pointed to before (so other mods' hooks keep working)
static uintptr_t HookCall(uintptr_t at, void* fn, uintptr_t expectOriginal) {
    uint8_t* p = (uint8_t*)at;
    if (p[0] != 0xE8) { Log("HookCall %08X: not a call (%02X) - skipped", at, p[0]); return 0; }
    uintptr_t cur = at + 5 + *(int32_t*)(p + 1);
    if (cur != expectOriginal) Log("HookCall %08X: already hooked by another mod (-> %08X), chaining", at, cur);
    uint8_t b[5]; b[0] = 0xE8; *(int32_t*)(b + 1) = (int32_t)((uintptr_t)fn - (at + 5));
    WriteMem(at, b, 5);
    return cur;
}
static uintptr_t o_IOInit = 0x63CD00, o_FEInitInput = 0x516D70, o_AddOption = 0x573960,
                 o_DisplayInner = 0x6DE300, o_FERender = 0x516F70;
template <class T> static inline T& At(uintptr_t a) { return *(T*)a; }
template <class T> static inline T& At(uint8_t* a, int off) { return *(T*)(a + off); }

// ------------------------------------------------------------------ player 2 input device
// Mimics the engine's InputDevice layout: vtable at +0, channel table at +0x18,
// previous values at +0x1c, current values at +0x20.
struct Channel { int type; uint32_t hash; float* prev; float* cur; };
struct P2Device {
    void** vtbl;
    uint8_t pad0[0x14];
    Channel* channels;   // +0x18
    float* prev;         // +0x1c
    float* cur;          // +0x20
    int port;            // +0x24
    int player;          // 0 = player 1, 1 = player 2
    uint8_t pad1[0x40];
};
static P2Device* g_p2dev = nullptr;
static int g_p2port = -1, g_p1port = -1;

typedef DWORD (WINAPI *XInputGetStateFn)(DWORD, XINPUT_STATE*);
static XInputGetStateFn pXInputGetState = nullptr;

static void LoadXInput() {
    const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (auto d : dlls) {
        HMODULE h = LoadLibraryA(d);
        if (h) { pXInputGetState = (XInputGetStateFn)GetProcAddress(h, "XInputGetState"); if (pXInputGetState) { Log("Loaded %s", d); return; } }
    }
    Log("WARNING: no XInput dll found - player 2 controller will not work");
}

static void* FASTCALL Dev_Dtor(P2Device* self, void*, int) { return self; }
static bool FASTCALL Dev_True(P2Device*, void*) { return true; }
static int FASTCALL Dev_Zero(P2Device*, void*) { return 0; }
static void FASTCALL Dev_Nop(P2Device*, void*) {}
static int FASTCALL Dev_NumChannels(P2Device*, void*) { return G::NUM_GAME_ACTIONS; }
static void FASTCALL Dev_Init(P2Device* self, void*) {
    for (int i = 0; i < G::NUM_GAME_ACTIONS; i++) {
        self->channels[i].type = 1;
        self->channels[i].hash = G::stringhash32(G::GameActionNames[i * 13]);
        self->channels[i].prev = &self->prev[i];
        self->channels[i].cur  = &self->cur[i];
    }
}

static float Axis(SHORT v, int dz, int sign) {
    int x = v * sign;
    if (x <= dz) return 0.f;
    float f = (float)(x - dz) / (32767.f - dz);
    return f > 1.f ? 1.f : f;
}

#include "p2input.inc"

static void* g_devVtbl[24];
static P2Device* AddPlayerDevice(int player) {
    int& count = At<int>(G::pIOModule, 4);
    if (count <= 0 || count >= 15) { Log("IOModule not ready (count=%d)", count); return nullptr; }
    P2Device* d = (P2Device*)calloc(1, sizeof(P2Device));
    d->channels = (Channel*)calloc(G::NUM_GAME_ACTIONS, sizeof(Channel));
    d->prev = (float*)calloc(256, sizeof(float));
    d->cur  = (float*)calloc(256, sizeof(float));
    if (!g_devVtbl[0]) {
        for (int i = 0; i < 24; i++) g_devVtbl[i] = (void*)Dev_Zero;
        g_devVtbl[0] = (void*)Dev_Dtor;
        g_devVtbl[1] = (void*)Dev_True;      // IsConnected
        g_devVtbl[3] = (void*)Dev_Init;      // Initialize
        g_devVtbl[4] = (void*)Dev_Poll;      // Poll
        g_devVtbl[5] = (void*)Dev_NumChannels;
        g_devVtbl[6] = (void*)Dev_Nop;
        g_devVtbl[7] = (void*)Dev_Nop;
        g_devVtbl[10] = (void*)G::InputDevice_HasChanged;
        g_devVtbl[11] = (void*)G::InputDevice_AnyNonZero;
        g_devVtbl[12] = (void*)G::InputDevice_Snapshot;
        g_devVtbl[13] = (void*)G::InputDevice_Restore;
    }
    d->vtbl = g_devVtbl;
    d->port = count;
    d->player = player;
    Dev_Init(d, nullptr);
    At<P2Device*>(G::pIOModule, 8 + count * 4) = d;
    At<uint32_t>(G::pIOModule, 0x48) |= 1u << count;
    count++;
    Log("Player %d input device added at port %d", player + 1, d->port);
    return d;
}
static void AddPlayer2Device() {
    if (g_p2dev) return;
    g_p2dev = AddPlayerDevice(1); if (g_p2dev) g_p2port = g_p2dev->port;   // port 1 (also feeds menus)
    P2Device* p1 = AddPlayerDevice(0); if (p1) g_p1port = p1->port;         // port 2
}

// Split screen races: player 1 drives with the mod's device too, so each player's saved
// controls apply whatever controller (or keyboard) they joined with.
static uintptr_t o_CreatePlayers = 0x6F2700;
static void FASTCALL Hook_CreatePlayers(void* game, void*) {
    uint8_t* db = *G::pFEDatabase;
    bool split = *G::pSimUserMode == 1 && db && g_p1port >= 0 && g_p2port >= 0;
    int8_t old0 = 0;
    if (split) {
        old0 = At<int8_t>(db, G::FEDB_JOYPORT0);
        At<int8_t>(db, G::FEDB_JOYPORT0) = (int8_t)g_p1port;
        At<int8_t>(db, G::FEDB_JOYPORT1) = (int8_t)g_p2port;
        Log("Creating players: P1 port %d (device %d), P2 port %d (device %d)", g_p1port, g_dev[0], g_p2port, g_dev[1]);
    }
    ((void(THISCALL*)(void*))o_CreatePlayers)(game);
    if (split) At<int8_t>(db, G::FEDB_JOYPORT0) = old0;
}

typedef void (THISCALL *IOInitFn)(void* io);
static void InstallLateHooks();
static void FASTCALL Hook_IOModuleInit(void* io, void*) {
    ((IOInitFn)o_IOInit)(io);
    InstallLateHooks();
    Log("IOModule initialized with %d devices", At<int>(G::pIOModule, 4));
    AddPlayer2Device();
}

// "Press Start" screens: on PC the Start action is bound to Space, so also accept Enter
typedef void (THISCALL *NotifyFn)(void* self, uint32_t msg, void* obj, uint32_t p1, uint32_t p2);
static void FASTCALL Hook_PressStartNotify(void* self, void*, uint32_t msg, void* obj, uint32_t p1, uint32_t p2) {
    if (msg == 0xC12E9E27 /*__PAD_ACCEPT_RELEASED__*/) msg = 0xEBFCDA65 /*__PAD_START_RELEASED__*/;
    ((NotifyFn)0x7A3F20)(self, msg, obj, p1, p2);
}

// The PC front end never sends controller input to PressStart.fng, so skip that screen:
// redirect switches to it straight to the matching destination.
typedef void (THISCALL *QPSFn)(void* fe, const char* name, int arg, uint32_t mask, int b);
static QPSFn g_qpsTramp = nullptr;
static void StartJoinScreen();
static int g_carSelectPlayer = -1;
static void FASTCALL Hook_QueuePackageSwitch(void* fe, void*, const char* name, int arg, uint32_t mask, int b) {
    if (name && cfg.enabled) {
        if (!_stricmp(name, "PressStart.fng")) {
            const char* to = name; int a = arg; uint32_t m = mask;
            if (arg == 0 && mask == 0 && cfg.joinScreen) { StartJoinScreen(); to = name; }    // forward: our join screen
            else if (arg == 0 && mask == 0) { to = "Car_Select.fng"; a = 0; m = 7; }       // forward from Track_Options
            else if (arg == 0) {                                                             // back from P1 car select
                uint8_t* db = *G::pFEDatabase;
                bool trackSel = db && (At<uint32_t>(db, G::FEDB_GAMEMODE) & 4) && db[0] == 2 && At<int>(db, 0x18) == 2;
                to = trackSel ? "Track_Select.fng" : "Track_Options.fng"; a = 0; m = 0;
            }
            else if (g_carSelectPlayer == 1) { to = "Car_Select.fng"; a = 0; m = 7; }      // back from P2 car select
            else { to = "Car_Select.fng"; a = 1; m = 7; }                                   // forward to P2 car select (P1 or P2 may drive the menu)
            Log("PressStart.fng(%d,%X) -> %s(%d,%X)", arg, mask, to, a, m);
            name = to; arg = a; mask = m;
        }
        if (!_stricmp(name, "Car_Select.fng")) g_carSelectPlayer = arg;
    }
    g_qpsTramp(fe, name, arg, mask, b);
}
static void InstallQPSDetour() {
    uint8_t* f = (uint8_t*)0x525940;
    const uint8_t expect[7] = { 0x83, 0x3D, 0x90, 0x5E, 0x92, 0x00, 0x06 };
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!memcmp(f, expect, 7)) {
        memcpy(t, expect, 7);
        t[7] = 0xE9; *(int32_t*)(t + 8) = (int32_t)(0x525947 - ((uintptr_t)t + 12));
    } else if (f[0] == 0xE9) {   // another mod detoured it: chain to their jump
        uintptr_t other = 0x525940 + 5 + *(int32_t*)(f + 1);
        t[0] = 0xE9; *(int32_t*)(t + 1) = (int32_t)(other - ((uintptr_t)t + 5));
        Log("QueuePackageSwitch already detoured by another mod, chaining");
    } else { Log("QueuePackageSwitch: unexpected prologue"); return; }
    g_qpsTramp = (QPSFn)t;
    uint8_t j[7] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
    *(int32_t*)(j + 1) = (int32_t)((uintptr_t)Hook_QueuePackageSwitch - (0x525940 + 5));
    WriteMem(0x525940, j, 7);
}

// PC's cFEngJoyInput only allocates one ActionQueue slot, but the split screen code indexes
// it with player 2's port. Grow it to 5 slots with real queues for ports 1..3.
typedef void* (THISCALL *AQCtorFn)(void* self, int port, uint32_t cfg, const char* name, int b);
typedef void (THISCALL *AQEnableFn)(void* self, int en);
static void Hook_FEInitInput() {
    ((G::VoidFn)o_FEInitInput)();
    void** inst = *(void***)0x91CAD0;
    if (!inst) return;
    void** n = (void**)calloc(8, sizeof(void*));
    n[0] = inst[0];
    for (int port = 1; port <= 3; port++) {
        void* q = G::GameNew(0x28C);
        if (!q) continue;
        memset(q, 0, 0x28C);
        ((AQCtorFn)0x648DB0)(q, port, 0x82D21520, "FEng", 0);
        ((AQEnableFn)0x644060)(q, 1);
        n[port] = q;
    }
    n[4] = inst[0];
    *(void***)0x91CAD0 = n;
    Log("FE joy input expanded to 4 ports");
}

// ------------------------------------------------------------------ quick race menu option
static void** g_ssVtbl = nullptr;
static void* g_ssVtblData[2];

static void FASTCALL SplitScreen_React(void* self, void*, const char* pkg, uint32_t data, void* obj, uint32_t p1, uint32_t p2) {
    if (data == G::BUTTON_PRESSED) {
        *G::pQRMode = 2;
        uint8_t* db = *G::pFEDatabase;
        if (db && g_p2port >= 0) At<int8_t>(db, G::FEDB_JOYPORT1) = (int8_t)g_p2port;
        Log("Split Screen selected (P2 port %d)", g_p2port);
    }
}

typedef void (THISCALL *AddOptionFn)(void* menu, void* opt);
typedef void* (THISCALL *IconOptionCtorFn)(void* self, uint32_t tex, uint32_t name, uint32_t desc);

static void FASTCALL Hook_QRAddOption(void* menu, void*, void* opt) {
    ((AddOptionFn)o_AddOption)(menu, opt);  // CustomRace
    if (!cfg.enabled) return;
    if (!g_ssVtbl) {
        g_ssVtblData[0] = ((void**)G::CustomRace_vtable)[0];
        g_ssVtblData[1] = (void*)SplitScreen_React;
        g_ssVtbl = g_ssVtblData;
    }
    void* o = G::GameNew(0x4C);
    if (!o) return;
    ((IconOptionCtorFn)G::IconOption_ctor)(o, G::TEX_SPLITSCREEN, G::STR_SPLITSCREEN, 0);
    *(void***)o = g_ssVtbl;
    ((AddOptionFn)G::IconScrollerMenu_AddOption)(menu, o);
    Log("Added Split Screen option to Quick Race menu");
}

// ------------------------------------------------------------------ rendering
static IDirect3DDevice9* Dev() { return (IDirect3DDevice9*)*G::ppD3DDevice; }
static IDirect3DTexture9* g_tex[2] = {};
static UINT g_texW = 0, g_texH = 0; static D3DFORMAT g_texFmt = D3DFMT_UNKNOWN;
static IDirect3DStateBlock9* g_sb = nullptr;
static int g_frame = 0;
static int g_pass = 0;          // 0 = normal, 1 = rendering P1, 2 = rendering P2
static bool g_captured[2];
static bool g_wasSplit = false;

static void ReleaseResources() {
    for (auto& t : g_tex) if (t) { t->Release(); t = nullptr; }
    if (g_sb) { g_sb->Release(); g_sb = nullptr; }
    g_texW = g_texH = 0;
}

typedef HRESULT (STDMETHODCALLTYPE *ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static ResetFn g_origReset = nullptr;
static void ReleaseUIResources();
static HRESULT STDMETHODCALLTYPE Hook_Reset(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS* pp) {
    ReleaseResources();
    ReleaseUIResources();
    return g_origReset(d, pp);
}
static void HookDevice() {
    static IDirect3DDevice9* hooked = nullptr;
    IDirect3DDevice9* d = Dev();
    if (!d || d == hooked) return;
    void** vt = *(void***)d;
    if (vt[16] != (void*)Hook_Reset) {
        g_origReset = (ResetFn)vt[16];
        void* f = (void*)Hook_Reset;
        WriteMem((uintptr_t)&vt[16], &f, sizeof f);
    }
    hooked = d;
    Log("D3D9 device hooked (%p)", d);
}

static bool Capture(int idx) {
    IDirect3DDevice9* d = Dev();
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(d->GetRenderTarget(0, &rt)) || !rt) return false;
    D3DSURFACE_DESC desc; rt->GetDesc(&desc);
    if (desc.Width != g_texW || desc.Height != g_texH || desc.Format != g_texFmt) {
        for (auto& t : g_tex) if (t) { t->Release(); t = nullptr; }
        g_texW = desc.Width; g_texH = desc.Height; g_texFmt = desc.Format;
        Log("Render target %ux%u fmt %d msaa %d", desc.Width, desc.Height, desc.Format, desc.MultiSampleType);
    }
    if (!g_tex[idx]) {
        HRESULT hr = d->CreateTexture(g_texW, g_texH, 1, D3DUSAGE_RENDERTARGET, g_texFmt, D3DPOOL_DEFAULT, &g_tex[idx], nullptr);
        if (FAILED(hr)) { Log("CreateTexture failed %08X", hr); rt->Release(); return false; }
    }
    IDirect3DSurface9* s = nullptr;
    g_tex[idx]->GetSurfaceLevel(0, &s);
    HRESULT hr = d->StretchRect(rt, nullptr, s, nullptr, D3DTEXF_NONE);
    s->Release(); rt->Release();
    if (FAILED(hr)) { static int n; if (n++ < 5) Log("StretchRect capture failed %08X", hr); return false; }
    g_captured[idx] = true;
    return true;
}

struct QuadV { float x, y, z, rhw, u, v; };
struct ColV { float x, y, z, rhw; DWORD c; };

static void Compose() {
    IDirect3DDevice9* d = Dev();
    if (!g_captured[0] || !g_captured[1]) return;
    if (!g_sb) d->CreateStateBlock(D3DSBT_ALL, &g_sb);
    if (g_sb) g_sb->Capture();
    IDirect3DSurface9* rt = nullptr; d->GetRenderTarget(0, &rt);
    D3DSURFACE_DESC desc; rt->GetDesc(&desc); rt->Release();
    float W = (float)desc.Width, H = (float)desc.Height;
    D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.f, 1.f };
    d->SetViewport(&vp);
    d->SetVertexShader(nullptr); d->SetPixelShader(nullptr);
    d->SetRenderState(D3DRS_ZENABLE, FALSE);
    d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_LIGHTING, FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    d->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    d->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
    d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    for (int i = 0; i < 2; i++) {
        float x0, y0, x1, y1;
        if (cfg.layout == 1) { x0 = i ? W / 2 : 0; x1 = i ? W : W / 2; y0 = 0; y1 = H; }
        else { y0 = i ? H / 2 : 0; y1 = i ? H : H / 2; x0 = 0; x1 = W; }
        x0 -= .5f; y0 -= .5f; x1 -= .5f; y1 -= .5f;
        QuadV q[4] = { {x0,y0,0,1,0,0}, {x1,y0,0,1,1,0}, {x0,y1,0,1,0,1}, {x1,y1,0,1,1,1} };
        d->SetTexture(0, g_tex[i]);
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(QuadV));
    }
    if (cfg.divider > 0) {
        float t = (float)cfg.divider;
        d->SetTexture(0, nullptr);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        float x0, y0, x1, y1;
        if (cfg.layout == 1) { x0 = W / 2 - t / 2; x1 = W / 2 + t / 2; y0 = 0; y1 = H; }
        else { y0 = H / 2 - t / 2; y1 = H / 2 + t / 2; x0 = 0; x1 = W; }
        DWORD c = 0xFF000000;
        ColV q[4] = { {x0,y0,0,1,c}, {x1,y0,0,1,c}, {x0,y1,0,1,c}, {x1,y1,0,1,c} };
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(ColV));
    }
    if (g_sb) g_sb->Apply();
}

typedef void (THISCALL *FERenderFn)(void* fe);

// ------------------------------------------------------------------ separate minimaps
// The console split screen HUD has one shared minimap (centred on player 1, straddling the
// divider). We draw the FE twice: pass A = everything, with the minimap scaled/moved into
// player 1's half; pass B = only the minimap, re-centred on player 2 and moved into his half.
struct PolyXform { float cx, cy, s, tx, ty; };
static PolyXform g_xfP1, g_xfP2;
static const PolyXform* g_xf = nullptr;
static int g_feMode = 0;                 // 0 normal, 1 = pass A, 2 = pass B
static uint8_t* g_mm = nullptr;          // Minimap*
static int g_mmFrame = -100; static DWORD g_mmTick = 0;
static bool g_inOurMMUpdate = false;
static void* g_mmSet[512]; static int g_mmSetN = 0;
static float g_mapMinX, g_mapMaxX, g_mapMinY, g_mapMaxY; static bool g_mapMeasuring = false, g_mapHave = false;
static float g_mapCx = 74.f, g_mapCy = 240.f, g_mapR = 63.f;

static void** LocalPlayers(int* n) { *n = *(int*)0x92D884; return *(void***)0x92D87C; }

static void AddToSet(void* o, int depth) {
    if (!o || g_mmSetN >= 512 || depth > 6) return;
    g_mmSet[g_mmSetN++] = o;
    if (At<int>((uint8_t*)o, 0x18) == 5) {                 // group: add children
        int cnt = At<int>((uint8_t*)o, 0x60);
        uint8_t* c = At<uint8_t*>((uint8_t*)o, 0x64);
        for (int i = 0; i < cnt && c && i < 256; i++) { AddToSet(c, depth + 1); c = At<uint8_t*>(c, 4); }
    }
}
static void BuildMinimapSet() {
    g_mmSetN = 0;
    uint8_t* head = g_mm + 8;
    uint8_t* n = At<uint8_t*>(head, 0);
    for (int i = 0; n && n != head && i < 400; i++) { AddToSet(At<void*>(n, 8), 0); n = At<uint8_t*>(n, 0); }
}
static bool InSet(void* o) { for (int i = 0; i < g_mmSetN; i++) if (g_mmSet[i] == o) return true; return false; }
static bool IsMapPiece(void* o) { for (int i = 0; i < 4; i++) if (At<void*>(g_mm, 0x3C + i * 4) == o) return true; return false; }
static bool IsPlayerArrow(void* o) { return At<void*>(g_mm, 0x90) == o; }   // sits at the map centre
// measured layout of the original minimap (averaged over the first frames of a race, then frozen,
// because the scrolling map tiles change their bounding box every frame)
static float g_arMinX, g_arMaxX, g_arMinY, g_arMaxY; static bool g_measureArrow = false;
static int g_layoutSamples = 0; static float g_sumCx, g_sumCy, g_sumR; static uint8_t* g_layoutFor = nullptr;

// per-poly transform (FE polys are in 640x480 space, 4 verts with x,y at [0,1],[4,5],[8,9],[12,13])
static void XformPoly(float* p, float* save) {
    for (int v = 0; v < 4; v++) {
        float& x = p[v * 4], &y = p[v * 4 + 1];
        save[v * 2] = x; save[v * 2 + 1] = y;
        if (g_measureArrow) {
            if (x < g_arMinX) g_arMinX = x; if (x > g_arMaxX) g_arMaxX = x;
            if (y < g_arMinY) g_arMinY = y; if (y > g_arMaxY) g_arMaxY = y;
        }
        if (g_mapMeasuring) {
            if (x < g_mapMinX) g_mapMinX = x; if (x > g_mapMaxX) g_mapMaxX = x;
            if (y < g_mapMinY) g_mapMinY = y; if (y > g_mapMaxY) g_mapMaxY = y;
        }
        x = g_xf->tx + (x - g_xf->cx) * g_xf->s;
        y = g_xf->ty + (y - g_xf->cy) * g_xf->s;
    }
}
static void RestorePoly(float* p, const float* save) { for (int v = 0; v < 4; v++) { p[v * 4] = save[v * 2]; p[v * 4 + 1] = save[v * 2 + 1]; } }
static void Hook_PolyRender3(float* poly, void* tex, int f) {
    if (!g_xf || !poly) { ((void(*)(float*, void*, int))0x6D1260)(poly, tex, f); return; }
    float sv[8]; XformPoly(poly, sv);
    ((void(*)(float*, void*, int))0x6D1260)(poly, tex, f);
    RestorePoly(poly, sv);
}
static void Hook_PolyRender4(float* poly, void* tex, void* mask, int f) {
    if (!g_xf || !poly) { ((void(*)(float*, void*, void*, int))0x6D1930)(poly, tex, mask, f); return; }
    float sv[8]; XformPoly(poly, sv);
    ((void(*)(float*, void*, void*, int))0x6D1930)(poly, tex, mask, f);
    RestorePoly(poly, sv);
}

typedef void (THISCALL *GIRenderObjFn)(void* gi, void* obj);
static GIRenderObjFn o_GIRenderObject = (GIRenderObjFn)0x516630;
static void FASTCALL Hook_GIRenderObject(void* gi, void*, void* obj) {
    if (g_feMode == 0) { o_GIRenderObject(gi, obj); return; }
    bool in = InSet(obj);
    if (g_feMode == 2 && !in) return;                         // pass B: minimap only
    if (in) {
        g_xf = g_feMode == 1 ? &g_xfP1 : &g_xfP2;
        g_mapMeasuring = g_feMode == 1 && IsMapPiece(obj);
        g_measureArrow = g_feMode == 1 && IsPlayerArrow(obj);
        // force a rebuild so pass B gets player 2's state and pass A player 1's again
        At<uint32_t>((uint8_t*)obj, 0x1C) |= 0x2000000;
    }
    o_GIRenderObject(gi, obj);
    g_xf = nullptr; g_mapMeasuring = false; g_measureArrow = false;
}

typedef void (THISCALL *MMUpdateFn)(void* mm, void* player);
static MMUpdateFn o_MMUpdate = (MMUpdateFn)0x59DB50;
static void FASTCALL Hook_MinimapUpdate(void* mm, void*, void* player) {
    o_MMUpdate(mm, player);
    if (g_inOurMMUpdate) return;
    int n; void** pl = LocalPlayers(&n);
    if (*G::pSimUserMode == 1 && n >= 2 && pl && player == pl[0]) { g_mm = (uint8_t*)mm; g_mmFrame = g_frame; g_mmTick = GetTickCount(); }
}

// keep the minimap tiles of both players decompressed (the manager has 9 slots, each map uses 4)
static short g_tilesP1[4] = { -1, -1, -1, -1 }, g_tilesP2[4] = { -1, -1, -1, -1 };
typedef void (THISCALL *UncompressFn)(void* mgr, short* list, int n);
static void FASTCALL Hook_UncompressMaps(void* mgr, void*, short* list, int n) {
    if (!cfg.separateMinimaps || *G::pSimUserMode != 1 || n != 4) { ((UncompressFn)0x5672F0)(mgr, list, n); return; }
    short* mine = g_inOurMMUpdate ? g_tilesP2 : g_tilesP1;
    short* other = g_inOurMMUpdate ? g_tilesP1 : g_tilesP2;
    memcpy(mine, list, 8);
    short all[8]; int k = 0;
    for (int i = 0; i < 4; i++) all[k++] = list[i];
    for (int i = 0; i < 4; i++) { bool dup = false; for (int j = 0; j < k; j++) if (all[j] == other[i]) dup = true; if (!dup && other[i] >= 0) all[k++] = other[i]; }
    ((UncompressFn)0x5672F0)(mgr, all, k);
}

static bool MinimapReady() {
    if (!cfg.separateMinimaps || !g_mm || *G::pSimUserMode != 1 || *G::pGameFlowState != 6) return false;
    int n; void** pl = LocalPlayers(&n);
    if (n < 2 || !pl || !pl[0] || !pl[n - 1]) return false;
    // HudElement::IsElementVisible
    uint32_t* m = (uint32_t*)(g_mm + 0x18);
    return ((m[2] & m[0]) | (m[3] & m[1])) != 0;
}

static void SetupMinimapXforms() {
    if (g_layoutFor != g_mm) { g_layoutFor = g_mm; g_layoutSamples = 0; g_sumCx = g_sumCy = g_sumR = 0; }
    if (g_mapHave && g_layoutSamples < 30) {
        bool arrow = g_arMaxX > g_arMinX;
        float cx = arrow ? (g_arMinX + g_arMaxX) * .5f : (g_mapMinX + g_mapMaxX) * .5f;
        float cy = arrow ? (g_arMinY + g_arMaxY) * .5f : (g_mapMinY + g_mapMaxY) * .5f;
        float r = (g_mapMaxY - g_mapMinY) * .5f;
        if (r > 10 && r < 200) {
            g_sumCx += cx; g_sumCy += cy; g_sumR += r; g_layoutSamples++;
            g_mapCx = g_sumCx / g_layoutSamples; g_mapCy = g_sumCy / g_layoutSamples; g_mapR = g_sumR / g_layoutSamples;
            if (g_layoutSamples == 30) Log("Minimap layout: centre %.1f,%.1f radius %.1f", g_mapCx, g_mapCy, g_mapR);
        }
    }
    float s = cfg.minimapScale, r = g_mapR * s, margin = 6.f;
    g_xfP1 = { g_mapCx, g_mapCy, s, g_mapCx, 240.f - r - margin };
    g_xfP2 = { g_mapCx, g_mapCy, s, g_mapCx, 480.f - r - margin };
    if (cfg.layout == 1) {   // side by side: both maps bottom-left of their half
        g_xfP1 = { g_mapCx, g_mapCy, s, g_mapCx * s + 4, 480.f - r - margin };
        g_xfP2 = { g_mapCx, g_mapCy, s, 320.f + g_mapCx * s + 4, 480.f - r - margin };
    }
}

static void RenderFEWithTwoMinimaps(void* fe) {
    int n; void** pl = LocalPlayers(&n);
    void* p1 = pl[0]; void* p2 = pl[n - 1];
    BuildMinimapSet();
    SetupMinimapXforms();
    // pass A: whole FE, minimap = player 1
    g_mapMinX = g_mapMinY = 1e9f; g_mapMaxX = g_mapMaxY = -1e9f;
    g_arMinX = g_arMinY = 1e9f; g_arMaxX = g_arMaxY = -1e9f;
    g_feMode = 1;
    ((FERenderFn)o_FERender)(fe);
    g_mapHave = g_mapMaxX > g_mapMinX;
    // pass B: minimap only, centred on player 2
    g_inOurMMUpdate = true;
    pl[0] = p2; pl[n - 1] = p1;          // UpdatePlayer2Element marks IPlayer::Last -> show player 1
    o_MMUpdate(g_mm, p2);
    pl[0] = p1; pl[n - 1] = p2;
    g_feMode = 2;
    void* engine = *(void**)*(void**)0x91CADC;      // cFEng::mInstance->FEngine
    ((void(THISCALL*)(void*))0x5BE800)(engine);
    g_feMode = 0;
    o_MMUpdate(g_mm, p1);                          // leave player 1's state for the game
    g_inOurMMUpdate = false;
}

#include "ui.inc"

static void FASTCALL Hook_FERender(void* fe, void*) {
    if (g_pass == 1) {                              // hide FE in P1 pass, keep the 3D image
        Capture(0);
        // the game normally clears the back buffer after Present; do it here so player 2's
        // frame doesn't get blended with player 1's image
        Dev()->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
        return;
    }
    bool split = g_pass == 2;
    if (split) { if (Capture(1)) Compose(); }
    g_pass = 0;
    if (MinimapReady()) RenderFEWithTwoMinimaps(fe);     // every split-screen frame, even if the 3D pass was skipped
    else ((FERenderFn)o_FERender)(fe);
    UpdateAndDrawMenus();
}

// ------------------------------------------------------------------ pause menu: "P2 CONTROLS" option
static const uint32_t STR_P2CONTROLS = 0x5C2B0001;   // our own language hash
static const char g_strP2Controls[] = "2P CONTROLS";
static const int OC_P2CONTROLS = 50;
static void* g_p2ctlVtbl[2];
static void FASTCALL P2Controls_React(void*, void*, const char*, uint32_t data, void*, uint32_t, uint32_t) {
    if (data == G::BUTTON_PRESSED) {
        uint8_t* db = *G::pFEDatabase;
        if (db) At<int>(At<uint8_t*>(db, 0x10), 0x24) = OC_P2CONTROLS;
    }
}
static void FASTCALL Hook_OptionsAddOption(void* menu, void*, void* opt) {
    ((AddOptionFn)G::IconScrollerMenu_AddOption)(menu, opt);   // Controller
    Log("Options menu setup: userMode=%d fromPause=%d", *G::pSimUserMode, At<uint8_t>((uint8_t*)menu, 0x148));
    if (!cfg.enabled || *G::pSimUserMode != 1 || !At<uint8_t>((uint8_t*)menu, 0x148)) return;
    if (!g_p2ctlVtbl[0]) { g_p2ctlVtbl[0] = ((void**)0x89B8F0)[0]; g_p2ctlVtbl[1] = (void*)P2Controls_React; }
    void* o = G::GameNew(0x4C);
    if (!o) return;
    ((IconOptionCtorFn)G::IconOption_ctor)(o, 0xA04A7B26, STR_P2CONTROLS, 0);
    *(void***)o = g_p2ctlVtbl;
    ((AddOptionFn)G::IconScrollerMenu_AddOption)(menu, o);
    Log("P2 CONTROLS option added to pause options (%s)", At<const char*>((uint8_t*)menu, 0x10));
}
typedef void (THISCALL *MenuNotifyFn)(void* self, uint32_t msg, void* obj, uint32_t p1, uint32_t p2);
static MenuNotifyFn o_OptionsNotify = (MenuNotifyFn)0x545310;
static void FASTCALL Hook_OptionsNotify(void* self, void*, uint32_t msg, void* obj, uint32_t p1, uint32_t p2) {
    o_OptionsNotify(self, msg, obj, p1, p2);
    uint8_t* db = *G::pFEDatabase;
    if (msg == 0xE1FDE1D1 && db && At<uint32_t>((uint8_t*)self, 0x138) == G::BUTTON_PRESSED &&
        At<int>(At<uint8_t*>(db, 0x10), 0x24) == OC_P2CONTROLS) {
        At<int>(At<uint8_t*>(db, 0x10), 0x24) = 0;
        OpenControlsScreen(At<const char*>((uint8_t*)self, 0x10), At<uint8_t>((uint8_t*)self, 0x148));
    }
}
// language lookup (hash arrives in EDX): return our own strings for our hashes
static void InstallStringHook() {
    uint8_t* f = (uint8_t*)0x56BB80;
    const uint8_t expect[7] = { 0x53, 0x8B, 0x1D, 0x80, 0xCF, 0x91, 0x00 };
    if (memcmp(f, expect, 7)) { Log("SearchForString: unexpected prologue - option label disabled"); return; }
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int n = 0;
    t[n++] = 0x81; t[n++] = 0xFA; *(uint32_t*)(t + n) = STR_P2CONTROLS; n += 4;      // cmp edx, hash
    t[n++] = 0x75; t[n++] = 0x06;                                                     // jne +6
    t[n++] = 0xB8; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)g_strP2Controls; n += 4; // mov eax, str
    t[n++] = 0xC3;                                                                    // ret
    memcpy(t + n, expect, 7); n += 7;                                                 // original instructions
    t[n++] = 0xE9; *(int32_t*)(t + n) = (int32_t)(0x56BB87 - ((uintptr_t)t + n + 4)); n += 4;
    uint8_t j[7] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
    *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x56BB80 + 5));
    WriteMem(0x56BB80, j, 7);
}
// block the game's menu input while one of our screens is open
typedef void (THISCALL *JoyFn)(void* self);
static void FASTCALL Hook_HandleJoy(void* self, void*) {
    if (g_inputBlocked) { ((JoyFn)0x5247A0)(self); return; }   // FlushActions
    ((JoyFn)0x559400)(self);
}

// bTList head swap that keeps node back-links valid
struct bNode { bNode* next; bNode* prev; };
static void SwapLists(bNode* a, bNode* b) {
    bNode an = *a, bn = *b;
    bool ae = (an.next == a), be = (bn.next == b);
    if (be) { a->next = a->prev = a; } else { a->next = bn.next; a->prev = bn.prev; bn.next->prev = a; bn.prev->next = a; }
    if (ae) { b->next = b->prev = b; } else { b->next = an.next; b->prev = an.prev; an.next->prev = b; an.prev->next = b; }
}
static bool HasMover(uint8_t* v) { bNode* h = (bNode*)(v + G::EV_MOVERLIST); return h->next && h->next != h; }

typedef void (THISCALL *CalcMatFn)(void* plat, void* view, uint32_t a, uint32_t farz, uint32_t b, uint32_t c);
typedef void (THISCALL *ViewFn)(void* view);

static void MatMul(float* o, const float* a, const float* b) {
    float r[16];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
        r[i*4+j] = a[i*4]*b[j] + a[i*4+1]*b[4+j] + a[i*4+2]*b[8+j] + a[i*4+3]*b[12+j];
    memcpy(o, r, sizeof r);
}

// Recompute player view matrices, then narrow the projection to the half-screen viewport.
// Done on the final matrices so it works with or without the widescreen fix.
// eViewPlatInfo layout (floats): [0] view, [16] proj, [32] view*proj, [48] proj(z-biased),
// [64] view*proj(z-biased), [80] six frustum planes
static void RecalcView1() {
    uint8_t* v1 = G::View(1);
    float* m = *(float**)(v1 + G::EV_PLAT);
    if (!m || !*(void**)(v1 + G::EV_RT) || !*(void**)(v1 + G::EV_CAMERA)) return;
    ((CalcMatFn)G::eView_CalcMatrices)(m, v1, 0, 0x466A6000 /*15000.0f*/, 0, 1);
    float t = cfg.widen;                 // 0 = keep full-screen horizontal FOV (console style)
    float kNarrow = powf(2.f, 1.f - t);  // scale on the split axis
    float kWide = powf(2.f, -t);         // scale on the other axis
    float kx = cfg.layout == 1 ? kNarrow : kWide, ky = cfg.layout == 1 ? kWide : kNarrow;
    for (int base : {16, 48}) {
        for (int r = 0; r < 4; r++) { m[base + r*4 + 0] *= kx; m[base + r*4 + 1] *= ky; }
    }
    MatMul(m + 32, m, m + 16);
    MatMul(m + 64, m, m + 48);
    const float* M = m + 32;
    for (int i = 0; i < 6; i++) {
        float* pl = m + 80 + i * 4;
        for (int r = 0; r < 4; r++) {
            float c3 = M[r*4 + 3];
            switch (i) {
                case 0: pl[r] = c3 + M[r*4 + 0]; break;
                case 1: pl[r] = c3 - M[r*4 + 0]; break;
                case 2: pl[r] = c3 + M[r*4 + 1]; break;
                case 3: pl[r] = c3 - M[r*4 + 1]; break;
                case 4: pl[r] = M[r*4 + 2]; break;
                default: pl[r] = c3 - M[r*4 + 2]; break;
            }
        }
        float len = sqrtf(pl[0]*pl[0] + pl[1]*pl[1] + pl[2]*pl[2]);
        if (len > 0) { float inv = 1.f / len; for (int r = 0; r < 4; r++) pl[r] *= inv; }
    }
}

static void SwapPlayerViews() {
    uint8_t* v1 = G::View(1); uint8_t* v2 = G::View(2);
    void* c = At<void*>(v1, G::EV_CAMERA);
    At<void*>(v1, G::EV_CAMERA) = At<void*>(v2, G::EV_CAMERA);
    At<void*>(v2, G::EV_CAMERA) = c;
    SwapLists((bNode*)(v1 + G::EV_MOVERLIST), (bNode*)(v2 + G::EV_MOVERLIST));
}

static bool SplitActive() {
    if (!cfg.enabled) return false;
    if (*G::pSimUserMode != 1) return false;
    if (*G::pGameFlowState != 6) return false;
    uint8_t* v1 = G::View(1); uint8_t* v2 = G::View(2);
    if (!At<void*>(v1, G::EV_CAMERA) || !At<void*>(v2, G::EV_CAMERA)) return false;
    if (!HasMover(v1) || !HasMover(v2)) return false;
    if (!At<int8_t>(v1, G::EV_ACTIVE)) return false;
    return true;
}

static void Hook_DisplayFrameInner() {
    g_frame++;
    HookDevice();
    bool split = SplitActive();
    // Motion blur blends with the previous frame, which in split mode is the other player's view.
    static int savedBlur = -1;
    if (split && savedBlur < 0 && !cfg.motionBlur) { savedBlur = *(int*)0x9017DC; *(int*)0x9017DC = 0; }
    if (!split && savedBlur >= 0) { *(int*)0x9017DC = savedBlur; savedBlur = -1; }
    if (*G::pSimUserMode != 1 || *G::pGameFlowState != 6) g_mm = nullptr;
    if (split != g_wasSplit) {
        Log("Split rendering %s (userMode=%d flow=%d viewMode=%d)", split ? "ON" : "OFF", *G::pSimUserMode, *G::pGameFlowState, *G::pCurrentViewMode);
        g_wasSplit = split;
    }
    // F12 (or automatically shortly after split starts) -> game writes ScreenshotNNN.tga
    static DWORD splitStart = 0; static int autoShots = 0;
    if (split && !splitStart) { splitStart = GetTickCount(); autoShots = 0; }
    if (!split) splitStart = 0;
    bool shot = (GetAsyncKeyState(VK_F12) & 1) != 0;
    if (split && cfg.autoShots && autoShots < 2 && GetTickCount() - splitStart > (autoShots ? 12000u : 4000u)) { shot = true; autoShots++; }
    if (shot) { *(int*)0x982BBC = 1; Log("Screenshot requested"); }
    if (!split) { g_pass = 0; ((G::VoidFn)o_DisplayInner)(); return; }

    g_captured[0] = g_captured[1] = false;
    // ---- player 1
    RecalcView1();
    g_pass = 1;
    ((G::VoidFn)o_DisplayInner)();
    G::PostDisplayFX();
    G::DisplayComposite(0);
    if (!g_captured[0]) { Capture(0); Dev()->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0); }

    // ---- player 2
    SwapPlayerViews();
    RecalcView1();
    g_pass = 2;
    G::DisplayComposite(1);
    ((ViewFn)G::eView_SetupWorldLight)(G::View(1));
    G::ResetRenderStates();
    ((G::VoidFn)o_DisplayInner)();
    SwapPlayerViews();
    RecalcView1();
    // eDisplayFrame continues with PostDisplayFX + DisplayComposite(0) -> Hook_FERender (pass 2)
}

// Track streaming: UpdateCameraMovers only feeds the streamer from *active* views, and the PC
// never marks player 2's view active, so the world around player 2 was never loaded.
static uintptr_t o_UpdateCameraMovers = 0x47C880;
static void Hook_UpdateCameraMovers(uint32_t arg) {
    uint8_t* v2 = G::View(2);
    bool force = cfg.enabled && *G::pSimUserMode == 1 && HasMover(v2) && !At<int8_t>(v2, G::EV_ACTIVE);
    if (force) At<int8_t>(v2, G::EV_ACTIVE) = 1;
    ((void(*)(uint32_t))o_UpdateCameraMovers)(arg);
    if (force) At<int8_t>(v2, G::EV_ACTIVE) = 0;
}

// ------------------------------------------------------------------ install
// installed once the game starts initialising, i.e. after every other .asi has applied its patches
static void InstallLateHooks() {
    static bool done = false; if (done) return; done = true;
    uintptr_t o;
    if ((o = HookCall(G::CALL_QRMenu_AddOption2, (void*)Hook_QRAddOption, G::IconScrollerMenu_AddOption))) o_AddOption = o;
    if ((o = HookCall(G::CALL_eDisplayFrameInner, (void*)Hook_DisplayFrameInner, 0x6DE300))) o_DisplayInner = o;
    if ((o = HookCall(G::CALL_FEManagerRender, (void*)Hook_FERender, G::FEManager_Render))) o_FERender = o;
    if (cfg.separateMinimaps) {
        HookCall(0x6BE70F, (void*)Hook_PolyRender3, 0x6D1260);
        HookCall(0x6BE734, (void*)Hook_PolyRender4, 0x6D1930);
        HookCall(0x579A8F, (void*)Hook_UncompressMaps, 0x5672F0);
        void* f;
        if (*(uintptr_t*)0x89B46C == 0x516630) { f = (void*)Hook_GIRenderObject; WriteMem(0x89B46C, &f, 4); }
        else { o_GIRenderObject = *(GIRenderObjFn*)0x89B46C; f = (void*)Hook_GIRenderObject; WriteMem(0x89B46C, &f, 4); Log("GI RenderObject already hooked, chaining"); }
        o_MMUpdate = *(MMUpdateFn*)0x8A2044; f = (void*)Hook_MinimapUpdate; WriteMem(0x8A2044, &f, 4);
    }
    HookCall(0x52909C, (void*)Hook_OptionsAddOption, G::IconScrollerMenu_AddOption);
    HookCall(0x56431B, (void*)Hook_HandleJoy, 0x559400);
    { void* f = (void*)Hook_OptionsNotify; o_OptionsNotify = *(MenuNotifyFn*)0x89E530; WriteMem(0x89E530, &f, 4); }
    InstallStringHook();
    if (cfg.p2MenuInput) { uint8_t* c = (uint8_t*)0x559627; if (c[0] == 0x83 && c[1] == 0xFD && c[2] == 0x01) { uint8_t three = 3; WriteMem(0x559629, &three, 1); } }
    if ((o = HookCall(0x6F4EDA, (void*)Hook_CreatePlayers, 0x6F2700))) o_CreatePlayers = o;
    if ((o = HookCall(0x480C16, (void*)Hook_UpdateCameraMovers, 0x47C880))) o_UpdateCameraMovers = o;
    Log("Late patches installed");
}

static void Install() {
    // car memory for 2 player races (PC ships with 1 KB, which can't load any car)
    int pool = cfg.poolKB > 0 ? cfg.poolKB : *G::pCarLoaderPool1P;
    Log("CarLoaderPoolSizes = {%d, %d} -> 2P %d", *G::pCarLoaderPool1P, *G::pCarLoaderPool2P, pool);
    WriteMem((uintptr_t)G::pCarLoaderPool2P, &pool, 4);

    // view mode switch: treat EVIEWMODE_TWOH/TWOV like EVIEWMODE_ONE (keeps player view active)
    uint8_t* j = (uint8_t*)G::JNE_ViewModeSwitch;
    if (j[0] == 0x75 && j[1] == 0x7D) { uint8_t b[2] = { 0x75, 0x37 }; WriteMem(G::JNE_ViewModeSwitch, b, 2); }
    else Log("view mode patch: unexpected bytes %02X %02X", j[0], j[1]);

    // front end: track 2 joypads (PC sets 1) so player 2's controller can drive menus
    if (cfg.p2MenuInput) { uint8_t* q = (uint8_t*)0x516925; if (q[0] == 0x6A && q[1] == 0x01) { uint8_t three = 3; WriteMem(0x516926, &three, 1); } else Log("SetNumJoyPads patch: unexpected bytes"); }

    // "player 2 press start": accept the same controller as player 1
    uint8_t* je = (uint8_t*)G::JE_PressStartSamePort;
    if (je[0] == 0x74) { uint8_t b[2] = { 0x90, 0x90 }; WriteMem(G::JE_PressStartSamePort, b, 2); }
    else Log("press start patch: unexpected byte %02X", je[0]);

    InstallQPSDetour();
    { uintptr_t o = HookCall(0x6660E2, (void*)Hook_FEInitInput, 0x516D70); if (o) o_FEInitInput = o; }
    { uintptr_t o = HookCall(0x6660DD, (void*)Hook_IOModuleInit, 0x63CD00); if (o) o_IOInit = o; }
    if (*(uint32_t*)0x8B72D0 == 0x7A3F20) { void* f = (void*)Hook_PressStartNotify; WriteMem(0x8B72D0, &f, 4); }
    else Log("press start vtable: unexpected");
    Log("Early patches installed");
}

static bool CheckVersion() {
    // speed.exe v1.3: UIQRMainMenu::Setup prologue and the PC CarLoaderPoolSizes value
    const uint8_t sig[] = { 0x6A, 0xFF, 0x68, 0x3C, 0x2B, 0x88, 0x00 };
    return !memcmp((void*)0x7AA730, sig, sizeof sig) && *(int*)0x8F7EF0 == 50000;
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    static int count = 0; static bool inside = false;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    bool fatal = code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                 code == EXCEPTION_STACK_OVERFLOW || code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_PRIV_INSTRUCTION;
    if (fatal && !inside && count < 3 && g_log) {
        inside = true; count++;
        CONTEXT* c = ep->ContextRecord;
        Log("CRASH: code %08lX at %08lX (pass=%d frame=%d split=%d)", ep->ExceptionRecord->ExceptionCode,
            (unsigned long)(uintptr_t)ep->ExceptionRecord->ExceptionAddress, g_pass, g_frame, (int)g_wasSplit);
        if (ep->ExceptionRecord->NumberParameters >= 2)
            Log("  access %s %08lX", ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]);
        Log("  eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        uint32_t* sp = (uint32_t*)c->Esp;
        char buf[1024]; int n = 0;
        for (int i = 0; i < 64 && n < 1000; i++) {
            uint32_t v = 0;
            if (IsBadReadPtr(sp + i, 4)) break;
            v = sp[i];
            if (v >= 0x401000 && v < 0x890000) n += snprintf(buf + n, sizeof buf - n, "%08X ", v);
        }
        buf[n] = 0;
        Log("  stack code refs: %s", buf);
        inside = false;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        GetModuleFileNameA(nullptr, g_dir, MAX_PATH);
        char* s = strrchr(g_dir, '\\'); if (s) *s = 0;
        LoadConfig();
        char lp[MAX_PATH]; snprintf(lp, sizeof lp, "%s\\NFSMWSplitScreen.log", g_dir);
        if (cfg.debug) g_log = fopen(lp, "w");
        Log("NFSMW Split Screen v1.6 loading");
        if (!CheckVersion()) { Log("Unsupported speed.exe version - mod disabled (needs v1.3)"); return TRUE; }
        LoadXInput();
        LoadBindings();
        Install();
        AddVectoredExceptionHandler(0, CrashFilter);
    }
    return TRUE;
}
