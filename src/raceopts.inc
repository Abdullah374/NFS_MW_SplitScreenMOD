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
static const uint32_t STR_COPS     = 0x5C2B0005;
static const char g_strCops[]     = "COPS";
static int g_splitCops = 0;                      // cops heat in split screen (0 = off, 1-5, saved)

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
    g_splitCops = GetPrivateProfileIntA("RaceOptions", "Cops", 0, g_ctlIni);
    g_splitCops = g_splitCops > 0 ? 5 : 0;                       // Off or On (heat 5): no lower heat levels
}
static void SaveRaceOptions() {
    char b[8]; snprintf(b, sizeof b, "%d", g_splitAI);
    WritePrivateProfileStringA("RaceOptions", "AIRacers", b, g_ctlIni);
    snprintf(b, sizeof b, "%d", g_splitCops);
    WritePrivateProfileStringA("RaceOptions", "Cops", b, g_ctlIni);
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

// "COPS" row: an extra toggle widget added after Catch Up in split screen (a copy of the
// Catch Up widget with its own Act / Draw). Off or On (the cops come at heat 5).
static void* g_copsVtbl[32];
static void FASTCALL CopsWidget_Act(uint8_t* w, void*, const char* pkg, uint32_t data) {
    if (data == PAD_LEFT || data == PAD_RIGHT) g_splitCops = g_splitCops ? 0 : 5;
    else return;
    SaveRaceOptions();
    Log("Split race options: cops = %d", g_splitCops);
    w[0x2A] = 1;
    void** vt = *(void***)w;
    ((WDataFn)vt[0x38 / 4])(w, data);       // BlinkArrows
    ((WVoidFn)vt[0xC / 4])(w);              // Draw
}
static void FASTCALL CopsWidget_Draw(uint8_t* w) {
    ((SetLangHashFn)0x515C00)(At<void*>(w, 0x2C), STR_COPS);
    if (g_splitCops) ((FEPrintfFn)0x515D70)(At<void*>(w, 0x30), "ON");
    else ((FEPrintfFn)0x515D70)(At<void*>(w, 0x30), "OFF");
}
typedef unsigned (THISCALL *AddToggleFn)(void* menu, void* widget, bool b);
// four player split screen: no AI racers / AI difficulty / catch up / cops rows (four players, or
// three and an AI 4th car, no extra AI racers and no cops). The widget just isn't added.
static bool FourModeOptions() { return cfg.enabled && FESplit() && g_fourMode; }
static bool IsAIOptionWidget(void* w) {
    uintptr_t vt = w ? *(uintptr_t*)w : 0;
    return vt == VT_NumOpponents || vt == VT_AISkill || vt == VT_CatchUp;
}
static unsigned FASTCALL Hook_AddAIOption(uint8_t* menu, void*, void* widget, bool b) {
    if (FourModeOptions() && IsAIOptionWidget(widget)) return 0;
    return ((AddToggleFn)0x588570)(menu, widget, b);
}
static unsigned FASTCALL Hook_AddCatchUpOption(uint8_t* menu, void*, void* widget, bool b) {
    if (FourModeOptions()) return IsAIOptionWidget(widget) ? 0 : ((AddToggleFn)0x588570)(menu, widget, b);
    unsigned r = ((AddToggleFn)0x588570)(menu, widget, b);
    if (!cfg.enabled || !FESplit()) return r;
    // the options package needs a free row for it
    char name[40]; snprintf(name, sizeof name, "%s%d", At<const char*>(menu, 0xB0), At<int>(menu, 0x104));
    uint32_t h = ((uint32_t(*)(const char*))0x5AF1C0)(name);              // FEHashUpper
    void* str = ((void*(*)(const char*, uint32_t))0x5251E0)(At<const char*>(menu, 0x10), h); // FEngFindString
    if (!str) { Log("Cops option: no free row in %s (%s)", At<const char*>(menu, 0x10), name); return r; }
    if (!g_copsVtbl[0]) { memcpy(g_copsVtbl, (void*)VT_CatchUp, sizeof g_copsVtbl); g_copsVtbl[1] = (void*)CopsWidget_Act; g_copsVtbl[3] = (void*)CopsWidget_Draw; }
    void* cw = G::GameNew(0x5C);
    if (!cw) return r;
    ((void(THISCALL*)(void*, bool))0x589300)(cw, true);                    // FEToggleWidget(true)
    *(void***)cw = g_copsVtbl;
    ((AddToggleFn)0x588570)(menu, cw, true);
    Log("Cops option added (row %s)", name);
    return r;
}

// UIQRTrackOptions accept: split screen forced SetNumOpponents(1) (= player 2 only)
// ---- cops in split screen races
// Quick races switch cops off (UIQRTrackOptions sets CopsEnabled = false). For split screen
// the race gets the same settings career races with cops use: cops on, and pursuits use the
// race's heat (UseWorldHeatInRace + ForceHeatLevel / MaxRaceHeatLevel, read by AIPursuit).
static int g_copsHeat = 0;               // heat for the current split race (0 = no cops)
static bool g_busted[2] = { false, false };
static bool g_despawned[2] = { false, false };            // busted player's car taken off the road              // player busted in this split race
static volatile LONG g_bustPending[2] = { 0, 0 };        // HUD messages still to show
static DWORD g_bustTime[2] = { 0, 0 };                   // when the bust was seen (pursuit ends 3 s later)
static bool g_bustPursuitEnded[2] = { false, false };
static int g_pursuitRequestsDenied = 0;
static int g_copsStage = 0; static DWORD g_copsTick = 0, g_copsLogTick = 0;
typedef void (THISCALL *SetAttrFn)(void* race, uint32_t key, const void* value, uint32_t index);
// Split screen races load the audio in its 2 player mode, which also switches police radio
// speech off (EAXSound::LoadInGameSoundBanks: mode 1 -> IsSpeechEnabled = 0, Speech::Manager::Init(3)).
// With cops on, keep speech in its normal mode (jne -> jmp at 0x4D1761).
static void SetSplitCopSpeech(bool on) {
    uint8_t* p = (uint8_t*)0x4D1761;
    if ((p[0] != 0x75 && p[0] != 0xEB) || p[1] != 0x0A) { Log("cop speech patch: unexpected bytes"); return; }
    uint8_t b = on ? 0xEB : 0x75;
    if (p[0] != b) WriteMem(0x4D1761, &b, 1);
}
// Test: with cops on, load the split screen race with the 1 player audio setup (streams, mix,
// speech), i.e. take the not-split branch in EAXSound::LoadInGameSoundBanks (je -> jmp at 0x4D16AE).
static void SetSplitCopAudio(bool onePlayerAudio) {
    uint8_t* p = (uint8_t*)0x4D16AE;
    if ((p[0] != 0x74 && p[0] != 0xEB) || p[1] != 0x15) { Log("cop audio patch: unexpected bytes"); return; }
    uint8_t b = onePlayerAudio ? 0xEB : 0x74;
    if (p[0] != b) WriteMem(0x4D16AE, &b, 1);
}
// Cop car sounds (engine + siren): CSTATEMGR_CarState::UpdateParams never attaches a sound
// state to car type 2 (cops) in split screen user mode (je at 0x4E643C). Allow it when cops are on.
static void SetSplitCopCarSounds(bool on) {
    uint8_t* p = (uint8_t*)0x4E643C;
    bool orig = p[0] == 0x74 && p[1] == 0x08, patched = p[0] == 0x90 && p[1] == 0x90;
    if (!orig && !patched) { Log("cop car sound patch: unexpected bytes"); return; }
    if (on && orig) { uint8_t n[2] = { 0x90, 0x90 }; WriteMem(0x4E643C, n, 2); }
    else if (!on && patched) { uint8_t o[2] = { 0x74, 0x08 }; WriteMem(0x4E643C, o, 2); }
}
// SoundAI (pursuit audio: sirens, pursuit state, police speech) is a singleton activity created
// with each local player, but SoundAI::Construct returns nothing in split screen user mode
// (je at 0x7207E7). Allow it when cops are on; both players then share the one SoundAI.
static void SetSplitSoundAI(bool on) {
    uint8_t* p = (uint8_t*)0x7207E7;
    bool orig = p[0] == 0x74 && p[1] == 0x4C, patched = p[0] == 0x90 && p[1] == 0x90;
    if (!orig && !patched) { Log("SoundAI patch: unexpected bytes"); return; }
    if (on && orig) { uint8_t n[2] = { 0x90, 0x90 }; WriteMem(0x7207E7, n, 2); }
    else if (!on && patched) { uint8_t o[2] = { 0x74, 0x4C }; WriteMem(0x7207E7, o, 2); }
}
// Pursuits during a race use the perp's "race" pursuit levels and support table (no roadblocks,
// spike strips or heavy support). AIPerpVehicle::SetHeat picks them when racing (mov bl,1 at
// 0x4090B8); with split screen cops, use the free roam tables instead (mov bl,0).
static void SetSplitFreeRoamCops(bool on) {
    uint8_t* p = (uint8_t*)0x4090B8;
    if (p[0] != 0xB3 || (p[1] != 0x01 && p[1] != 0x00)) { Log("free roam cops patch: unexpected bytes"); return; }
    uint8_t b = on ? 0x00 : 0x01;
    if (p[1] != b) WriteMem(0x4090B9, &b, 1);
}
static bool FourRace();
static void SetupSplitCops(void* race) {
    { uint8_t* m = *(uint8_t**)0x92C67C; if (m && (At<int>(m, 0x98 - 0x54) == 16 || At<int>(m, 0x98 - 0x54) == 24 || At<int>(m, 0x98 - 0x54) == 32)) At<int>(m, 0x98 - 0x54) = 8; }   // single player's cop car limit
    // no cops in the 4 player mode (picked from its menu entry: the players join only after this)
    g_copsHeat = cfg.enabled && !g_fourMode && !FourRace() ? g_splitCops : 0;
    g_copsStage = 0;
    SetSplitCopSpeech(false);   // police speech alone (with the 2 player audio) crashes
    SetSplitCopCarSounds(g_copsHeat > 0);
    SetSplitSoundAI(g_copsHeat > 0);
    SetSplitFreeRoamCops(g_copsHeat > 0);
    SetSplitCopAudio(false);    // 1 player audio did not bring the sirens back and loses player 2's sounds
    Log("Split cops: audio set up as %s", g_copsHeat > 0 ? "1 player (cop sounds)" : "2 player");
    if (!g_copsHeat) return;
    bool on = true; int heat = g_copsHeat; float fheat = (float)g_copsHeat; int density = 20 * g_copsHeat;
    ((void(THISCALL*)(void*, bool))0x5E9B80)(race, true);                 // GRaceCustom::SetCopsEnabled
    ((SetAttrFn)0x5E75E0)(race, 0x45F2AD6C, &on, 0);                      // UseWorldHeatInRace
    ((SetAttrFn)0x5E74C0)(race, 0xE4211F4F, &heat, 0);                    // ForceHeatLevel
    ((SetAttrFn)0x5E74C0)(race, 0xF5A03629, &fheat, 0);                   // MaxHeatLevel / MaxRaceHeatLevel
    ((SetAttrFn)0x5E74C0)(race, 0xDBC08D32, &density, 0);                 // cop density
    bool en = ((bool(THISCALL*)(void*))0x5FC030)(race);
    bool world = ((bool(THISCALL*)(void*))0x5FC380)(race);
    float fh = ((float(THISCALL*)(void*))0x5FC3E0)(race);
    float mh = ((float(THISCALL*)(void*))0x5FC440)(race);
    int dens = ((int(FASTCALL*)(void*))0x5FD240)(race);
    Log("Split cops: heat %d -> race CopsEnabled %d, UseWorldHeat %d, ForceHeat %.1f, MaxHeat %.1f, density %d",
        g_copsHeat, en, world, fh, mh, dens);
    g_copsStage = 1;
}

// Start a pursuit on a local player. AICopManager::PursueAtHeatLevel only pursues the first
// local player, so for player 2 the two entries of the local player list are swapped around the call.
static void PursuePlayer(int idx, int heat) {
    void* mgr = *(void**)0x92C67C;                                         // ICopMgr singleton
    int n; void** pl = LocalPlayers(&n);
    if (!mgr || !pl || n < 2 || !pl[idx]) return;
    void* first = pl[0];
    if (idx) { pl[0] = pl[idx]; pl[idx] = first; }
    ((void(THISCALL*)(void*, int))(*(void***)mgr)[0x34 / 4])(mgr, heat);    // PursueAtHeatLevel
    if (idx) { pl[idx] = pl[0]; pl[0] = first; }
    Log("Split cops: pursuit started on P%d at heat %d (cops disabled flag %d)", idx + 1, heat, *(int*)0x90D5EC);
}

static void LogPursuits(const char* when) {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr || !cfg.debug) return;
    int n; void** pl = LocalPlayers(&n);
    void* sim[2] = { nullptr, nullptr };
    for (int i = 0; i < 2 && i < n; i++) if (pl[i]) sim[i] = ((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]);
    void** head = At<void**>(mgr, 0xD4);
    if (!head) return;
    int k = 0; char buf[256]; int len = 0; buf[0] = 0;
    for (void** node = (void**)head[0]; node && node != head && k < 16; node = (void**)node[0], k++) {
        void* pur = node[2];
        if (!pur) continue;
        bool player = ((bool(THISCALL*)(void*))(*(void***)pur)[0x8C / 4])(pur);
        uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pur)[2])(pur);
        void* ts = tgt ? At<void*>(tgt, 0x1C) : nullptr;
        const char* who = ts == sim[0] ? "P1" : ts == sim[1] ? "P2" : "other";
        len += snprintf(buf + len, sizeof buf - len, " [%s%s]", who, player ? "" : " (not player)");
        if (len > 200) break;
    }
    Log("Split cops %s: %d pursuit(s)%s, cops disabled flag %d", when, k, buf, *(int*)0x90D5EC);
    // spawning state (ICopMgr subobject of AICopManager; the manager itself starts 0x54 earlier)
    bool sp0 = ((bool(THISCALL*)(void*, bool))(*(void***)mgr)[1])(mgr, false);
    bool sp1 = ((bool(THISCALL*)(void*, bool))(*(void***)mgr)[1])(mgr, true);
    bool pending = ((bool(THISCALL*)(void*))(*(void***)mgr)[0x18 / 4])(mgr);
    Log("  SoundAI %p, speech enabled %d, sound enabled %d", *(void**)0x993CC8, *(int*)0x8F8700, *(int*)0x8F86F8);
    // car sound state managers (EAXSound::m_pStateMgr[13]) and car sound connections
    {
        char b[400]; int l = 0;
        for (int i = 0; i < 13; i++) {
            uint8_t* m = ((uint8_t**)0x911F60)[i];
            if (!m || IsBadReadPtr(m, 0x28)) continue;
            int states = 0, att = 0;
            for (uint8_t* st = At<uint8_t*>(m, 0x10); st && states < 64; st = At<uint8_t*>(st, 4)) { states++; if (st[0x38]) att++; }
            l += snprintf(b + l, sizeof b - l, " [%d t%d %d/%d]", i, At<int>(m, 0x1C), att, states);
        }
        Log("  sound managers (index type attached/states):%s", b);
        uint8_t** conns = *(uint8_t***)0x913E74; int nc = *(int*)0x913E7C;
        l = 0; b[0] = 0;
        for (int i = 0; conns && i < nc && i < 40; i++) {
            uint8_t* c = conns[i]; if (!c) continue;
            uint8_t* cs = At<uint8_t*>(c, 0x14);
            if (cs) l += snprintf(b + l, sizeof b - l, " t%d/%d/%d", At<int>(cs, 0x228), cs[0x22C], c[0x11]);
            else l += snprintf(b + l, sizeof b - l, " -/%d", c[0x11]);
            if (l > 360) break;
        }
        Log("  car sound conns %d (type/flag22c/active):%s", nc, b);
    }
    Log("  spawning enabled %d/%d, lock %d, timer %.2f, NIS %p, spawn requests %d, cop vehicles %d, pending %d",
        sp0, sp1, mgr[0x60], At<float>(mgr, 0x58), *(void**)0x9885C8, At<int>(mgr, 0x1C), At<int>(mgr, 0x7C), pending);
}
static void LogCopHudInfo() {
    if (g_mm) Log("  minimap %p cop objs %p %p %p %p heli %p %p", g_mm, At<void*>(g_mm, 0xD0), At<void*>(g_mm, 0xD4), At<void*>(g_mm, 0xD8), At<void*>(g_mm, 0xDC), At<void*>(g_mm, 0xC8), At<void*>(g_mm, 0xCC));
    const char* pk[] = { "HUD_Player1.fng", "HUD_Player2.fng", "HUD_SingleRace.fng" };
    const uint32_t hs[] = { 0x7F91DA62, 0x6F85ED55, 0x862824C9, 0x4B2CBE1B };
    for (const char* p : pk) {
        void* o[4]; for (int i = 0; i < 4; i++) o[i] = ((void*(*)(const char*, uint32_t))0x524850)(p, hs[i]);
        Log("  %s heat meter objects %p %p %p %p", p, o[0], o[1], o[2], o[3]);
    }
}
static void TryCopVehicle() {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr) return;
    void* v = ((void*(THISCALL*)(void*, const char*))0x41ECD0)(mgr - 0x54, "copmidsize");
    Log("Split cops: test GetAvailableCopVehicleByName(copmidsize) -> %p", v);
}

