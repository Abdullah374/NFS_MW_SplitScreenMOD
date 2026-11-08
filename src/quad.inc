// ------------------------------------------------------------------ 4 player split screen
// "4 PLAYER SPLIT SCREEN" in Quick Race: the game's 2 player split screen race gets two more local
// players. Each has an input device of the mod (ports 3 and 4), a camera in a free eView (render
// ports 5 and 6) and its own HUD, and the screen is split into four quarters. Players 2-4 pick
// their cars one after another on the game's player 2 car select. With 3 players, player 4 is an
// AI racer (a local player on the game's own AI, AIVehicleHuman::SetAiControl).
// (FourPlayerTest=1 still turns any 2 player race into a 4 player one with two AI players.)
static const int kQuadView[4] = { 1, 2, 5, 6 };
static bool g_quad = false;                 // this race has the two extra players
static bool g_quadIsAI[2] = { false, false };   // player 3 / 4 is driven by the AI
static void* g_quadVeh[2] = {};             // ISimable* of players 3 and 4
static void* g_quadPl[2] = {};              // IPlayer*
static uint8_t g_quadCam[2][0x2C0] __attribute__((aligned(16)));
static bool g_quadAI = false;
static DWORD g_quadSince = 0;
static bool g_quadDiag = false; static int g_quadDiagLines = 0; static bool g_quadDiagDone = false;
static void* g_hudSeen[64]; static int g_hudSeenN = 0;
static void* g_quadPkg[8]; static int g_quadPkgQ[8]; static float g_quadPkgY[8]; static int g_quadPkgN = 0;
#define g_diagObj g_diagObjM
#define g_diagFn g_diagFnM

struct SimParam { uint32_t a, b; void* c; uint32_t d; };
typedef void* (__cdecl *SimableCreateFn)(uint32_t type, SimParam p);
typedef void* (__cdecl *EntityCreateFn)(uint32_t type, SimParam p);

static float g_sndRawVel[2][3]; static void* g_sndState[2];
static float g_sndRawPos[2][3]; static bool g_sndRawPosOk[2]; static float g_sndDopVel[2][3];
// players 3/4's sound space mapping: moved = B + R(real - A), R a turn about the vertical (c, s)
struct QuadSndMap { float A[3], B[3], c, s; bool ok; };
static QuadSndMap g_sndMap[2];
static void QuadSndUnmap(int k, float* p) {          // moved -> real (x, y)
    const QuadSndMap& m = g_sndMap[k];
    float x = p[0] - m.B[0], y = p[1] - m.B[1];
    p[0] = m.A[0] + x * m.c + y * m.s; p[1] = m.A[1] - x * m.s + y * m.c;
}   // players 3/4's real car positions (sound space)
static uint8_t g_shk[2][0x20];             // camera shakers of players 3 and 4
static bool g_creatingQuadVeh = false; static int g_creatingQuadIdx = 0;
static short g_xZone[2] = { 0, 0 }, g_xPend[2] = { 0, 0 }; static int g_xPendN[2] = { 0, 0 }, g_xBit[2] = { 0, 0 };
     // creating player 3/4's car

static void SetFourMode(bool on) { g_fourMode = on; g_fourModeSel = on; }
static bool FourRace() { return (g_fourMode && g_fourHumans >= 3) || cfg.quadTest; }

// ---- car select for players 2, 3, 4
// The game's player 2 car select ends in sub_7A3AF0: DeleteMultiplayerProfile(1) writes the
// chosen car to FEDatabase +0x470 (name) / +0x46C (customization, bMalloc'd), then
// RaceStarter::StartRace. In the 4 player mode the choice is kept and the car select opens
// again for the next player; the last one starts the race with player 2's car put back.
struct FourCar { char name[64]; uint8_t cust[0x198]; bool hasCust; bool valid; };
static FourCar g_fourCar[3];                // players 2, 3, 4
static int g_fourSel = 0;                   // which of players 2..4 is choosing (0 = player 2)
static void FourCarSelReset() { g_fourSel = 0; for (auto& c : g_fourCar) c.valid = false; }
static void FourCaptureCar(int k) {
    uint8_t* db = *G::pFEDatabase;
    ((void(THISCALL*)(void*, int))0x58FFA0)(db, 1);                 // DeleteMultiplayerProfile(1)
    FourCar& c = g_fourCar[k];
    snprintf(c.name, sizeof c.name, "%s", (const char*)(db + 0x470));
    uint8_t* cu = At<uint8_t*>(db, 0x46C);
    c.hasCust = cu != nullptr;
    if (cu) { memcpy(c.cust, cu, sizeof c.cust); ((void(*)(void*))0x4655F0)(cu); At<uint8_t*>(db, 0x46C) = nullptr; }   // bFree
    c.valid = true;
    Log("4 players: player %d picked %s%s", k + 2, c.name, c.hasCust ? " (customized)" : "");
}
static void __stdcall Hook_CarSelStartRace(int arg) {
    if (!g_fourMode) { ((void(__stdcall*)(int))0x7A3AF0)(arg); return; }
    int last = g_fourHumans - 2;                                    // index of the last human to choose (1 or 2)
    if (g_fourSel > last) g_fourSel = last;
    FourCaptureCar(g_fourSel);
    if (g_fourSel < last) {
        g_fourSel++;
        QueueSwitch("Car_Select.fng", 1, 7);                        // next player's turn
        return;
    }
    // everyone has a car: player 2's goes back where the race start reads it
    uint8_t* db = *G::pFEDatabase;
    FourCar& p2 = g_fourCar[0];
    snprintf((char*)(db + 0x470), 32, "%s", p2.name);
    if (p2.hasCust) {
        uint8_t* cu = (uint8_t*)((void*(*)(int, int))0x465A70)(0x198, 0x47);   // bMalloc
        if (cu) memcpy(cu, p2.cust, 0x198);
        At<uint8_t*>(db, 0x46C) = cu;
    }
    ((uint8_t*)((void*(*)())0x516E10)())[1] = (uint8_t)arg;        // FEManager::Get()->[1]
    Log("4 players: all cars picked, starting the race");
    ((void(*)())0x57ECA0)();                                         // RaceStarter::StartRace
}
// The game's player 2 car select title says "PLAYER 2" (UIQRCarSelect ctor). When players 3 and 4
// choose on it, the title says their number instead.
static void __cdecl Hook_CarSelTitle(const char* pkg, uint32_t obj, uint32_t lang) {
    ((void(__cdecl*)(const char*, uint32_t, uint32_t))0x525220)(pkg, obj, lang);   // FEngSetLanguageHash
    if (!g_fourMode || g_fourSel <= 0) return;
    const char* p2 = ((const char*(__cdecl*)(unsigned))0x56BC10)(lang);           // "PLAYER 2"
    char b[48]; snprintf(b, sizeof b, "%s", p2 ? p2 : "PLAYER 2");
    char* d = strrchr(b, '2');
    if (d) *d = (char)('2' + g_fourSel); else snprintf(b, sizeof b, "PLAYER %d", 2 + g_fourSel);
    ((int(__cdecl*)(const char*, uint32_t, const char*, ...))0x5252D0)(pkg, obj, "%s", b);   // FEPrintf
    Log("4 players: car select title %s", b);
}

// QuickGame::CreateCars, player 2's car: build two more cars from the same parameters
// (VehicleParams +0x14 car key, +0x18 direction*, +0x1C position*, +0x20 customization*),
// 3 and 6 m further ahead, with players 3 and 4's cars
static void* __cdecl Hook_CreateP2Vehicle(uint32_t type, SimParam p) {
    void* v = ((SimableCreateFn)0x41CB10)(type, p);
    g_quadVeh[0] = g_quadVeh[1] = nullptr;
    if (!FourRace() || *G::pSimUserMode != 1 || !p.c) return v;
    uint8_t* vp = (uint8_t*)p.c;
    float* pos = *(float**)(vp + 0x1C); float* dir = *(float**)(vp + 0x18);
    if (!pos || !dir || IsBadWritePtr(pos, 12) || IsBadReadPtr(dir, 12)) { Log("4 players: car parameters not found"); return v; }
    float save[3] = { pos[0], pos[1], pos[2] };
    uint32_t key0 = At<uint32_t>(vp, 0x14); void* cust0 = At<void*>(vp, 0x20);
    for (int k = 0; k < 2; k++) {
        bool human = !cfg.quadTest && g_fourMode && (k == 0 || g_fourHumans >= 4);
        g_quadIsAI[k] = !human;
        FourCar& c = g_fourCar[1 + k];
        if (human && c.valid) {                                     // this player's own car
            At<uint32_t>(vp, 0x14) = ((uint32_t(*)(const char*))0x454640)(c.name);   // Attrib::StringToKey
            At<void*>(vp, 0x20) = c.hasCust ? (void*)c.cust : nullptr;
        } else { At<uint32_t>(vp, 0x14) = key0; At<void*>(vp, 0x20) = cust0; }   // AI: player 2's car
        for (int i = 0; i < 3; i++) pos[i] = save[i] + dir[i] * 3.f * (k + 1);
        g_creatingQuadVeh = true; g_creatingQuadIdx = k;
        g_quadVeh[k] = ((SimableCreateFn)0x41CB10)(type, p);
        g_creatingQuadVeh = false;
    }
    for (int i = 0; i < 3; i++) pos[i] = save[i];
    At<uint32_t>(vp, 0x14) = key0; At<void*>(vp, 0x20) = cust0;
    Log("4 players: cars for players 3 and 4: %p (%s) %p (%s)", g_quadVeh[0], g_quadIsAI[0] ? "AI" : g_fourCar[1].name,
        g_quadVeh[1], g_quadIsAI[1] ? "AI" : g_fourCar[2].name);
    return v;
}

