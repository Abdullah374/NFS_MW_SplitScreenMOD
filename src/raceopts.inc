// ------------------------------------------------------------------ split screen race options
// The Quick Race options screen already has Opponents / AI Skill / Catch Up / Traffic Level,
// but hides the AI ones in split screen and the race code only spawns AI in single player.
// In split screen we show them as "AI RACERS" (0..cfg.maxAI) and "AI DIFFICULTY",
// grey out difficulty + catch up when there are no AI racers, and let the race spawn them.
// GRaceParameters::NumOpponents counts player 2 as an opponent: AI racers = NumOpponents - 1.

static const uint32_t STR_AIRACERS = 0x5C2B0002;
static const uint32_t STR_AIDIFF   = 0x5C2B0003;
static const char g_strAIRacers[] = "AI RACERS";
static const char g_strAIDiff[]   = "AI DIFFICULTY";

static int g_splitAI = 0;                        // AI racers in split screen (saved)
static const uint32_t PAD_LEFT  = 0x9120409E;
static const uint32_t PAD_RIGHT = 0xB5971BF1;
static const uintptr_t VT_NumOpponents = 0x8B779C, VT_AISkill = 0x8B77D8, VT_CatchUp = 0x8B7814;

typedef void (THISCALL *WActFn)(void* w, const char* pkg, uint32_t data);
typedef void (THISCALL *WVoidFn)(void* w);
typedef void (THISCALL *WDataFn)(void* w, uint32_t data);
typedef uint8_t* (THISCALL *GetQRSFn)(void* db, int type);
typedef void (THISCALL *SetIntFn)(void* obj, int v);
typedef int (THISCALL *GetIntFn)(void* obj);
typedef void (*SetLangHashFn)(void* str, uint32_t hash);
typedef int (*FEPrintfFn)(void* str, const char* fmt, ...);

static WActFn  o_NumOppAct  = (WActFn)0x7ABFD0;
static WVoidFn o_NumOppDraw = (WVoidFn)0x7AC080;
static WVoidFn o_AISkillDraw = (WVoidFn)0x7AC150;
static WVoidFn o_CatchUpDraw = (WVoidFn)0x7AC230;

static int MaxSplitAI() { return cfg.maxAI < 0 ? 0 : cfg.maxAI > 4 ? 4 : cfg.maxAI; }
static bool FESplit() { uint8_t* db = *G::pFEDatabase; return db && (db[0x12C] & 4) && db[0] == 2; }
static uint8_t* QRSettings() { return ((GetQRSFn)0x56DBE0)(*G::pFEDatabase, 0xB); }

static void LoadRaceOptions() {
    g_splitAI = GetPrivateProfileIntA("RaceOptions", "AIRacers", 0, g_ctlIni);
    if (g_splitAI < 0 || g_splitAI > MaxSplitAI()) g_splitAI = 0;
}
static void SaveRaceOptions() {
    char b[8]; snprintf(b, sizeof b, "%d", g_splitAI);
    WritePrivateProfileStringA("RaceOptions", "AIRacers", b, g_ctlIni);
}

// grey out (and skip) a toggle widget, or bring it back
static void SetWidgetGrey(uint8_t* w, bool grey) {
    void** vt = *(void***)w;
    bool enabled = w[0x28] != 0;
    if (grey && enabled) ((WVoidFn)vt[0x20 / 4])(w);         // FEToggleWidget::Disable
    else if (!grey && !enabled) ((WVoidFn)vt[0x1C / 4])(w);  // FEToggleWidget::Enable
}
// redraw the AI Skill / Catch Up widgets that share this widget's menu list
static void RedrawAISiblings(uint8_t* w) {
    uint8_t* start = w + 4;                 // bTNode inside the widget (next at +0)
    uint8_t* node = *(uint8_t**)start;
    for (int i = 0; i < 32 && node && node != start; i++) {
        uint8_t* o = node - 4;
        uintptr_t vt = *(uintptr_t*)o;
        if (vt == VT_AISkill || vt == VT_CatchUp) ((WVoidFn)(*(void***)o)[0xC / 4])(o);
        node = *(uint8_t**)node;
    }
}

