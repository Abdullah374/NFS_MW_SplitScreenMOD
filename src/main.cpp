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
    int p2CareerCars = 1;
    int p2MenuInput = 1;
    float minimapScale = 0.8f;
    int stickDeadzone = 7849;
    int triggerThreshold = 30;
    int maxAI = 4;           // most AI racers selectable in split screen
    uint32_t aiIconColor = 0xFF3C3C;   // minimap tint for AI racers (RGB)
    int speedTrapScore = 1;  // speed trap score on each player's HUD
    int randomGrid = 1;      // shuffle the starting grid in split screen
    int resultsDelayMs = 4000; // split screen: time to read the race over message before the results
    int splitReflections = 0; // separate car reflections per player (experimental, off)
    int quadTest = 0;        // 4 player test: 2 extra self-driving players in 2P split screen
    int fourPlayer = 0;      // show the 4 Player Split Screen entry (hidden until its release)
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
    cfg.p2CareerCars = GetPrivateProfileIntA("SplitScreen", "Player2UsesPlayer1Cars", 1, ini);
    cfg.autoShots = GetPrivateProfileIntA("SplitScreen", "AutoScreenshots", 0, ini);
    cfg.separateMinimaps = GetPrivateProfileIntA("SplitScreen", "SeparateMinimaps", 1, ini);
    cfg.maxAI = GetPrivateProfileIntA("SplitScreen", "MaxAIRacers", 4, ini);
    cfg.speedTrapScore = GetPrivateProfileIntA("SplitScreen", "ShowSpeedTrapScore", 1, ini);
    cfg.randomGrid = GetPrivateProfileIntA("SplitScreen", "RandomStartGrid", 1, ini);
    cfg.resultsDelayMs = GetPrivateProfileIntA("SplitScreen", "ResultsDelayMs", 4000, ini);
    cfg.splitReflections = GetPrivateProfileIntA("SplitScreen", "SeparateReflections", 0, ini);
    cfg.quadTest = GetPrivateProfileIntA("SplitScreen", "FourPlayerTest", 0, ini);
    cfg.fourPlayer = GetPrivateProfileIntA("SplitScreen", "FourPlayerSplitScreen", 0, ini);
    { char b[32]; GetPrivateProfileStringA("SplitScreen", "AIMinimapColor", "FF3C3C", b, sizeof b, ini); cfg.aiIconColor = (uint32_t)strtoul(b, nullptr, 16) & 0xFFFFFF; }
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
// ------------------------------------------------------------------ bigger frame memory
// The game's per-frame scratch heap (FrameMalloc) is 400 KB. Two views plus a full
// pursuit (a dozen cops, roadblocks, spikes, flares) run it dry, and some renderers
// (RenderTextureHeadlights) then write through a null pointer. Swap in larger buffers.
static const uint32_t kFrameMemSize = 0x400000;   // 4 MB per buffer
static uint8_t* g_fmBuf[2] = {}; static int g_fmIdx = 1; static uint32_t g_fmLogged = 0;
static void Hook_SwapFrameMalloc() {
    uint8_t*& bufCur = *(uint8_t**)0x915128;  uint8_t*& bufOther = *(uint8_t**)0x91512C;
    uint8_t*& pos = *(uint8_t**)0x915F84;
    uint32_t& failed = *(uint32_t*)0x915F94, &failAmt = *(uint32_t*)0x915F98, &maxFail = *(uint32_t*)0x915F9C;
    int& largest = *(int*)0x915FA0;
    if (failed && failAmt > maxFail) maxFail = failAmt;
    int used = (int)(pos - bufCur);
    failed = 0; failAmt = 0;
    if (used > largest) largest = used;
    *(int*)0x915FA4 = used;
    *(int*)0x915124 = *(int*)0x915120; *(int*)0x915120 = used;
    g_fmIdx ^= 1;
    uint8_t* next = g_fmBuf[g_fmIdx];
    bufOther = bufCur; bufCur = next;                       // last frame's buffer stays intact for one more frame
    *(uint8_t**)0x915F80 = next; pos = next; *(uint8_t**)0x915F88 = next + kFrameMemSize;
    *(uint32_t*)0x915F7C = kFrameMemSize; *(uint32_t*)0x915F90 = 0;
    if ((uint32_t)largest >= g_fmLogged + 0x80000) { g_fmLogged = (uint32_t)largest & ~0x7FFFFu; Log("Frame memory: peak use %u KB of %u KB", (unsigned)largest / 1024, kFrameMemSize / 1024); }
}
static void InstallFrameMemory() {
    for (int i = 0; i < 2; i++) g_fmBuf[i] = (uint8_t*)VirtualAlloc(nullptr, kFrameMemSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_fmBuf[0] || !g_fmBuf[1]) { Log("Frame memory: allocation failed, game default kept"); return; }
    if (!HookCall(0x6E7641, (void*)Hook_SwapFrameMalloc, 0x4FAD80)) return;
    // RenderTextureHeadlights: when frame memory runs out, skip the draw instead of writing to null
    uint8_t* p = (uint8_t*)0x7381A0;
    if (p[0] == 0xA1 && *(uint32_t*)(p + 1) == 0x915F98) {
        uint8_t b[5] = { 0xE9 }; *(int32_t*)(b + 1) = (int32_t)(0x7382F5 - (0x7381A0 + 5)); WriteMem(0x7381A0, b, 5);
    }
    Log("Frame memory: %u KB x2", kFrameMemSize / 1024);
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
static int g_quadPort[2] = { -1, -1 };    // input ports of players 3 and 4
static void AddPlayer2Device() {
    if (g_p2dev) return;
    g_p2dev = AddPlayerDevice(1); if (g_p2dev) g_p2port = g_p2dev->port;   // port 1 (also feeds menus)
    P2Device* p1 = AddPlayerDevice(0); if (p1) g_p1port = p1->port;         // port 2
    for (int k = 0; k < 2; k++) { P2Device* d = AddPlayerDevice(2 + k); if (d) g_quadPort[k] = d->port; }   // ports 3, 4
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
static void FourCarSelReset();
static int g_carSelectPlayer = -1;
static char g_lastPkg[64];
// Player 2's car select creates a fresh multiplayer profile for player 2 (CreateMultiplayerProfile,
// called from the UIQRCarSelect constructor) with only the stock cars, so there are no career or
// My Cars entries. Right after it is created, give player 2 a copy of player 1's car stable
// (FEPlayerCarDB, profile + 0x414, 0x8CC8 bytes: car records, customizations, career records).
static void CopyCarStableToP2() {
    uint8_t* db = *G::pFEDatabase;
    if (!db) { Log("Player 2 car stable: no FE database"); return; }
    uint8_t* p0 = At<uint8_t*>(db, 0x10); uint8_t* p1 = At<uint8_t*>(db, 0x14);
    if (!p0 || !p1 || p0 == p1) { Log("Player 2 car stable: profiles %p %p, not copied", p0, p1); return; }
    int n0 = 0, n1 = 0;
    for (int i = 0; i < 200; i++) { if (At<uint32_t>(p0, 0x414 + i * 0x14) != 0xFFFFFFFF) n0++; if (At<uint32_t>(p1, 0x414 + i * 0x14) != 0xFFFFFFFF) n1++; }
    DWORD old;
    VirtualProtect(p1 + 0x414, 0x8CC8, PAGE_READWRITE, &old);   // no-op on normal heap memory
    memcpy(p1 + 0x414, p0 + 0x414, 0x8CC8);
    Log("Player 2 car stable: copied from player 1 (profiles %p %p, cars %d -> was %d)", p0, p1, n0, n1);
}
static uintptr_t o_CreateMPProfile = 0x5ABA50;
static void FASTCALL Hook_CreateMPProfile(void* db, void*, int idx) {
    ((void(THISCALL*)(void*, int))o_CreateMPProfile)(db, idx);
    if (idx == 1 && cfg.p2CareerCars) CopyCarStableToP2();
}
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
        if (!_stricmp(name, "Car_Select.fng")) { g_carSelectPlayer = arg; if (arg == 0) FourCarSelReset(); }
        // split screen race: menus (pause, options, ...) accept every player's controller
        if (cfg.p2MenuInput && *G::pSimUserMode == 1 && *G::pGameFlowState == 6) mask |= 7;
    }
    if (name) snprintf(g_lastPkg, sizeof g_lastPkg, "%s", name);
    g_qpsTramp(fe, name, arg, mask, b);
}
// the pause menu is pushed for the controller of the player who paused; open it to both players
typedef void (THISCALL *QPushFn)(void* fe, const char* name, int arg, uint32_t mask, int b);
static uintptr_t o_PausePush = 0x5257F0;
static void FASTCALL Hook_PausePush(void* fe, void*, const char* name, int arg, uint32_t mask, int b) {
    if (cfg.p2MenuInput && *G::pSimUserMode == 1) mask |= 7;
    ((QPushFn)o_PausePush)(fe, name, arg, mask, b);
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

static bool g_fourModeSel = false;     // mirrors g_fourMode (ui.inc), set from the menu
static void SetFourMode(bool on);
// Split screen starts with every player on the near chase camera (PlayerSettings +0xC = 2,
// POV_OUTSIDE_CLOSE; FEDatabase +0x10 -> profile, +0x80: the two players' settings, 0x14 bytes each).
// Player 1's own camera (saved in the profile) is put back when the Quick Race menu opens again.
static int g_p1CamSaved = -1;
static void QuadCamerasNear();
static void SplitCamerasNear() {
    uint8_t* db = *G::pFEDatabase;
    uint8_t* prof = db ? At<uint8_t*>(db, 0x10) : nullptr;
    if (!prof) return;
    if (g_p1CamSaved < 0) g_p1CamSaved = At<int>(prof, 0x80 + 0xC);
    At<int>(prof, 0x80 + 0xC) = 2; At<int>(prof, 0x80 + 0x14 + 0xC) = 2;
    QuadCamerasNear();
    Log("Split screen: every player's camera set to near (player 1's was %d)", g_p1CamSaved);
}
static void RestoreP1Camera() {
    uint8_t* db = *G::pFEDatabase;
    uint8_t* prof = db ? At<uint8_t*>(db, 0x10) : nullptr;
    if (!prof || g_p1CamSaved < 0) return;
    At<int>(prof, 0x80 + 0xC) = g_p1CamSaved;
    Log("Player 1's own camera (%d) back", g_p1CamSaved);
    g_p1CamSaved = -1;
}
static void FASTCALL SplitScreen_React(void* self, void*, const char* pkg, uint32_t data, void* obj, uint32_t p1, uint32_t p2) {
    if (data == G::BUTTON_PRESSED) {
        SetFourMode(false);
        *G::pQRMode = 2;
        uint8_t* db = *G::pFEDatabase;
        if (db && g_p2port >= 0) At<int8_t>(db, G::FEDB_JOYPORT1) = (int8_t)g_p2port;
        SplitCamerasNear();
        Log("Split Screen selected (P2 port %d)", g_p2port);
    }
}

static void FASTCALL FourPlayer_React(void* self, void*, const char* pkg, uint32_t data, void* obj, uint32_t p1, uint32_t p2) {
    if (data == G::BUTTON_PRESSED) {
        SetFourMode(true);
        *G::pQRMode = 2;
        uint8_t* db = *G::pFEDatabase;
        if (db && g_p2port >= 0) At<int8_t>(db, G::FEDB_JOYPORT1) = (int8_t)g_p2port;
        SplitCamerasNear();
        Log("4 Player Split Screen selected");
    }
}
static const uint32_t STR_FOURPLAYER = 0x5C2B0006;
static const uint32_t TEX_FOURPLAYER = 0x5C2B0007;

// ------------------------------------------------------------------ 4 player menu icon
// The game has no 4 player icon, so we make one. GetTextureInfo is detoured: for our hash
// it returns a copy of the 2 player icon's TextureInfo (0x7C bytes) whose PlatInfo (0x20 bytes)
// points at our own D3D texture (white glyph, alpha from icon4p.inc).
#include "icon4p.inc"
static IDirect3DDevice9* Dev();
typedef void* (__cdecl *GetTexInfoFn)(uint32_t hash, int useDefault, int b);
static GetTexInfoFn g_gtiTramp = nullptr;
alignas(16) static uint8_t g_4pTI[0x80], g_4pPlat[0x20];
static void* g_4pBase = nullptr;
static IDirect3DTexture9* g_4pTex = nullptr;
static bool g_4pTexFailed = false;
static bool Make4PTexture() {
    if (g_4pTex) return true;
    if (g_4pTexFailed || !Dev()) return false;
    if (FAILED(Dev()->CreateTexture(64, 64, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_4pTex, nullptr)) || !g_4pTex) {
        g_4pTexFailed = true; g_4pTex = nullptr; Log("4P icon: CreateTexture failed"); return false;
    }
    static uint8_t a[64 * 64]; memcpy(a, kIcon4PAlpha, sizeof a);
    int n = 64;
    for (DWORD lv = 0; lv < g_4pTex->GetLevelCount() && n >= 1; lv++) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(g_4pTex->LockRect(lv, &lr, nullptr, 0))) {
            for (int y = 0; y < n; y++) {
                uint32_t* row = (uint32_t*)((uint8_t*)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < n; x++) row[x] = ((uint32_t)a[y * n + x] << 24) | 0xFFFFFF;
            }
            g_4pTex->UnlockRect(lv);
        }
        if (n == 1) break;
        int h = n / 2;   // box-filter the next mip level in place
        for (int y = 0; y < h; y++) for (int x = 0; x < h; x++)
            a[y * h + x] = (uint8_t)((a[2*y*n + 2*x] + a[2*y*n + 2*x + 1] + a[(2*y+1)*n + 2*x] + a[(2*y+1)*n + 2*x + 1] + 2) / 4);
        n = h;
    }
    Log("4P icon texture created");
    return true;
}
static void* Get4PIconInfo() {
    uint8_t* base = (uint8_t*)g_gtiTramp(G::TEX_SPLITSCREEN, 0, 0);
    if (!base || !*(void**)base || !Make4PTexture()) return nullptr;
    if (base != g_4pBase || *(IDirect3DTexture9**)(g_4pPlat + 0x18) != g_4pTex) {
        memcpy(g_4pTI, base, 0x7C);
        memcpy(g_4pPlat, *(void**)base, 0x20);
        *(void**)(g_4pPlat + 0) = g_4pPlat;            // unlinked list node
        *(void**)(g_4pPlat + 4) = g_4pPlat;
        *(uint32_t*)(g_4pPlat + 0x14) = D3DFMT_A8R8G8B8;
        *(IDirect3DTexture9**)(g_4pPlat + 0x18) = g_4pTex;
        *(void**)(g_4pTI + 0) = g_4pPlat;
        memset(g_4pTI + 0xC, 0, 0x18); strcpy((char*)g_4pTI + 0xC, "MODE_ICON_4PLAYER_SPLIT");
        *(uint32_t*)(g_4pTI + 0x24) = TEX_FOURPLAYER;
        *(uint32_t*)(g_4pTI + 0x2C) = 0;               // no parent image
        *(void**)(g_4pTI + 0x6C) = g_4pTI;
        *(void**)(g_4pTI + 0x70) = nullptr;
        *(void**)(g_4pTI + 0x74) = nullptr;
        g_4pBase = base;
    }
    return g_4pTI;
}
static void* __cdecl Hook_GetTextureInfo(uint32_t hash, int useDefault, int b) {
    if (hash == TEX_FOURPLAYER) {
        if (void* t = Get4PIconInfo()) return t;
        hash = G::TEX_SPLITSCREEN;                     // fall back to the 2 player icon
    }
    return g_gtiTramp(hash, useDefault, b);
}
static void InstallGetTextureInfoDetour() {
    uint8_t* f = (uint8_t*)0x503400;
    const uint8_t expect[8] = { 0x53, 0x55, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x14 };
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    if (!memcmp(f, expect, 8)) {
        memcpy(t, expect, 8);
        t[8] = 0xE9; *(int32_t*)(t + 9) = (int32_t)(0x503408 - ((uintptr_t)t + 13));
    } else if (f[0] == 0xE9) {
        uintptr_t other = 0x503400 + 5 + *(int32_t*)(f + 1);
        t[0] = 0xE9; *(int32_t*)(t + 1) = (int32_t)(other - ((uintptr_t)t + 5));
        Log("GetTextureInfo already detoured by another mod, chaining");
    } else { Log("GetTextureInfo: unexpected prologue, 4P icon off"); return; }
    g_gtiTramp = (GetTexInfoFn)t;
    uint8_t j[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    *(int32_t*)(j + 1) = (int32_t)((uintptr_t)Hook_GetTextureInfo - (0x503400 + 5));
    WriteMem(0x503400, j, 8);
}
static const char g_strFourPlayer[] = "4 PLAYER SPLIT SCREEN";
static void* g_4pVtblData[2];
typedef void (THISCALL *AddOptionFn)(void* menu, void* opt);
typedef void* (THISCALL *IconOptionCtorFn)(void* self, uint32_t tex, uint32_t name, uint32_t desc);

static void FASTCALL Hook_QRAddOption(void* menu, void*, void* opt) {
    ((AddOptionFn)o_AddOption)(menu, opt);  // CustomRace
    RestoreP1Camera();                      // back on the Quick Race menu: player 1's own camera
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
    if (!cfg.fourPlayer) return;            // 4 player split screen: not released yet
    if (!g_4pVtblData[0]) { g_4pVtblData[0] = ((void**)G::CustomRace_vtable)[0]; g_4pVtblData[1] = (void*)FourPlayer_React; }
    void* o4 = G::GameNew(0x4C);
    if (!o4) return;
    ((IconOptionCtorFn)G::IconOption_ctor)(o4, g_gtiTramp ? TEX_FOURPLAYER : G::TEX_SPLITSCREEN, STR_FOURPLAYER, 0);
    *(void***)o4 = g_4pVtblData;
    ((AddOptionFn)G::IconScrollerMenu_AddOption)(menu, o4);
}

// ------------------------------------------------------------------ rendering
static IDirect3DDevice9* Dev() { return (IDirect3DDevice9*)*G::ppD3DDevice; }
static IDirect3DTexture9* g_tex[4] = {};
static UINT g_texW = 0, g_texH = 0; static D3DFORMAT g_texFmt = D3DFMT_UNKNOWN;
static IDirect3DStateBlock9* g_sb = nullptr;
static int g_frame = 0;
static int g_pass = 0;          // 0 = normal, 1 = an intermediate player pass, 2 = the last pass
static int g_passSlot = 0;      // capture slot of the pass being drawn
static bool g_quadRender = false;   // this frame is drawn in four quarters (4 player test)
static void QuadTick();
static void QuadFindHudPackages();
static bool g_captured[4];
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
    int views = g_quadRender ? 4 : 2;
    for (int i = 0; i < views; i++) if (!g_captured[i]) return;
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
    for (int i = 0; i < views; i++) {
        float x0, y0, x1, y1;
        if (g_quadRender) { x0 = (i & 1) ? W / 2 : 0; x1 = x0 + W / 2; y0 = (i & 2) ? H / 2 : 0; y1 = y0 + H / 2; }
        else if (cfg.layout == 1) { x0 = i ? W / 2 : 0; x1 = i ? W : W / 2; y0 = 0; y1 = H; }
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
        DWORD c = 0xFF000000;
        for (int line = 0; line < (g_quadRender ? 2 : 1); line++) {
            bool vert = g_quadRender ? line == 0 : cfg.layout == 1;
            float x0, y0, x1, y1;
            if (vert) { x0 = W / 2 - t / 2; x1 = W / 2 + t / 2; y0 = 0; y1 = H; }
            else { y0 = H / 2 - t / 2; y1 = H / 2 + t / 2; x0 = 0; x1 = W; }
            ColV q[4] = { {x0,y0,0,1,c}, {x1,y0,0,1,c}, {x0,y1,0,1,c}, {x1,y1,0,1,c} };
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(ColV));
        }
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

static void* g_onlyObj = nullptr;         // pass 3: the FE object(s) to draw
static void* g_onlyObj2 = nullptr;
static const PolyXform* g_aiXf = nullptr;
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
// 4 player HUDs (quad.inc): the quarter of the package being drawn and the object's anchors
struct HudQuad { int q; float hy0; };
static HudQuad g_hudQ; static bool g_hudQOn = false; static int g_hudAx = -1, g_hudAy = -1;
static void XformHudPoly(float* p, float* save);
static void* g_diagObjM = nullptr; static int g_diagFnM = 0;
static void RestorePoly(float* p, const float* save) { for (int v = 0; v < 4; v++) { p[v * 4] = save[v * 2]; p[v * 4 + 1] = save[v * 2 + 1]; } }
static void Hook_PolyRender3(float* poly, void* tex, int f) {
    if (g_hudQOn && !g_xf && poly) {
        g_diagFnM = 3; float sv[8]; XformHudPoly(poly, sv);
        ((void(*)(float*, void*, int))0x6D1260)(poly, tex, f);
        RestorePoly(poly, sv); return;
    }
    if (!g_xf || !poly) { ((void(*)(float*, void*, int))0x6D1260)(poly, tex, f); return; }
    float sv[8]; XformPoly(poly, sv);
    ((void(*)(float*, void*, int))0x6D1260)(poly, tex, f);
    RestorePoly(poly, sv);
}
static void Hook_PolyRender4(float* poly, void* tex, void* mask, int f) {
    if (g_hudQOn && !g_xf && poly) {
        g_diagFnM = 4; float sv[8]; XformHudPoly(poly, sv);
        ((void(*)(float*, void*, void*, int))0x6D1930)(poly, tex, mask, f);
        RestorePoly(poly, sv); return;
    }
    if (!g_xf || !poly) { ((void(*)(float*, void*, void*, int))0x6D1930)(poly, tex, mask, f); return; }
    float sv[8]; XformPoly(poly, sv);
    ((void(*)(float*, void*, void*, int))0x6D1930)(poly, tex, mask, f);
    RestorePoly(poly, sv);
}

typedef void (THISCALL *GIRenderObjFn)(void* gi, void* obj);
static GIRenderObjFn o_GIRenderObject = (GIRenderObjFn)0x516630;
static void FASTCALL Hook_GIRenderObject(void* gi, void*, void* obj) {
    if (g_feMode == 0) { g_hudAx = g_hudAy = -1; g_diagObjM = obj; o_GIRenderObject(gi, obj); g_diagObjM = nullptr; return; }
    if (g_feMode == 3) {                                      // AI racer icon pass: one object only
        if (obj != g_onlyObj && obj != g_onlyObj2) return;
        g_xf = g_aiXf;
        At<uint32_t>((uint8_t*)obj, 0x1C) |= At<int>((uint8_t*)obj, 0x18) == 2 ? 0x400000 : 0x2000000;   // strings: text dirty, images: rebuild
        o_GIRenderObject(gi, obj);
        g_xf = nullptr;
        return;
    }
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
static MMUpdateFn o_MMUpdateRaw = (MMUpdateFn)0x59DB50;

// Speed trap races: when anyone drives through a trap, the game drops its icon from the list of
// visible map icons (GManager +0x2C0 count / +0x2C4 GIcon*[]) for everybody. The minimap gets its
// icons from GManager::GatherVisibleIcons(out, player), so per player we take that list, drop the
// traps this player has crossed and add back the ones he hasn't (GRacerInfo +0x78: speed recorded
// at each race trap; GRaceStatus +0x4504 trap count, +0x4508 GTrigger*[16]; GTrigger +0xC0 GIcon*).
typedef int (THISCALL *GatherFn)(void* gm, void** out, void* player);
static int FASTCALL Hook_GatherVisibleIcons(void* gm, void*, void** out, void* player) {
    int n = ((GatherFn)0x5E4940)(gm, out, player);
    if (!cfg.enabled || *G::pSimUserMode != 1 || !player) return n;
    uint8_t* rs = *(uint8_t**)0x91E000;
    if (!rs || !At<void*>(rs, 0x1968)) return n;
    if (((int(THISCALL*)(void*))0x5FAA20)(At<void*>(rs, 0x1968)) != 5) return n;    // speed trap race
    int traps = At<int>(rs, 0x4504);
    if (traps <= 0 || traps > 16) return n;
    void* sim = ((void*(THISCALL*)(void*))(*(void***)player)[1])(player);
    uint8_t* info = sim ? (uint8_t*)((void*(THISCALL*)(void*, void*))0x5E8B20)(rs, sim) : nullptr;
    if (!info) return n;
    for (int i = 0; i < traps; i++) {
        uint8_t* trig = At<uint8_t*>(rs, 0x4508 + i * 4);
        void* icon = trig ? At<void*>(trig, 0xC0) : nullptr;
        if (!icon) continue;
        bool crossed = At<float>(info, 0x78 + i * 4) > 0.f;
        int at = -1; for (int k = 0; k < n; k++) if (out[k] == icon) { at = k; break; }
        if (crossed && at >= 0) { for (int k = at; k < n - 1; k++) out[k] = out[k + 1]; n--; }
        else if (!crossed && at < 0 && n < 190) out[n++] = icon;
    }
    return n;
}
static void o_MMUpdate(void* mm, void* player) { o_MMUpdateRaw(mm, player); }
static void* FASTCALL Hook_MinimapDelete(uint8_t* mm, void*, unsigned flags) {
    if (mm == g_mm) g_mm = nullptr;
    if (mm == g_layoutFor) g_layoutFor = nullptr;
    return ((void*(THISCALL*)(void*, unsigned))0x5A6520)(mm, flags);
}
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

// The split screen HUD has no AI racer icons (the console mode had no AI), so draw each AI racer
// by borrowing the "other player" icon: place it with Minimap::UpdateElementArt, tint it and
// render just that object, once per AI racer, on each player's map.
static void DrawAIRacerIcons(const PolyXform* xf) {
    uint8_t* obj = At<uint8_t*>(g_mm, 0x94);
    if (!obj) return;
    uint8_t** list = *(uint8_t***)0x92CE3C;                 // vehicle list 3 (AI racers)
    int cnt = *(int*)0x92CE44;
    if (!list || cnt <= 0 || cnt > 16) return;
    uint32_t col = ((uint32_t(*)(void*))0x51A180)(obj);      // FEngGetColor
    void* engine = *(void**)*(void**)0x91CADC;
    for (int i = 0; i < cnt; i++) {
        uint8_t* v = list[i];
        if (!v) continue;
        void** vt = *(void***)v;
        if (!((bool(THISCALL*)(void*))vt[0x88 / 4])(v)) continue;            // IVehicle::IsActive
        void* sim = ((void*(THISCALL*)(void*))vt[1])(v);                       // IVehicle -> ISimable
        if (!sim) continue;
        float pos[2], dir[2];
        ((void(*)(float*, float*, void*))0x579840)(pos, dir, sim);            // GetVehicleVectors
        ((void(*)(void*, uint32_t))0x5157E0)(obj, (col & 0xFF000000u) | cfg.aiIconColor);
        ((void(THISCALL*)(void*, float*, float*, void*, bool))0x5675D0)(g_mm, pos, dir, obj, false);
        if (!((bool(*)(void*))0x564AD0)(obj)) continue;                        // FEngIsVisible
        g_onlyObj = obj; g_aiXf = xf; g_feMode = 3;
        ((void(THISCALL*)(void*))0x5BE800)(engine);
        g_feMode = 0; g_onlyObj = nullptr;
    }
    ((void(*)(void*, uint32_t))0x5157E0)(obj, col);
}

// Cops on the minimaps: the split screen HUD packages have no cop icons, so cops are drawn the
// same way as the AI racers, flashing red / blue (vehicle list 4: cops, incl. the helicopter).
static void DrawCopIcons(const PolyXform* xf) {
    if (!g_mm) return;
    uint8_t* obj = At<uint8_t*>(g_mm, 0x94);
    if (!obj) return;
    uint8_t** list = *(uint8_t***)0x92CE9C;
    int cnt = *(int*)0x92CEA4;
    if (!list || cnt <= 0 || cnt > 32) return;
    uint32_t col = ((uint32_t(*)(void*))0x51A180)(obj);
    void* engine = *(void**)*(void**)0x91CADC;
    bool red = (GetTickCount() / 110) & 1;
    for (int i = 0; i < cnt; i++) {
        uint8_t* v = list[i];
        if (!v) continue;
        void** vt = *(void***)v;
        if (!((bool(THISCALL*)(void*))vt[0x88 / 4])(v)) continue;            // IVehicle::IsActive
        void* sim = ((void*(THISCALL*)(void*))vt[1])(v);
        if (!sim) continue;
        float pos[2], dir[2];
        ((void(*)(float*, float*, void*))0x579840)(pos, dir, sim);
        ((void(*)(void*, uint32_t))0x5157E0)(obj, (col & 0xFF000000u) | (((i & 1) ^ red) ? 0xFF2020u : 0x3060FFu));
        ((void(THISCALL*)(void*, float*, float*, void*, bool))0x5675D0)(g_mm, pos, dir, obj, false);
        if (!((bool(*)(void*))0x564AD0)(obj)) continue;
        g_onlyObj = obj; g_aiXf = xf; g_feMode = 3;
        ((void(THISCALL*)(void*))0x5BE800)(engine);
        g_feMode = 0; g_onlyObj = nullptr;
    }
    ((void(*)(void*, uint32_t))0x5157E0)(obj, col);
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
    DrawAIRacerIcons(&g_xfP1);
    DrawCopIcons(&g_xfP1);
    // pass B: minimap only, centred on player 2
    g_inOurMMUpdate = true;
    pl[0] = p2; pl[n - 1] = p1;          // UpdatePlayer2Element marks IPlayer::Last -> show player 1
    o_MMUpdate(g_mm, p2);
    pl[0] = p1; pl[n - 1] = p2;
    g_feMode = 2;
    void* engine = *(void**)*(void**)0x91CADC;      // cFEng::mInstance->FEngine
    ((void(THISCALL*)(void*))0x5BE800)(engine);
    g_feMode = 0;
    DrawAIRacerIcons(&g_xfP2);
    DrawCopIcons(&g_xfP2);
    o_MMUpdate(g_mm, p1);                          // leave player 1's state for the game
    g_inOurMMUpdate = false;
}

#include "ui.inc"
#include "raceopts.inc"
#include "racehud.inc"

// the game is a 32-bit program (2 GB of address space without the large address aware flag):
// how much is left, and the biggest free block (D3D / streaming allocations need one piece)
static void LogMemory(const char* tag) {
    MEMORYSTATUSEX ms = { sizeof ms }; GlobalMemoryStatusEx(&ms);
    SIZE_T biggest = 0; MEMORY_BASIC_INFORMATION mbi; uintptr_t a = 0x10000;
    while (a < (uintptr_t)ms.ullTotalVirtual && VirtualQuery((void*)a, &mbi, sizeof mbi)) {
        if (mbi.State == MEM_FREE && mbi.RegionSize > biggest) biggest = mbi.RegionSize;
        uintptr_t n = (uintptr_t)mbi.BaseAddress + mbi.RegionSize; if (n <= a) break; a = n;
    }
    Log("%s: address space %llu of %llu MB free, biggest free block %u MB", tag,
        (unsigned long long)(ms.ullAvailVirtual >> 20), (unsigned long long)(ms.ullTotalVirtual >> 20), (unsigned)(biggest >> 20));
}
static void FASTCALL Hook_FERender(void* fe, void*) {
    if (g_pass == 1) {                              // hide FE in P1 pass, keep the 3D image
        Capture(g_passSlot);
        // the game normally clears the back buffer after Present; do it here so player 2's
        // frame doesn't get blended with player 1's image
        Dev()->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
        return;
    }
    bool split = g_pass == 2;
    if (split) { if (Capture(g_passSlot)) Compose(); }
    g_pass = 0;
    QuadTick();
    { static DWORD memAt = 0; DWORD now = GetTickCount(); if (*G::pGameFlowState == 6 && now - memAt > 30000) { memAt = now; LogMemory("Memory"); } }
    DrawBustedMeters();                                  // under the HUD and the pause menu
    // the BUSTED / OUT OF THE RACE screen goes over the HUD and minimap, but under the pause menu
    // (the sim clock stops while paused: then it's drawn before the FE as before)
    static float lastSimT = -1.f; static DWORD simMoved = 0;
    { float t = ((float(*)())0x6E8DE0)(); DWORD now = GetTickCount(); if (t != lastSimT) { lastSimT = t; simMoved = now; } }
    bool paused = GetTickCount() - simMoved > 150;
    if (paused) DrawBustedLabels();
    if (g_quadRender) QuadFindHudPackages();
    if (!g_quadRender && MinimapReady()) RenderFEWithTwoMinimaps(fe);     // every split-screen frame, even if the 3D pass was skipped
    else ((FERenderFn)o_FERender)(fe);
    if (!paused) DrawBustedLabels();
    DrawSpeedTrapScoreRows();
    StartPositionDiagnostics();
    PostRaceTick();
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
    struct { uint32_t hash; const char* str; } tbl[] = {
        { STR_P2CONTROLS, g_strP2Controls }, { STR_FOURPLAYER, g_strFourPlayer }, { STR_AIRACERS, g_strAIRacers }, { STR_AIDIFF, g_strAIDiff }, { STR_SCORE, g_strScore }, { STR_COPS, g_strCops },
    };
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int n = 0;
    for (auto& e : tbl) {
        t[n++] = 0x81; t[n++] = 0xFA; *(uint32_t*)(t + n) = e.hash; n += 4;              // cmp edx, hash
        t[n++] = 0x75; t[n++] = 0x06;                                                     // jne +6
        t[n++] = 0xB8; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)e.str; n += 4;          // mov eax, str
        t[n++] = 0xC3;                                                                    // ret
    }
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
    if (g_quadRender) kx = ky = 1.f;            // a quarter has the full screen's shape
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

static void SwapPlayerViews(int other = 2) {
    uint8_t* v1 = G::View(1); uint8_t* v2 = G::View(other);
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

// Car reflections come from one environment map placed around the car of whoever view 1 follows,
// and normally only 1-3 of its 6 faces are redrawn per frame. Split screen draws two players into
// the same map, so before each player's pass place it around that player's car and redraw all six
// faces (UpdateEnvironmentMapCameras with the "every face every frame" setting).
static void RefreshEnvMapForView1() {
    int* level = (int*)0x9017B8;                 // EnableEnvMap: 0 off, 1-3 = faces per frame setting
    if (!cfg.splitReflections || *level <= 0) return;
    int saved = *level;
    *level = 3;
    ((G::VoidFn)0x756060)();                     // UpdateEnvironmentMapCameras (uses view 1's camera + target)
    *level = saved;
}
#include "quad.inc"

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
    if (!split) { g_pass = 0; g_quadRender = false; ((G::VoidFn)o_DisplayInner)(); return; }

    bool quad = QuadViewsReady();
    if (quad != g_quadRender) Log("4P test: %s rendering", quad ? "four view" : "two view");
    g_quadRender = quad;
    for (auto& c : g_captured) c = false;
    // ---- player 1
    RecalcView1();
    RefreshEnvMapForView1();
    g_pass = 1; g_passSlot = 0;
    ((G::VoidFn)o_DisplayInner)();
    G::PostDisplayFX();
    G::DisplayComposite(0);
    if (!g_captured[0]) { Capture(0); Dev()->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0); }

    // ---- players 2 and 3 of four: complete passes like player 1's
    if (quad) {
        for (int k = 1; k <= 2; k++) {
            SwapPlayerViews(kQuadView[k]);
            RecalcView1();
            RefreshEnvMapForView1();
            g_pass = 1; g_passSlot = k;
            G::DisplayComposite(1);
            ((ViewFn)G::eView_SetupWorldLight)(G::View(1));
            G::ResetRenderStates();
            ((G::VoidFn)o_DisplayInner)();
            G::PostDisplayFX();
            G::DisplayComposite(0);
            if (!g_captured[k]) { Capture(k); Dev()->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0); }
            SwapPlayerViews(kQuadView[k]);
        }
    }
    // ---- last player (2 of 2, or 4 of 4)
    int last = quad ? kQuadView[3] : 2;
    SwapPlayerViews(last);
    RecalcView1();
    RefreshEnvMapForView1();
    g_pass = 2; g_passSlot = quad ? 3 : 1;
    G::DisplayComposite(1);
    ((ViewFn)G::eView_SetupWorldLight)(G::View(1));
    G::ResetRenderStates();
    ((G::VoidFn)o_DisplayInner)();
    SwapPlayerViews(last);
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
    QuadStreaming();
}