// ---- timed backup: every 20 s at heat 5 (10 s longer per heat level below), per player
// pursuit, the cops call in a roadblock (layouts with cars, sawhorses and spike strips) or heavy
// support (Rhino SUVs), through the same fields the game's own logic uses:
//  roadblock: AICopManager (+0xB8 size, +0xBC pursuit) -> UpdatePursuits -> CreateRoadBlock
//  heavy:     IPursuit +0x194 HeavySupport option, +0x208, +0x20C = 1 -> RequestGroundSupport / UpdateSupportCops
static DWORD g_nextBackup[2] = { 0, 0 };
static int PlayerIndexOfSimable(void* sim);
static int BackupIntervalMs() { return (20 + (5 - g_copsHeat) * 10) * 1000; }   // per player: heat 5: 20 s ... heat 1: 60 s
// While a roadblock is pending (AICopManager +0xBC) UpdatePursuits spawns no cop cars for any pursuit,
// and a roadblock that can't be placed stays pending: give it 10 s, then drop it.
static DWORD g_rbQueuedAt = 0; static int g_rbTries = 0, g_rbFor = -1;
static bool QueueRoadBlock(uint8_t* mgr, void* pursuit) {
    if (At<void*>(mgr, 0x68)) return false;                    // one already pending
    At<int>(mgr, 0x64) = 4; At<void*>(mgr, 0x68) = pursuit;
    g_rbQueuedAt = GetTickCount() | 1;
    return true;
}
static void CopPoolStatus(uint8_t* mgr, char* out, int n);
static void RoadBlockTimeout(uint8_t* mgr) {
    if (!At<void*>(mgr, 0x68)) { g_rbQueuedAt = 0; return; }
    if (g_rbQueuedAt && (int)(GetTickCount() - g_rbQueuedAt) > 10000) {
        At<void*>(mgr, 0x68) = nullptr; At<int>(mgr, 0x64) = 0; g_rbQueuedAt = 0;
        char why[96]; CopPoolStatus(mgr, why, sizeof why);
        Log("Split cops: P%d's roadblock could not be placed in 10 s (%d tries: no fitting road 250 m ahead, or not enough free cop cars; %s), dropped", g_rbFor + 1, g_rbTries, why);
    }
}
static bool QueueHeavySupport(uint8_t* pursuit) {
    if (At<int>(pursuit, 0x20C)) return false;                 // a request is already pending
    void* attr = ((void*(THISCALL*)(void*))0x418EE0)(pursuit - 0x48);           // GetPursuitSupportAttrib
    if (!attr) return false;
    unsigned n = ((unsigned(THISCALL*)(void*))0x403600)(attr);                 // Num_HeavySupportOptions
    if (!n) return false;
    // some heavy support options are Rhino roadblocks (type 4): they wait until the pursuit has no
    // roadblock standing, often too long. Only the ones that drive at the player.
    uint8_t* opt = nullptr; unsigned start = GetTickCount() % n;
    for (unsigned k = 0; k < n && !opt; k++) {
        uint8_t* o = ((uint8_t*(THISCALL*)(void*, unsigned))0x4035E0)(attr, (start + k) % n);
        if (o && At<int>(o, 0) != 4) opt = o;
    }
    if (!opt) { Log("Split cops: no Rhino option that drives at the player (only Rhino roadblocks)"); return false; }
    At<void*>(pursuit, 0x194) = opt; At<void*>(pursuit, 0x198) = nullptr;
    At<int>(pursuit, 0x208) = At<int>(opt, 8); At<int>(pursuit, 0x20C) = 1;
    return true;
}
// fixed order per player: 0 roadblock (cars / sawhorses), 1 spike strip roadblock, 2 heavy support
// (Rhinos head-on). The roadblock kind is forced in PickRoadblockSetup(width, cars, spikes), which
// picks from SPIKES_RoadblockCandidateList or RoadblockCandidateList.
static int g_backupStep[2] = { 0, 0 };
static volatile int g_rbForce = -1;              // -1 game's choice, 0 no spikes, 1 spikes (next roadblock)
typedef void* (*PickRoadblockFn)(float width, int cars, bool spikes);
static void* Hook_PickRoadblockSetup(float width, int cars, bool spikes) {
    int f = g_rbForce;
    if (f < 0 || !cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat) return ((PickRoadblockFn)0x4063D0)(width, cars, spikes);
    g_rbForce = -1;
    void* r = ((PickRoadblockFn)0x4063D0)(width, cars, f == 1);
    if (!r) r = ((PickRoadblockFn)0x4063D0)(width, cars, f != 1);      // nothing fits: the other kind
    Log("Split cops: roadblock setup %s%s", f == 1 ? "with spikes" : "without spikes", r ? "" : " (none fits)");
    return r;
}
// ---- one backup clock for both players (it takes turns), placed so it never blocks the wrong player:
//  - a roadblock (or spike strip) goes ahead of its player only when the other player isn't on the
//    road just ahead of them (it would land in front of the other player); then it's Rhinos instead
//  - players close together (200 m) count as one group: the roadblock goes ahead of whoever leads
static const float* SimPosition(void* sim);
static void* PlayerSim(int i) {
    int n; void** pl = LocalPlayers(&n);
    return pl && i < n && pl[i] ? ((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]) : nullptr;
}
static float RaceMetres(void* sim) {          // how far along the race route (metres), -1 if unknown
    uint8_t* rs = *(uint8_t**)0x91E000;
    uint8_t* r = rs && sim ? (uint8_t*)((void*(THISCALL*)(void*, void*))0x5E8B20)(rs, sim) : nullptr;
    float total = rs ? At<float>(rs, 0x4484) : 0.f;                 // GRaceStatus: race length (all laps)
    return r && total > 0 ? At<float>(r, 0x18) * total / 100.f : -1.f;   // +0x18: % of the whole race
}
static uint8_t* PursuitOf(uint8_t* mgr, int idx) {
    void** head = At<void**>(mgr, 0xD4);
    int k = 0;
    for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 16; node = (void**)node[0], k++) {
        uint8_t* pur = (uint8_t*)node[2];
        if (!pur || At<int>(pur, 0x218) == 4) continue;
        uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pur)[2])(pur);
        if (tgt && PlayerIndexOfSimable(At<void*>(tgt, 0x1C)) == idx) return pur;
    }
    return nullptr;
}
static bool PlayerOut(int i);
static bool ResultsShown();
static DWORD g_nextBackupAll = 0; static int g_backupTurn = 0; static DWORD g_heavyQueuedAt[2] = { 0, 0 }; static DWORD g_copTransferUntil = 0; static void* g_copMoved[32]; static int g_copMovedN = 0;
// ---- backup: one shared sequence for both players - roadblock, spike strips, Rhinos, again - always
// for whoever leads the race (metres along the route). A roadblock stays where it was built until every
// player still racing has driven past it, and is taken off the road once they're all 150 m beyond it.
// To keep it, it's detached from the leader's pursuit as soon as it's built (no pursuit = the game
// doesn't release its cops when the leader passes, and the pursuit can get its next roadblock).
static int g_backupStepAll = 0;
struct HeldRoadBlock { void* rb; DWORD since; };
static HeldRoadBlock g_heldRb[8]; static int g_heldRbN = 0;
static bool RoadBlockExists(uint8_t* mgr, void* rb) {
    void** head = At<void**>(mgr, 0x134 - 0x54);                  // AICopManager +0x134: the roadblocks
    int k = 0;
    for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 32; node = (void**)node[0], k++)
        if (node[2] == rb) return true;
    return false;
}
static void HoldRoadBlock(uint8_t* pursuit) {
    void* rb = ((void*(THISCALL*)(void*))(*(void***)pursuit)[0x3C / 4])(pursuit);       // IPursuit::GetRoadBlock
    if (!rb || g_heldRbN >= 8) return;
    // as AIRoadBlock::SetPursuit does, without attaching a new pursuit (Attach(null) crashes):
    // detach the old one (both sides' OnDetached clear their pointer) and clear the roadblock's field
    void* old = At<void*>((uint8_t*)rb, 0x28);
    if (old) {
        uint8_t* att = (uint8_t*)rb - 0x1C;                                            // its IAttachable
        ((void(THISCALL*)(void*, void*))(*(void***)att)[0xC / 4])(att, old);           // Detach
        At<void*>((uint8_t*)rb, 0x28) = nullptr;
    }
    if (At<void*>(pursuit, 0x84) == rb) At<void*>(pursuit, 0x84) = nullptr;
    g_heldRb[g_heldRbN++] = { rb, GetTickCount() };
}
static void DespawnRoadBlockCops(uint8_t* mgr, void* rb) {
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C);
    void* list[16]; int n = 0;
    for (int i = 0; cops && i < nc && i < 40 && n < 16; i++) {
        uint8_t* v = (uint8_t*)cops[i];
        if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;
        void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
        if (ai && ((void*(THISCALL*)(void*))(*(void***)ai)[0xA4 / 4])(ai) == rb) list[n++] = v;   // the cop's roadblock
    }
    for (int i = 0; i < n; i++) {
        void* v = list[i];
        void* sim = ((void*(THISCALL*)(void*))(*(void***)v)[1])(v);
        ((void(THISCALL*)(void*, void*))0x406450)(mgr - 0x54, v);                                  // RemoveActiveCopVehicle
        uint8_t msg[0x40];
        void* h = sim ? ((void*(THISCALL*)(void*))(*(void***)sim)[0x34 / 4])(sim) : nullptr;
        ((void*(THISCALL*)(void*, void*, int))0x405680)(msg, h, 5);                                // MUnspawnCop
        At<uint32_t>(msg, 4) = G::stringhash32("SoundAI");
        ((void(THISCALL*)(void*))0x65FAF0)(msg);
    }
}
// the cops standing in a held roadblock (their IPursuitAI): with the roadblock off the pursuit they're
// not "in pursuit", so they'd ask for one on the first player they see and drive off - the roadblock
// then has no cops and the game removes it. Hook_PursuitRequest turns those requests down.
static void* g_heldCopAI[32]; static int g_heldCopAIN = 0;
static bool HeldRoadBlockCop(void* ai) { for (int i = 0; i < g_heldCopAIN; i++) if (g_heldCopAI[i] == ai) return true; return false; }
static void CollectHeldRoadBlockCops(uint8_t* mgr) {
    g_heldCopAIN = 0;
    if (!g_heldRbN) return;
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C);
    for (int i = 0; cops && i < nc && i < 40 && g_heldCopAIN < 32; i++) {
        uint8_t* v = (uint8_t*)cops[i];
        if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;
        void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
        void* rb = ai ? ((void*(THISCALL*)(void*))(*(void***)ai)[0xA4 / 4])(ai) : nullptr;
        bool held = false;
        for (int j = 0; j < g_heldRbN; j++) if (g_heldRb[j].rb == rb) held = true;
        if (!rb || !held) continue;
        void* pai = ((void*(THISCALL*)(void*, void*))0x5D59F0)(At<void*>((uint8_t*)v, 4), (void*)0x4038E0);   // Find(IPursuitAI)
        if (pai) g_heldCopAI[g_heldCopAIN++] = pai;
    }
}
static void UpdateHeldRoadBlocks(uint8_t* mgr) {
    static DWORD last = 0; DWORD now = GetTickCount();
    CollectHeldRoadBlockCops(mgr);
    if (now - last < 500) return;
    last = now;
    for (int j = 0; j < g_heldRbN; ) {
        void* rb = g_heldRb[j].rb;
        if (!RoadBlockExists(mgr, rb)) { Log("Split cops: a held roadblock was removed by the game (its cops left or were wrecked)"); g_heldRb[j] = g_heldRb[--g_heldRbN]; continue; }
        const float* c = ((const float*(THISCALL*)(void*))(*(void***)rb)[0x38 / 4])(rb);       // centre
        const float* d = ((const float*(THISCALL*)(void*))(*(void***)rb)[0x3C / 4])(rb);       // direction
        bool allPast = true; int racing = 0;
        for (int i = 0; i < 2; i++) {
            if (PlayerOut(i)) continue;
            const float* p = SimPosition(PlayerSim(i));
            if (!p) continue;
            racing++;
            float v[3] = { c[0] - p[0], c[1] - p[1], c[2] - p[2] };
            bool past = v[0] * d[0] + v[1] * d[1] + v[2] * d[2] < 0;                          // as UpdateRoadBlocks
            if (!past || v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 150.f * 150.f) allPast = false;
        }
        if ((racing && !allPast) && now - g_heldRb[j].since < 180000) { j++; continue; }
        Log("Split cops: roadblock (%d cops held) taken off the road (%s)", g_heldCopAIN, racing ? "every player is 150 m past it" : "nobody racing");
        DespawnRoadBlockCops(mgr, rb);
        g_heldRb[j] = g_heldRb[--g_heldRbN];
    }
}
static void SplitBackupTick() {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr || g_copsStage < 3) return;
    DWORD now = GetTickCount();
    uint8_t* pur[2] = { nullptr, nullptr };
    for (int i = 0; i < 2; i++) if (!PlayerOut(i)) pur[i] = PursuitOf(mgr, i);
    if (!pur[0] && !pur[1]) return;
    // twice as often with both players in a pursuit, so each still gets one every interval
    int ms = BackupIntervalMs() / ((pur[0] && pur[1]) ? 2 : 1);
    if (!g_nextBackupAll) { g_nextBackupAll = now + ms; return; }
    if ((int)(now - g_nextBackupAll) < 0) return;
    int t = g_backupTurn;
    if (!pur[t]) t = 1 - t;
    int o = 1 - t;
    static const char* kNames[3] = { "roadblock", "spike strips", "heavy support (Rhinos)" };
    int step = 2;                                    // roadblocks and spike strips are off for now: Rhinos only
    int rbFor = t;                                   // whose pursuit the roadblock is for
    const char* why = "";
    if (step < 2 && pur[o]) {
        const float* pt = SimPosition(PlayerSim(t)); const float* po = SimPosition(PlayerSim(o));
        float d = 1e9f;
        if (pt && po) { float dx = pt[0] - po[0], dy = pt[1] - po[1], dz = pt[2] - po[2]; d = sqrtf(dx * dx + dy * dy + dz * dz); }
        // metres along the race route between them (straight line if the route length is unknown)
        float mo = RaceMetres(PlayerSim(o)), mt = RaceMetres(PlayerSim(t));
        float gap = (mo >= 0 && mt >= 0) ? mo - mt : d;
        bool oAhead = (mo >= 0 && mt >= 0) ? gap > 0 : false;
        if (mo < 0 || mt < 0) gap = d;
        if (fabsf(gap) < 200.f && d < 400.f) { if (oAhead) { rbFor = o; why = " (players together: ahead of the leader)"; } }
        // the game puts a roadblock 250 m ahead of its player (CreateRoadBlock): with the other player
        // up to 300 m ahead it would land in front of them (or on them), further ahead it lands behind them
        else if (oAhead && gap < 300.f) {
            step = 2; why = " (the other player is less than 300 m ahead: Rhinos instead of a roadblock)"; }
    }
    // a pursuit has one roadblock at a time: the game builds no new one while the last still stands
    if (step < 2 && ((void*(THISCALL*)(void*))(*(void***)pur[rbFor])[0x3C / 4])(pur[rbFor])) {
        step = 2; why = rbFor == t ? " (their last roadblock is still standing: Rhinos instead)" : " (the leader's last roadblock is still standing: Rhinos instead)";
    }
    bool ok;
    if (step < 2) { ok = QueueRoadBlock(mgr, pur[rbFor]); if (ok) { g_rbForce = step; g_rbTries = 0; g_rbFor = rbFor; } }
    else ok = QueueHeavySupport(pur[t]);
    Log("Split cops backup for P%d: %s%s%s", t + 1, kNames[step], why, ok ? "" : " (busy, retry)");
    if (ok) {
        if (step == g_backupStep[t]) g_backupStep[t] = (step + 1) % 3;   // a skipped roadblock stays next
        if (step == 2) g_heavyQueuedAt[t] = now | 1;
        g_backupTurn = 1 - t;
        g_nextBackupAll = now + ms;
    } else {
        // busy (a roadblock being set up, or this player's Rhinos still on the way): the other player's
        // turn in 3 s, this player keeps their next backup
        g_backupTurn = pur[o] ? o : t;
        g_nextBackupAll = now + 3000;
    }
    // Rhinos that never came (the request still pending after 20 s): clear it
    for (int i = 0; i < 2; i++)
        if (g_heavyQueuedAt[i] && (int)(now - g_heavyQueuedAt[i]) > 20000) {
            g_heavyQueuedAt[i] = 0;
            if (pur[i] && At<int>(pur[i], 0x20C)) { At<int>(pur[i], 0x20C) = 0; Log("Split cops: P%d's Rhinos never came, request cleared", i + 1); }
        }
}