// ---- own player settings for players 3 and 4: they were seated with player 2's (SetSettings(1)),
// so the camera (PlayerSettings +0xC, changed by InputPlayer::FetchInput -> ScrollDriveCam, read by
// CDActionDrive::Update) was shared: player 2 changing it changed theirs. LocalPlayer::GetSettings
// (IPlayer slot 0x14) gives them a copy each instead, made from player 2's the first time.
static uint8_t g_quadSet[2][0x14]; static bool g_quadSetInit[2] = { false, false };
static void QuadCamerasNear() { for (int k = 0; k < 2; k++) if (g_quadSetInit[k]) At<int>(g_quadSet[k], 0xC) = 2; }
static void* FASTCALL Hook_PlayerGetSettings(void* pl, void*) {
    for (int k = 0; k < 2; k++) if (pl && pl == g_quadPl[k] && g_quadSetInit[k]) return g_quadSet[k];
    return ((void*(THISCALL*)(void*))0x6E99A0)(pl);
}

// after the game's two players have their cars: create players 3 and 4 and seat them
static void FASTCALL Hook_CreateCars(void* game, void*, void* where) {
    ((void(THISCALL*)(void*, void*))0x6F47A0)(game, where);
    g_quad = false; g_quadAI = false; g_quadSince = 0; g_quadDiagDone = false; g_xZone[0] = g_xZone[1] = 0; g_sndState[0] = g_sndState[1] = nullptr; g_sndRawPosOk[0] = g_sndRawPosOk[1] = false; g_sndMap[0].ok = g_sndMap[1].ok = false; memset(g_shk, 0, sizeof g_shk);
    g_quadPl[0] = g_quadPl[1] = nullptr;
    if (!FourRace() || *G::pSimUserMode != 1 || !g_quadVeh[0] || !g_quadVeh[1]) return;
    uint32_t hash = G::stringhash32("LocalPlayer");
    for (int k = 0; k < 2; k++) {
        // a camera for the free view first: the camera director attaches to it next frame
        uint8_t* view = G::View(kQuadView[2 + k]);
        void* cam2 = At<void*>(G::View(2), G::EV_CAMERA);
        if (!At<void*>(view, G::EV_CAMERA) && cam2) { memcpy(g_quadCam[k], cam2, sizeof g_quadCam[k]); At<void*>(view, G::EV_CAMERA) = g_quadCam[k]; }
        SimParam sp = { 0, 0, nullptr, 0 };
        uint8_t* ent = (uint8_t*)((EntityCreateFn)0x6EFF90)(hash, sp);
        void* pl = ent ? ((FindFn)0x5D59F0)(At<void*>(ent, 4), (void*)0x6200D0) : nullptr;   // IPlayer
        if (!pl) { Log("4 players: player %d not created", k + 3); return; }
        void** vt = *(void***)pl;
        int port = g_quadIsAI[k] ? 7 : g_quadPort[k];                   // 7: no device (AI)
        ((void(THISCALL*)(void*, int))vt[0x30 / 4])(pl, port);           // SetControllerPort
        ((void(THISCALL*)(void*, int))vt[0x18 / 4])(pl, 1);              // SetSettings (player 2's)
        if (!g_quadSetInit[k]) {                                         // their own copy, kept for the session
            void* s2 = ((void*(THISCALL*)(void*))0x6E99A0)(pl);
            if (s2) { memcpy(g_quadSet[k], s2, sizeof g_quadSet[k]); g_quadSetInit[k] = true; }
        }
        ((void(THISCALL*)(void*, int))vt[0x28 / 4])(pl, kQuadView[2 + k]); // SetRenderPort = view
        void* veh = g_quadVeh[k];
        ((void(THISCALL*)(void*, void*))(*(void***)veh)[0xC / 4])(veh, pl);   // ISimable::Attach(player)
        g_quadPl[k] = pl;
        Log("4 players: player %d %p (%s, port %d), render port %d, car %p", k + 3, pl, g_quadIsAI[k] ? "AI" : "human", port, kQuadView[2 + k], veh);
    }
    g_quad = true;
}


// ---- world streaming for four players
// TrackStreamer keeps two streaming positions (player 1 and 2's cameras, fed by UpdateCameraMovers).
// Each position gives a zone (GetPredictedZone) and the sections to load are rebuilt from those
// two zones (sub_72DD70: VisibleSectionManager::GetSectionsToLoad + AddCurrentStreamingSections).
// Players 3 and 4 get zones of their own, worked out the same way from their cameras; when one
// of them changes the streamer is told to rebuild (TrackStreamer +0x2B0), and the rebuild adds
// their zones' sections too (marked for the nearer of the two real positions).
static uint8_t g_xEntry[2][100];
static void QuadStreaming() {
    if (!g_quad || *G::pSimUserMode != 1 || *G::pGameFlowState != 6) { g_xZone[0] = g_xZone[1] = 0; return; }
    uint8_t* ts = (uint8_t*)0x9AA9D0;
    for (int k = 0; k < 2; k++) {
        uint8_t* v = G::View(kQuadView[2 + k]);
        float* cam = At<float*>(v, G::EV_CAMERA);
        if (!cam || !HasMover(v)) continue;
        uint8_t* e = g_xEntry[k];
        memset(e, 0, sizeof g_xEntry[k]);
        float* f = (float*)e;
        f[0] = cam[0x40 / 4]; f[1] = cam[0x44 / 4]; f[2] = cam[0x48 / 4];         // position
        f[3] = cam[0x200 / 4]; f[4] = cam[0x204 / 4];                              // velocity
        f[5] = cam[0x50 / 4]; f[6] = cam[0x54 / 4];                                // direction
        e[0x1C] = 1;
        short z = ((short(THISCALL*)(void*, void*))0x72DB80)(ts, e);            // GetPredictedZone
        // the nearer real position carries this player's sections
        float best = 1e30f;
        for (int i = 0; i < 2; i++) {
            float* rp = (float*)(ts + 0xC0 + i * 100);
            float dx = rp[0] - f[0], dy = rp[1] - f[1], d = dx * dx + dy * dy;
            if (ts[0xC0 + i * 100 + 0x1C] && d < best) { best = d; g_xBit[k] = i; }
        }
        if (z == g_xZone[k]) { g_xPendN[k] = 0; continue; }
        if (z != g_xPend[k]) { g_xPend[k] = z; g_xPendN[k] = 0; }
        if (++g_xPendN[k] >= 10 || !g_xZone[k]) {                              // steady for 10 frames
            Log("4 players: player %d streaming zone %d -> %d", k + 3, g_xZone[k], z);
            g_xZone[k] = z; g_xPendN[k] = 0;
            ts[0x2B0] = 1;                                                         // rebuild the sections to load
        }
    }
}
static short g_xSections[0x200];
static void FASTCALL Hook_RebuildStreamingSections(uint8_t* ts) {
    ((void(FASTCALL*)(void*))0x72DD70)(ts);
    if (!g_quad || *G::pSimUserMode != 1) return;
    for (int k = 0; k < 2; k++) {
        short z = g_xZone[k];
        if (z <= 0 || z == At<short>(ts, 0xDE) || z == At<short>(ts, 0xDE + 100)) continue;
        int n = 0;
        // the loading section that holds this zone, else the plain list for the zone
        uint8_t* ls = nullptr;
        for (uint8_t* l = *(uint8_t**)0x9A3B70; l && l != (uint8_t*)0x9A3B70 && !ls; l = *(uint8_t**)l) {
            int c = At<short>(l, 0x18);
            for (int i = 0; i < c && i < 64; i++) if (At<short>(l, 0x1A + i * 2) == z) { ls = l; break; }
        }
        if (ls) n = ((int(THISCALL*)(void*, void*, short*, int))0x72C750)((void*)0x9A3B50, ls, g_xSections, 0x180);   // GetSectionsToLoad
        else {
            for (uint8_t* l = *(uint8_t**)0x9A3B60; l && l != (uint8_t*)0x9A3B60; l = *(uint8_t**)l)
                if (At<short>(l, 0xC) == z) {
                    n = At<short>(l, 0x10); if (n > 0x180) n = 0x180;
                    memcpy(g_xSections, l + 0x12, n * 2);
                    break;
                }
        }
        if (n <= 0) continue;
        ((void(THISCALL*)(void*, short*, int, int))0x728060)(ts, g_xSections, n, g_xBit[k]);   // AddCurrentStreamingSections
        static short logged[2];
        if (logged[k] != z) { logged[k] = z; Log("4 players: player %d zone %d adds %d sections (position %d)", k + 3, z, n, g_xBit[k]); }
    }
}

