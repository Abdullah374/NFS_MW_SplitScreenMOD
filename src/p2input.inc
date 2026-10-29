// ---------------------------------------------------------------- split screen player input (both players)
// included from main.cpp

enum PadInput {
    PI_NONE = 0, PI_A, PI_B, PI_X, PI_Y, PI_LB, PI_RB, PI_BACK, PI_START, PI_LS, PI_RS,
    PI_DUP, PI_DDOWN, PI_DLEFT, PI_DRIGHT, PI_LT, PI_RT,
    PI_LSL, PI_LSR, PI_LSU, PI_LSD, PI_RSL, PI_RSR, PI_RSU, PI_RSD, PI_COUNT
};
static const char* const kPadIds[PI_COUNT] = {
    "NONE", "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS",
    "DUP", "DDOWN", "DLEFT", "DRIGHT", "LT", "RT",
    "LSLEFT", "LSRIGHT", "LSUP", "LSDOWN", "RSLEFT", "RSRIGHT", "RSUP", "RSDOWN" };
static const char* const kPadNames[PI_COUNT] = {
    "-", "A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Left Stick Click", "Right Stick Click",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "LT", "RT",
    "Left Stick Left", "Left Stick Right", "Left Stick Up", "Left Stick Down",
    "Right Stick Left", "Right Stick Right", "Right Stick Up", "Right Stick Down" };
static const WORD kPadButtonMask[PI_DRIGHT + 1] = {
    0, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT };

struct BindAction { const char* name; const char* id; int channel; int defPad; int defKey[2]; bool steer; };
static const BindAction kActions[] = {
    // name               id              ch  pad        P1 key        P2 key
    { "Accelerate",      "Gas",          0,  PI_RT,    { VK_UP,      'W' },    false },
    { "Brake / Reverse", "Brake",        1,  PI_LT,    { VK_DOWN,    'S' },    false },
    { "Steer Left",      "SteerLeft",    2,  PI_LSL,   { VK_LEFT,    'A' },    true  },
    { "Steer Right",     "SteerRight",   3,  PI_LSR,   { VK_RIGHT,   'D' },    true  },
    { "Handbrake",       "Handbrake",    4,  PI_A,     { VK_SPACE,   'F' },    false },
    { "Nitrous",         "Nitrous",      6,  PI_B,     { VK_LMENU,   'Q' },    false },
    { "Speedbreaker",    "Speedbreaker", 5,  PI_X,     { VK_RCONTROL,'E' },    false },
    { "Shift Up",        "ShiftUp",      8,  PI_RB,    { VK_LSHIFT,  'X' },    false },
    { "Shift Down",      "ShiftDown",    7,  PI_LB,    { VK_LCONTROL,'Z' },    false },
    { "Change Camera",   "Camera",       15, PI_Y,     { 'C',        'V' },    false },
    { "Look Back",       "LookBack",     16, PI_RS,    { 'L',        VK_TAB }, false },
    { "Reset Car",       "Reset",        9,  PI_BACK,  { 'R',        'G' },    false },
    { "Pause",           "Pause",        18, PI_START, { VK_ESCAPE,  0 },      false },
};
static const int NUM_BIND = sizeof(kActions) / sizeof(kActions[0]);
static int g_padBind[2][NUM_BIND];
static int g_keyBind[2][NUM_BIND];
static int g_dev[2] = { -1, 1 };          // per player: -1 keyboard, 0..3 XInput controller
static int g_gamePad = -1;                // XInput pad that the game's own input (port 0) also reads

static float PadValue(const XINPUT_GAMEPAD& p, int code) {
    if (code <= PI_NONE || code >= PI_COUNT) return 0.f;
    if (code <= PI_DRIGHT) return (p.wButtons & kPadButtonMask[code]) ? 1.f : 0.f;
    int th = cfg.triggerThreshold, dz = cfg.stickDeadzone;
    switch (code) {
        case PI_LT: return p.bLeftTrigger > th ? (p.bLeftTrigger - th) / (255.f - th) : 0.f;
        case PI_RT: return p.bRightTrigger > th ? (p.bRightTrigger - th) / (255.f - th) : 0.f;
        case PI_LSL: return Axis(p.sThumbLX, dz, -1);
        case PI_LSR: return Axis(p.sThumbLX, dz, 1);
        case PI_LSU: return Axis(p.sThumbLY, dz, 1);
        case PI_LSD: return Axis(p.sThumbLY, dz, -1);
        case PI_RSL: return Axis(p.sThumbRX, dz, -1);
        case PI_RSR: return Axis(p.sThumbRX, dz, 1);
        case PI_RSU: return Axis(p.sThumbRY, dz, 1);
        case PI_RSD: return Axis(p.sThumbRY, dz, -1);
    }
    return 0.f;
}