// ---- more cops per pursuit: AIPursuit::CopRequest / RemoveUnwantedVehicles go by the heat level's cop
// counts (GetAdjustedCopCounts: records of 0x18 bytes, +8 type, +0x10 count). Split screen at
// heat 4-5: twice as many plus one of each type, heat 1-3: half as many again plus one (helicopters unchanged).
static void FASTCALL Hook_AdjustedCopCounts(void* pur, void*, uint8_t* rec, int* n) {
    ((void(THISCALL*)(void*, void*, int*))0x424110)(pur, rec, n);
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || !rec || !n) return;
    uint32_t heli = *(uint32_t*)0x90D8C8;                        // _heliHash1
    for (int i = 0; i < *n && i < 8; i++) {
        uint8_t* r = rec + i * 0x18;
        if (At<uint32_t>(r, 8) == heli) continue;
        int c = At<int>(r, 0x10);
        if (c > 0) At<int>(r, 0x10) = g_copsHeat >= 4 ? c * 2 + 1 : c + c / 2 + 1;   // heat 4-5: twice as many plus one
    }
}

// ---- where new cops appear. AISpawnManager::GetBasePosition (spawn spots must be within a distance
// of it) is view 1's camera, and SpawnVehicleBehindTarget puts a car behind the first local player.
// So cops for player 2 appeared at player 1 and drove over, worst once player 1 was out. Both now go
// by the player the cop is for: view 2's camera while spawning for player 2 (or once player 1 is out),
// and behind the pursuit's own target.
static int g_spawnFor = -1;                     // reinforcement being spawned for this player
static void FASTCALL Hook_GetBasePosition(void* sm, void*, float* out) {
    int v = 1;
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat && (g_spawnFor == 1 || (g_spawnFor < 0 && PlayerOut(0) && !PlayerOut(1)))) v = 2;
    // (view 2 is never "active" in split screen - the mod draws player 2 through view 1 - but its
    // camera follows player 2; checking the flag sent every spawn for player 2 back to player 1)
    uint8_t* view = G::View(v);
    uint8_t* cam = At<uint8_t*>(view, G::EV_CAMERA);
    if (!cam && v == 2) { view = G::View(1); cam = At<uint8_t*>(view, G::EV_CAMERA); v = 1; }
    if (!cam || (v == 1 && !At<uint8_t>(view, 8))) { out[0] = out[1] = out[2] = 0; return; }
    out[0] = -At<float>(cam, 0x44); out[1] = At<float>(cam, 0x48); out[2] = At<float>(cam, 0x40);
}
static float CopDistTo(void* v, const float* p) {             // a cop car's distance to a point (sim space)
    void* sim = ((void*(THISCALL*)(void*))(*(void***)v)[1])(v);
    const float* c = sim ? SimPosition(sim) : nullptr;
    if (!c || !p) return 1e9f;
    float dx = c[0] - p[0], dy = c[1] - p[1], dz = c[2] - p[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
// ---- cops drive off instead of vanishing
// When the heat level goes up, the game sends the lower heat's cops away (AIPursuit::RemoveUnwantedVehicles
// -> FleeCopOfType -> IPursuitAI::StartFlee, AIVehiclePursuit::StartFlee 0x423370): sirens off, goal
// AIGoalFleePursuit (AIActionRace in flee mode drives them off along the road), and they are only taken
// off the road once nobody can see them. The mod uses the same instead of removing cops on the spot.
static uint32_t FleeGoalHash() { static uint32_t h = 0; if (!h) h = G::stringhash32("AIGoalFleePursuit"); return h; }
static bool CopIsFleeing(void* v) {
    void* vai = ((void*(THISCALL*)(void*, void*))0x5D59F0)(At<void*>((uint8_t*)v, 4), (void*)0x403700);   // Find(IVehicleAI)
    if (!vai) return false;
    uint32_t* g = ((uint32_t*(THISCALL*)(void*))(*(void***)vai)[0xC0 / 4])(vai);                 // GetGoalName
    return g && *g == FleeGoalHash();
}
static bool CopStartFlee(void* v) {
    if (CopIsFleeing(v)) return true;
    void* pai = ((void*(THISCALL*)(void*, void*))0x5D59F0)(At<void*>((uint8_t*)v, 4), (void*)0x4038E0);   // Find(IPursuitAI)
    if (!pai) return false;
    ((void(THISCALL*)(void*))(*(void***)pai)[0xC / 4])(pai);                                      // StartFlee
    return true;
}
static void CopUnspawn(uint8_t* mgr, void* v) {        // as AICopManager does with a cop out of view
    void* sim = ((void*(THISCALL*)(void*))(*(void***)v)[1])(v);
    ((void(THISCALL*)(void*, void*))0x406450)(mgr - 0x54, v);                                     // RemoveActiveCopVehicle
    uint8_t msg[0x40];
    void* h = sim ? ((void*(THISCALL*)(void*))(*(void***)sim)[0x34 / 4])(sim) : nullptr;
    ((void*(THISCALL*)(void*, void*, int))0x405680)(msg, h, 5);                                   // MUnspawnCop
    At<uint32_t>(msg, 4) = G::stringhash32("SoundAI");
    ((void(THISCALL*)(void*))0x65FAF0)(msg);                                                      // Hermes::Message::Deliver
}
static bool CopIsChopper(void* v) {
    uint32_t* cls = ((uint32_t*(THISCALL*)(void*))(*(void***)v)[0x44 / 4])(v);
    return cls && *cls == *(uint32_t*)0x92C4F4;
}
static float CopOffscreen(void* v) { return ((float(THISCALL*)(void*))(*(void***)v)[0x60 / 4])(v); }
static void* CopRoadBlock(void* v) {                   // the roadblock this cop stands in (AI +0xA4), or null
    void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
    return ai ? ((void*(THISCALL*)(void*))(*(void***)ai)[0xA4 / 4])(ai) : nullptr;
}
static void* PlayerSim(int i);
static float CopDistToPlayers(void* v) {               // to the nearest local player (busted ones too: their screen still shows the road)
    float d = 1e9f;
    for (int k = 0; k < 2; k++) { const float* p = SimPosition(PlayerSim(k)); if (p) { float x = CopDistTo(v, p); if (x < d) d = x; } }
    return d;
}

// cops in a pursuit that are within r of its player (far ones - left behind, or from the other player's
// ended pursuit - don't count, so new cars still come where the player is)
static int PursuitCopsNear(uint8_t* mgr, uint8_t* pur, const float* p, float r) {
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), n = 0;
    for (int i = 0; cops && i < nc && i < 40; i++) {
        uint8_t* v = (uint8_t*)cops[i];
        if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;
        void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
        uint8_t* cur = ai ? (uint8_t*)((void*(THISCALL*)(void*))(*(void***)ai)[0xA0 / 4])(ai) : nullptr;
        if (!cur || At<void*>(cur, 4) != At<void*>(pur, 4)) continue;                 // same object (as UpdatePursuits compares)
        if (CopIsFleeing(v)) continue;                                                // leaving
        if (CopDistTo(v, p) < r) n++;
    }
    return n;
}
static int PursuitPlayer(uint8_t* pursuit) {
    if (!pursuit) return -1;
    uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pursuit)[2])(pursuit);
    return tgt ? PlayerIndexOfSimable(At<void*>(tgt, 0x1C)) : -1;
}
// Rhinos (UpdatePursuits -> UpdateSupportCops -> StartHeavySupport: spots ahead of the target, checked
// against the spawn base) go by the pursuit's player as well; log why a Rhino call fails
static int g_supportFor = -1;
static void FASTCALL Hook_UpdateSupportCops(void* mgrBase, void*, uint8_t* pursuit) {
    int prev = g_spawnFor;
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat) g_spawnFor = PursuitPlayer(pursuit);
    g_supportFor = g_spawnFor;
    ((void(THISCALL*)(void*, void*))0x43E770)(mgrBase, pursuit);
    g_spawnFor = prev; g_supportFor = -1;
}
static DWORD g_rhinoLog[2] = { 0, 0 };
static bool FASTCALL Hook_GetHeavySupportVehicles(void* mgrBase, void*, void* req) {
    bool r = ((bool(THISCALL*)(void*, void*))0x41F130)(mgrBase, req);
    int i = g_supportFor;
    if (!r && i >= 0 && i < 2 && GetTickCount() - g_rhinoLog[i] > 5000) { g_rhinoLog[i] = GetTickCount(); Log("Split cops: P%d's Rhinos waiting: no Rhino cars available", i + 1); }
    return r;
}
static bool FASTCALL Hook_StartHeavySupport(void* mgrBase, void*, void* pursuit, void* req) {
    bool r = ((bool(THISCALL*)(void*, void*, void*))0x41F2E0)(mgrBase, pursuit, req);
    int i = g_supportFor;
    static DWORD deniedLog[2] = { 0, 0 };
    if (i >= 0 && i < 2) {
        if (r) Log("Split cops: P%d's Rhinos are coming", i + 1);
        else if (GetTickCount() - deniedLog[i] > 5000) { deniedLog[i] = GetTickCount(); Log("Split cops: P%d's Rhinos denied: no spawn spot ahead of the player", i + 1); }
    }
    return r;
}
// CreateRoadBlock looks 250 m ahead (push 250.0f at 0x43DE45) and gives up if that piece of road doesn't
// suit (a junction, a bridge, too narrow): most dropped blocks failed there, not for lack of cars. In split
// screen each try looks a bit further or nearer: 250, 300, 350, 200, 400 m.
static float* g_rbDist = nullptr;
static bool FASTCALL Hook_CreateRoadBlockCall(void* mgrBase, void*, uint8_t* pursuit, int size, void* veh, void* list) {
    static const float kD[5] = { 250.f, 300.f, 350.f, 200.f, 400.f };
    if (g_rbDist) *g_rbDist = (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat && g_rbQueuedAt) ? kD[g_rbTries % 5] : 250.f;
    bool r = ((bool(THISCALL*)(void*, void*, int, void*, void*))0x43DD30)(mgrBase, pursuit, size, veh, list);
    if (g_rbDist) *g_rbDist = 250.f;
    if (g_rbQueuedAt) { g_rbTries++; if (r) Log("Split cops: P%d's roadblock built (try %d, %.0f m ahead)", PursuitPlayer(pursuit) + 1, g_rbTries, kD[(g_rbTries - 1) % 5]); }
    return r;
}
// the game's own cop cars for a pursuit (UpdatePursuits -> SpawnPursuitIVehicle: ahead of the target,
// checked against the spawn base) go by that pursuit's player too
static bool FASTCALL Hook_SpawnAheadOfTarget(void* mgrBase, void*, uint8_t* pursuit, float* pos, float* dir, float dist);
// the other player is racing close behind (within 400 m along the route) the player this pursuit is
// for: a cop dropped in behind that player would land right in front of the other one
static bool OtherCloseBehind(uint8_t* pursuit) {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || !pursuit) return false;
    int i = PursuitPlayer(pursuit), j = 1 - i;
    if (i < 0 || i > 1 || PlayerOut(j) || PlayerOut(i)) return false;
    float mi = RaceMetres(PlayerSim(i)), mj = RaceMetres(PlayerSim(j));
    if (mi >= 0 && mj >= 0) return mj <= mi && mi - mj <= 400.f;
    const float* a = SimPosition(PlayerSim(i)); const float* b = SimPosition(PlayerSim(j));
    if (!a || !b) return false;
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz < 400.f * 400.f;
}
static int g_behindSkipped = 0;
static bool ChaseFull(uint8_t* mgr);
static bool FASTCALL Hook_SpawnPursuitIVehicle(void* mgrBase, void*, uint8_t* pursuit, void* vehicle) {
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat && ChaseFull((uint8_t*)mgrBase + 0x54)) return false;   // keep cars for roadblocks
    // the game's own new cop cars mostly appear behind their player (SpawnPursuitIVehicle drops an
    // existing car or a new one behind the target). In split screen with cops: ahead, like the cars
    // SpawnPursuitCar sends (and like player 1's extra cops), when there's a spot; otherwise as before.
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat && pursuit && vehicle &&
        ((int(THISCALL*)(void*))(*(void***)pursuit)[0x114 / 4])(pursuit) != 2) {
        uint8_t* gm = *(uint8_t**)0x91E00C;
        float dist = (gm && (gm[0x2B4] || gm[0x111])) ? 80.f : 340.f;
        float pos[4] = { 0, 0, 0, 0 }, dir[4] = { 0, 0, 0, 0 };
        if (Hook_SpawnAheadOfTarget(mgrBase, nullptr, pursuit, pos, dir, dist)) {
            uint8_t* v = (uint8_t*)vehicle;
            ((void(THISCALL*)(void*))(*(void***)v)[0x80 / 4])(v);                              // Activate
            uint8_t* ai = ((uint8_t*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
            if (ai) {
                ((void(THISCALL*)(void*, float*, float*))(*(void***)ai)[0x5C / 4])(ai, pos, dir); // place
                ((void(THISCALL*)(void*))(*(void***)ai)[0x88 / 4])(ai);
                void* pai = ((void*(THISCALL*)(void*, void*))0x5D59F0)(At<void*>(ai, 4), (void*)0x4038E0);   // IPursuitAI
                if (pai) ((void(THISCALL*)(void*))(*(void***)pai)[0x78 / 4])(pai);
            }
            ((bool(THISCALL*)(void*, void*))(*(void***)pursuit)[0x78 / 4])(pursuit, v);       // IPursuit::AddVehicle
            At<int>((uint8_t*)mgrBase, 0x94)++;
            static DWORD t = 0; static int n = 0; n++;
            if (GetTickCount() - t > 10000) { t = GetTickCount(); Log("Split cops: %d of the game's own cop cars placed ahead of their player", n); n = 0; }
            return true;
        }
    }
    int prev = g_spawnFor;
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat) g_spawnFor = PursuitPlayer(pursuit);
    // no spot ahead: the game would drop the car in behind its player - not with the other player close behind
    // (with the other player close behind: the car goes to the other player's pursuit, dropped in
    // behind them, rather than behind this player right in front of them)
    uint8_t* forPur = pursuit;
    if (OtherCloseBehind(pursuit)) {
        uint8_t* mgr = *(uint8_t**)0x92C67C;
        int j = 1 - PursuitPlayer(pursuit);
        uint8_t* other = mgr ? PursuitOf(mgr, j) : nullptr;
        if (other) { forPur = other; g_spawnFor = j; g_behindSkipped++; }
    }
    bool r = ((bool(THISCALL*)(void*, void*, void*))0x430E60)(mgrBase, forPur, vehicle);
    g_spawnFor = prev;
    return r;
}
// Where a new cop car for a pursuit appears ahead (GetSpawnPositionAheadOfTarget: 340 m ahead of the
// target along the road). With the players together (within 400 m along the route) a car for either
// player is placed ahead of whoever leads, so a car meant for the player behind never appears right in
// front of the leader, and both get it coming at them. Further apart, or with the other player out,
// ahead of the pursuit's own player.
static bool FASTCALL Hook_SpawnAheadOfTarget(void* mgrBase, void*, uint8_t* pursuit, float* pos, float* dir, float dist) {
    typedef bool (THISCALL *AheadFn)(void*, void*, float*, float*, float);
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat) return ((AheadFn)0x41EED0)(mgrBase, pursuit, pos, dir, dist);
    int i = PursuitPlayer(pursuit), j = 1 - i;
    uint8_t* use = pursuit; int lead = i;
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (mgr && i >= 0 && i < 2 && !PlayerOut(i) && !PlayerOut(j)) {
        uint8_t* other = PursuitOf(mgr, j);
        float mi = RaceMetres(PlayerSim(i)), mj = RaceMetres(PlayerSim(j));
        if (other && mi >= 0 && mj >= 0 && fabsf(mj - mi) <= 400.f && mj > mi) { use = other; lead = j; }
    }
    int prev = g_spawnFor;
    if (lead >= 0) g_spawnFor = lead;
    bool r = ((AheadFn)0x41EED0)(mgrBase, use, pos, dir, dist);
    g_spawnFor = prev;
    return r;
}
static void FASTCALL Hook_SpawnBehindTarget(void* mgrBase, void*, uint8_t* pursuit, void* vehicle) {
    int idx = -1;
    if (cfg.enabled && *G::pSimUserMode == 1 && pursuit) {
        uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pursuit)[2])(pursuit);
        idx = tgt ? PlayerIndexOfSimable(At<void*>(tgt, 0x1C)) : -1;
    }
    // behind this player would be right in front of the other player close behind: behind the other
    // player instead (SpawnVehicleBehindTarget places the car behind the first local player, so that's
    // the one swapped in); the car still chases this pursuit's player
    int behind = idx;
    if (OtherCloseBehind(pursuit)) {
        behind = 1 - idx; g_behindSkipped++;
        static DWORD t = 0; if (GetTickCount() - t > 10000) { t = GetTickCount(); Log("Split cops: %d cop car(s) dropped in behind the player at the back instead (players close together)", g_behindSkipped); g_behindSkipped = 0; }
    }
    int n; void** pl = LocalPlayers(&n);
    bool swap = behind == 1 && pl && n >= 2 && pl[1];
    if (swap) { void* t = pl[0]; pl[0] = pl[1]; pl[1] = t; }
    ((void(THISCALL*)(void*, void*, void*))0x426850)(mgrBase, pursuit, vehicle);
    if (swap) { void* t = pl[0]; pl[0] = pl[1]; pl[1] = t; }
}