// ---- engine sound for players 3 and 4
// A car gets its engine sound from SoundRacer with context "player" (human driver) or "AI".
// The player context only has room for two cars, so players 3 and 4 use the AI racers' sounds
// (free: there are no AI racers in a four player race).
static void* FASTCALL Hook_SoundRacerCtor(void* self, void*, uint8_t* params, int ctx) {
    // human players 3 and 4 keep the player context (player car sound states, see below);
    // only an AI player 4 sounds like an AI racer
    bool ai = g_creatingQuadVeh && g_quadIsAI[g_creatingQuadIdx];
    if (!g_creatingQuadVeh && g_quad && params) {
        uint8_t* o = At<uint8_t*>(params, 4);
        uint8_t* sim = o ? o + 0x2C : nullptr;
        void* veh = sim ? ((FindFn)0x5D59F0)(At<void*>(sim, 4), (void*)0x4040E0) : nullptr;
        for (int k = 0; k < 2; k++) if (veh && g_quadVeh[k] && g_quadIsAI[k] && veh == VehicleOfSimable(g_quadVeh[k])) ai = true;
    }
    if (ai && ctx == 0) { ctx = 1; Log("4 players: engine sound of the AI player as an AI racer's"); }
    return ((void*(THISCALL*)(void*, void*, int))0x6B4B10)(self, params, ctx);
}


// diagnostics: car sound managers and connections in a 4 player race (debug log, every 5 s for 30 s)
static void QuadSoundDiag() {
    static DWORD last = 0;
    DWORD now = GetTickCount();
    if (!cfg.debug || !g_quad || !g_quadSince || now - g_quadSince > 32000 || now - last < 5000) return;
    last = now;
    char b[600]; int l = 0;
    for (int i = 0; i < 13; i++) {
        uint8_t* m = ((uint8_t**)0x911F60)[i];
        if (!m || IsBadReadPtr(m, 0x28)) continue;
        int states = 0, att = 0;
        for (uint8_t* st = At<uint8_t*>(m, 0x10); st && states < 64; st = At<uint8_t*>(st, 4)) { states++; if (st[0x38]) att++; }
        l += snprintf(b + l, sizeof b - l, " [%d t%d %d/%d lim %.0f]", i, At<int>(m, 0x1C), att, states, At<float>(m, 0x20));
    }
    Log("4P sound: managers%s", b);
    float* cp = (float*)0x914430;
    Log("4P sound: listeners %d at %.0f,%.0f / %.0f,%.0f", *(int*)0x8EF998, cp[0], cp[1], cp[4], cp[5]);
    uint8_t** conns = *(uint8_t***)0x913E74; int nc = *(int*)0x913E7C;
    for (int i = 0; conns && i < nc && i < 16; i++) {
        uint8_t* c = conns[i]; if (!c) continue;
        uint8_t* cs = At<uint8_t*>(c, 0x14);
        if (!cs) { Log("4P sound:   conn %d no state, active %d", i, c[0x11]); continue; }
        float* p = (float*)(cs + 0x40);
        float d0 = sqrtf((p[0]-cp[0])*(p[0]-cp[0]) + (p[1]-cp[1])*(p[1]-cp[1])), d1 = sqrtf((p[0]-cp[4])*(p[0]-cp[4]) + (p[1]-cp[5])*(p[1]-cp[5]));
        Log("4P sound:   conn %d type %d f22c %d f22d %d active %d pos %.0f,%.0f dist %.0f / %.0f", i, At<int>(cs, 0x228), cs[0x22C], cs[0x22D], c[0x11], p[0], p[1], d0, d1);
    }
}

// The AI car sound manager gets one sound state per AI racer in the race when the world loads
// (CSTATEMGR_AICar::EnterWorld, at most 4); a 4 player race has none, so give it two for
// players 3 and 4.
static void FASTCALL Hook_AICarEnterWorld(uint8_t* mgr, void*, int mode) {
    int n = *(int*)0x92CE44 + *(int*)0x92CFC4;
    if (n > 4) n = 4;
    if (FourRace() && n < 2) { Log("4 players: AI car sound states %d -> 2", n); n = 2; }
    int type = *(uint8_t*)0x9142E7 ? 0x92 : 0x91;
    for (int i = 0; i < n; i++) {
        uint8_t* st = (uint8_t*)((void*(THISCALL*)(void*, int, int))0x4E5970)(mgr, 0, type);   // CreateState
        if (!st) continue;
        ((void(THISCALL*)(void*, int))(*(void***)st)[1])(st, type);
        for (int b = 6; b <= 7; b++) ((void*(THISCALL*)(void*, int))0x4EB8E0)(st, b);          // NewSFXCtrl
    }
    mgr[0x18] = 1;
}

// Player car engine sounds: CSTATEMGR_PlayerCar::EnterWorld makes one sound state per local
// player (2 in split screen) and EAXSound::AttachPlayerCars gives them to the player cars, at most
// 2 in split screen. With four players: four states and up to four player cars.
static void FASTCALL Hook_PlayerCarEnterWorld(uint8_t* mgr, void*, int mode) {
    ((void(THISCALL*)(void*, int))0x4F2E90)(mgr, mode);
    if (!FourRace() || *G::pSimUserMode != 1) return;
    int type = *(uint8_t*)0x9142E4 ? 0xD3BFC : 0x53BFC;                       // IsTruck
    for (int i = 0; i < 2; i++) {
        uint8_t* st = (uint8_t*)((void*(THISCALL*)(void*, int, int))0x4E5970)(mgr, 0, type);   // CreateState
        if (!st) continue;
        ((void(THISCALL*)(void*, int))(*(void***)st)[1])(st, type);
        ((void*(THISCALL*)(void*, int))0x4EB8E0)(st, 8);                       // NewSFXCtrl
    }
    Log("4 players: player car sound states 2 -> 4");
    // a new race or a restart: diagnostics again, autopilot for an AI player again
    g_quadSince = 0; g_quadDiagDone = false; g_quadAI = false; g_hudSeenN = 0;
}

// Players 3 and 4 hear their own cars: the game has two sound listeners (player 1 and 2's
// cameras), and a player car's sounds are placed relative to them. After a player 3/4 car's sound
// state is updated (CarSoundConn::UpdateState), its position, orientation and velocity are moved
// to the same place relative to listener 1 (player 3) or 2 (player 4) as the car has relative to
// its own camera, so it sounds like player 1 and 2's own cars do.
static uint8_t* PlayerSoundState(int idx) {                       // EAX_CarState of player car sound state idx
    uint8_t* mgr = *(uint8_t**)0x911F68;
    if (!mgr) return nullptr;
    uint8_t* car = (uint8_t*)((void*(THISCALL*)(void*, int))0x4D9680)(mgr, idx);   // GetStateObj
    return car && !IsBadReadPtr(car, 0x40) ? At<uint8_t*>(car, 0x34) : nullptr;
}
static void FASTCALL Hook_CarSoundUpdate(uint8_t* conn, void*, float dt) {
    uint8_t* cs = At<uint8_t*>(conn, 0x14);
    int k = -1;
    if (g_quad && g_quadRender && cs)
        for (int i = 0; i < 2; i++) if (!g_quadIsAI[i] && cs == PlayerSoundState(2 + i)) k = i;
    if (k >= 0 && g_sndState[k] == cs) memcpy(cs + 0x50, g_sndRawVel[k], 12);      // the game works on the real velocity
    ((void(THISCALL*)(void*, float))0x4B15E0)(conn, dt);
    if (k < 0) return;
    float* cam = At<float*>(G::View(kQuadView[2 + k]), G::EV_CAMERA);
    if (!cam) return;
    float* L = (float*)(0x914430 + k * 0x10);                          // SndCamera::m_CurCamPos[k]
    float* LD = (float*)(0x914450 + k * 0x10);                         // m_CurCamDir[k]
    float* LV = (float*)(0x914540 + k * 0x10);                         // m_v3WorldCamVel[k]
    float* C = cam + 0x40 / 4; float* D = cam + 0x50 / 4; float* CV = cam + 0x200 / 4;
    float a1 = atan2f(LD[1], LD[0]), a3 = atan2f(D[1], D[0]);
    if (!(fabsf(LD[0]) + fabsf(LD[1]) > 1e-3f) || !(fabsf(D[0]) + fabsf(D[1]) > 1e-3f)) return;
    float th = a1 - a3, cs_ = cosf(th), sn = sinf(th);
    auto rot = [&](float* v) { float x = v[0], y = v[1]; v[0] = x * cs_ - y * sn; v[1] = x * sn + y * cs_; };
    float* P = (float*)(cs + 0x40);
    memcpy(g_sndRawPos[k], P, 12); g_sndRawPosOk[k] = true;
    // the car sits where player 1/2's car is (their own car sounds are mixed close up, skids especially)
    uint8_t* own = PlayerSoundState(k);
    QuadSndMap& mp = g_sndMap[k];
    mp.c = cs_; mp.s = sn;
    if (own && own != cs) { memcpy(mp.A, P, 12); memcpy(mp.B, own + 0x40, 12); memcpy(P, own + 0x40, 12); }
    else { memcpy(mp.A, C, 12); memcpy(mp.B, L, 12); float rel[3] = { P[0] - C[0], P[1] - C[1], P[2] - C[2] }; rot(rel); P[0] = L[0] + rel[0]; P[1] = L[1] + rel[1]; P[2] = L[2] + rel[2]; }
    mp.ok = true;
    for (int r = 0; r < 3; r++) rot((float*)(cs + 0x10 + r * 0x10));     // matrix axes
    // the velocity stays the real one: skids, road noise, wind and the engine mix go by the car's speed.
    // Doppler (relative to listener k's car and camera) uses a moved copy, see Hook_UpdateDoppler.
    float* V = (float*)(cs + 0x50);
    memcpy(g_sndRawVel[k], V, 12); g_sndState[k] = cs;
    float rv[3] = { V[0] - CV[0], V[1] - CV[1], V[2] - CV[2] };
    rot(rv);
    g_sndDopVel[k][0] = LV[0] + rv[0]; g_sndDopVel[k][1] = LV[1] + rv[1]; g_sndDopVel[k][2] = LV[2] + rv[2];
}
static void FASTCALL Hook_UpdateDoppler(uint8_t* obj, void*, int idx, float dt) {
    float* v = At<float*>(obj, 0x30);
    for (int k = 0; k < 2 && g_quad; k++)
        if (g_sndState[k] && v == (float*)((uint8_t*)g_sndState[k] + 0x50)) {
            At<float*>(obj, 0x30) = g_sndDopVel[k];
            ((void(THISCALL*)(void*, int, float))0x4BF2E0)(obj, idx, dt);
            At<float*>(obj, 0x30) = v;
            return;
        }
    ((void(THISCALL*)(void*, int, float))0x4BF2E0)(obj, idx, dt);
}