static bool GameHasFocus() {
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}
static bool KeyDown(int vk) { return vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0; }

static const char* KeyName(int vk, char* buf, int n) {
    if (vk <= 0) { snprintf(buf, n, "-"); return buf; }
    switch (vk) {
        case VK_LEFT: return "Left Arrow"; case VK_RIGHT: return "Right Arrow";
        case VK_UP: return "Up Arrow"; case VK_DOWN: return "Down Arrow";
        case VK_PRIOR: return "Page Up"; case VK_NEXT: return "Page Down";
        case VK_INSERT: return "Insert"; case VK_DELETE: return "Delete";
        case VK_HOME: return "Home"; case VK_END: return "End";
        case VK_LCONTROL: return "Left Ctrl"; case VK_RCONTROL: return "Right Ctrl";
        case VK_LSHIFT: return "Left Shift"; case VK_RSHIFT: return "Right Shift";
        case VK_LMENU: return "Left Alt"; case VK_RMENU: return "Right Alt";
    }
    UINT sc = MapVirtualKeyA(vk, 0 /*MAPVK_VK_TO_VSC*/);
    if (sc && GetKeyNameTextA((LONG)(sc << 16), buf, n) > 0) return buf;
    snprintf(buf, n, "Key 0x%02X", vk);
    return buf;
}

static void ResetBindings(int pl) {
    for (int i = 0; i < NUM_BIND; i++) { g_padBind[pl][i] = kActions[i].defPad; g_keyBind[pl][i] = kActions[i].defKey[pl]; }
}

static const char* const kSecDev[2]  = { "Player1", "Player2" };
static const char* const kSecPad[2]  = { "Player1Controller", "Player2Controller" };
static const char* const kSecKey[2]  = { "Player1Keyboard", "Player2Keyboard" };

static void LoadBindings() {
    char b[64];
    for (int pl = 0; pl < 2; pl++) {
        ResetBindings(pl);
        g_dev[pl] = GetPrivateProfileIntA(kSecDev[pl], "Device", pl == 0 ? -1 : cfg.p2pad, g_ctlIni);
        if (g_dev[pl] < -1 || g_dev[pl] > 3) g_dev[pl] = pl == 0 ? -1 : 1;
        for (int i = 0; i < NUM_BIND; i++) {
            GetPrivateProfileStringA(kSecPad[pl], kActions[i].id, "", b, sizeof b, g_ctlIni);
            if (b[0]) { for (int c = 0; c < PI_COUNT; c++) if (!_stricmp(b, kPadIds[c])) { g_padBind[pl][i] = c; break; } }
            GetPrivateProfileStringA(kSecKey[pl], kActions[i].id, "", b, sizeof b, g_ctlIni);
            if (b[0]) g_keyBind[pl][i] = (int)strtol(b, nullptr, 0);
        }
    }
    cfg.p2pad = g_dev[1];
    Log("Controls loaded: P1 device %d, P2 device %d", g_dev[0], g_dev[1]);
}
static void SaveBindings() {
    char b[32];
    for (int pl = 0; pl < 2; pl++) {
        snprintf(b, sizeof b, "%d", g_dev[pl]);
        WritePrivateProfileStringA(kSecDev[pl], "Device", b, g_ctlIni);
        for (int i = 0; i < NUM_BIND; i++) {
            WritePrivateProfileStringA(kSecPad[pl], kActions[i].id, kPadIds[g_padBind[pl][i]], g_ctlIni);
            snprintf(b, sizeof b, "0x%02X", g_keyBind[pl][i]);
            WritePrivateProfileStringA(kSecKey[pl], kActions[i].id, b, g_ctlIni);
        }
    }
    Log("Controls saved (P1 device %d, P2 device %d)", g_dev[0], g_dev[1]);
}