// ---- both pursuits get cop cars: AICopManager::UpdatePursuits hands out new cop cars (up to its
// limit of 8, for single player's one pursuit) to the pursuits in list order, so player 1's pursuit
// took them all. With two pursuits: twice the limit, and the pursuit with fewer cops goes first.
// ---- roadblocks and spike strips (v2.4)
//  - one at a time for both players; the next no sooner than 25 s later (heat 5, +10 s per lower heat),
//    roadblock and spike strips taking turns. The game places it 250 m ahead of its pursuit's player on
//    the race route (CreateRoadBlock, kept on the route by Hook_RoadBlockNavPathType).
//  - players under 1 km apart (route metres): it's for whoever leads. Further apart: each player's turn
//    in order, ahead of themselves (the one behind is 1 km+ behind the leader, out of the way).
//  - the game lets a roadblock go once its pursuit's player is past it (UpdateRoadBlocks: SetDodged, the
//    cops leave it to chase, and a roadblock with no cops is removed). So when the leader drives past
//    one that the other player (under 1 km behind) still has to get through, it's handed to the other
//    player's pursuit the way CreateRoadBlock gives one to a pursuit (IPursuit::AddRoadBlock, then
//    IRoadBlock::SetPursuit, which detaches the old pursuit). Then the game lets it go when they pass.
//  - one left without a pursuit (its player busted) goes to the other player if they still have to pass
//    it, else its cops leave once nobody can see them.
static DWORD g_nextBlockAt = 0; static int g_blockKind = 0;
static int BlockIntervalMs() { return (25 + (5 - g_copsHeat) * 10) * 1000; }
// A roadblock takes 2-5 cop cars (CreateRoadBlock: free cars of the cop car pool, else active cops out of
// view for 3 s and 100 m away). The pool doesn't grow past the game's vehicle budget (18 cop cars), so
// with both players together and every cop chasing in sight, none were free and nearly every block was
// dropped. From 8 s before a block is due until it's built, no extra cops are sent and cops out of
// sight are taken off the road sooner, so the block gets its cars.
static bool BlockReserve(uint8_t* mgr) {
    if (At<void*>(mgr, 0x68)) return true;                                   // one waiting to be placed
    return g_nextBlockAt && (int)(g_nextBlockAt - GetTickCount()) < 8000;
}
static float CopOffscreen(void* v);
static bool CopIsChopper(void* v);
static void* CopRoadBlock(void* v);
static void CopPoolStatus(uint8_t* mgr, char* out, int n) {
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), act = 0, free = 0, unseen = 0, rb = 0;
    for (int i = 0; cops && i < nc && i < 40; i++) {
        void* v = cops[i]; if (!v) continue;
        if (!((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) { free++; continue; }
        act++;
        if (CopRoadBlock(v)) rb++;
        if (!CopIsChopper(v) && CopOffscreen(v) > 3.f) unseen++;
    }
    snprintf(out, n, "cop car pool %d: %d on the road (%d in roadblocks, %d out of sight), %d free, %u physics objects", nc, act, rb, unseen, free, *(unsigned*)0x9377C8);
}
// The chase may use 13 cop cars; roadblock / spike strip cars (2-5 each) and cops driving off don't
// count, so a roadblock comes on top of the chase (up to kBlockCopCap more cars) instead of out of it.
static const int kChaseCopCap = 13, kBlockCopCap = 10;
static int CopsOnRoad(uint8_t* mgr) {
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), n = 0;
    for (int i = 0; cops && i < nc && i < 40; i++) {
        void* v = cops[i];
        if (v && ((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v) && !CopIsChopper(v) && !CopRoadBlock(v) && !CopIsFleeing(v)) n++;   // chasers only
    }
    return n;
}
static bool ChaseFull(uint8_t* mgr) {
    if (CopsOnRoad(mgr) < kChaseCopCap) return false;
    static DWORD t = 0; if (GetTickCount() - t > 15000) { t = GetTickCount(); Log("Split cops: %d cop cars chasing, no more chasers (roadblocks get their own %d)", kChaseCopCap, kBlockCopCap); }
    return true;
}
static int RoadBlockCount(uint8_t* mgr) {
    void** head = At<void**>(mgr, 0x134 - 0x54); int k = 0;
    for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 32; node = (void**)node[0]) k++;
    return k;
}
static bool PlayerPastPoint(int i, const float* c, const float* d, float margin) {   // as UpdateRoadBlocks: behind the player
    const float* p = SimPosition(PlayerSim(i));
    if (!p) return true;
    float v[3] = { c[0] - p[0], c[1] - p[1], c[2] - p[2] };
    if (!(v[0] * d[0] + v[1] * d[1] + v[2] * d[2] < 0)) return false;
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] >= margin * margin;
}
// route metres between the players: >0 when player 1 leads; straight line (sign unknown) if no route
static float PlayersGap(bool* known) {
    float m0 = RaceMetres(PlayerSim(0)), m1 = RaceMetres(PlayerSim(1));
    if (m0 >= 0 && m1 >= 0) { if (known) *known = true; return m0 - m1; }
    if (known) *known = false;
    const float* a = SimPosition(PlayerSim(0)); const float* b = SimPosition(PlayerSim(1));
    if (!a || !b) return 1e9f;
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
static void SplitBlockTick() {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr || g_copsStage < 3 || ResultsShown()) return;
    uint8_t* pur[2] = { nullptr, nullptr };
    for (int i = 0; i < 2; i++) if (!PlayerOut(i)) pur[i] = PursuitOf(mgr, i);
    if (!pur[0] && !pur[1]) return;
    DWORD now = GetTickCount();
    if (!g_nextBlockAt) { g_nextBlockAt = now + BlockIntervalMs(); return; }
    if ((int)(now - g_nextBlockAt) < 0) return;
    static int turn = 0;
    int forP; const char* why;
    if (pur[0] && pur[1]) {
        bool known; float gap = PlayersGap(&known);
        if (fabsf(gap) < 1000.f) { forP = !known ? turn : (gap >= 0 ? 0 : 1); why = known ? "ahead of the leader" : "players together (no route)"; }
        else { forP = turn; why = "players over 1 km apart: ahead of this player"; }
    } else { forP = pur[0] ? 0 : 1; why = "the only player racing"; }
    const char* kind = g_blockKind ? "spike strips" : "roadblock";
    const char* busy = nullptr;
    if (At<void*>(mgr, 0x68)) busy = "another roadblock being set up";
    else if (((void*(THISCALL*)(void*))(*(void***)pur[forP])[0x3C / 4])(pur[forP])) busy = "this player's last one still stands";
    else if (pur[0] && pur[1] && fabsf(PlayersGap(nullptr)) < 1000.f && RoadBlockCount(mgr) > 0) busy = "one still on the road";
    else if (!(pur[0] && pur[1]) && RoadBlockCount(mgr) > 0) busy = "one still on the road";
    if (busy || !QueueRoadBlock(mgr, pur[forP])) {
        static DWORD lastBusyLog = 0;
        if (now - lastBusyLog > 15000) { lastBusyLog = now; Log("Split cops: %s for P%d waits (%s)", kind, forP + 1, busy ? busy : "busy"); }
        g_nextBlockAt = now + 3000;
        return;
    }
    g_rbForce = g_blockKind; g_rbTries = 0; g_rbFor = forP;
    Log("Split cops: %s for P%d (%s)", kind, forP + 1, why);
    g_blockKind ^= 1; turn = 1 - forP;
    g_nextBlockAt = now + BlockIntervalMs();
}
static void HandRoadBlock(void* rb, uint8_t* from, uint8_t* to) {
    ((void(THISCALL*)(void*, void*))(*(void***)to)[0x80 / 4])(to, rb);              // IPursuit::AddRoadBlock
    ((void(THISCALL*)(void*, void*))(*(void***)rb)[0x18 / 4])(rb, to);              // IRoadBlock::SetPursuit
    if (from && At<void*>(from, 0x84) == rb) At<void*>(from, 0x84) = nullptr;
}
typedef void (THISCALL *UpdateRoadBlocksFn)(void* mgrBase);
static void FASTCALL Hook_UpdateRoadBlocks(uint8_t* base, void*) {
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat && g_copsStage >= 3) {
        uint8_t* mgr = base + 0x54;
        uint8_t* pur[2] = { nullptr, nullptr };
        for (int i = 0; i < 2; i++) if (!PlayerOut(i)) pur[i] = PursuitOf(mgr, i);
        void** head = At<void**>(base, 0x134); int k = 0;
        for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 32; node = (void**)node[0], k++) {
            void* rb = node[2];
            if (!rb || ((bool(THISCALL*)(void*))(*(void***)rb)[0x24 / 4])(rb)) continue;          // GetDodged
            const float* c = ((const float*(THISCALL*)(void*))(*(void***)rb)[0x38 / 4])(rb);     // centre
            const float* d = ((const float*(THISCALL*)(void*))(*(void***)rb)[0x3C / 4])(rb);     // direction
            uint8_t* own = (uint8_t*)((void*(THISCALL*)(void*))(*(void***)rb)[0x1C / 4])(rb);   // GetPursuit
            int p = own ? PursuitPlayer(own) : -1;
            if (own && p >= 0 && p < 2) {
                int o = 1 - p;
                if (!PlayerPastPoint(p, c, d, 0.f) || !pur[o] || PlayerPastPoint(o, c, d, 0.f)) continue;
                if (fabsf(PlayersGap(nullptr)) >= 1000.f) continue;                                // far behind: let it go
                if (((void*(THISCALL*)(void*))(*(void***)pur[o])[0x3C / 4])(pur[o])) continue;   // they have their own
                HandRoadBlock(rb, own, pur[o]);
                Log("Split cops: P%d drove past the roadblock, it stays for P%d", p + 1, o + 1);
            } else if (!own) {
                // no pursuit (its player is out): to whoever still has to get through it
                int o = -1;
                for (int i = 0; i < 2; i++) if (pur[i] && !PlayerPastPoint(i, c, d, 0.f) && !((void*(THISCALL*)(void*))(*(void***)pur[i])[0x3C / 4])(pur[i])) o = i;
                if (o >= 0) { HandRoadBlock(rb, nullptr, pur[o]); Log("Split cops: a roadblock without a pursuit now stands for P%d", o + 1); continue; }
                // everyone past it (or nobody racing): its cops go once nobody can see them
                void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), gone = 0;
                for (int i = 0; cops && i < nc && i < 40; i++) {
                    void* v = cops[i];
                    if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;
                    void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
                    if (!ai || ((void*(THISCALL*)(void*))(*(void***)ai)[0xA4 / 4])(ai) != rb) continue;
                    if (gone < 2 && CopOffscreen(v) > 2.f && CopDistToPlayers(v) > 150.f) { CopUnspawn(mgr, v); gone++; }
                }
                if (gone) Log("Split cops: %d cop car(s) of a left-over roadblock taken off the road out of sight", gone);
            }
        }
    }
    ((UpdateRoadBlocksFn)0x439EE0)(base);
}