static void FASTCALL Hook_NumOppAct(uint8_t* w, void*, const char* pkg, uint32_t data) {
    if (!cfg.enabled || !FESplit()) { o_NumOppAct(w, pkg, data); return; }
    int mx = MaxSplitAI();
    if (data == PAD_LEFT)       { if (--g_splitAI < 0) g_splitAI = mx; }
    else if (data == PAD_RIGHT) { if (++g_splitAI > mx) g_splitAI = 0; }
    else return;
    SaveRaceOptions();
    uint8_t* db = *G::pFEDatabase;
    if (At<int>(db, 0x18) == 3) {                       // knockout: one lap per eliminated racer
        QRSettings()[4] = (uint8_t)(g_splitAI + 1);
        ((void (THISCALL*)(void*, uint32_t, const char*, uint32_t))0x516C50)(*(void**)0x91CADC, 0x92B703B5, pkg, 0xFF);
    }
    Log("Split race options: AI racers = %d", g_splitAI);
    w[0x2A] = 1;
    void** vt = *(void***)w;
    ((WDataFn)vt[0x38 / 4])(w, data);       // BlinkArrows
    ((WVoidFn)vt[0xC / 4])(w);              // Draw
    RedrawAISiblings(w);
}
static void FASTCALL Hook_NumOppDraw(uint8_t* w) {
    if (!cfg.enabled || !FESplit()) { o_NumOppDraw(w); return; }
    ((SetLangHashFn)0x515C00)(At<void*>(w, 0x2C), STR_AIRACERS);
    ((FEPrintfFn)0x515D70)(At<void*>(w, 0x30), "%d", g_splitAI);
}
static void FASTCALL Hook_AISkillDraw(uint8_t* w) {
    o_AISkillDraw(w);
    if (!cfg.enabled || !FESplit()) return;
    ((SetLangHashFn)0x515C00)(At<void*>(w, 0x2C), STR_AIDIFF);
    SetWidgetGrey(w, g_splitAI == 0);
}
static void FASTCALL Hook_CatchUpDraw(uint8_t* w) {
    o_CatchUpDraw(w);
    if (!cfg.enabled || !FESplit()) return;
    SetWidgetGrey(w, g_splitAI == 0);
}

// UIQRTrackOptions accept: split screen forced SetNumOpponents(1) (= player 2 only)
static void FASTCALL Hook_SplitSetNumOpponents(void* race, void*, int n) {
    if (cfg.enabled) n = g_splitAI + 1;
    Log("Split race: NumOpponents %d (%d AI)", n, n - 1);
    ((SetIntFn)0x5DCE60)(race, n);
    // lap knockout: one lap per eliminated racer, like single player
    if (cfg.enabled && ((GetIntFn)0x5FAA20)(race) == 3) {
        ((SetIntFn)0x5E9A70)(race, n);
        Log("Split knockout: %d laps", n);
    }
}

// ---- lap knockout in split screen
// The race only stops AI cars when they're knocked out. A knocked out player's car is stopped
// too and gets the game's KNOCKED OUT race over message on their HUD, while the other player races on.
static DWORD g_knockedOut[2] = { 0, 0 };
static void PostRaceOverMessage(void* player);
typedef void (FASTCALL *KnockOutFn)(void* racer);
static void FASTCALL Hook_KnockOut(uint8_t* racer) {
    ((KnockOutFn)0x600210)(racer);
    uint8_t* rs = *(uint8_t**)0x91E000;
    if (!rs) return;
    int idx = (int)((racer - (rs + 0x18)) / 0x194);
    bool human = At<void*>(racer, 4) == nullptr;
    Log("Knocked out: racer %d (%s)", idx, human ? (idx == 0 ? "P1" : "P2") : "AI");
    if (!cfg.enabled || *G::pSimUserMode != 1 || !human || idx < 0 || idx > 1) return;
    ((void(THISCALL*)(void*))0x5E9240)(racer);                  // GRacerInfo::ForceStop
    g_knockedOut[idx] = GetTickCount() | 1;
    // the game's own race over message on that player's HUD ("KNOCKED OUT"), as when a
    // player finishes: EShowRaceOverMessage(IPlayer*)
    int np; void** pl = LocalPlayers(&np);
    if (pl && idx < np && pl[idx]) PostRaceOverMessage(pl[idx]);
}