// ---- camera shake for players 3 and 4
// There are two camera shakers (CameraShakers[2], 0x20 bytes each), one per player view.
// CDActionDrive::Update starts a shake with MaybeCameraShake(view - 1, impulse) - for views 5 and 6
// that wrote past the table - and the camera mover applies it with ApplyCameraShake(view, matrix),
// which only knows views 1-3. Players 3 and 4 get shakers of their own, used through slot 1.
static uint8_t* const kShaker1 = (uint8_t*)0x925AC0;
static void ShakerSwap(int k) { uint8_t t[0x20]; memcpy(t, kShaker1, 0x20); memcpy(kShaker1, g_shk[k], 0x20); memcpy(g_shk[k], t, 0x20); }
static void __cdecl Hook_MaybeCameraShake(int idx, float* impulse) {
    if (idx == 4 || idx == 5) { int k = idx - 4; ShakerSwap(k); ((void(*)(int, float*))0x64C5F0)(1, impulse); ShakerSwap(k); return; }
    if (idx < 0 || idx > 1) return;
    ((void(*)(int, float*))0x64C5F0)(idx, impulse);
}
static void __cdecl Hook_ApplyCameraShake(int view, void* m) {
    if (view == 5 || view == 6) { int k = view - 5; ShakerSwap(k); ((void(*)(int, void*))0x64C420)(2, m); ShakerSwap(k); return; }
    ((void(*)(int, void*))0x64C420)(view, m);
}
static void __cdecl Hook_UpdateCameraShakers(float dt) {
    ((void(*)(float))0x64C2D0)(dt);
    for (int k = 0; k < 2; k++) {                                      // the same as UpdateCameraShakers does per shaker
        float** env = At<float**>(g_shk[k], 0x10);
        if (!env) continue;
        float& t = At<float>(g_shk[k], 0x14); float dur = At<float>(g_shk[k], 0x18);
        if (dur > 0) t += dt / dur;
        float* keys = env[0]; int n = ((int*)env)[1];
        if (!keys || n <= 0 || t < keys[0] || t >= keys[n * 2 - 2]) At<float**>(g_shk[k], 0x10) = nullptr;
    }
}