// AICopManager::TrySpawnCop first calls UpdateCopPriorities(active + 1): over the cap of 8 cop cars it
// takes the least needed cop off the road on the spot, judged from player 1's camera only (so it can be
// right in front of player 2). In split screen: one out of sight goes; otherwise the cop farthest from
// both players drives off like the lower heat's cops do, and goes once nobody can see it (SplitCopShare).
static void FASTCALL Hook_UpdateCopPriorities(uint8_t* base, void*, int want) {
    if (!(cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat)) { ((void(THISCALL*)(void*, int))0x426D50)(base, want); return; }
    if (want <= At<int>(base, 0x8C)) return;
    uint8_t* mgr = base + 0x54;
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C);
    void* unseen = nullptr; float unseenD = -1.f; void* leave = nullptr; float leaveD = -1.f; int fleeing = 0;
    for (int i = 0; cops && i < nc && i < 40; i++) {
        void* v = cops[i];
        if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v) || CopIsChopper(v) || CopRoadBlock(v)) continue;   // roadblocks stay
        float d = CopDistToPlayers(v);
        bool fl = CopIsFleeing(v);
        if (fl) fleeing++;
        if (CopOffscreen(v) > 3.f && d > 100.f) { if (d > unseenD) { unseenD = d; unseen = v; } continue; }
        if (!fl && d > leaveD) { leaveD = d; leave = v; }
    }
    static DWORD lastLog = 0; static int nGone = 0, nLeft = 0;
    if (unseen) { CopUnspawn(mgr, unseen); nGone++; }
    else if (leave && fleeing < 4 && CopStartFlee(leave)) nLeft++;
    DWORD now = GetTickCount();
    if ((nGone || nLeft) && now - lastLog > 10000) { lastLog = now; Log("Split cops: over 8 cop cars: %d out of sight taken off the road, %d sent away (%d driving off)", nGone, nLeft, fleeing); nGone = nLeft = 0; }
}
// AICopManager::GetActiveCopVehicleFromOutOfView (a car for a new cop or a roadblock when no free one is
// left: an active cop out of view 3 s and 100 m from the view, taken off the road and reused). It took
// cops standing in roadblocks (the roadblock fell apart, or vanished before anyone got to it) and, for
// roadblocks, chasing cops. In split screen: never a roadblock's cop, at least 120 m from both players,
// and for a roadblock only a cop left 300 m+ behind everyone.
static void* OutOfViewCop(uint8_t* base, uint32_t cls, bool forBlock) {
    uint8_t* sm = base + 0x54;
    if (!((bool(THISCALL*)(void*, int))(*(void***)sm)[1])(sm, 0)) return nullptr;    // spawning allowed
    uint8_t* mgr = base + 0x54;
    void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C);
    void* best = nullptr; float bestD = forBlock ? 300.f : 120.f;
    for (int i = 0; cops && i < nc && i < 40; i++) {
        void* v = cops[i];
        if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;
        uint32_t* c = ((uint32_t*(THISCALL*)(void*))(*(void***)v)[0x44 / 4])(v);
        if (!c || *c != cls || CopRoadBlock(v) || !(CopOffscreen(v) > 3.f)) continue;
        float d = CopDistToPlayers(v);
        if (d > bestD) { bestD = d; best = v; }
    }
    if (best) CopUnspawn(mgr, best);
    return best;
}
typedef void* (THISCALL *OutOfViewFn)(void* base, uint32_t cls);
static bool SplitCopsOn() { return cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat; }
static void* FASTCALL Hook_OutOfViewForSpawn(uint8_t* base, void*, uint32_t cls) {
    if (!SplitCopsOn()) return ((OutOfViewFn)0x410E70)(base, cls);
    return OutOfViewCop(base, cls, false);
}
static void* FASTCALL Hook_OutOfViewForBlock(uint8_t* base, void*, uint32_t cls) {
    if (!SplitCopsOn()) return ((OutOfViewFn)0x410E70)(base, cls);
    return OutOfViewCop(base, cls, true);
}
// The cop manager's task (AICopManager::OnTask: pursuits, roadblocks, Rhinos, spawn requests, every 0.5 s)
// once stopped being run mid-race and never came back: no more roadblocks, Rhinos or the game's own cops.
// If its clock (+0x58 from the ICopMgr) stands still while the race runs, run it from here.
static void CopManagerWatchdog(uint8_t* mgr, float simT) {
    static float lastTimer = 0, lastMoved = -1, lastRun = 0; static bool stuck = false;
    float t = At<float>(mgr, 0x58);
    if (t != lastTimer || lastMoved < 0 || simT < lastMoved) { lastTimer = t; lastMoved = simT; if (stuck) { stuck = false; Log("Split cops: the cop manager runs again by itself"); } return; }
    if (simT - lastMoved < 2.f) return;
    uint8_t* base = mgr - 0x54;
    if (!stuck) { stuck = true; lastRun = simT; Log("Split cops: the cop manager stopped running (%.1f s), running it from the mod", simT - lastMoved); }
    if (simT - lastRun < 0.5f) return;
    float dt = simT - lastRun; if (dt > 1.f) dt = 0.5f;
    lastRun = simT;
    ((bool(THISCALL*)(void*, void*, float))0x43EF00)(base + 8, At<void*>(base, 0x80), dt);   // AICopManager::OnTask
    lastTimer = At<float>(mgr, 0x58);
}
static void SplitCopShare() {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr) return;
    int& maxCops = At<int>(mgr, 0x98 - 0x54);
    uint8_t* pur[2] = { nullptr, nullptr };
    for (int i = 0; i < 2; i++) if (!PlayerOut(i)) pur[i] = PursuitOf(mgr, i);
    // roadblock and Rhino cars count too, and so do cops left over from an ended pursuit
    int want = (pur[0] && pur[1]) ? 32 : (pur[0] || pur[1]) ? 24 : 8;
    RoadBlockTimeout(mgr);
    if (maxCops != want && (maxCops == 8 || maxCops == 16 || maxCops == 24 || maxCops == 32)) { Log("Split cops: cop car limit %d -> %d", maxCops, want); maxCops = want; }
    // a player just went out: their cops (now without a pursuit) join the other player's pursuit
    DWORD now = GetTickCount();
    static DWORD lastMove = 0;
    if (g_copTransferUntil && (int)(now - g_copTransferUntil) < 0 && now - lastMove > 500) {
        lastMove = now;
        uint8_t* to = pur[0] ? pur[0] : pur[1];
        void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), moved = 0, removedFar = 0;
        for (int i = 0; to && cops && i < nc && i < 32; i++) {
            uint8_t* v = (uint8_t*)cops[i];
            if (!v) continue;
            bool done = false;
            for (int j = 0; j < g_copMovedN; j++) if (g_copMoved[j] == v) done = true;
            if (done) continue;
            void* ai = ((void*(THISCALL*)(void*))(*(void***)v)[0xAC / 4])(v);
            if (!ai) continue;
            void* cur = ((void*(THISCALL*)(void*))(*(void***)ai)[0xA0 / 4])(ai);
            if (cur) continue;                                                       // in a pursuit already
            if (CopRoadBlock(v)) continue;                                           // standing in a roadblock: stays there
            if (!((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;      // not on the road
            // far from the player still racing (they'd only come from behind): they drive off with their
            // sirens off instead (as when the heat level goes up), so new cops are sent ahead of that player
            const float* pp = SimPosition(PlayerSim(pur[0] ? 0 : 1));
            if (pp && CopDistTo(v, pp) > 250.f) {
                if (CopStartFlee(v)) removedFar++;
                if (g_copMovedN < 32) g_copMoved[g_copMovedN++] = v;
                continue;
            }
            if (((bool(THISCALL*)(void*, void*))(*(void***)to)[0x78 / 4])(to, v)) { moved++; if (g_copMovedN < 32) g_copMoved[g_copMovedN++] = v; }  // IPursuit::AddVehicle
        }
        if (moved || removedFar) Log("Split cops: %d cop car(s) nearby joined P%d's pursuit, %d far away drive off", moved, pur[0] ? 1 : 2, removedFar);
    } else if (g_copTransferUntil && (int)(now - g_copTransferUntil) >= 0) g_copTransferUntil = 0;
    // reinforcements: the heat level's cop request logic sends few cars in a race, so whenever a
    // pursuit has fewer cops close to its player than the heat level wants, spawn one more for it
    // (AICopManager::SpawnPursuitCar: a car ahead of the target, as when a pursuit starts)
    float st = ((float(*)())0x6E8DE0)();                                  // Sim::GetTime (stops when paused)
    static float lastSt = 0; static DWORD reinforceAt[2] = { 0, 0 }, lastLog = 0; static int spawned[2] = { 0, 0 };
    bool running = st != lastSt; lastSt = st;
    if (running && !ResultsShown() && (pur[0] || pur[1])) CopManagerWatchdog(mgr, st);
    int active = At<int>(mgr, 0x94 - 0x54);
    int wantClose = 2 + g_copsHeat;                                       // heat 5: 7 cops on each player
    for (int i = 0; i < 2 && running && !ResultsShown(); i++) {
        if (!pur[i] || (int)(now - reinforceAt[i]) < 0) continue;
        int total = PursuitCopsNear(mgr, pur[i], SimPosition(PlayerSim(i)), 300.f);   // cops within 300 m
        int close = ((int(THISCALL*)(void*))(*(void***)pur[i])[0x14 / 4])(pur[i]);
        if (close >= wantClose || total >= wantClose + 4 || active >= maxCops) { reinforceAt[i] = now + 1000; continue; }
        if (BlockReserve(mgr)) { reinforceAt[i] = now + 1000; continue; }          // keep cars free for the roadblock
        if (ChaseFull(mgr)) { reinforceAt[i] = now + 1000; continue; }
        g_spawnFor = i;
        bool ok = ((bool(THISCALL*)(void*, void*))0x42EA90)(mgr - 0x54, pur[i]);   // SpawnPursuitCar
        g_spawnFor = -1;
        if (ok) { spawned[i]++; active++; }
        reinforceAt[i] = now + (ok ? 2500 : 1500);
    }
    if (cfg.debug && now - lastLog > 10000 && (spawned[0] || spawned[1])) {
        lastLog = now;
        int c[2] = { -1, -1 }, e[2] = { -1, -1 };
        for (int i = 0; i < 2; i++) if (pur[i]) { c[i] = ((int(THISCALL*)(void*))(*(void***)pur[i])[0xC / 4])(pur[i]); e[i] = ((int(THISCALL*)(void*))(*(void***)pur[i])[0x14 / 4])(pur[i]); }
        Log("Split cops: reinforcements P1 %d P2 %d; cops P1 %d (%d close) P2 %d (%d close), active %d of %d", spawned[0], spawned[1], c[0], e[0], c[1], e[1], active, maxCops);
        spawned[0] = spawned[1] = 0;
    }
    // the game can only have so many vehicles at once: cop cars stranded far behind every player still
    // racing (300 m+, out of sight for 3 s) are taken off the road, two every 2 s at most, so new cops
    // and Rhinos can spawn where the players are (as GetActiveCopVehicleFromOutOfView does)
    static DWORD lastRecycle = 0; static int recycled = 0; static DWORD recycleLog = 0;
    if (running && !ResultsShown() && now - lastRecycle > 2000) {
        lastRecycle = now;
        const float* pp[2] = { nullptr, nullptr };
        for (int i = 0; i < 2; i++) if (pur[i]) pp[i] = SimPosition(PlayerSim(i));
        void** cops = At<void**>(mgr, 0x74); int nc = At<int>(mgr, 0x7C), done = 0;
        for (int i = 0; (pp[0] || pp[1]) && cops && i < nc && i < 40 && done < 3; i++) {
            uint8_t* v = (uint8_t*)cops[i];
            if (!v || !((bool(THISCALL*)(void*))(*(void***)v)[0x88 / 4])(v)) continue;          // active
            if (CopIsChopper(v)) continue;                                                       // helicopter
            if (CopRoadBlock(v)) continue;                                                       // roadblocks go by their own rules
            // cops driving off go once out of sight for 2 s and 120 m from everyone; the rest 3 s / 300 m
            // from every player still racing
            bool fleeing = CopIsFleeing(v);
            if (!(CopOffscreen(v) > (fleeing ? 2.f : 3.f))) continue;
            void* sim = ((void*(THISCALL*)(void*))(*(void***)v)[1])(v);
            const float* cp = SimPosition(sim);
            if (!cp) continue;
            bool farAway = true;
            float farM = BlockReserve(mgr) ? 150.f : 300.f;                              // a roadblock needs cars: sooner
            if (fleeing) farAway = CopDistToPlayers(v) > 120.f;
            else for (int k = 0; k < 2; k++) if (pp[k]) {
                float dx = cp[0] - pp[k][0], dy = cp[1] - pp[k][1], dz = cp[2] - pp[k][2];
                if (dx * dx + dy * dy + dz * dz < farM * farM) farAway = false;
            }
            if (farAway && CopDistToPlayers(v) < 120.f) farAway = false;                   // a busted player still sees the road
            if (!farAway) continue;
            CopUnspawn(mgr, v);
            done++; recycled++;
        }
        if (recycled && now - recycleLog > 10000) { recycleLog = now; Log("Split cops: %d cop car(s) out of sight taken off the road", recycled); recycled = 0; }
    }
    if (!pur[0] || !pur[1]) return;
    void** head = At<void**>(mgr, 0xD4);
    if (!head) return;
    void** first = (void**)head[0];
    if (!first || first == head) return;
    int c0 = ((int(THISCALL*)(void*))(*(void***)pur[0])[0xC / 4])(pur[0]);
    int c1 = ((int(THISCALL*)(void*))(*(void***)pur[1])[0xC / 4])(pur[1]);
    uint8_t* fewer = c1 < c0 ? pur[1] : c0 < c1 ? pur[0] : nullptr;
    if (!fewer || first[2] == fewer) return;
    int k = 0;
    for (void** node = (void**)first[0]; node && node != head && k < 16; node = (void**)node[0], k++)
        if (node[2] == fewer) { void* tmp = first[2]; first[2] = node[2]; node[2] = tmp; break; }
}

// once the race is on screen: wait for the countdown, then send the cops after both players
static void SplitCopsTick() {
    if (!g_copsStage || !g_copsHeat) return;
    if (*G::pGameFlowState != 6 || *G::pSimUserMode != 1) return;
    DWORD now = GetTickCount();
    if (g_copsStage == 1) { g_copsStage = 2; g_copsTick = now; g_copsLogTick = now; return; }
    if (g_copsStage == 2 && now - g_copsTick > 8000) {
        // races lock cop spawning out for 60 s at the start (AICopManager::LockoutCops(true));
        // lift it so the pursuits get cars straight away
        void* mgr = *(void**)0x92C67C;
        if (mgr) ((void(THISCALL*)(void*, bool))(*(void***)mgr)[0x2C / 4])(mgr, false);
        PursuePlayer(0, g_copsHeat);
        PursuePlayer(1, g_copsHeat);
        g_copsStage = 3; LogPursuits("after start"); LogCopHudInfo();
    }
    if (g_copsStage >= 3) { SplitBlockTick(); SplitBackupTick(); SplitCopShare(); }
    if (g_copsStage >= 2 && now - g_copsLogTick > 10000) { g_copsLogTick = now; LogPursuits("status"); }
}

static void FASTCALL Hook_SplitSetNumOpponents(void* race, void*, int n) {
    if (cfg.enabled) n = g_splitAI + 1;
    if (cfg.enabled && FourRace()) n = 3;           // 4 players: players 2-4 are the opponents, no AI racers
    Log("Split race: NumOpponents %d (%d AI)", n, n - 1);
    ((SetIntFn)0x5DCE60)(race, n);
    SetupSplitCops(race);
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
    Log("Knocked out: racer %d (%s)", idx, human ? (idx == 0 ? "P1" : idx == 1 ? "P2" : idx == 2 ? "P3" : "P4") : "AI");
    if (cfg.enabled && *G::pSimUserMode == 1 && human && FourRace() && (idx == 2 || idx == 3)) {   // players 3 and 4
        ((void(THISCALL*)(void*))0x5E9240)(racer);              // GRacerInfo::ForceStop
        int np; void** pl = LocalPlayers(&np);
        if (pl && idx < np && pl[idx]) PostRaceOverMessage(pl[idx]);
        return;
    }
    if (!cfg.enabled || *G::pSimUserMode != 1 || !human || idx < 0 || idx > 1) return;
    ((void(THISCALL*)(void*))0x5E9240)(racer);                  // GRacerInfo::ForceStop
    g_knockedOut[idx] = GetTickCount() | 1;
    // the game's own race over message on that player's HUD ("KNOCKED OUT"), as when a
    // player finishes: EShowRaceOverMessage(IPlayer*)
    int np; void** pl = LocalPlayers(&np);
    if (pl && idx < np && pl[idx]) PostRaceOverMessage(pl[idx]);
}

// Race standings (GRaceStatus::CalculateRankings compares racers with sub_5E31D0(a, b): true = a is
// behind b). Knocked out racers count as finished at the time they were knocked out; when the winner's
// last lap knocks out everyone left (in knockout, nobody else finished the lap), they all have the same
// time and the order was random, the winner too. A racer who wasn't knocked out is ahead of those who
// were, and knocked out racers with the same time are ordered by how far they got.
static bool FASTCALL Hook_RacerBehind(uint8_t* a, void*, uint8_t* b) {
    bool r = ((bool(THISCALL*)(void*, void*))0x5E31D0)(a, b);
    if (!cfg.enabled || *G::pSimUserMode != 1 || !a || !b) return r;
    if (At<uint8_t>(a, 0x21) && At<uint8_t>(b, 0x21)) {            // both finished
        bool ka = At<uint8_t>(a, 0x1C) != 0, kb = At<uint8_t>(b, 0x1C) != 0;
        if (ka != kb) return ka;
        if (ka && kb && !At<uint8_t>(a, 0x154) && !At<uint8_t>(b, 0x154) && fabsf(At<float>(a, 0x150) - At<float>(b, 0x150)) < 0.05f) {
            if (At<int>(a, 0x28) != At<int>(b, 0x28)) return At<int>(a, 0x28) < At<int>(b, 0x28);
            return At<float>(a, 0x18) < At<float>(b, 0x18);
        }
    }
    return r;
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
static bool g_resultsShown = false;          // results screen already opened in this race
static bool ResultsShown() { return g_resultsShown; }
static bool PlayerOut(int i) { return g_busted[i] || g_bustPursuitEnded[i] || g_raceOverShown[i] || g_knockedOut[i]; }
static void BuildGridRows(int rows);
static void LogRacerPositions(const char* when);
static int g_startLogStage = 0; static DWORD g_startLogTick = 0; static int g_flareStage = 0;
// per-race cop / bust / results state: cleared at race start and on restart
static void ResetSplitRaceState() {
    if (g_copsHeat) g_copsStage = 1;
    g_busted[0] = g_busted[1] = false; g_bustTime[0] = g_bustTime[1] = 0; g_bustPursuitEnded[0] = g_bustPursuitEnded[1] = false; g_pursuitRequestsDenied = 0;
    g_nextBackup[0] = g_nextBackup[1] = 0; g_nextBackupAll = 0; g_backupTurn = 0; g_backupStepAll = 0; g_heldRbN = 0; g_heldCopAIN = 0; g_copTransferUntil = 0; g_heavyQueuedAt[0] = g_heavyQueuedAt[1] = 0; g_backupStep[0] = g_backupStep[1] = 0; g_rbForce = -1; g_nextBlockAt = 0; g_blockKind = 0;
    g_bustPending[0] = g_bustPending[1] = 0; g_knockedOut[0] = g_knockedOut[1] = 0;
    g_raceOverShown[0] = g_raceOverShown[1] = false; g_postRaceAt = 0; g_resultsShown = false;
    g_despawned[0] = g_despawned[1] = false;
}
static void SetupGrid(int humans, int ai) {
    g_gridHumans = humans; g_gridN = humans + ai; g_gridAIPlaced = 0; g_p1StartValid = false; g_finalValid = false; g_startLogStage = 1; g_flareStage = 1; ResetSplitRaceState();
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
    } else { g_startLogStage = 1; SetSplitFreeRoamCops(false); }   // single player: game's own race pursuit tables
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
    PatchCall(0x60E232, (void*)Hook_ShowPostRaceScreen, 0x60AC20);   // FE_ShowLosingPostRaceScreen
    PatchCall(0x60E242, (void*)Hook_ShowPostRaceScreen, 0x60AC20);   // FE_ShowWinningPostRaceScreen
}

// ---- busted in split screen
// AIPursuit::OnTask busts a player by sending MPerpBusted (BUSTED HUD) and, after kBustedHUDTime,
// a second MPerpBusted that ends the game. AI racers instead get GRacerInfo::Busted + ForceStop
// and the race goes on. In split screen with cops, a busted player takes the AI racer path
// (the "is player pursuit" test at both places is answered by SplitBustOverride), then each
// player's HUD gets the game's message banner: BUSTED / PLAYER n BUSTED.
static int PlayerIndexOfSimable(void* sim) {
    int n; void** pl = LocalPlayers(&n);
    for (int i = 0; i < 2 && pl && i < n; i++)
        if (pl[i] && ((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]) == sim) return i;
    return -1;
}
// is this pursuit's target a player we handle (split screen with cops)? -1 = no
static int SplitBustTarget(uint8_t* pursuit) {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || !pursuit) return -1;
    uint8_t* tgt = At<uint8_t*>(pursuit, 0xB4);
    if (!tgt) return -1;
    return PlayerIndexOfSimable(At<void*>(tgt, 0x1C));
}
// the moment a player is busted: racer busted + car stopped, pursuit left in its busted state
// (OnTask then only runs the busted-timer branch, which SplitBustOverride keeps from ending the game)
static int __cdecl SplitBustNow(uint8_t* pursuit) {
    int idx = SplitBustTarget(pursuit);
    if (idx < 0) return 0;
    At<uint8_t>(pursuit, 0x128) = 1;            // busted
    At<int>(pursuit, 0x258) = 3;
    uint8_t* rs = *(uint8_t**)0x91E000;
    void* sim = At<void*>(At<uint8_t*>(pursuit, 0xB4), 0x1C);
    void* info = rs ? ((void*(THISCALL*)(void*, void*))0x5E8B20)(rs, sim) : nullptr;   // GetRacerInfo
    if (info) {
        ((void(THISCALL*)(void*))0x600370)(info);                                        // GRacerInfo::Busted
        ((void(THISCALL*)(void*))0x5E9240)(info);                                        // ForceStop
    }
    if (!g_busted[idx]) { g_busted[idx] = true; InterlockedExchange(&g_bustPending[idx], 1); }
    Log("Split cops: P%d busted (racer %p), race goes on", idx + 1, info);
    return 1;
}
// later frames: answer "not a player pursuit" so the busted timer never sends the game-ending message
static int __cdecl SplitBustOverride(uint8_t* pursuit) { return SplitBustTarget(pursuit) >= 0 ? 1 : 0; }
static void* HudGenericMessage(void* player) {
    if (!player) return nullptr;
    uint8_t* hud = ((uint8_t*(THISCALL*)(void*))(*(void***)player)[0x20 / 4])(player);
    if (!hud || !At<void*>(hud, 4)) return nullptr;
    return ((void*(THISCALL*)(void*, void*))0x5D59F0)(At<void*>(hud, 4), (void*)0x5650B0);   // Find(IGenericMessage)
}
static void ShowHudMessage(void* player, const char* text) {
    void* msg = HudGenericMessage(player);
    if (!msg) { if (player) Log("HUD message: no IGenericMessage"); return; }
    ((void(THISCALL*)(void*, const char*, int, uint32_t, int, int, int))(*(void***)msg)[1])(msg, text, 0, 0x8AB83EDB, 0, 0, 5);
}
// keep BUSTED on a busted player's half until the results: the banner clears itself when its
// animation ends (IsGenericMessageShowing false), so put it back up then
static void KeepBustedMessages() {
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    if (g_resultsShown || *G::pGameFlowState != 6) return;
    int n; void** pl = LocalPlayers(&n);
    if (!pl || n < 2) return;
    for (int i = 0; i < 2; i++) {
        if (!g_busted[i] || g_bustPending[i]) continue;
        void* msg = HudGenericMessage(pl[i]);
        if (msg && !((bool(THISCALL*)(void*))(*(void***)msg)[3])(msg)) ShowHudMessage(pl[i], "BUSTED");
    }
}
// A few seconds after a bust, end that player's pursuit (state 4 makes AIPursuit::ShouldEnd true);
// AICopManager::UpdatePursuits then releases its cops and the other player's pursuit pulls them in.
// Its career bookkeeping (CommitPursuitDetails) is skipped for split screen races with cops.
// A cop with no pursuit asks for one on its current target (AIVehiclePursuit::PursuitRequest) and
// AICopManager::UpdatePursuits starts it. A player who is out of the race (busted, finished, knocked
// out) is no target for a new pursuit: before, a cop that still had them as its target started a new
// pursuit every frame, which SplitEndBustedPursuits ended again - and every start flashed that half.