// Game_InitRacers: AI racers are only added when Sim::GetUserMode() == 0
static int Hook_InitRacersUserMode() {
    int m = *G::pSimUserMode;
    return (cfg.enabled && m == 1) ? 0 : m;
}
// ---- split screen starting grid
// Two cars per row behind the start line, all facing the way the race starts. Every racer
// (both players and the AI) gets a slot; the slots are shuffled when RandomStartGrid=1.
// AI cars are put in their slot when Game_InitRacers creates them (moving them later doesn't
// stick), the players are moved once all racers exist.
static uint8_t* g_initRacersArg = nullptr;    // Game_InitRacers' start info: +0x28 position, +0x34 direction
static int g_gridSlot[16], g_gridN = 0, g_gridHumans = 0, g_gridAIPlaced = 0;
static float g_p1StartPos[3], g_p1StartDir[3]; static bool g_p1StartValid = false;
static float g_finalPos[16][3], g_finalDir[16][3]; static bool g_finalValid = false;
static bool g_raceOverShown[2] = { false, false };
static DWORD g_postRaceAt = 0; static char g_postRaceArg = 1;
static void BuildGridRows(int rows);
static void LogRacerPositions(const char* when);
static int g_startLogStage = 0; static DWORD g_startLogTick = 0; static int g_flareStage = 0;
static void SetupGrid(int humans, int ai) {
    g_gridHumans = humans; g_gridN = humans + ai; g_gridAIPlaced = 0; g_p1StartValid = false; g_finalValid = false; g_startLogStage = 1; g_flareStage = 1; g_knockedOut[0] = g_knockedOut[1] = 0; g_raceOverShown[0] = g_raceOverShown[1] = false; g_postRaceAt = 0;
    if (g_gridN > 16) g_gridN = 16;
    for (int i = 0; i < g_gridN; i++) g_gridSlot[i] = i;
    if (cfg.randomGrid) {
        srand(GetTickCount());
        for (int i = g_gridN - 1; i > 0; i--) { int j = rand() % (i + 1); int t = g_gridSlot[i]; g_gridSlot[i] = g_gridSlot[j]; g_gridSlot[j] = t; }
    }
    char order[128]; int o = 0;
    for (int k = 0; k < g_gridN; k++) {
        int who = -1; for (int i = 0; i < g_gridN; i++) if (g_gridSlot[i] == k) who = i;
        o += snprintf(order + o, sizeof order - o, "%s%s", k ? ", " : "", who == 0 ? "P1" : who == 1 && humans > 1 ? "P2" : "AI");
    }
    Log("Grid (%d humans, %d AI): %s", humans, ai, order);
    BuildGridRows((g_gridN + 1) / 2);
}
// Row centres follow the road the same way the game places single player quick race AI:
// a WRoadNav started at the race start, facing backwards, stepped back one row at a time.
static float g_rowPos[8][3], g_rowFwd[8][3]; static int g_rows = 0;
static void BuildGridRows(int rows) {
    g_rows = 0;
    if (!g_initRacersArg || IsBadReadPtr(g_initRacersArg + 0x28, 24)) return;
    float* S = (float*)(g_initRacersArg + 0x28);
    float* f = (float*)(g_initRacersArg + 0x34);
    float len = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (len < 0.01f) return;
    float back[3] = { -f[0] / len, -f[1] / len, -f[2] / len };
    alignas(16) static uint8_t nav[0x600];
    memset(nav, 0, sizeof nav);
    ((void(THISCALL*)(void*))0x77B9E0)(nav);                                            // WRoadNav::WRoadNav
    At<int>(nav, 0x78) = 2;
    ((void(THISCALL*)(void*, float*, float*, bool, float))0x7862D0)(nav, S, back, false, 1.0f);   // InitAtPoint
    const float gap = 9.f;
    if (rows > 8) rows = 8;
    for (int r = 0; r < rows; r++) {
        if (r) ((void(THISCALL*)(void*, float, float*, float))0x77BBA0)(nav, gap, back, 0.f);   // IncNavPosition
        float* p = (float*)(nav + 0x98); float* nf = (float*)(nav + 0xBC);
        float fl = sqrtf(nf[0] * nf[0] + nf[2] * nf[2]);
        bool ok = fl > 0.01f && p[0] == p[0];
        // fall back to a straight line if the road couldn't be followed
        g_rowPos[r][0] = ok ? p[0] : S[0] + back[0] * gap * r;
        g_rowPos[r][1] = ok ? p[1] : S[1];
        g_rowPos[r][2] = ok ? p[2] : S[2] + back[2] * gap * r;
        g_rowFwd[r][0] = ok ? -nf[0] / fl : -back[0];
        g_rowFwd[r][1] = 0;
        g_rowFwd[r][2] = ok ? -nf[2] / fl : -back[2];
        Log("Grid row %d: %.1f %.1f %.1f fwd %.2f %.2f%s", r, g_rowPos[r][0], g_rowPos[r][1], g_rowPos[r][2], g_rowFwd[r][0], g_rowFwd[r][2], ok ? "" : " (straight)");
    }
    ((void(THISCALL*)(void*))0x76E220)(nav);                                            // ~WRoadNav
    g_rows = rows;
}
static bool GridSlotPos(int slot, float* pos, float* dir) {
    int row = slot / 2;
    if (row >= g_rows) return false;
    const float lane = 2.2f;
    float* c = g_rowPos[row]; float* F = g_rowFwd[row];
    float R[3] = { F[2], 0, -F[0] };
    float side = (slot % 2) ? lane : -lane;
    pos[0] = c[0] + R[0] * side; pos[1] = c[1] + 0.5f; pos[2] = c[2] + R[2] * side;
    dir[0] = F[0]; dir[1] = 0; dir[2] = F[2];
    return true;
}