// ---- particle effects (sparks, smoke, ...) near players 3 and 4
// EmitterSystem only spawns effects close to its interest points, player 1 and 2's cameras
// (IsCloseEnough). Players 3 and 4's cameras count too.
static int g_iceCalls = 0, g_iceHit[4] = { 0, 0, 0, 0 };
static bool FASTCALL Hook_EmitterIsCloseEnough(uint8_t* es, void*, float* p, float dist, int mode, float dotMin) {
    g_iceCalls++;
    float* pts[4]; float* dirs[4]; int n = 0;
    pts[n] = (float*)(es + 0x320); dirs[n] = (float*)(es + 0x340); n++;
    if (*G::pCurrentViewMode == 3) { pts[n] = (float*)(es + 0x330); dirs[n] = (float*)(es + 0x350); n++; }
    if (g_quad && g_quadRender)
        for (int k = 0; k < 2; k++) {
            float* cam = At<float*>(G::View(kQuadView[2 + k]), G::EV_CAMERA);
            if (cam) { pts[n] = cam + 0x40 / 4; dirs[n] = cam + 0x50 / 4; n++; }
        }
    for (int i = 0; i < n; i++) {
        float dx = p[0] - pts[i][0], dy = p[1] - pts[i][1], dz = p[2] - pts[i][2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (!(d < dist)) continue;
        if (mode == 0) { g_iceHit[i]++; return true; }
        if (d > 0 && (dx * dirs[i][0] + dy * dirs[i][1] + dz * dirs[i][2]) / d > dotMin) { g_iceHit[i]++; return true; }
    }
    return false;
}

// ---- effect culling near players 3 and 4
// WorldEffectConn::Update (sparks, smoke and their sounds) and a few sound connections cull by
// Sound::DistanceToView, the distance to the cameras of views 1 and 2 only. Players 3 and 4's views count too.
static float __cdecl Hook_DistanceToView(const float* p) {
    float best = 100000.f;
    static const int views[4] = { 1, 2, 5, 6 };
    int n = (g_quad && g_quadRender) ? 4 : 2;
    for (int i = 0; i < n; i++) {
        bNode* h = (bNode*)(G::View(views[i]) + G::EV_MOVERLIST);
        bNode* m = h->next;
        if (!m || m == h || (uintptr_t)m == 4) continue;
        uint8_t* cam = At<uint8_t*>((uint8_t*)m - 4, 0x1C);
        if (!cam) continue;
        float dx = At<float>(cam, 0x40) - p[0], dy = At<float>(cam, 0x44) - p[1], dz = At<float>(cam, 0x48) - p[2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < best) best = d;
    }
    return best;
}

static int PlayerOfSimable(void* s);
// ---- tunnels, overpasses and reverb zones for players 3 and 4
// SFXCTL_Tunnel (one per player car) looks up track zones (tunnels, overpasses: the woosh when going
// under something, the tunnel echo) and the city reverb map at the car's sound position. Players 3/4's
// cars sit next to listener 1/2 in sound space, so the lookups are moved back to where the car really is.
static int g_tunK = -1;
static void FASTCALL Hook_TunnelUpdate(uint8_t* t, void*, float dt) {
    g_tunK = -1;
    uint8_t* st = At<uint8_t*>(t, 0x14);
    uint8_t* cs = st ? At<uint8_t*>(st, 0x34) : nullptr;
    for (int k = 0; k < 2 && g_quad; k++) if (cs && g_sndState[k] == cs && g_sndMap[k].ok) g_tunK = k;
    ((void(THISCALL*)(void*, float))0x4C8E00)(t, dt);
    g_tunK = -1;
}
static void* FASTCALL Hook_TunnelFindZone(void* mgr, void*, const float* pos, int type, void* prev) {
    float p[3] = { pos[0], pos[1], 0 };
    if (g_tunK >= 0) QuadSndUnmap(g_tunK, p);
    return ((void*(THISCALL*)(void*, const float*, int, void*))0x727480)(mgr, g_tunK >= 0 ? p : pos, type, prev);
}
static void FASTCALL Hook_ReverbCapture(void* acc, void*, float x, float y) {
    float p[3] = { x, y, 0 };
    if (g_tunK >= 0) QuadSndUnmap(g_tunK, p);
    ((void(THISCALL*)(void*, float, float))0x74B640)(acc, p[0], p[1]);
}

// ---- race names: in split screen every racer after the first is "PLAYER 2"; players 3 and 4 get theirs
static char g_quadName[2][48];
static void* FASTCALL Hook_AddSimablePlayer(void* rs, void*, void* simable) {
    uint8_t* r = (uint8_t*)((void*(THISCALL*)(void*, void*))0x5E8BF0)(rs, simable);
    if (r && g_quad && *G::pSimUserMode == 1) {
        int i = PlayerOfSimable(simable);
        if (i >= 2) {
            const char* p2 = ((const char*(__cdecl*)(unsigned))0x56BC10)(0x7B070985);   // "PLAYER 2"
            char* b = g_quadName[i - 2];
            snprintf(b, sizeof g_quadName[0], "%s", p2 ? p2 : "PLAYER 2");
            char* d = strrchr(b, '2');
            if (d) *d = (char)('1' + i); else snprintf(b, sizeof g_quadName[0], "PLAYER %d", i + 1);
            At<const char*>(r, 8) = b;
            Log("4 players: racer name for player %d: %s", i + 1, b);
        }
    }
    return r;
}

// ---- collision and scrape sounds of players 3 and 4
// Effect sounds (WorldEffectConn audio events: crashes, scrapes, hits) are 3D sounds heard from
// listeners 1 and 2 only. One closest to player 3 or 4's camera is moved to the same place relative to
// listener 1 or 2, the same as players 3 and 4's engine sounds.
static int QuadNearestListener(const float* p) {               // 0/1 = players 3/4 own this sound, -1 = no
    if (!g_quad || !g_quadRender || !p) return -1;
    static const int views[4] = { 1, 2, 5, 6 };
    float best = 1e30f; int bi = -1;
    for (int i = 0; i < 4; i++) {
        float* cam = At<float*>(G::View(views[i]), G::EV_CAMERA);
        if (!cam) continue;
        float dx = cam[0x10] - p[0], dy = cam[0x11] - p[1], dz = cam[0x12] - p[2], d = dx * dx + dy * dy + dz * dz;
        if (d < best) { best = d; bi = i; }
    }
    return bi >= 2 && !g_quadIsAI[bi - 2] ? bi - 2 : -1;
}
// pos, dir and vel (any may be null) relative to player 3/4's camera -> the same relative to listener k
static bool QuadMapToListener(int k, float* P, float* Dir, float* V) {
    float* cam = At<float*>(G::View(kQuadView[2 + k]), G::EV_CAMERA);
    if (!cam) return false;
    float* L = (float*)(0x914430 + k * 0x10);
    float* LD = (float*)(0x914450 + k * 0x10);
    float* LV = (float*)(0x914540 + k * 0x10);
    float* C = cam + 0x40 / 4; float* D = cam + 0x50 / 4; float* CV = cam + 0x200 / 4;
    if (!(fabsf(LD[0]) + fabsf(LD[1]) > 1e-3f) || !(fabsf(D[0]) + fabsf(D[1]) > 1e-3f)) return false;
    float th = atan2f(LD[1], LD[0]) - atan2f(D[1], D[0]), c = cosf(th), sn = sinf(th);
    auto rot = [&](float* v) { float x = v[0], y = v[1]; v[0] = x * c - y * sn; v[1] = x * sn + y * c; };
    if (P) {   // relative to player 3/4's car -> the same relative to player 1/2's car (as their car sounds)
        uint8_t* own = PlayerSoundState(k);
        const float* A = C; const float* B = L;
        if (own && g_sndRawPosOk[k]) { A = g_sndRawPos[k]; B = (float*)(own + 0x40); }
        float r[3] = { P[0] - A[0], P[1] - A[1], P[2] - A[2] }; rot(r); P[0] = B[0] + r[0]; P[1] = B[1] + r[1]; P[2] = B[2] + r[2];
    }
    if (Dir) rot(Dir);
    if (V) { float r[3] = { V[0] - CV[0], V[1] - CV[1], V[2] - CV[2] }; rot(r); V[0] = LV[0] + r[0]; V[1] = LV[1] + r[1]; V[2] = LV[2] + r[2]; }
    return true;
}
static int g_fxSnd[3] = { 0, 0, 0 };                           // diagnostics: effect sounds started for P3, P4, others
typedef void* (__cdecl *AudioEventCreateFn)(uint8_t*);
static void* __cdecl Hook_FxAudioCreate(uint8_t* params) {      // params: +0 position, +0x10 direction, +0x20 velocity
    int k = QuadNearestListener((float*)params);
    g_fxSnd[k >= 0 ? k : 2]++;
    if (k < 0) return ((AudioEventCreateFn)0x779430)(params);
    uint8_t copy[0x60]; memcpy(copy, params, sizeof copy);
    QuadMapToListener(k, (float*)copy, (float*)(copy + 0x10), (float*)(copy + 0x20));
    return ((AudioEventCreateFn)0x779430)(copy);
}
static void __stdcall Hook_FxAudioUpdate(void* ev, float* pos, float* dir, float* vel, float vol) {
    float p[3] = { pos[0], pos[1], pos[2] }, d[3] = { dir[0], dir[1], dir[2] }, v[3] = { vel[0], vel[1], vel[2] };
    int k = QuadNearestListener(pos);
    if (k >= 0) QuadMapToListener(k, p, d, v);
    ((void(THISCALL*)(void*, float*, float*, float*, float))(*(void***)ev)[0xC / 4])(ev, p, d, v, vol);
}

// diagnostics: collision effects fired near each player (Effects::DoHit -> Sim::Effect::Fire)
static int g_fxFire[4] = { 0, 0, 0, 0 }, g_fxFireOther = 0;
typedef void (__cdecl *EffectFireFn)(void*, float*, float*, unsigned, void*, void*, unsigned);
static void __cdecl Hook_EffectFire(void* c, float* pos, float* vel, unsigned a, void* c1, void* c2, unsigned b) {
    if (g_quad && pos) {
        int n; void** pl = LocalPlayers(&n); int best = -1; float bd = 25.f;
        for (int i = 0; i < n && i < 4 && pl; i++) {
            const float* q = SimPosition(((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]));
            if (!q) continue;
            float dx = q[0] - pos[0], dy = q[1] - pos[1], dz = q[2] - pos[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) g_fxFire[best]++; else g_fxFireOther++;
    }
    ((EffectFireFn)0x6F4430)(c, pos, vel, a, c1, c2, b);
}
// ---- diagnostics: race progress of the four players (finish line, laps)
static void* PlayerSimable(int i) {
    int n; void** pl = LocalPlayers(&n);
    if (!pl || i >= n || !pl[i]) return nullptr;
    return ((void*(THISCALL*)(void*))(*(void***)pl[i])[1])(pl[i]);
}
static int PlayerOfSimable(void* s) { for (int i = 0; i < 4; i++) if (s && PlayerSimable(i) == s) return i; return -1; }
static uint8_t* RacerOf(void* s) { return *(void**)0x91E000 && s ? (uint8_t*)((void*(THISCALL*)(void*, void*))0x5E8B20)(*(void**)0x91E000, s) : nullptr; }
static void QuadRaceDiag(const char* when) {
    if (!cfg.debug || !g_quad) return;
    for (int i = 0; i < 4; i++) {
        void* s = PlayerSimable(i); uint8_t* r = RacerOf(s);
        int idx = s ? ((int(__cdecl*)(void*))0x60DAE0)(s) : -1;
        int human = idx > 0 ? ((int(__cdecl*)(int))0x611E20)(idx) & 0xFF : -1;
        if (r) Log("4P race %s: P%d simable %p racer %d (%p) human %d, %.1f %% lap %d flags %02X %02X %02X %02X %02X", when, i + 1, s, idx, r, human,
                   At<float>(r, 0x18), At<int>(r, 0x28), At<uint8_t>(r, 0x1C), At<uint8_t>(r, 0x1D), At<uint8_t>(r, 0x1E), At<uint8_t>(r, 0x1F), At<uint8_t>(r, 0x21));
        else Log("4P race %s: P%d simable %p, no racer info", when, i + 1, s);
    }
}
static void* g_lastTrig[4];
static void FASTCALL Hook_NotifySimableTrigger(void* trig, void*, void* simable, int type) {
    int i = PlayerOfSimable(simable);
    if (cfg.debug && g_quad && i >= 0 && type == 1 && g_lastTrig[i] != trig) { g_lastTrig[i] = trig; Log("4P race: P%d is in trigger %p", i + 1, trig); }
    ((void(THISCALL*)(void*, void*, int))0x5F6A30)(trig, simable, type);
}
static int __cdecl Hook_RacerIndexRaceFinished(void* s) {
    int r = ((int(__cdecl*)(void*))0x60DAE0)(s);
    if (g_quad) Log("4P race: race finished for P%d (racer %d)", PlayerOfSimable(s) + 1, r);
    return r;
}
static int __cdecl Hook_RacerIndexLapFinished(void* s) {
    int r = ((int(__cdecl*)(void*))0x60DAE0)(s);
    if (g_quad) Log("4P race: lap finished for P%d (racer %d)", PlayerOfSimable(s) + 1, r);
    return r;
}

static void QuadFxDiag() {
    static DWORD last = 0; DWORD now = GetTickCount();
    if (!cfg.debug || !g_quad || now - last < 10000) return;
    last = now;
    Log("4P fx: collision effects fired near P1 %d P2 %d P3 %d P4 %d other %d; emitter range checks %d, in range of P1 %d P2 %d P3 %d P4 %d",
        g_fxFire[0], g_fxFire[1], g_fxFire[2], g_fxFire[3], g_fxFireOther, g_iceCalls, g_iceHit[0], g_iceHit[1], g_iceHit[2], g_iceHit[3]);
    QuadRaceDiag("status");
    Log("4P fx: effect sounds started for P3 %d P4 %d others %d", g_fxSnd[0], g_fxSnd[1], g_fxSnd[2]); memset(g_fxSnd, 0, sizeof g_fxSnd);
    memset(g_fxFire, 0, sizeof g_fxFire); g_fxFireOther = 0; g_iceCalls = 0; memset(g_iceHit, 0, sizeof g_iceHit);
}

static bool QuadViewsReady() {
    if (!g_quad) return false;
    for (int k = 2; k < 4; k++) {
        uint8_t* v = G::View(kQuadView[k]);
        if (!At<void*>(v, G::EV_CAMERA) || !HasMover(v)) return false;
    }
    return true;
}

// players 3 and 4 drive themselves once the race is going
static void QuadTick() {
    if (!g_quad || *G::pGameFlowState != 6 || *G::pSimUserMode != 1) return;
    DWORD now = GetTickCount();
    if (!g_quadSince) { g_quadSince = now; return; }
    QuadSoundDiag();
    QuadFxDiag();
    // one frame of HUD diagnostics 10 s in
    if (g_quadDiag) { g_quadDiag = false; g_quadDiagDone = true; }
    else if (cfg.debug && !g_quadDiagDone && now - g_quadSince > 10000) {
        g_quadDiag = true; g_quadDiagLines = 0;
        int n; void** pl = LocalPlayers(&n);
        for (int i = 0; i < n && pl && i < 4; i++) Log("4P HUD diag: player %d hud %p", i + 1, ((void*(THISCALL*)(void*))(*(void***)pl[i])[0x20 / 4])(pl[i]));
        for (int i = 0; i < g_quadPkgN; i++) {
            int tot = 0, vis = 0;
            struct W { static void walk(uint8_t* o, int depth, int& tot, int& vis) {
                for (int g = 0; o && g < 400; o = At<uint8_t*>(o, 4), g++) {
                    tot++; if (!(At<uint32_t>(o, 0x1C) & 1)) vis++;
                    if (At<int>(o, 0x18) == 5 && depth < 6) walk(At<uint8_t*>(o, 100), depth + 1, tot, vis);
                } } };
            W::walk(At<uint8_t*>((uint8_t*)g_quadPkg[i], 0x4C), 0, tot, vis);
            Log("4P HUD diag: package %p -> quarter %d (layout y %.0f), %d objects, %d visible, pkg flags %08X %08X %08X",
                g_quadPkg[i], g_quadPkgQ[i], g_quadPkgY[i], tot, vis, At<uint32_t>((uint8_t*)g_quadPkg[i], 0x10), At<uint32_t>((uint8_t*)g_quadPkg[i], 0x14), At<uint32_t>((uint8_t*)g_quadPkg[i], 0x18));
        }
        for (int i = 0; i < n && pl && i < 4; i++) {
            uint8_t* hud = (uint8_t*)((void*(THISCALL*)(void*))(*(void***)pl[i])[0x20 / 4])(pl[i]);
            if (hud) { uint8_t* sp = At<uint8_t*>(hud, 0x2C0 - 0x0);   // Speedometer element (FEngHud +0x2C0) if hud is the FEngHud
                char nm[32] = "?"; const char* src = At<char*>(hud, 0x24);
                if (src && !IsBadReadPtr(src, 32)) { memcpy(nm, src, 31); nm[31] = 0; for (char* c = nm; *c; c++) if (*c < 32 || *c > 126) { *c = 0; break; } }
                Log("4P HUD diag: hud %d name %s speedo %p", i + 1, nm, sp); }
        }
    }
    if (g_quadAI || now - g_quadSince < 500) return;      // on autopilot from the start (the AI waits for the lights)
    g_quadAI = true;
    for (int k = 0; k < 2; k++) {
        uint8_t* veh = (uint8_t*)g_quadVeh[k];
        if (!veh || !g_quadIsAI[k]) continue;
        void* ai = ((FindFn)0x5D59F0)(At<void*>(veh, 4), (void*)0x403770);   // IHumanAI
        if (ai) ((void(THISCALL*)(void*, bool))(*(void***)ai)[0x10 / 4])(ai, true);   // SetAiControl
        Log("4P test: player %d on autopilot (%p)", k + 3, ai);
    }
}

static void InstallQuadHud();
static void InstallQuadTest() {
    if (!PatchCall(0x6F4CC4, (void*)Hook_CreateP2Vehicle, 0x41CB10)) return;
    PatchCall(0x6F4EE5, (void*)Hook_CreateCars, 0x6F47A0);
    // CameraAI::Update makes a camera director for render ports 1..2; go up to 6
    const uint8_t exp[3] = { 0x83, 0xF8, 0x02 };
    if (!memcmp((void*)0x480EF9, exp, 3)) { uint8_t b = 6; WriteMem(0x480EFB, &b, 1); }
    else Log("4P test: camera director loop not found");
    InstallQuadHud();
    PatchCall(0x7C1CCC, (void*)Hook_CarSelStartRace, 0x7A3AF0);
    PatchCall(0x6B4C8F, (void*)Hook_SoundRacerCtor, 0x6B4B10);
    PatchCall(0x73069E, (void*)Hook_RebuildStreamingSections, 0x72DD70);
    PatchCall(0x4BA95B, (void*)Hook_CarSoundUpdate, 0x4B15E0);
    PatchCall(0x6A7CD5, (void*)Hook_EffectFire, 0x6F4430);
    {   // CDActionDrive::Update only starts camera shakes for views 1 and 2: also 5 and 6
        const uint8_t e[9] = { 0x83, 0xF8, 0x02, 0x0F, 0x85, 0x2B, 0x01, 0x00, 0x00 };
        if (!memcmp((void*)0x473077, e, 9)) {
            uint8_t* t = MakeStub(40); int n = 0;
            t[n++] = 0x83; t[n++] = 0xF8; t[n++] = 0x02; t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x473080 - ((uintptr_t)t + n + 4)); n += 4;   // je go
            t[n++] = 0x83; t[n++] = 0xF8; t[n++] = 0x05; t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x473080 - ((uintptr_t)t + n + 4)); n += 4;
            t[n++] = 0x83; t[n++] = 0xF8; t[n++] = 0x06; t[n++] = 0x0F; t[n++] = 0x84; *(int32_t*)(t + n) = (int32_t)(0x473080 - ((uintptr_t)t + n + 4)); n += 4;
            EmitJmp(t + n, 0xE9, 0x4731AB); n += 5;                                                                                                              // skip
            uint8_t j[9] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x473077 + 5));
            WriteMem(0x473077, j, 9);
        } else Log("4 players: camera shake view check not found");
    }
    PatchCall(0x473089, (void*)Hook_MaybeCameraShake, 0x64C5F0);
    PatchCall(0x473189, (void*)Hook_MaybeCameraShake, 0x64C5F0);
    PatchCall(0x47DCCD, (void*)Hook_ApplyCameraShake, 0x64C420);
    PatchCall(0x480C1C, (void*)Hook_UpdateCameraShakers, 0x64C2D0);
    {   // EmitterSystem::IsCloseEnough -> ours (jmp at its entry)
        const uint8_t e[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };
        if (!memcmp((void*)0x504000, e, 6)) { uint8_t j[5] = { 0xE9 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)Hook_EmitterIsCloseEnough - (0x504000 + 5)); WriteMem(0x504000, j, 5); }
        else Log("4 players: EmitterSystem::IsCloseEnough not patched");
    }
    PatchCall(0x4C62A6, (void*)Hook_UpdateDoppler, 0x4BF2E0);
    { void* f = (void*)Hook_TunnelUpdate; if (*(uintptr_t*)0x896D98 == 0x4C8E00) WriteMem(0x896D98, &f, 4); else Log("4 players: tunnel sound hook not installed"); }
    PatchCall(0x4B5636, (void*)Hook_TunnelFindZone, 0x727480);
    PatchCall(0x4BEAA6, (void*)Hook_ReverbCapture, 0x74B640);
    PatchCall(0x60A5D6, (void*)Hook_AddSimablePlayer, 0x5E8BF0);
    PatchCall(0x63898B, (void*)Hook_NotifySimableTrigger, 0x5F6A30);
    PatchCall(0x611A34, (void*)Hook_RacerIndexRaceFinished, 0x60DAE0);
    PatchCall(0x61186E, (void*)Hook_RacerIndexLapFinished, 0x60DAE0);
    {   // SFXCTL_3DRearPos::UpdateParams picks listener min(state index, 1): players 3/4 (state 2/3) -> listener 1/2 (index & 1)
        const uint8_t e[3] = { 0x8B, 0x49, 0x10 };
        if (!memcmp((void*)0x4E5AE1, e, 3) && *(uint8_t*)0x4E5AEF == 0xB8) {
            uint8_t* t = MakeStub(64); int n = 0;
            t[n++] = 0x8B; t[n++] = 0x49; t[n++] = 0x10;                                   // mov ecx,[ecx+10]
            t[n++] = 0x80; t[n++] = 0x3D; *(uint32_t*)(t + n) = (uint32_t)(uintptr_t)&g_quad; n += 4; t[n++] = 0;   // cmp byte [g_quad],0
            t[n++] = 0x74; t[n++] = 10;                                                    // je orig
            t[n++] = 0x83; t[n++] = 0xE1; t[n++] = 0x01;                                   // and ecx,1
            t[n++] = 0x89; t[n++] = 0xC8;                                                  // mov eax,ecx
            EmitJmp(t + n, 0xE9, 0x4E5AF4); n += 5;
            // orig: test ecx,ecx; jle 4E5B54; cmp ecx,1; mov eax,ecx; jle 4E5AF4; mov eax,1; jmp 4E5AF4
            t[n++] = 0x85; t[n++] = 0xC9;
            t[n++] = 0x0F; t[n++] = 0x8E; *(int32_t*)(t + n) = (int32_t)(0x4E5B54 - ((uintptr_t)t + n + 4)); n += 4;
            t[n++] = 0x83; t[n++] = 0xF9; t[n++] = 0x01;
            t[n++] = 0x89; t[n++] = 0xC8;
            t[n++] = 0x0F; t[n++] = 0x8E; *(int32_t*)(t + n) = (int32_t)(0x4E5AF4 - ((uintptr_t)t + n + 4)); n += 4;
            t[n++] = 0xB8; *(uint32_t*)(t + n) = 1; n += 4;
            EmitJmp(t + n, 0xE9, 0x4E5AF4); n += 5;
            uint8_t j[5] = { 0xE9 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x4E5AE1 + 5)); WriteMem(0x4E5AE1, j, 5);
        } else Log("4 players: rear sound position fix not installed");
    }
    PatchCall(0x77990C, (void*)Hook_FxAudioCreate, 0x779430);   // continuous effects (scrapes)
    PatchCall(0x78728D, (void*)Hook_FxAudioCreate, 0x779430);   // one-shot effects (crashes, hits)
    {   // WorldEffectConn::Update's audio event update (call [edx+0xC]) -> Hook_FxAudioUpdate
        const uint8_t e[6] = { 0x8B, 0x44, 0x24, 0x1C, 0x8B, 0x4E };
        if (!memcmp((void*)0x779991, e, 6) && !memcmp((void*)0x7799AA, "\xFF\x52\x0C", 3)) {
            uint8_t* t = MakeStub(64); int n = 0;
            memcpy(t + n, (void*)0x779991, 0x7799AA - 0x779991); n += 0x7799AA - 0x779991;   // the same pushes
            t[n++] = 0x51;                                                                       // push ecx (the event)
            EmitJmp(t + n, 0xE8, (uintptr_t)Hook_FxAudioUpdate); n += 5;
            EmitJmp(t + n, 0xE9, 0x7799AD); n += 5;
            uint8_t j[5] = { 0xE9 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)t - (0x779991 + 5)); WriteMem(0x779991, j, 5);
        } else Log("4 players: effect sound update not patched");
    }
    {   // Sound::DistanceToView -> ours (jmp at its entry)
        const uint8_t e[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };
        if (!memcmp((void*)0x4BA050, e, 6)) { uint8_t j[5] = { 0xE9 }; *(int32_t*)(j + 1) = (int32_t)((uintptr_t)Hook_DistanceToView - (0x4BA050 + 5)); WriteMem(0x4BA050, j, 5); }
        else Log("4 players: Sound::DistanceToView not patched");
    }
    { void* f = (void*)Hook_PlayerCarEnterWorld; if (*(uintptr_t*)0x898668 == 0x4F2E90) WriteMem(0x898668, &f, 4); else Log("4 players: player car sound hook not installed"); }
    { const uint8_t e[3] = { 0x83, 0xFF, 0x02 }; if (!memcmp((void*)0x4C214E, e, 3)) { uint8_t b = 4; WriteMem(0x4C2150, &b, 1); } else Log("4 players: AttachPlayerCars limit not found"); }
    { void* f = (void*)Hook_AICarEnterWorld; if (*(uintptr_t*)0x898D2C == 0x4F3000) WriteMem(0x898D2C, &f, 4); else Log("4 players: AI car sound hook not installed"); }
    PatchCall(0x7C2545, (void*)Hook_CarSelStartRace, 0x7A3AF0);
    PatchCall(0x7C2DD8, (void*)Hook_CarSelTitle, 0x525220);
    {   void* f = (void*)Hook_PlayerGetSettings; const uintptr_t vt[2] = { 0x8B0B24, 0x8B0BE4 };   // LocalPlayer::GetSettings
        for (uintptr_t a : vt) if (*(uintptr_t*)a == 0x6E99A0) WriteMem(a, &f, 4); else Log("4 players: player settings hook: unexpected vtable entry at %08X", (unsigned)a); }
    Log("4 player split screen installed");
}