static bool HeldRoadBlockCop(void* ai);
static void* FASTCALL Hook_PursuitRequest(void* ai, void*) {
    if (cfg.enabled && *G::pSimUserMode == 1 && HeldRoadBlockCop(ai)) {                    // stays in its roadblock
        static DWORD t = 0; if (GetTickCount() - t > 10000) { t = GetTickCount(); Log("Split cops: a roadblock cop wanted to leave its roadblock, kept there"); }
        return nullptr;
    }
    void* tgt = ((void*(THISCALL*)(void*))0x409890)(ai);
    if (!tgt || !cfg.enabled || *G::pSimUserMode != 1) return tgt;
    int i = PlayerIndexOfSimable(At<void*>((uint8_t*)tgt, 0x1C));
    if (i >= 0 && i < 2 && (g_busted[i] || g_raceOverShown[i] || g_knockedOut[i]) && g_bustPursuitEnded[i]) {
        if (g_pursuitRequestsDenied++ == 0) Log("Split cops: new pursuits on P%d (out of the race) are not started", i + 1);
        return nullptr;
    }
    return tgt;
}
static void SplitEndBustedPursuits() {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr) return;
    DWORD now = GetTickCount();
    for (int i = 0; i < 2; i++) {
        // busted: 3 s after the bust; finished or knocked out: 1 s after crossing the line
        bool done = !g_busted[i] && (g_raceOverShown[i] || g_knockedOut[i]);
        if (!g_busted[i] && !done) continue;
        if (!g_bustTime[i]) { g_bustTime[i] = now; continue; }
        if (now - g_bustTime[i] < (DWORD)(done ? 1000 : 3000)) continue;
        // from then on, every new pursuit on this player ends too (a finished car keeps
        // driving under AI control and the cops would pick it up again)
        void** head = At<void**>(mgr, 0xD4);
        int k = 0;
        for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 16; node = (void**)node[0], k++) {
            uint8_t* pur = (uint8_t*)node[2];
            if (!pur || At<int>(pur, 0x218) == 4) continue;
            uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pur)[2])(pur);
            if (!tgt || PlayerIndexOfSimable(At<void*>(tgt, 0x1C)) != i) continue;
            At<int>(pur, 0x218) = 4;
            if (!g_bustPursuitEnded[i]) { Log("Split cops: P%d's pursuit ended (%s), its cops go after the other player", i + 1, g_busted[i] ? "busted" : "finished"); g_copTransferUntil = (now + 8000) | 1; g_copMovedN = 0; }
            else Log("Split cops: new pursuit on P%d (out of the race) ended", i + 1);
        }
        g_bustPursuitEnded[i] = true;
    }
}
// results screen: nobody is racing any more - end every pursuit and keep new cops away
static void SplitEndAllPursuits() {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat) return;
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr) return;
    ((void(THISCALL*)(void*, bool))(*(void***)mgr)[0x2C / 4])(mgr, true);       // LockoutCops
    void** head = At<void**>(mgr, 0xD4);
    int k = 0, n = 0;
    for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 16; node = (void**)node[0], k++) {
        uint8_t* pur = (uint8_t*)node[2];
        if (pur && At<int>(pur, 0x218) != 4) { At<int>(pur, 0x218) = 4; n++; }
    }
    g_copsStage = 0;
    Log("Split cops: results - %d pursuit(s) ended, cops locked out", n);
}
// a busted player's car leaves the road (IVehicle::Deactivate) once the busted screen has faded in; restart brings it back
static void* PlayerVehicle(int i) {
    int n; void** pl = LocalPlayers(&n);
    if (!pl || i >= n || !pl[i]) return nullptr;
    return VehicleOfSimable(((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]));
}
static void SplitDespawnTick() {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || *G::pGameFlowState != 6) return;
    DWORD now = GetTickCount();
    for (int i = 0; i < 2; i++) {
        if (!g_busted[i] || g_despawned[i] || !g_bustTime[i] || now - g_bustTime[i] < 3800) continue;   // busted screen: 3 s + 0.6 s fade
        g_despawned[i] = true;
        void* v = PlayerVehicle(i);
        if (!v) continue;
        ((void(THISCALL*)(void*))(*(void***)v)[0x84 / 4])(v);
        Log("Split cops: P%d's car taken off the road", i + 1);
    }
}
static void SplitRespawnBusted() {
    for (int i = 0; i < 2; i++) {
        if (!g_despawned[i]) continue;
        g_despawned[i] = false;
        void* v = PlayerVehicle(i);
        if (v) { ((void(THISCALL*)(void*))(*(void***)v)[0x80 / 4])(v); Log("Split cops: P%d's car back for the restart", i + 1); }
    }
}
// an evaded pursuit goes through AICopManager::PursuitIsEvaded (ICopMgr slot 0x10), which adjusts the
// player's career stable heat (needs a profile per player: crashes for player 2). Skip it in split screen.
typedef void (THISCALL *EvadedFn)(void* mgr, void* pursuit);
static void FASTCALL Hook_PursuitIsEvaded(void* mgr, void*, void* pursuit) {
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat) { Log("Split cops: pursuit evaded (career heat skipped)"); return; }
    ((EvadedFn)0x411070)(mgr, pursuit);
}
// Roadblocks are placed 250 m ahead of the perp along the road network (CreateRoadBlock: WRoadNav
// SetPathType / InitAtPoint / IncNavPosition), which can turn off onto a side road. In split screen
// races with cops, keep that WRoadNav on the race route (WRoadNav::SetRaceFilter(true)).
typedef void (THISCALL *SetPathTypeFn)(void* nav, int type);
static void FASTCALL Hook_RoadBlockNavPathType(void* nav, void*, int type) {
    ((SetPathTypeFn)0x7658A0)(nav, type);
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat) ((void(THISCALL*)(void*, bool))0x404570)(nav, true);
}
typedef void (THISCALL *CommitPursuitFn)(void* mgr, void* pursuit);
static void FASTCALL Hook_CommitPursuitDetails(void* mgr, void*, void* pursuit) {
    if (cfg.enabled && *G::pSimUserMode == 1 && g_copsHeat) { Log("Split cops: pursuit career details skipped"); return; }
    ((CommitPursuitFn)0x411240)(mgr, pursuit);
}
// busted player's half, 5 s after the bust: the view dims and a HUD style panel says BUSTED /
// OUT OF THE RACE (white condensed text on a dark band edged with police red and blue).
// The text is rendered once with GDI into a texture (alpha from coverage, soft shadow).
static const DWORD BUST_SCREEN_MS = 3000;
static IDirect3DTexture9* g_bustTex = nullptr;
static const int BUST_TEX_W = 1024, BUST_TEX_H = 256;
static HFONT BustFont(int px, int weight, const char* const* faces) {
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT f = nullptr;
    for (int i = 0; faces[i] && !f; i++) {
        f = CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        ANTIALIASED_QUALITY, DEFAULT_PITCH, faces[i]);
        char got[64] = {}; HGDIOBJ o = SelectObject(dc, f); GetTextFaceA(dc, sizeof got, got); SelectObject(dc, o);
        if (_stricmp(got, faces[i]) && faces[i + 1]) { DeleteObject(f); f = nullptr; }   // not installed: next one
    }
    DeleteDC(dc);
    return f;
}
static bool BuildBustTex() {
    if (g_bustTex) return true;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = BUST_TEX_W;
    bi.bmiHeader.biHeight = -BUST_TEX_H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* bitsT = nullptr; void* bitsS = nullptr;
    HBITMAP bmT = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bitsT, nullptr, 0);   // white text: title
    HBITMAP bmS = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bitsS, nullptr, 0);   // grey text: subtitle
    if (!bmT || !bmS) { DeleteDC(dc); return false; }
    static const char* const big[] = { "Bahnschrift SemiBold Condensed", "Bahnschrift Condensed", "Arial Narrow", "Arial", nullptr };
    static const char* const small[] = { "Bahnschrift SemiLight Condensed", "Bahnschrift Condensed", "Arial Narrow", "Arial", nullptr };
    HFONT fT = BustFont(150, FW_BOLD, big), fS = BustFont(46, FW_NORMAL, small);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
    HGDIOBJ oldBm = SelectObject(dc, bmT); HGDIOBJ oldF = SelectObject(dc, fT);
    SetTextCharacterExtra(dc, 22);
    RECT rT = { 0, 0, BUST_TEX_W, 172 }; DrawTextA(dc, "BUSTED", -1, &rT, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    SelectObject(dc, bmS); SelectObject(dc, fS);
    SetTextCharacterExtra(dc, 12);
    RECT rS = { 0, 176, BUST_TEX_W, 240 }; DrawTextA(dc, "OUT OF THE RACE", -1, &rS, DT_CENTER | DT_TOP | DT_SINGLELINE);
    GdiFlush();
    SelectObject(dc, oldF); SelectObject(dc, oldBm); DeleteObject(fT); DeleteObject(fS);
    IDirect3DDevice9* d = Dev();
    bool ok = d && SUCCEEDED(d->CreateTexture(BUST_TEX_W, BUST_TEX_H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_bustTex, nullptr));
    if (ok) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(g_bustTex->LockRect(0, &lr, nullptr, 0))) {
            const uint32_t* t = (const uint32_t*)bitsT; const uint32_t* sb = (const uint32_t*)bitsS;
            auto cov = [&](const uint32_t* b, int x, int y) -> uint32_t {
                if (x < 0 || y < 0 || x >= BUST_TEX_W || y >= BUST_TEX_H) return 0; return b[y * BUST_TEX_W + x] & 0xFF; };
            for (int y = 0; y < BUST_TEX_H; y++) {
                uint32_t* row = (uint32_t*)((uint8_t*)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < BUST_TEX_W; x++) {
                    uint32_t ft = cov(t, x, y), fs = cov(sb, x, y);
                    // soft shadow 4 px down-right
                    uint32_t sh = (cov(t, x - 4, y - 4) + cov(t, x - 3, y - 5) + cov(t, x - 5, y - 3)) / 3;
                    uint32_t shs = cov(sb, x - 2, y - 2);
                    uint32_t shadow = (sh > shs ? sh : shs) * 150 / 255;
                    uint32_t fill = ft > fs ? ft : fs;
                    uint32_t lum = ft >= fs ? 255 : 196;                 // subtitle a little grey
                    uint32_t a = fill + shadow * (255 - fill) / 255;
                    uint32_t c = a ? lum * fill / a : 0;                   // shadow is black
                    row[x] = (a << 24) | (c << 16) | (c << 8) | c;
                }
            }
            g_bustTex->UnlockRect(0);
        }
    }
    DeleteObject(bmT); DeleteObject(bmS); DeleteDC(dc);
    return ok;
}
static void DrawBustedLabels() {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || g_resultsShown || *G::pGameFlowState != 6) return;
    if (!g_busted[0] && !g_busted[1]) return;
    if (!BuildBustTex()) return;
    UIBegin();
    IDirect3DDevice9* d = Dev();
    DWORD now = GetTickCount();
    for (int i = 0; i < 2; i++) {
        if (!g_busted[i] || !g_bustTime[i] || now - g_bustTime[i] < BUST_SCREEN_MS) continue;   // 3 s after the bust
        float k = (now - g_bustTime[i] - BUST_SCREEN_MS) / 600.f; if (k > 1) k = 1;             // fade in
        float W = g_uiW, H = g_uiH, x0, y0, w0, h0;
        if (cfg.layout == 1) { x0 = W * 0.5f * i; y0 = 0; w0 = W * 0.5f; h0 = H; }
        else { x0 = 0; y0 = H * 0.5f * i; w0 = W; h0 = H * 0.5f; }
        auto A = [&](float a) -> DWORD { return (DWORD)(a * k * 255.f) << 24; };
        UIRect(x0, y0, w0, h0, A(0.45f));                                             // dim the view
        float bh = h0 * 0.30f, by = y0 + h0 * 0.42f - bh / 2, line = (float)(int)(3 * g_uiS + 0.5f);
        if (line < 2) line = 2;
        UIRect(x0, by, w0, bh, A(0.62f));                                             // band
        // police light edges: red | blue, swapping slowly
        bool sw = ((now / 700) & 1) != 0;
        DWORD red = A(0.95f) | 0xE01818, blue = A(0.95f) | 0x1A4CFF;
        UIRect(x0, by - line, w0 / 2, line, sw ? blue : red);       UIRect(x0 + w0 / 2, by - line, w0 / 2, line, sw ? red : blue);
        UIRect(x0, by + bh, w0 / 2, line, sw ? red : blue);         UIRect(x0 + w0 / 2, by + bh, w0 / 2, line, sw ? blue : red);
        // text
        float th = bh * 0.92f, tw = th * BUST_TEX_W / BUST_TEX_H;
        if (tw > w0 * 0.95f) { tw = w0 * 0.95f; th = tw * BUST_TEX_H / BUST_TEX_W; }
        float x = x0 + (w0 - tw) / 2 - .5f, y = by + (bh - th) / 2 - .5f;
        DWORD c = A(1.f) | 0xFFFFFF;
        d->SetTexture(0, g_bustTex);
        UIV q[4] = { {x, y, 0, 1, c, 0, 0}, {x + tw, y, 0, 1, c, 1, 0},
                     {x, y + th, 0, 1, c, 0, 1}, {x + tw, y + th, 0, 1, c, 1, 1} };
        d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(UIV));
        d->SetTexture(0, nullptr);
    }
    UIEnd();
}