// ...and the count is NumOpponents + 1 - racers so far; keep it in range in split screen
static int FASTCALL Hook_InitRacersNumOpp(void* params) {
    int n = ((GetIntFn)0x5FACD0)(params);
    if (*G::pSimUserMode == 1) {
        int rc = ((GetIntFn)0x5DBEF0)(*(void**)0x91E000);
        int lo = rc - 1, hi = rc - 1 + MaxSplitAI();
        int c = n < lo ? lo : n > hi ? hi : n;
        Log("InitRacers (split): %d racers, NumOpponents %d -> %d, AI to add %d", rc, n, c, c + 1 - rc);
        n = c;
        SetupGrid(rc, c + 1 - rc);
    } else g_startLogStage = 1;          // single player: diagnostics only
    return n;
}

typedef void (THISCALL *PlaceFn)(void* veh, float* pos, float* dir);
static void FASTCALL Hook_PlaceAIVehicle(void* veh, void*, float* pos, float* dir) {
    if (cfg.enabled && *G::pSimUserMode == 1) {
        int idx = g_gridHumans + g_gridAIPlaced++;
        float p[3], d[3];
        if (idx < g_gridN && GridSlotPos(g_gridSlot[idx], p, d)) {
            Log("AI %d -> slot %d (%.1f %.1f %.1f)", idx - g_gridHumans + 1, g_gridSlot[idx], p[0], p[1], p[2]);
            memcpy(pos, p, 12); memcpy(dir, d, 12);
        }
    }
    ((PlaceFn)(*(void***)veh)[0x68 / 4])(veh, pos, dir);
}
typedef void* (THISCALL *FindFn)(void* list, void* handle);
static void* VehicleOfSimable(void* sim) {
    if (!sim) return nullptr;
    void* list = At<void*>((uint8_t*)sim, 4);
    return list ? ((FindFn)0x5D59F0)(list, (void*)0x4040E0) : nullptr;     // _IList::Find(IVehicle::_IHandle)
}
static void PlacePlayersOnGrid() {
    if (!cfg.enabled || *G::pSimUserMode != 1 || g_gridN < 2) return;
    int np; void** pl = LocalPlayers(&np);
    for (int i = 0; i < np && i < g_gridHumans && pl; i++) {
        void* p = pl[i]; if (!p) continue;
        void* sim = ((void*(THISCALL*)(void*))(*(void***)p)[1])(p);         // IPlayer::GetSimable
        void* v = VehicleOfSimable(sim);
        float pos[3], dir[3];
        if (v && GridSlotPos(g_gridSlot[i], pos, dir)) {
            ((PlaceFn)(*(void***)v)[0x68 / 4])(v, pos, dir);
            Log("P%d -> slot %d", i + 1, g_gridSlot[i]);
            if (i == 0) { memcpy(g_p1StartPos, pos, 12); memcpy(g_p1StartDir, dir, 12); g_p1StartValid = true; }
        }
    }
    LogRacerPositions("after grid placement");
}
// Diagnostics: where every racer's car actually is
static const float* SimPosition(void* sim) {
    if (!sim) return nullptr;
    void* rb = ((void*(THISCALL*)(void*))(*(void***)sim)[0x54 / 4])(sim);    // ISimable::GetRigidBody
    if (!rb) return nullptr;
    return ((const float*(THISCALL*)(void*))(*(void***)rb)[0x20 / 4])(rb);  // IRigidBody::GetPosition
}
static void LogRacerPositions(const char* when) {
    if (!cfg.debug) return;
    char b[512]; int o = 0;
    int np; void** pl = LocalPlayers(&np);
    for (int i = 0; i < np && pl; i++) {
        void* sim = pl[i] ? ((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]) : nullptr;
        const float* p = SimPosition(sim);
        if (p) o += snprintf(b + o, sizeof b - o, " P%d(%.1f,%.1f)", i + 1, p[0], p[2]);
    }
    uint8_t** ai = *(uint8_t***)0x92CE3C; int cnt = *(int*)0x92CE44;
    for (int i = 0; ai && i < cnt && i < 8 && o < 450; i++) {
        if (!ai[i]) continue;
        void* sim = ((void*(THISCALL*)(void*))(*(void***)ai[i])[1])(ai[i]);
        const float* p = SimPosition(sim);
        if (p) o += snprintf(b + o, sizeof b - o, " AI%d(%.1f,%.1f)", i + 1, p[0], p[2]);
    }
    Log("Positions %s:%s", when, o ? b : " (none)");
}
// Game_SaveStartPositions: every racer's saved start (+0x170 position, +0x180 direction, used when
// the race script resets the cars) = its grid slot
typedef void (FASTCALL *SaveStartFn)(void* racer);
static void FASTCALL Hook_SaveStartPosition(uint8_t* racer) {
    ((SaveStartFn)0x600540)(racer);
    if (!cfg.enabled || *G::pSimUserMode != 1) return;
    uint8_t* rs = *(uint8_t**)0x91E000;
    int idx = (int)((racer - (rs + 0x18)) / 0x194);
    float pos[3], dir[3];
    if (idx >= 0 && idx < g_gridN && g_finalValid) { memcpy(pos, g_finalPos[idx], 12); memcpy(dir, g_finalDir[idx], 12); }
    else if (!(idx >= 0 && idx < g_gridN && GridSlotPos(g_gridSlot[idx], pos, dir))) return;
    {
        Log("Saved start racer %d: %.1f %.1f -> slot %d (%.1f %.1f)", idx, At<float>(racer, 0x170), At<float>(racer, 0x178), g_gridSlot[idx], pos[0], pos[2]);
        memcpy(racer + 0x170, pos, 12); memcpy(racer + 0x180, dir, 12);
    }
}
// The race script then sets player 1's saved start to the race start marker
// (Game_SetPlayerStartPosition) and warps everyone to their saved starts, which put player 1
// back on the front row on top of whichever AI had that slot. Give it player 1's grid slot instead.
typedef void (THISCALL *SetStartFn)(void* racer, float* pos, float* dir);
static void FASTCALL Hook_SetPlayerSavedStart(void* racer, void*, float* pos, float* dir) {
    if (cfg.enabled && *G::pSimUserMode == 1 && g_p1StartValid) {
        Log("Player 1 start: race marker %.1f %.1f %.1f -> grid slot %.1f %.1f %.1f", pos[0], pos[1], pos[2], g_p1StartPos[0], g_p1StartPos[1], g_p1StartPos[2]);
        pos = g_p1StartPos; dir = g_p1StartDir;
    }
    ((SetStartFn)0x5DC750)(racer, pos, dir);
}
typedef void (THISCALL *PreloadFn)(void* mgr, void* activity);
static void FASTCALL Hook_AfterInitRacers(void* mgr, void*, void* activity) {
    PlacePlayersOnGrid();
    ((PreloadFn)0x602990)(mgr, activity);
}
// more AI than the race lists opponents for: reuse an earlier opponent character
typedef void* (THISCALL *OppCharFn)(void* params, unsigned i);
static void* FASTCALL Hook_GetOpponentChar(void* params, void*, unsigned i) {
    void* c = ((OppCharFn)0x5FD2D0)(params, i);
    if (!c && *G::pSimUserMode == 1) {
        for (int j = (int)i - 1; j >= 0 && !c; j--) c = ((OppCharFn)0x5FD2D0)(params, j);
        Log("Opponent character %u missing, reusing an earlier one (%p)", i, c);
    }
    return c;
}
static uint8_t* MakeStub(int size) { return (uint8_t*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); }
static void EmitJmp(uint8_t* at, uint8_t op, uintptr_t to) { at[0] = op; *(int32_t*)(at + 1) = (int32_t)(to - ((uintptr_t)at + 5)); }
static void Hook_SplitWarp();
static void* FASTCALL Hook_RaceOverMessageCtor(void* ev, void*, void* player);
static void Hook_ShowPostRaceScreen(char arg);
static void InstallAIPlacement() {
    // 1) the user mode check at the top of Game_InitRacers also records the function's argument
    {
        uint8_t* t = MakeStub(32); int n = 0;
        t[n++] = 0x8B; t[n++] = 0x84; t[n++] = 0x24; *(uint32_t*)(t + n) = 4 + 0x3D8; n += 4;     // mov eax,[esp+4+0x3d8]
        t[n++] = 0xA3; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)&g_initRacersArg; n += 4;       // mov [g_initRacersArg],eax
        EmitJmp(t + n, 0xE9, (uintptr_t)Hook_InitRacersUserMode); n += 5;
        PatchCall(0x61DC86, t, 0x6E8DF0);
    }
    // 2) split screen: skip the character start markers, use the road placement
    {
        uint8_t* p = (uint8_t*)0x61DE9E;
        if (p[0] == 0x0F && p[1] == 0x84 && *(int32_t*)(p + 2) == 0x243) {
            uint8_t* t = MakeStub(32); int n = 0;
            t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x61E0E7 - ((uintptr_t)t + n + 4)); n += 4;  // je road
            t[n++] = 0x83; t[n++] = 0x3D; *(uint32_t*)(t + n) = 0x988E04; n += 4; t[n++] = 0x01;                         // cmp [userMode],1
            t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x61E0E7 - ((uintptr_t)t + n + 4)); n += 4;  // je road
            EmitJmp(t + n, 0xE9, 0x61DEA4); n += 5;                                                                       // marker path
            uint8_t j[6]; j[0] = 0xE9; j[5] = 0x90;
            *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x61DE9E + 5));
            WriteMem(0x61DE9E, j, 6);
        } else Log("AI marker patch: unexpected bytes");
    }
    // 3) vehicle placement call: mov ecx,esi / call [edx+0x68]  ->  call stub (mov ecx,esi / jmp hook)
    {
        uint8_t* p = (uint8_t*)0x61E214;
        const uint8_t exp[5] = { 0x8B, 0xCE, 0xFF, 0x52, 0x68 };
        if (!memcmp(p, exp, 5)) {
            uint8_t* t = MakeStub(16); t[0] = 0x8B; t[1] = 0xCE; EmitJmp(t + 2, 0xE9, (uintptr_t)Hook_PlaceAIVehicle);
            uint8_t c[5]; EmitJmp(c, 0xE8, 0); *(int32_t*)(c + 1) = (int32_t)((uintptr_t)t - (0x61E214 + 5));
            WriteMem(0x61E214, c, 5);
        } else Log("AI placement patch: unexpected bytes");
    }
    PatchCall(0x61DCC8, (void*)Hook_GetOpponentChar, 0x5FD2D0);
    PatchCall(0x61E27C, (void*)Hook_AfterInitRacers, 0x602990);
    PatchCall(0x6116F3, (void*)Hook_SetPlayerSavedStart, 0x5DC750);
    PatchCall(0x604EB0, (void*)Hook_SaveStartPosition, 0x600540);
    PatchCall(0x611613, (void*)Hook_SplitWarp, 0x60D820);
    PatchCall(0x611ACD, (void*)Hook_RaceOverMessageCtor, 0x625160);
    PatchCall(0x611F9C, (void*)Hook_ShowPostRaceScreen, 0x60AC20);
}