// ------------------------------------------------------------------ install
// installed once the game starts initialising, i.e. after every other .asi has applied its patches
static void InstallLateHooks() {
    static bool done = false; if (done) return; done = true;
    uintptr_t o;
    if ((o = HookCall(G::CALL_QRMenu_AddOption2, (void*)Hook_QRAddOption, G::IconScrollerMenu_AddOption))) o_AddOption = o;
    if ((o = HookCall(G::CALL_eDisplayFrameInner, (void*)Hook_DisplayFrameInner, 0x6DE300))) o_DisplayInner = o;
    if ((o = HookCall(G::CALL_FEManagerRender, (void*)Hook_FERender, G::FEManager_Render))) o_FERender = o;
    InstallFrameMemory();
    InstallQuadTest();
    if (cfg.separateMinimaps) {
        HookCall(0x6BE70F, (void*)Hook_PolyRender3, 0x6D1260);
        HookCall(0x6BE734, (void*)Hook_PolyRender4, 0x6D1930);
        HookCall(0x579A8F, (void*)Hook_UncompressMaps, 0x5672F0);
        HookCall(0x57A33F, (void*)Hook_GatherVisibleIcons, 0x5E4940);
        void* f;
        o_MMUpdateRaw = *(MMUpdateFn*)0x8A2044; f = (void*)Hook_MinimapUpdate; WriteMem(0x8A2044, &f, 4);
        // forget the minimap when the game deletes it (restart rebuilds the HUD)
        if (*(uintptr_t*)0x8A2040 == 0x5A6520) { f = (void*)Hook_MinimapDelete; WriteMem(0x8A2040, &f, 4); }
        else Log("Minimap destructor slot unexpected: %08X", *(uintptr_t*)0x8A2040);
    }
    {   // FE object render filter (minimap passes, speed trap score row)
        void* f;
        if (*(uintptr_t*)0x89B46C == 0x516630) { f = (void*)Hook_GIRenderObject; WriteMem(0x89B46C, &f, 4); }
        else { o_GIRenderObject = *(GIRenderObjFn*)0x89B46C; f = (void*)Hook_GIRenderObject; WriteMem(0x89B46C, &f, 4); Log("GI RenderObject already hooked, chaining"); }
    }
    HookCall(0x52909C, (void*)Hook_OptionsAddOption, G::IconScrollerMenu_AddOption);
    HookCall(0x56431B, (void*)Hook_HandleJoy, 0x559400);
    { void* f = (void*)Hook_OptionsNotify; o_OptionsNotify = *(MenuNotifyFn*)0x89E530; WriteMem(0x89E530, &f, 4); }
    InstallStringHook();
    InstallRaceOptions();
    InstallRaceHud();
    if (cfg.p2MenuInput) { uint8_t* c = (uint8_t*)0x559627; if (c[0] == 0x83 && c[1] == 0xFD && c[2] == 0x01) { uint8_t three = 3; WriteMem(0x559629, &three, 1); } }
    if ((o = HookCall(0x62F33E, (void*)Hook_PausePush, 0x5257F0))) o_PausePush = o;
    if ((o = HookCall(0x6F4EDA, (void*)Hook_CreatePlayers, 0x6F2700))) o_CreatePlayers = o;
    if ((o = HookCall(0x480C16, (void*)Hook_UpdateCameraMovers, 0x47C880))) o_UpdateCameraMovers = o;
    if ((o = HookCall(0x7C2DC5, (void*)Hook_CreateMPProfile, 0x5ABA50))) o_CreateMPProfile = o;   // player 2 car stable
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
    InstallGetTextureInfoDetour();
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

static void LogCrashMarker() {}
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
        {   // which module crashed
            HMODULE m = nullptr; char path[MAX_PATH] = "?";
            void* at = ep->ExceptionRecord->ExceptionAddress;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)at, &m) && m) {
                GetModuleFileNameA(m, path, sizeof path);
                const char* b = strrchr(path, '\\'); Log("  module %s +%08lX", b ? b + 1 : path, (unsigned long)((uintptr_t)at - (uintptr_t)m));
            } else Log("  module: none");
        }
        if (ep->ExceptionRecord->NumberParameters >= 2)
            Log("  access %s %08lX", ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
                (unsigned long)ep->ExceptionRecord->ExceptionInformation[1]);
        LogMemory("  memory at the crash");
        Log("  eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        uint32_t* sp = (uint32_t*)c->Esp;
        char buf[1024]; int n = 0;
        for (int i = 0; i < 64 && n < 1000; i++) {
            uint32_t v = 0;
            if (IsBadReadPtr(sp + i, 4)) break;
            v = sp[i];
            if (v >= 0x401000 && v < 0x890000) n += snprintf(buf + n, sizeof buf - n, "%08X ", v);
            else { HMODULE me = nullptr; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(void*)&LogCrashMarker, &me);
                   MEMORY_BASIC_INFORMATION mi; if (me && VirtualQuery((void*)(uintptr_t)v, &mi, sizeof mi) && mi.AllocationBase == (void*)me) n += snprintf(buf + n, sizeof buf - n, "asi+%X ", (unsigned)(v - (uintptr_t)me)); }
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
        Log("NFSMW Split Screen v2.4 loading");
        if (!CheckVersion()) { Log("Unsupported speed.exe version - mod disabled (needs v1.3)"); return TRUE; }
        LoadXInput();
        LoadBindings();
        Install();
        AddVectoredExceptionHandler(0, CrashFilter);
    }
    return TRUE;
}