// busted meter above each player's speedometer while that player is in a pursuit: fills red
// as the cops box the player in (IPursuit::TimeUntilBusted, 1 = busted), like the single
// player pursuit bar. Drawn under the FE, so the pause menu covers it.
static float g_bustMeter[2] = { 0, 0 };
static uint8_t* PursuitOfPlayer(int i) {
    uint8_t* mgr = *(uint8_t**)0x92C67C;
    if (!mgr) return nullptr;
    void** head = At<void**>(mgr, 0xD4);
    int k = 0;
    for (void** node = head ? (void**)head[0] : nullptr; node && node != head && k < 16; node = (void**)node[0], k++) {
        uint8_t* pur = (uint8_t*)node[2];
        if (!pur || At<int>(pur, 0x218) == 4) continue;
        uint8_t* tgt = ((uint8_t*(THISCALL*)(void*))(*(void***)pur)[2])(pur);
        if (tgt && PlayerIndexOfSimable(At<void*>(tgt, 0x1C)) == i) return pur;
    }
    return nullptr;
}
static void DrawBustedMeters() {
    if (!cfg.enabled || *G::pSimUserMode != 1 || !g_copsHeat || g_copsStage < 3 || g_resultsShown || *G::pGameFlowState != 6) return;
    if (!BuildFont()) return;
    bool any = false;
    uint8_t* pur[2] = {};
    for (int i = 0; i < 2; i++) {
        if (g_busted[i] || g_raceOverShown[i] || g_knockedOut[i]) { g_bustMeter[i] = 0; continue; }
        pur[i] = PursuitOfPlayer(i);
        if (pur[i]) any = true; else g_bustMeter[i] = 0;
    }
    if (!any) return;
    UIBegin();
    DWORD now = GetTickCount();
    for (int i = 0; i < 2; i++) {
        uint8_t* p = pur[i];
        if (!p) continue;
        float v = ((float(THISCALL*)(void*))(*(void***)p)[0x60 / 4])(p);
        if (((bool(THISCALL*)(void*))(*(void***)p)[0x74 / 4])(p)) v = 1;
        if (!(v > 0)) v = 0; if (v > 1) v = 1;
        g_bustMeter[i] += (v - g_bustMeter[i]) * 0.25f;                                 // smooth
        float W = g_uiW, H = g_uiH, x0, y0, w0, h0;
        if (cfg.layout == 1) { x0 = W * 0.5f * i; y0 = 0; w0 = W * 0.5f; h0 = H; }
        else { x0 = 0; y0 = H * 0.5f * i; w0 = W; h0 = H * 0.5f; }
        float u = (cfg.layout == 1 ? w0 * 0.5625f : h0) / 360.f;                       // 1.0 = a 360 px tall half
        float right = x0 + w0 - 40 * u, pw = 222 * u, ph = 26 * u, px = right - pw, py = y0 + h0 - 118 * u;
        UIRect(px, py, pw, ph, 0x99000000);                                           // panel
        float ts = 17 * u, lx = px + 8 * u;
        bool hot = g_bustMeter[i] > 0.75f && ((now / 120) & 1);
        UIText(lx, py + (ph - ts * 1.25f) / 2, ts, hot ? 0xFFFF3030 : 0xFFE8E8E8, "BUSTED");
        float bx = lx + UITextWidth("BUSTED", ts) + 8 * u, bw = right - 6 * u - bx, by = py + 7 * u, bh = ph - 14 * u;
        UIRect(bx, by, bw, bh, 0x60FFFFFF);                                           // track
        if (g_bustMeter[i] > 0.005f) UIRect(bx, by, bw * g_bustMeter[i], bh, 0xFFE01818);
    }
    UIEnd();
}

static void SplitBustTick() {
    SplitEndBustedPursuits();
    for (int i = 0; i < 2; i++) {
        if (!InterlockedExchange(&g_bustPending[i], 0)) continue;
        int n; void** pl = LocalPlayers(&n);
        if (!pl || n < 2) continue;
        static char other[32];
        snprintf(other, sizeof other, "PLAYER %d\nBUSTED", i + 1);
        ShowHudMessage(pl[i], "BUSTED");
        if (!g_busted[1 - i]) ShowHudMessage(pl[1 - i], other);
    }
}
// both players out (busted, finished or knocked out) with at least one bust: nobody is left
// to finish, so end the race like a drag race with a blown engine: the results a few seconds later
static bool g_bustResultsQueued = false;
static void SplitBustEndCheck() {
    if (!g_busted[0] && !g_busted[1]) { g_bustResultsQueued = false; return; }
    if (g_bustResultsQueued || g_postRaceAt || g_resultsShown || *G::pGameFlowState != 6) return;
    for (int i = 0; i < 2; i++) if (!g_busted[i] && !g_raceOverShown[i] && !g_knockedOut[i]) return;
    g_bustResultsQueued = true;
    g_postRaceAt = GetTickCount() + (cfg.resultsDelayMs > 0 ? cfg.resultsDelayMs : 3000);
    g_postRaceArg = 0;
    Log("Split cops: both players out (busted %d/%d) - results screen in %d ms", g_busted[0], g_busted[1], (int)(g_postRaceAt - GetTickCount()));
}
// Results screen: RaceResultStat::Draw shows knocked out / DNF / time, but has no case for a
// busted racer (GRacerInfo +0x1F). Show BUSTED in the time column for those.
typedef void (THISCALL *ResultDrawFn)(void* stat);
static ResultDrawFn o_ResultStatDraw = (ResultDrawFn)0x5953F0;
static void FASTCALL Hook_ResultStatDraw(uint8_t* stat) {
    o_ResultStatDraw(stat);
    uint8_t* info = At<uint8_t*>(stat, 0x50);
    if (info && info[0x1F] && At<void*>(stat, 0x30)) ((FEPrintfFn)0x515D70)(At<void*>(stat, 0x30), "%s", "BUSTED");
}

// Police speech is off in split screen (no speech module), but SoundAI still asks it to drop a
// cop's lines when the cop despawns: Speech::Manager::FlushSpeechForActor reads the module
// (*0x992230)->+0x88 without a null check. Add the check.
static void InstallSpeechFlushGuard() {
    const uint8_t exp[12] = { 0x8B, 0x0D, 0x30, 0x22, 0x99, 0x00, 0x8B, 0x91, 0x88, 0x00, 0x00, 0x00 };
    if (memcmp((void*)0x709DEA, exp, 12)) { Log("speech flush guard: unexpected bytes"); return; }
    uint8_t* t = MakeStub(48); int n = 0;
    t[n++] = 0x8B; t[n++] = 0x0D; *(uint32_t*)(t + n) = 0x992230; n += 4;   // mov ecx,[0x992230]
    t[n++] = 0x85; t[n++] = 0xC9;                                          // test ecx,ecx
    t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x709E08 - ((uintptr_t)t + n + 4)); n += 4;   // jz end
    t[n++] = 0x8B; t[n++] = 0x91; *(uint32_t*)(t + n) = 0x88; n += 4;      // mov edx,[ecx+0x88]
    EmitJmp(t + n, 0xE9, 0x709DF6); n += 5;                                // back: cmp edx,ebx ...
    uint8_t j[6]; j[0] = 0xE9; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x709DEA + 5)); j[5] = 0x90;
    WriteMem(0x709DEA, j, 6);
}