static void InstallRaceOptions() {
    // show NumOpponents / AISkill / CatchUp in split screen (circuit, sprint, drag, speed trap)
    // world models (e.g. the flares at the finish line) are skipped in the split view mode unless
    // flagged for it; draw them all
    { uint8_t* p = (uint8_t*)0x75390B; if (p[0] == 0x84 && p[1] == 0xDB && p[2] == 0x74 && p[3] == 0x0B) { uint8_t b[4] = { 0xEB, 0x0D, 0x90, 0x90 }; WriteMem(0x75390B, b, 4); } else Log("world model patch: unexpected bytes"); }
    // Quick Race mode list: the game leaves Lap Knockout out in split screen
    { uint8_t* p = (uint8_t*)0x7AAB76; if (p[0] == 0x74 && p[1] == 0x3D) { uint8_t n[2] = { 0x90, 0x90 }; WriteMem(0x7AAB76, n, 2); } else Log("knockout menu patch: unexpected bytes"); }
    PatchCall(0x61146A, (void*)Hook_KnockOut, 0x600210);
    const uintptr_t jes[] = { 0x7AC9CB, 0x7ACBAE, 0x7ACD51, 0x7AD1CE };
    for (uintptr_t a : jes) {
        uint8_t* p = (uint8_t*)a;
        if (p[0] == 0x0F && p[1] == 0x84) { uint8_t n[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; WriteMem(a, n, 6); }
        else Log("race options patch %08X: unexpected bytes", a);
    }
    struct { uintptr_t slot; void* fn; uintptr_t expect; } vt[] = {
        { VT_NumOpponents + 0x4, (void*)Hook_NumOppAct,   0x7ABFD0 },
        { VT_NumOpponents + 0xC, (void*)Hook_NumOppDraw,  0x7AC080 },
        { VT_AISkill + 0xC,      (void*)Hook_AISkillDraw, 0x7AC150 },
        { VT_CatchUp + 0xC,      (void*)Hook_CatchUpDraw, 0x7AC230 },
    };
    for (auto& v : vt) {
        uintptr_t cur = *(uintptr_t*)v.slot;
        if (cur != v.expect) Log("race options vtable %08X already hooked (-> %08X), chaining", v.slot, cur);
        if (v.slot == VT_NumOpponents + 0x4) o_NumOppAct = (WActFn)cur;
        else if (v.slot == VT_NumOpponents + 0xC) o_NumOppDraw = (WVoidFn)cur;
        else if (v.slot == VT_AISkill + 0xC) o_AISkillDraw = (WVoidFn)cur;
        else o_CatchUpDraw = (WVoidFn)cur;
        WriteMem(v.slot, &v.fn, 4);
    }
    PatchCall(0x7AB500, (void*)Hook_SplitSetNumOpponents, 0x5DCE60);
    InstallAIPlacement();
    PatchCall(0x61DCB7, (void*)Hook_InitRacersNumOpp, 0x5FACD0);
    LoadRaceOptions();
    Log("Race options installed (AI racers %d, max %d)", g_splitAI, MaxSplitAI());
}


// ---- final grid, after the race script has warped the players to the start
// In split screen Game_WarpPlayerToTrigger puts player 1 and 2 side by side in the two
// lanes at the start line (sub_60D820) and saves that as their restart position. That runs
// after Game_InitRacers and again on every restart, so the whole grid is laid out here:
// the two lanes the players were given are the two grid columns, each row 9 m further back.
static void* RacerSimable(uint8_t* racer) {
    uint32_t h = At<uint32_t>(racer, 0);
    if (!h) return nullptr;
    uint8_t* b = *(uint8_t**)0x92FEB4; int c = *(int*)0x92FEBC;
    uint8_t* node = ((uint8_t*(*)(void*, void*, uint32_t))0x5D51F0)(b, b + c * 8, h);
    return node ? At<void*>(node, 4) : nullptr;
}
static void Hook_SplitWarp() {
    ((void(*)())0x60D820)();
    if (!cfg.enabled || *G::pSimUserMode != 1) return;
    uint8_t* rs = *(uint8_t**)0x91E000;
    if (!rs) return;
    int n = At<int>(rs, 0x1958);
    if (n < 2 || n != g_gridN || n > 16) { Log("Warp: %d racers, grid has %d - left as is", n, g_gridN); return; }
    void* veh[16] = {};
    for (int i = 0; i < n; i++) veh[i] = VehicleOfSimable(RacerSimable(rs + 0x18 + i * 0x194));
    const float* a = SimPosition(RacerSimable(rs + 0x18));
    const float* b = SimPosition(RacerSimable(rs + 0x18 + 0x194));
    if (!a || !b || !g_initRacersArg) { Log("Warp: player positions unknown"); return; }
    float* f = (float*)(g_initRacersArg + 0x34);
    float fl = sqrtf(f[0] * f[0] + f[2] * f[2]);
    if (fl < 0.01f) return;
    float F[3] = { f[0] / fl, 0, f[2] / fl }, R[3] = { F[2], 0, -F[0] };
    float lane[2][3] = { { a[0], a[1], a[2] }, { b[0], b[1], b[2] } };
    // column 0 = the left lane
    if ((a[0] - b[0]) * R[0] + (a[2] - b[2]) * R[2] > 0) for (int k = 0; k < 3; k++) { float t = lane[0][k]; lane[0][k] = lane[1][k]; lane[1][k] = t; }
    // line both lanes up with the front-most of the two
    float da = (lane[0][0] - lane[1][0]) * F[0] + (lane[0][2] - lane[1][2]) * F[2];
    int behind = da < 0 ? 0 : 1; float d = da < 0 ? -da : da;
    lane[behind][0] += F[0] * d; lane[behind][2] += F[2] * d;
    // lanes too close (single lane road): spread them
    float sep = (lane[1][0] - lane[0][0]) * R[0] + (lane[1][2] - lane[0][2]) * R[2];
    if (sep < 3.6f) { float mid[3] = { (lane[0][0] + lane[1][0]) / 2, lane[0][1], (lane[0][2] + lane[1][2]) / 2 };
        for (int k = 0; k < 3; k += 2) { lane[0][k] = mid[k] - R[k] * 2.2f; lane[1][k] = mid[k] + R[k] * 2.2f; } }
    const float gap = 9.f;
    for (int i = 0; i < n; i++) {
        int slot = g_gridSlot[i], row = slot / 2, col = slot % 2;
        float pos[3] = { lane[col][0] - F[0] * gap * row, lane[col][1] + 0.5f, lane[col][2] - F[2] * gap * row };
        float dir[3] = { F[0], 0, F[2] };
        memcpy(g_finalPos[i], pos, 12); memcpy(g_finalDir[i], dir, 12);
        if (veh[i]) ((PlaceFn)(*(void***)veh[i])[0x68 / 4])(veh[i], pos, dir);
        ((SetStartFn)0x5DC750)(rs + 0x18 + i * 0x194, pos, dir);      // restart position
    }
    g_finalValid = true;
    LogRacerPositions("after start warp + grid");
}


// ---- end of a split screen race
// The game shows a player's race over message ("1ST" + time, or KNOCKED OUT) when they finish,
// but goes to the results screen the moment the race is over, so whoever ends the race never
// sees theirs (e.g. the knockout winner). In split screen: make sure both players got their
// message, then open the results screen a few seconds later.
static int PlayerIndexOf(void* player) {
    int np; void** pl = LocalPlayers(&np);
    for (int i = 0; pl && i < np && i < 2; i++) if (pl[i] == player) return i;
    return -1;
}
static void PostRaceOverMessage(void* player) {
    void* ev = ((void*(*)(unsigned))0x627400)(0xC);                  // Event::operator new
    if (ev) ((void*(THISCALL*)(void*, void*))0x625160)(ev, player);   // EShowRaceOverMessage(IPlayer*)
    int i = PlayerIndexOf(player); if (i >= 0) g_raceOverShown[i] = true;
}
typedef void* (THISCALL *RaceOverCtorFn)(void* ev, void* player);
static void* FASTCALL Hook_RaceOverMessageCtor(void* ev, void*, void* player) {
    int i = PlayerIndexOf(player); if (i >= 0) g_raceOverShown[i] = true;
    return ((RaceOverCtorFn)0x625160)(ev, player);
}
static void Hook_ShowPostRaceScreen(char arg) {
    if (!cfg.enabled || *G::pSimUserMode != 1 || cfg.resultsDelayMs <= 0) { ((void(*)(char))0x60AC20)(arg); return; }
    int np; void** pl = LocalPlayers(&np);
    for (int i = 0; pl && i < np && i < 2; i++) if (!g_raceOverShown[i] && pl[i]) PostRaceOverMessage(pl[i]);
    g_postRaceAt = GetTickCount() + cfg.resultsDelayMs; g_postRaceArg = arg;
    Log("Race over: results screen in %d ms", cfg.resultsDelayMs);
}
static void PostRaceTick() {
    if (!g_postRaceAt) return;
    if (*G::pGameFlowState != 6) { g_postRaceAt = 0; return; }       // left the race some other way
    if ((int)(GetTickCount() - g_postRaceAt) < 0) return;
    g_postRaceAt = 0;
    ((void(*)(char))0x60AC20)(g_postRaceArg);
}


// Diagnostics: triggers that carry particle effects (e.g. the flares at the finish line)
static void LogTriggerEffects(const char* when) {
    if (!cfg.debug) return;
    uint8_t* head = *(uint8_t**)0x91DFF8;
    if (!head) { Log("Trigger effects %s: no triggers", when); return; }
    uint8_t* t = head; int n = 0, k = 0;
    do {
        if (IsBadReadPtr(t, 0xD0)) break;
        void* e0 = At<void*>(t, 0xB8); void* e1 = At<void*>(t, 0xBC);
        if (e0 || e1) {
            uint32_t f0 = e0 ? At<uint32_t>((uint8_t*)e0, 0x18) : 0, f1 = e1 ? At<uint32_t>((uint8_t*)e1, 0x18) : 0;
            Log("Trigger effects %s: trig %p type %d enabled %d refs %d pos %.0f %.0f %.0f emitters %p(%s) %p(%s)", when, t,
                At<short>(t, 0x18), At<uint8_t>(t, 0xC4), At<int>(t, 0xC8), At<float>(t, 0x58), At<float>(t, 0x5C), At<float>(t, 0x60),
                e0, e0 ? ((f0 & 0x10) ? "on" : "off") : "-", e1, e1 ? ((f1 & 0x10) ? "on" : "off") : "-");
            n++;
        }
        t = At<uint8_t*>(t, 0x24); k++;
    } while (t && t != head && k < 2000);
    Log("Trigger effects %s: %d of %d instances have particle effects (user mode %d)", when, n, k, *G::pSimUserMode);
}

// Finish line flares: single player recreates and enables the world particle effects
// (trigger emitters such as the finish line flares) when the intro NIS ends. Split screen
// skips that NIS, so the emitters stay off. Do the same refresh once the split race is on screen.
static DWORD g_flareTick = 0;
static void SplitFlareRefresh() {
    if (!g_flareStage || *G::pGameFlowState != 6) return;
    if (*G::pSimUserMode != 1) { g_flareStage = 0; return; }
    if (g_flareStage == 1) { g_flareStage = 2; g_flareTick = GetTickCount(); return; }
    if (GetTickCount() - g_flareTick < 300) return;
    g_flareStage = 0;
    ((void(__thiscall*)(void*))0x50D170)((void*)0x9166C0);   // EmitterSystem::RefreshWorldEffects
    Log("Finish line flares: world effects refreshed for split screen");
}

// log the start positions once the race is on screen and again just after the start
static void StartPositionDiagnostics() {
    SplitFlareRefresh();
    if (!g_startLogStage || *G::pGameFlowState != 6) return;
    if (g_startLogStage == 1) { LogRacerPositions("race on screen"); LogTriggerEffects("race on screen"); g_startLogStage = 2; g_startLogTick = GetTickCount(); }
    else if (g_startLogStage == 2 && GetTickCount() - g_startLogTick > 6000) { LogRacerPositions("6 s later"); LogTriggerEffects("6 s later"); g_startLogStage = 0; }
}