// ---- HUDs for four players
// The split screen HUD packages are HUD_Player1/2.fng (HUD_Drag_Player1/2.fng in drag races). Each
// HUD finds its objects by package name, so players 3 and 4 get packages of their own: HUD_Player3/4
// (and HUD_Drag_Player3/4), loaded from player 1's package data and renamed after loading.
// Every HUD is drawn into its player's quarter: each FE object keeps its size and is anchored to
// the quarter's left, centre or right and top or bottom, like it was in the half-screen layout.
// No minimaps in four player races.
static void* g_hudPlayer = nullptr;          // LocalPlayer::SetHud in progress for this player
static const char* QuadHudName(int k, int type) {   // k = 0/1 for players 3/4
    static const char* n[2][2] = { { "HUD_Player3.fng", "HUD_Drag_Player3.fng" }, { "HUD_Player4.fng", "HUD_Drag_Player4.fng" } };
    return n[k][(type == 5 || type == 6) ? 1 : 0];
}
static const char* __cdecl Hook_HudFengName(int type) {
    const char* name = ((const char* (*)(int))0x5694E0)(type);
    for (int k = 0; k < 2; k++)
        if (g_quad && g_hudPlayer && g_hudPlayer == g_quadPl[k] && type >= 3 && type <= 6) {
            const char* q = QuadHudName(k, type);
            Log("4P test: player %d HUD type %d -> %s", k + 3, type, q);
            return q;
        }
    return name;
}
static void FASTCALL Hook_SetHud(void* pl, void*, int type) {
    g_hudPlayer = pl;
    ((void(THISCALL*)(void*, int))0x6E99C0)(pl, type);
    g_hudPlayer = nullptr;
}
static const char* QuadAlias(const char* name) {
    if (!name) return name;
    if (!strcmp(name, "HUD_Player3.fng") || !strcmp(name, "HUD_Player4.fng")) return "HUD_Player1.fng";
    if (!strcmp(name, "HUD_Drag_Player3.fng") || !strcmp(name, "HUD_Drag_Player4.fng")) return "HUD_Drag_Player1.fng";
    return name;
}
typedef void* (THISCALL *GetPkgDataFn)(void* gi, const char* name, void* out, void* flag);
static void* FASTCALL Hook_GetPackageData(void* gi, void*, const char* name, void* out, void* flag) {
    return ((GetPkgDataFn)0x516840)(gi, QuadAlias(name), out, flag);
}
// the HUD packages, and the one each name has now: after a restart players 3 and 4 get new HUDs
// while the old packages can still be on the list (drawn in the half screen layout with old values)
static const char* const kHudPkgNames[8] = { "HUD_Player1.fng", "HUD_Player2.fng", "HUD_Player3.fng", "HUD_Player4.fng",
                                             "HUD_Drag_Player1.fng", "HUD_Drag_Player2.fng", "HUD_Drag_Player3.fng", "HUD_Drag_Player4.fng" };