// GRaceStatus::CalculateRankings compares racers with sub_5E31D0 (a = this, b = argument; true moves
// b ahead of a). It knows knocked out / totalled / blown engine but not busted (+0x1F): put busted
// racers behind everyone who isn't busted.
static bool FASTCALL Hook_RankCompare(uint8_t* a, void*, uint8_t* b) {
    bool ab = a && a[0x1F], bb = b && b[0x1F];
    if (ab != bb) return ab;                 // a busted, b not -> b ahead
    return ((bool(THISCALL*)(void*, void*))0x5E31D0)(a, b);
}

static void InstallSplitBusted() {
    InstallSpeechFlushGuard();
    PatchCall(0x5FDF76, (void*)Hook_RankCompare, 0x5E31D0);
    PatchCall(0x43EB6E, (void*)Hook_CommitPursuitDetails, 0x411240);
    PatchCall(0x43DDFD, (void*)Hook_RoadBlockNavPathType, 0x7658A0);
    PatchCall(0x43E1D0, (void*)Hook_PickRoadblockSetup, 0x4063D0);
    { void* fn = (void*)Hook_PursuitIsEvaded; if (*(uintptr_t*)0x891910 == 0x411070) WriteMem(0x891910, &fn, 4); else Log("evade patch: unexpected vtable"); }
    { void* fn = (void*)Hook_ResultStatDraw; if (*(uintptr_t*)0x8A1A94 == 0x5953F0) WriteMem(0x8A1A94, &fn, 4); else Log("results busted patch: unexpected vtable"); }
    const uint8_t exp[6] = { 0x8A, 0x44, 0x24, 0x58, 0x84, 0xC0 };    // mov al,[esp+0x58] / test al,al
    // site A (0x4445DD, the bust): player pursuit and ours -> SplitBustNow, continue at 0x44479C
    {
        uint8_t* t = MakeStub(64); int n = 0;
        t[n++] = 0x8A; t[n++] = 0x44; t[n++] = 0x24; t[n++] = 0x5C;      // mov al,[esp+0x5c]
        t[n++] = 0x84; t[n++] = 0xC0;                                    // test al,al
        t[n++] = 0x74; int jz = n++;                                     // jz done
        t[n++] = 0x51; t[n++] = 0x52; t[n++] = 0x56;                     // push ecx / edx / esi
        t[n++] = 0xB8; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)SplitBustNow; n += 4;
        t[n++] = 0xFF; t[n++] = 0xD0;                                    // call eax
        t[n++] = 0x83; t[n++] = 0xC4; t[n++] = 0x04;                     // add esp,4
        t[n++] = 0x5A; t[n++] = 0x59;                                    // pop edx / ecx
        t[n++] = 0x85; t[n++] = 0xC0;                                    // test eax,eax
        t[n++] = 0x74; int jn = n++;                                     // jz player (game's own path)
        t[n++] = 0x83; t[n++] = 0xC4; t[n++] = 0x04;                     // add esp,4 (drop return)
        t[n++] = 0x68; *(uint32_t*)(t + n) = 0x44479C; n += 4;           // push 0x44479C
        t[n++] = 0xC3;                                                   // ret -> continue there
        t[jn] = (uint8_t)(n - (jn + 1));
        t[n++] = 0xB0; t[n++] = 0x01;                                    // player: mov al,1
        t[jz] = (uint8_t)(n - (jz + 1));
        t[n++] = 0x84; t[n++] = 0xC0;                                    // done: test al,al
        t[n++] = 0xC3;
        uintptr_t a = 0x4445DD;
        if (memcmp((void*)a, exp, 6)) Log("busted patch %08X: unexpected bytes", a);
        else { uint8_t c[6]; c[0] = 0xE8; *(int32_t*)(c + 1) = (int32_t)((uintptr_t)t - (a + 5)); c[5] = 0x90; WriteMem(a, c, 6); }
    }
    // site B (0x444730, busted timer): al = player pursuit && !ours
    {
        uint8_t* t = MakeStub(48); int n = 0;
        t[n++] = 0x8A; t[n++] = 0x44; t[n++] = 0x24; t[n++] = 0x5C;
        t[n++] = 0x84; t[n++] = 0xC0;
        t[n++] = 0x74; int jz = n++;
        t[n++] = 0x51; t[n++] = 0x52; t[n++] = 0x56;
        t[n++] = 0xB8; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)SplitBustOverride; n += 4;
        t[n++] = 0xFF; t[n++] = 0xD0;
        t[n++] = 0x83; t[n++] = 0xC4; t[n++] = 0x04;
        t[n++] = 0x5A; t[n++] = 0x59;
        t[n++] = 0x85; t[n++] = 0xC0;
        t[n++] = 0x0F; t[n++] = 0x94; t[n++] = 0xC0;                     // sete al
        t[jz] = (uint8_t)(n - (jz + 1));
        t[n++] = 0x84; t[n++] = 0xC0;
        t[n++] = 0xC3;
        uintptr_t a = 0x444730;
        if (memcmp((void*)a, exp, 6)) Log("busted patch %08X: unexpected bytes", a);
        else { uint8_t c[6]; c[0] = 0xE8; *(int32_t*)(c + 1) = (int32_t)((uintptr_t)t - (a + 5)); c[5] = 0x90; WriteMem(a, c, 6); }
    }
}

static void InstallRaceOptions() {
    // show NumOpponents / AISkill / CatchUp in split screen (circuit, sprint, drag, speed trap)
    // world models (e.g. the flares at the finish line) are skipped in the split view mode unless
    // flagged for it; draw them all
    { uint8_t* p = (uint8_t*)0x75390B; if (p[0] == 0x84 && p[1] == 0xDB && p[2] == 0x74 && p[3] == 0x0B) { uint8_t b[4] = { 0xEB, 0x0D, 0x90, 0x90 }; WriteMem(0x75390B, b, 4); } else Log("world model patch: unexpected bytes"); }
    // Quick Race mode list: the game leaves Lap Knockout out in split screen
    { uint8_t* p = (uint8_t*)0x7AAB76; if (p[0] == 0x74 && p[1] == 0x3D) { uint8_t n[2] = { 0x90, 0x90 }; WriteMem(0x7AAB76, n, 2); } else Log("knockout menu patch: unexpected bytes"); }
    PatchCall(0x61146A, (void*)Hook_KnockOut, 0x600210);
    PatchCall(0x5FDF76, (void*)Hook_RacerBehind, 0x5E31D0);
    PatchCall(0x42BB92, (void*)Hook_AdjustedCopCounts, 0x424110);   // CopRequest
    PatchCall(0x426989, (void*)Hook_SpawnBehindTarget, 0x426850);   // SpawnCopCarNow
    PatchCall(0x42EB18, (void*)Hook_SpawnAheadOfTarget, 0x41EED0);  // SpawnPursuitCar (the mod's extra cops)
    PatchCall(0x42E721, (void*)Hook_UpdateCopPriorities, 0x426D50); // TrySpawnCop
    PatchCall(0x43EF4A, (void*)Hook_UpdateRoadBlocks, 0x439EE0);    // AICopManager::OnTask
    PatchCall(0x431438, (void*)Hook_SpawnAheadOfTarget, 0x41EED0);  // SpawnPursuitIVehicle (the game's own)
    PatchCall(0x41F34F, (void*)Hook_SpawnAheadOfTarget, 0x41EED0);  // StartHeavySupport (Rhinos)
    PatchCall(0x43EBD8, (void*)Hook_SpawnPursuitIVehicle, 0x430E60); // UpdatePursuits
    PatchCall(0x43EB7F, (void*)Hook_UpdateSupportCops, 0x43E770);    // UpdatePursuits
    PatchCall(0x43EC35, (void*)Hook_CreateRoadBlockCall, 0x43DD30);  // UpdatePursuits (requested roadblocks)
    PatchCall(0x42EAF3, (void*)Hook_OutOfViewForSpawn, 0x410E70);   // SpawnPursuitCar
    PatchCall(0x43E113, (void*)Hook_OutOfViewForBlock, 0x410E70);   // CreateRoadBlock
    {   uint8_t* p = (uint8_t*)0x43DE45;                                // CreateRoadBlock: push 250.0f
        if (p[0] == 0x68 && *(uint32_t*)(p + 1) == 0x437A0000) { DWORD o; VirtualProtect(p + 1, 4, PAGE_EXECUTE_READWRITE, &o); g_rbDist = (float*)(p + 1); }
        else Log("roadblock distance patch: unexpected bytes"); }
    PatchCall(0x43E7A3, (void*)Hook_GetHeavySupportVehicles, 0x41F130);
    PatchCall(0x43E836, (void*)Hook_StartHeavySupport, 0x41F2E0);
    {   const uint8_t e[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };   // AISpawnManager::GetBasePosition -> ours
        if (!memcmp((void*)0x40B0D0, e, 6)) { uint8_t j[5] = { 0xE9 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)Hook_GetBasePosition - (0x40B0D0 + 5)); WriteMem(0x40B0D0, j, 5); }
        else Log("spawn base position hook not installed"); }
    PatchCall(0x42B8F2, (void*)Hook_AdjustedCopCounts, 0x424110);   // RemoveUnwantedVehicles
    {   void* f = (void*)Hook_PursuitRequest; const uintptr_t vt[3] = { 0x891D74, 0x891F8C, 0x892414 };
        for (uintptr_t a : vt) if (*(uintptr_t*)a == 0x409890) WriteMem(a, &f, 4); else Log("pursuit request hook: unexpected vtable entry at %08X", (unsigned)a); }
    InstallSplitBusted();
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
    const uintptr_t catchUpAdds[] = { 0x7ACA83, 0x7ACC66, 0x7ACE09, 0x7AD286, 0x7AD0CD /* knockout (shared tail) */ };
    for (uintptr_t a : catchUpAdds) PatchCall(a, (void*)Hook_AddCatchUpOption, 0x588570);
    const uintptr_t aiAdds[] = { 0x7ACA09, 0x7ACA46, 0x7ACBEC, 0x7ACC29, 0x7ACD8F, 0x7ACDCC, 0x7ACF18, 0x7ACFFC, 0x7AD090, 0x7AD20C, 0x7AD249 };
    for (uintptr_t a : aiAdds) PatchCall(a, (void*)Hook_AddAIOption, 0x588570);
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
    if (cfg.enabled && *G::pSimUserMode == 1) SplitRespawnBusted();
    ((void(*)())0x60D820)();
    if (!cfg.enabled || *G::pSimUserMode != 1) return;
    ResetSplitRaceState();
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
    if (g_resultsShown || g_postRaceAt) { Log("Race over: results already on the way"); return; }
    int np; void** pl = LocalPlayers(&np);
    for (int i = 0; pl && i < np && i < 2; i++) if (!g_raceOverShown[i] && !g_busted[i] && pl[i]) PostRaceOverMessage(pl[i]);
    g_postRaceAt = GetTickCount() + cfg.resultsDelayMs; g_postRaceArg = arg;
    Log("Race over: results screen in %d ms", cfg.resultsDelayMs);
}
static void PostRaceTick() {
    if (!g_postRaceAt) return;
    if (*G::pGameFlowState != 6) { g_postRaceAt = 0; return; }       // left the race some other way
    if ((int)(GetTickCount() - g_postRaceAt) < 0) return;
    g_postRaceAt = 0;
    if (g_resultsShown) return;
    g_resultsShown = true;
    SplitEndAllPursuits();
    Log("Results screen opened");
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

// 4 player research: which of the 24 eViews are in use during a race, and the streamer's state
static void LogViews(const char* when) {
    if (!cfg.debug) return;
    Log("eViews %s (user mode %d, view mode %d):", when, *G::pSimUserMode, *G::pCurrentViewMode);
    for (int i = 0; i < 24; i++) {
        uint8_t* v = G::View(i);
        int movers = 0; void* first = nullptr;
        void** h = (void**)(v + G::EV_MOVERLIST);
        for (void** n = (void**)h[0]; n && n != h && movers < 16; n = (void**)n[0]) { if (!movers) first = n; movers++; }
        uint8_t* rt = At<uint8_t*>(v, G::EV_RT);
        void* mvt = first ? *(void**)((uint8_t*)first - 4) : nullptr;
        Log("  view %2d id %d active %d plat %p cam %p movers %d (vt %p type %d) rt %p [%d %dx%d] fx %p rain %p",
            i, At<int>(v, G::EV_ID), At<int8_t>(v, G::EV_ACTIVE), At<void*>(v, 0), At<void*>(v, G::EV_CAMERA), movers,
            mvt, first ? *(int*)((uint8_t*)first + 8) : -1,
            rt, rt ? At<int>(rt, 0x10) : 0, rt ? At<int>(rt, 0x14) : 0, rt ? At<int>(rt, 0x18) : 0,
            At<void*>(v, G::EV_SCREENFX), At<void*>(v, G::EV_RAIN));
    }
    uint8_t* ts = (uint8_t*)0x9AA9D0;
    int pool = ((int(THISCALL*)(void*))0x72DA70)(ts);
    for (int k = 0; k < 2; k++) {
        uint8_t* e = ts + 0xC0 + k * 100;
        Log("  streaming position %d: on %d pos %.0f %.0f %.0f zone %d/%d", k, e[0x1C], At<float>(e, 0), At<float>(e, 4), At<float>(e, 8), At<short>(e, 0x20), At<short>(e, 0x22));
    }
    Log("  streamer pool %d KB, sections %d", pool / 1024, At<int>(ts, 0x2C0));
    int n; void** pl = LocalPlayers(&n);
    for (int i = 0; i < n && pl; i++) {
        void* p = pl[i];
        Log("  player %d: %p port %d settings %d render port %d hud %p", i, p,
            ((int(THISCALL*)(void*))(*(void***)p)[0x34 / 4])(p), ((int(THISCALL*)(void*))(*(void***)p)[0x1C / 4])(p),
            ((int(THISCALL*)(void*))(*(void***)p)[0x2C / 4])(p), ((void*(THISCALL*)(void*))(*(void***)p)[0x20 / 4])(p));
    }
}
// log the start positions once the race is on screen and again just after the start
static void StartPositionDiagnostics() {
    SplitFlareRefresh();
    SplitCopsTick();
    SplitBustTick();
    SplitDespawnTick();
    SplitBustEndCheck();
    // single player: same cop / sound status every 10 s for comparison (debug log only)
    { static DWORD t = 0; DWORD now = GetTickCount();
      if (cfg.debug && *G::pSimUserMode != 1 && *G::pGameFlowState == 6 && now - t > 10000) { t = now; LogPursuits("single player"); } }
    if (!g_startLogStage || *G::pGameFlowState != 6) return;
    if (g_startLogStage == 1) { LogRacerPositions("race on screen"); LogTriggerEffects("race on screen"); g_startLogStage = 2; g_startLogTick = GetTickCount(); }
    else if (g_startLogStage == 2 && GetTickCount() - g_startLogTick > 6000) { LogRacerPositions("6 s later"); LogTriggerEffects("6 s later"); LogViews("6 s into the race"); g_startLogStage = 0; }
}
