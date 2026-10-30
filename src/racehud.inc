// ------------------------------------------------------------------ split screen race HUD: speed trap score
// Each player's HUD has a RaceInformation element ("COMPLETE 95 %"). In split screen speed trap
// races we add a SCORE row (total trap speed so far, in the HUD's gold) under that line.
// RaceInformation (PC v1.3): +0x10 package, +0x3C laps, +0x44 lap number, +0x50 % complete, +0x7C data string
typedef void (THISCALL *RIUpdateFn)(void* ri, void* player);
static RIUpdateFn o_RIUpdate = (RIUpdateFn)0x57A900;
typedef void* (THISCALL *RacerOfSimFn)(void* rs, void* sim);
static const uint32_t STR_SCORE = 0x5C2B0004;
static const char g_strScore[] = "SCORE";
struct ScoreRow { uint8_t* ri; char text[48]; int frame; int pct; };
static ScoreRow g_score[2];

static void FASTCALL Hook_RaceInfoUpdate(uint8_t* ri, void*, void* player) {
    o_RIUpdate(ri, player);
    if (!cfg.enabled || !cfg.speedTrapScore || *G::pSimUserMode != 1 || !player) return;
    uint8_t* rs = *(uint8_t**)0x91E000;
    if (!rs || !At<void*>(rs, 0x1968)) return;
    void* sim = ((void*(THISCALL*)(void*))(*(void***)player)[1])(player);       // IPlayer::GetSimable
    uint8_t* info = sim ? (uint8_t*)((RacerOfSimFn)0x5E8B20)(rs, sim) : nullptr; // GRaceStatus::GetRacerInfo(ISimable*)
    if (!info) return;
    int type = ((GetIntFn)0x5FAA20)(At<void*>(rs, 0x1968));                     // 0 sprint 1 circuit 2 drag 3 knockout 4 tollbooth 5 speed trap
    if (type != 5) return;                                                       // speed trap only
    float pts = At<float>(info, 0x120);
    uint8_t* prof = At<uint8_t*>(*G::pFEDatabase, 0x10);
    const char* unit = "";
    if (prof && At<uint8_t>(prof, 0x3B)) unit = ((const char*(*)(uint32_t))0x56BC10)(0x8569A25F);
    else { pts *= 0.6213712f * 2.2369363f; unit = ((const char*(*)(uint32_t))0x56BC10)(0x8569AB44); }
    int np; void** pl = LocalPlayers(&np);
    int idx = (pl && np > 0 && pl[0] == player) ? 0 : 1;
    ScoreRow& r = g_score[idx];
    r.ri = ri; r.frame = g_frame; r.pct = (int)At<float>(ri, 0x50);
    snprintf(r.text, sizeof r.text, "%.0f %s", pts, unit ? unit : "");
}

// Draw the score as an extra row under "COMPLETE": the row's own label and value objects are
// moved down one row, relabelled, rendered on their own, and put back.
typedef float (*GetFFn)(void* o);
typedef void (*SetFFn)(void* o, float v);
static void DrawSpeedTrapScoreRows() {
    if (!cfg.enabled || !cfg.speedTrapScore || *G::pSimUserMode != 1 || *G::pGameFlowState != 6) return;
    void* engine = *(void**)*(void**)0x91CADC;
    for (int i = 0; i < 2; i++) {
        ScoreRow& s = g_score[i];
        if (!s.ri || g_frame - s.frame > 3) continue;
        void* label = At<void*>(s.ri, 0x60), *data = At<void*>(s.ri, 0x7C), *lap = At<void*>(s.ri, 0x5C);
        if (!label || !data || !((bool(*)(void*))0x564AD0)(data)) continue;
        // move the objects by their raw position (FEObject data +0x1C); the centre helpers
        // measure the current text, so they don't round-trip when the text changes
        float* pl = (float*)(At<uint8_t*>((uint8_t*)label, 0x2C) + 0x1C);
        float* pd = (float*)(At<uint8_t*>((uint8_t*)data, 0x2C) + 0x1C);
        float* pt = lap ? (float*)(At<uint8_t*>((uint8_t*)lap, 0x2C) + 0x1C) : nullptr;
        float savL[3] = { pl[0], pl[1], pl[2] }, savD[3] = { pd[0], pd[1], pd[2] };
        float dy = pt ? pd[1] - pt[1] : 0.f;
        if (dy < 4.f || dy > 80.f) dy = 22.f;
        typedef void (THISCALL *SetPosFn)(void* o, float* v, bool b);
        float nl[3] = { savL[0], savL[1] + dy, savL[2] }, nd[3] = { savD[0], savD[1] + dy, savD[2] };
        uint32_t colData = ((uint32_t(*)(void*))0x51A180)(data);
        uint32_t gold = lap ? ((uint32_t(*)(void*))0x51A180)(lap) : 0xFFF0B060;
        ((SetPosFn)0x5B8190)(label, nl, false); ((SetPosFn)0x5B8190)(data, nd, false);
        ((SetLangHashFn)0x515C00)(label, STR_SCORE);
        ((FEPrintfFn)0x515D70)(data, "%s", s.text);
        ((void(*)(void*, uint32_t))0x5157E0)(data, gold);
        g_onlyObj = label; g_onlyObj2 = data; g_aiXf = nullptr; g_feMode = 3;
        ((void(THISCALL*)(void*))0x5BE800)(engine);
        g_feMode = 0; g_onlyObj = g_onlyObj2 = nullptr;
        ((SetPosFn)0x5B8190)(label, savL, false); ((SetPosFn)0x5B8190)(data, savD, false);
        ((SetLangHashFn)0x515C00)(label, 0x59BB1918);              // "COMPLETE"
        ((FEPrintfFn)0x515D70)(data, "%d %%", s.pct);
        ((void(*)(void*, uint32_t))0x5157E0)(data, colData);
        At<uint32_t>((uint8_t*)label, 0x1C) |= 0x400000;               // text changed: rebuild
        At<uint32_t>((uint8_t*)data, 0x1C) |= 0x400000;
    }
}
static void InstallRaceHud() {
    uintptr_t slot = 0x8A22F8, cur = *(uintptr_t*)slot;
    if (cur != 0x57A900) Log("RaceInformation::Update already hooked (-> %08X), chaining", cur);
    o_RIUpdate = (RIUpdateFn)cur;
    void* f = (void*)Hook_RaceInfoUpdate; WriteMem(slot, &f, 4);
}