static bool g_inputBlocked = false;     // set while one of our menus is open
static float g_kbSteer[2][2];           // smoothed keyboard steering per player (left, right)
static DWORD g_lastPollTick[2];
static int g_lastPadState[2] = { -1, -1 };

static void FASTCALL Dev_Poll(P2Device* self, void*) {
    int pl = self->player;
    memcpy(self->prev, self->cur, sizeof(float) * G::NUM_GAME_ACTIONS);
    memset(self->cur, 0, sizeof(float) * G::NUM_GAME_ACTIONS);
    DWORD now = GetTickCount();
    float dt = g_lastPollTick[pl] ? (now - g_lastPollTick[pl]) / 1000.f : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    g_lastPollTick[pl] = now;
    if (g_inputBlocked) return;
    float* c = self->cur;
    int dev = g_dev[pl];
    auto mx = [](float a, float b) { return a > b ? a : b; };

    if (dev < 0) {                                        // keyboard
        if (!GameHasFocus()) { g_kbSteer[pl][0] = g_kbSteer[pl][1] = 0; return; }
        for (int i = 0; i < NUM_BIND; i++) {
            float v = KeyDown(g_keyBind[pl][i]) ? 1.f : 0.f;
            if (kActions[i].steer) {            // ramp keyboard steering so it isn't twitchy
                float& s = g_kbSteer[pl][kActions[i].channel - 2];
                float rate = v > s ? 5.f : 8.f;
                s += (v - s) * (rate * dt > 1.f ? 1.f : rate * dt);
                if (v == 0 && s < 0.02f) s = 0;
                v = s;
            }
            c[kActions[i].channel] = mx(c[kActions[i].channel], v);
        }
        c[23] = KeyDown(VK_RETURN) ? 1.f : 0.f;           // skip NIS
        return;
    }

    if (!pXInputGetState) return;
    XINPUT_STATE st; memset(&st, 0, sizeof st);
    DWORD r = pXInputGetState(dev, &st);
    int connected = (r == ERROR_SUCCESS);
    if (connected != g_lastPadState[pl]) { Log("Player %d controller (XInput %d) %s", pl + 1, dev, connected ? "connected" : "NOT connected"); g_lastPadState[pl] = connected; }
    if (pl == 0 && GameHasFocus() && KeyDown(VK_ESCAPE)) c[18] = 1.f;   // keyboard can always pause
    if (!connected) return;
    const XINPUT_GAMEPAD& p = st.Gamepad;
    for (int i = 0; i < NUM_BIND; i++) c[kActions[i].channel] = mx(c[kActions[i].channel], PadValue(p, g_padBind[pl][i]));
    auto B = [&](WORD m) { return (p.wButtons & m) ? 1.f : 0.f; };
    c[10] = B(XINPUT_GAMEPAD_DPAD_UP);
    c[11] = B(XINPUT_GAMEPAD_DPAD_DOWN);
    c[12] = B(XINPUT_GAMEPAD_DPAD_LEFT);
    c[13] = B(XINPUT_GAMEPAD_DPAD_RIGHT);
    c[23] = B(XINPUT_GAMEPAD_A);
    // menu actions (skipped for the pad the game already reads itself, to avoid double moves)
    if (dev != g_gamePad) {
        float sl = Axis(p.sThumbLX, cfg.stickDeadzone, -1), sr = Axis(p.sThumbLX, cfg.stickDeadzone, 1);
        float su = Axis(p.sThumbLY, cfg.stickDeadzone, 1), sd = Axis(p.sThumbLY, cfg.stickDeadzone, -1);
        c[19] = mx(B(XINPUT_GAMEPAD_DPAD_LEFT), sl > 0.5f ? 1.f : 0.f);
        c[20] = mx(B(XINPUT_GAMEPAD_DPAD_RIGHT), sr > 0.5f ? 1.f : 0.f);
        c[21] = mx(B(XINPUT_GAMEPAD_DPAD_UP), su > 0.5f ? 1.f : 0.f);
        c[22] = mx(B(XINPUT_GAMEPAD_DPAD_DOWN), sd > 0.5f ? 1.f : 0.f);
        c[32] = B(XINPUT_GAMEPAD_A);
        c[33] = B(XINPUT_GAMEPAD_B);
        c[34] = B(XINPUT_GAMEPAD_START);
    }
}