static void* g_hudCur[8];
static int HudPkgIndex(const char* name) {
    if (!name || IsBadReadPtr(name, 8)) return -1;
    for (int i = 0; i < 8; i++) if (!strcmp(name, kHudPkgNames[i])) return i;
    return -1;
}
typedef void* (THISCALL *PushPkgFn)(void* eng, const char* name, unsigned prio, unsigned param);
static void* FASTCALL Hook_FEPushPackage(void* eng, void*, const char* name, unsigned prio, unsigned param) {
    void* pkg = ((PushPkgFn)0x5C4DF0)(eng, name, prio, param);
    { int hi = HudPkgIndex(name); if (hi >= 0 && pkg) g_hudCur[hi] = pkg; }
    if (pkg && name && QuadAlias(name) != name) {
        ((bool(THISCALL*)(void*, const char*))0x5B59C0)(pkg, name);       // FENode::SetName
        Log("4P test: HUD package %s (%p)", name, pkg);
    }
    return pkg;
}
typedef uint64_t (THISCALL *HudFeatFn)(void* hud, void* pl);
// players 3 and 4: DetermineHudFeatures turns everything off for them (it checks things only
// players 1 and 2 have), so their HUDs show what player 2's HUD shows
static uint64_t g_p2HudFeat = 0;
static uint64_t FASTCALL Hook_HudFeatures(void* hud, void*, void* pl) {
    uint64_t f = ((HudFeatFn)0x57CA60)(hud, pl);
    if (!g_quad) return f;
    int n; void** lp = LocalPlayers(&n);
    if (lp && n >= 2 && pl == lp[1]) g_p2HudFeat = f;
    else if (pl && (pl == g_quadPl[0] || pl == g_quadPl[1])) {
        static uint64_t seen[2] = { ~0ull, ~0ull };
        int k = pl == g_quadPl[0] ? 0 : 1;
        if (f != seen[k]) { Log("4P test: player %d HUD features %08X%08X (player 2 %08X%08X)", k + 3, (unsigned)(f >> 32), (unsigned)f, (unsigned)(g_p2HudFeat >> 32), (unsigned)g_p2HudFeat); seen[k] = f; }
        if (!f) f = g_p2HudFeat;
    }
    return f & ~(uint64_t)0x10000;                                         // minimap off
}

// which quarter a package belongs to, and whether it has the bottom half layout (HUD_Player2)
static void* g_curPkg = nullptr;

static void QuadFindHudPackages() {
    g_quadPkgN = 0;
    void* fe = *(void**)0x91CADC;
    for (int i = 0; i < 8; i++) {
        void* p = ((void*(THISCALL*)(void*, const char*))0x516AF0)(fe, kHudPkgNames[i]);   // cFEng::FindPackage
        if (!p) continue;
        if (g_hudCur[i]) p = g_hudCur[i];                                                 // the newest one of that name
        g_quadPkg[g_quadPkgN] = p; g_quadPkgQ[g_quadPkgN] = i & 3; g_quadPkgY[g_quadPkgN] = (i & 3) == 1 ? 240.f : 0.f;
        g_quadPkgN++;
    }
}
// ~FEngHud unloads its package with PopNoControlPackage(name), which asks the package manager -
// that knows packages by their data's name: HUD_Player3/4 aren't in it, and HUD_Player1's entry is
// whichever package was loaded last from player 1's data (player 3 or 4's). So after a race, player 3's
// and player 1's HUDs stayed, and a restart drew them over the new ones. Unload the package of that name.
static void FASTCALL Hook_HudPopPackage(void* cfeng, void*, const char* name) {
    int hi = HudPkgIndex(name);
    // only four player races have the aliased packages; otherwise the game's own unload, untouched
    // (unloading by node name in 2 player races could pick the wrong one of two same-named packages)
    if (hi < 0 || !g_quad) { ((void(THISCALL*)(void*, const char*))0x5169B0)(cfeng, name); return; }
    void* pkg = ((void*(THISCALL*)(void*, const char*))0x516AF0)(cfeng, name);              // cFEng::FindPackage (by name)
    if (pkg) ((bool(THISCALL*)(void*, void*))0x5BE6A0)(*(void**)cfeng, pkg);                // FEngine::UnloadPackage
    if (cfg.debug) Log("4P HUD: %s unloaded (%p)", name, pkg);
    if (g_hudCur[hi] == pkg) g_hudCur[hi] = nullptr;
}
typedef void (THISCALL *PkgRenderFn)(void* gi, void* pkg);
static void FASTCALL Hook_BeginPkgRender(void* gi, void*, void* pkg) {
    g_curPkg = pkg;
    g_hudQOn = false;
    if (g_quadRender)
        for (int i = 0; i < g_quadPkgN; i++) if (g_quadPkg[i] == pkg) { g_hudQ = { g_quadPkgQ[i], g_quadPkgY[i] }; g_hudQOn = true; }
    if (g_quadRender && cfg.debug && pkg) {   // diagnostics: every package drawn in a four player race, once
        bool seen = false;
        for (int i = 0; i < g_hudSeenN; i++) if (g_hudSeen[i] == pkg) seen = true;
        if (!seen && g_hudSeenN < 64) {
            g_hudSeen[g_hudSeenN++] = pkg;
            const char* nm = At<const char*>((uint8_t*)pkg, 0xC);
            char b[40] = "?"; if (nm && !IsBadReadPtr(nm, 32)) { memcpy(b, nm, 39); b[39] = 0; for (char* c = b; *c; c++) if (*c < 32 || *c > 126) { *c = 0; break; } }
            Log("4P HUD: drawing package %p %s, quarter %d", pkg, b, g_hudQOn ? g_hudQ.q : -1);
        }
    }
    if (g_quadDiag && g_quadDiagLines++ < 200) Log("4P HUD diag: render package %p (HUD quarter %d)", pkg, g_hudQOn ? g_hudQ.q : -1);
    ((PkgRenderFn)0x5166A0)(gi, pkg);
    if (g_quadRender && pkg) {   // an old HUD package left over from before a restart: not drawn
        int hi = HudPkgIndex(At<const char*>((uint8_t*)pkg, 0xC));
        if (hi >= 0 && g_hudCur[hi] && g_hudCur[hi] != pkg) {
            static void* logged[8]; static int nl = 0; bool was = false;
            for (int i = 0; i < nl; i++) if (logged[i] == pkg) was = true;
            if (!was) { logged[nl++ & 7] = pkg; if (nl > 8) nl = 8; Log("4P HUD: old package %p %s hidden (current %p)", pkg, kHudPkgNames[hi], g_hudCur[hi]); }
            At<uint8_t>((uint8_t*)gi, 4) = 0; g_hudQOn = false;
            return;
        }
    }
    // BeginPackageRendering asks the package manager whether the package is visible, by name; it
    // has no entry for HUD_Player3/4 (they use player 1's package data), so ask with that name
    const char* name = pkg ? At<const char*>((uint8_t*)pkg, 0xC) : nullptr;
    const char* alias = QuadAlias(name);
    if (alias != name) {
        void* mgr = ((void*(*)())0x572070)();                                        // FEPackageManager::Get
        bool vis = mgr && ((bool(THISCALL*)(void*, const char*))0x591E30)(mgr, alias);  // GetVisibility
        if (vis && !*(int*)0x9257F8) At<uint8_t>((uint8_t*)gi, 4) = 1;
    }
}
static void FASTCALL Hook_EndPkgRender(void* gi, void*, void* pkg) {
    ((PkgRenderFn)0x516710)(gi, pkg);
    g_curPkg = nullptr; g_hudQOn = false;
}
// per FE object: anchors chosen from its first polygon
static const float kHudScale = 0.9f;
static void XformHudPoly(float* p, float* save) {
    // (g_hudQ, g_hudAx/Ay are declared with the poly hooks in main.cpp)
    // FE space is 480 high and 640 wide, or wider with the widescreen fix (853 at 16:9)
    static float feW = 640.f;
    for (int v = 0; v < 4; v++) if (p[v * 4] > 660.f && feW == 640.f && g_texH > 0) { feW = 480.f * g_texW / g_texH; Log("4P test: FE width %.0f", feW); }
    if (g_hudAx < 0) {
        float cx = (p[0] + p[4] + p[8] + p[12]) * .25f, cy = (p[1] + p[5] + p[9] + p[13]) * .25f - g_hudQ.hy0;
        g_hudAx = cx < feW / 3 ? 0 : cx > feW * 2 / 3 ? 2 : 1;
        g_hudAy = cy > 120.f ? 1 : 0;
        if (g_quadDiag && g_quadDiagLines++ < 200 && g_diagObj)
            Log("4P HUD diag:   obj %p type %d hash %08X fn %d q %d centre %.0f,%.0f -> anchor %d,%d  poly %.0f,%.0f %.0f,%.0f %.0f,%.0f %.0f,%.0f",
                g_diagObj, At<int>((uint8_t*)g_diagObj, 0x18), At<uint32_t>((uint8_t*)g_diagObj, 0x14), g_diagFn, g_hudQ.q, cx, cy + g_hudQ.hy0, g_hudAx, g_hudAy,
                p[0], p[1], p[4], p[5], p[8], p[9], p[12], p[13]);
    }
    float hw = feW / 2, qx0 = (g_hudQ.q & 1) ? hw : 0.f, qy0 = (g_hudQ.q & 2) ? 240.f : 0.f, s = kHudScale;
    for (int v = 0; v < 4; v++) {
        float& x = p[v * 4], &y = p[v * 4 + 1];
        save[v * 2] = x; save[v * 2 + 1] = y;
        float ly = y - g_hudQ.hy0;
        x = g_hudAx == 0 ? qx0 + x * s : g_hudAx == 2 ? qx0 + hw - (feW - x) * s : qx0 + hw / 2 + (x - hw) * s;
        y = g_hudAy == 0 ? qy0 + ly * s : qy0 + 240.f - (240.f - ly) * s;
    }
}

static void InstallQuadHud() {
    void* f;
    f = (void*)Hook_SetHud;
    if (*(uintptr_t*)0x8B0BF4 == 0x6E99C0) WriteMem(0x8B0BF4, &f, 4);
    if (*(uintptr_t*)0x8B0B34 == 0x6E99C0) WriteMem(0x8B0B34, &f, 4);
    PatchCall(0x6E9A13, (void*)Hook_HudFengName, 0x5694E0);
    f = (void*)Hook_GetPackageData; if (*(uintptr_t*)0x89B438 == 0x516840) WriteMem(0x89B438, &f, 4);
    f = (void*)Hook_BeginPkgRender; if (*(uintptr_t*)0x89B458 == 0x5166A0) WriteMem(0x89B458, &f, 4);
    f = (void*)Hook_EndPkgRender;   if (*(uintptr_t*)0x89B45C == 0x516710) WriteMem(0x89B45C, &f, 4);
    PatchCall(0x51699E, (void*)Hook_FEPushPackage, 0x5C4DF0);
    PatchCall(0x5A0B9D, (void*)Hook_HudPopPackage, 0x5169B0);
    PatchCall(0x58CA50, (void*)Hook_HudFeatures, 0x57CA60);
    if (!cfg.separateMinimaps) {          // the FE polygon hooks normally come with the separate minimaps
        HookCall(0x6BE70F, (void*)Hook_PolyRender3, 0x6D1260);
        HookCall(0x6BE734, (void*)Hook_PolyRender4, 0x6D1930);
        void* g = (void*)Hook_GIRenderObject; if (*(uintptr_t*)0x89B46C == 0x516630) WriteMem(0x89B46C, &g, 4);
    }
}
