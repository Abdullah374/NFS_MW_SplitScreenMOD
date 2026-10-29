// ---------------------------------------------------------------- mod menus (drawn with D3D)
// included from main.cpp after the rendering helpers

// ---- font atlas built with GDI (managed texture, survives device resets)
static IDirect3DTexture9* g_fontTex = nullptr;
static const int FONT_CELL_W = 32, FONT_CELL_H = 40, FONT_COLS = 16, FONT_TEX_W = 512, FONT_TEX_H = 256;
static float g_glyphW[128];

static bool BuildFont() {
    if (g_fontTex) return true;
    IDirect3DDevice9* d = Dev();
    if (!d) return false;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = FONT_TEX_W; bi.bmiHeader.biHeight = -FONT_TEX_H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldb = SelectObject(dc, bmp);
    HFONT font = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, "Arial");
    HGDIOBJ oldf = SelectObject(dc, font);
    memset(bits, 0, FONT_TEX_W * FONT_TEX_H * 4);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
    for (int ch = 32; ch < 127; ch++) {
        int i = ch - 32, cx = (i % FONT_COLS) * FONT_CELL_W, cy = (i / FONT_COLS) * FONT_CELL_H;
        char s[2] = { (char)ch, 0 };
        SIZE sz; GetTextExtentPoint32A(dc, s, 1, &sz);
        g_glyphW[ch] = (float)sz.cx;
        TextOutA(dc, cx + 1, cy + 2, s, 1);
    }
    GdiFlush();
    HRESULT hr = d->CreateTexture(FONT_TEX_W, FONT_TEX_H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_fontTex, nullptr);
    if (SUCCEEDED(hr)) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(g_fontTex->LockRect(0, &lr, nullptr, 0))) {
            for (int y = 0; y < FONT_TEX_H; y++) {
                uint32_t* src = (uint32_t*)bits + y * FONT_TEX_W;
                uint32_t* dst = (uint32_t*)((uint8_t*)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < FONT_TEX_W; x++) { uint32_t a = src[x] & 0xFF; dst[x] = (a << 24) | 0xFFFFFF; }
            }
            g_fontTex->UnlockRect(0);
        }
    } else Log("font texture failed %08lX", hr);
    SelectObject(dc, oldf); DeleteObject(font);
    SelectObject(dc, oldb); DeleteObject(bmp); DeleteDC(dc);
    return g_fontTex != nullptr;
}

// ---- 2D drawing (screen pixels)
struct UIV { float x, y, z, rhw; DWORD c; float u, v; };
static float g_uiW, g_uiH, g_uiS;      // screen size, scale (1.0 at 1080p)
static IDirect3DStateBlock9* g_uiSB = nullptr;

static void UIBegin() {
    IDirect3DDevice9* d = Dev();
    if (!g_uiSB) d->CreateStateBlock(D3DSBT_ALL, &g_uiSB);
    if (g_uiSB) g_uiSB->Capture();
    IDirect3DSurface9* rt = nullptr; d->GetRenderTarget(0, &rt);
    D3DSURFACE_DESC desc; rt->GetDesc(&desc); rt->Release();
    g_uiW = (float)desc.Width; g_uiH = (float)desc.Height; g_uiS = g_uiH / 1080.f;
    D3DVIEWPORT9 vp = { 0, 0, desc.Width, desc.Height, 0.f, 1.f };
    d->SetViewport(&vp);
    d->SetVertexShader(nullptr); d->SetPixelShader(nullptr);
    d->SetRenderState(D3DRS_ZENABLE, FALSE);
    d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    d->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    d->SetRenderState(D3DRS_FOGENABLE, FALSE);
    d->SetRenderState(D3DRS_LIGHTING, FALSE);
    d->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    d->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
}
static void UIEnd() { if (g_uiSB) g_uiSB->Apply(); }

static void UIRect(float x, float y, float w, float h, DWORD c) {
    IDirect3DDevice9* d = Dev();
    d->SetTexture(0, nullptr);
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    x -= .5f; y -= .5f;
    UIV q[4] = { {x,y,0,1,c,0,0}, {x+w,y,0,1,c,0,0}, {x,y+h,0,1,c,0,0}, {x+w,y+h,0,1,c,0,0} };
    d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(UIV));
    d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
}

static float UITextWidth(const char* s, float size) {
    float w = 0, k = size / 30.f;
    for (; *s; s++) { unsigned ch = (unsigned char)*s; if (ch >= 32 && ch < 127) w += g_glyphW[ch] * k; }
    return w;
}
// align: 0 left, 1 centre, 2 right
static void UIText(float x, float y, float size, DWORD c, const char* s, int align = 0) {
    if (!g_fontTex) return;
    IDirect3DDevice9* d = Dev();
    float k = size / 30.f;
    if (align) { float w = UITextWidth(s, size); x -= align == 1 ? w / 2 : w; }
    d->SetTexture(0, g_fontTex);
    UIV v[6 * 64]; int n = 0;
    auto flush = [&]() { if (n) d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, n / 3, v, sizeof(UIV)); n = 0; };
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) continue;
        int i = ch - 32;
        float u0 = (float)((i % FONT_COLS) * FONT_CELL_W) / FONT_TEX_W, v0 = (float)((i / FONT_COLS) * FONT_CELL_H) / FONT_TEX_H;
        float u1 = u0 + (g_glyphW[ch] + 2) / FONT_TEX_W, v1 = v0 + (float)FONT_CELL_H / FONT_TEX_H;
        float x0 = x - .5f, y0 = y - .5f, x1 = x0 + (g_glyphW[ch] + 2) * k, y1 = y0 + FONT_CELL_H * k;
        UIV a = {x0,y0,0,1,c,u0,v0}, b = {x1,y0,0,1,c,u1,v0}, cc = {x0,y1,0,1,c,u0,v1}, dd = {x1,y1,0,1,c,u1,v1};
        v[n++] = a; v[n++] = b; v[n++] = cc; v[n++] = b; v[n++] = dd; v[n++] = cc;
        if (n >= 6 * 64) flush();
        x += g_glyphW[ch] * k;
    }
    flush();
}

static float* GameDeviceValues();
// ---- menu input (keyboard + every XInput pad, edge triggered)
struct MenuInput { bool up, down, left, right, accept, back, clear; int padAccept; bool kbAccept; };
static bool g_keyPrev[256];
static WORD g_padPrev[4]; static int g_padAxisPrev[4];
static XINPUT_STATE g_padNow[4]; static bool g_padConn[4];

static void PollPads() {
    for (int i = 0; i < 4; i++) {
        memset(&g_padNow[i], 0, sizeof(XINPUT_STATE));
        g_padConn[i] = pXInputGetState && pXInputGetState(i, &g_padNow[i]) == ERROR_SUCCESS;
    }
}
static bool KeyPressed(int vk) {
    bool down = GameHasFocus() && KeyDown(vk);
    bool p = down && !g_keyPrev[vk];
    g_keyPrev[vk] = down;
    return p;
}
static MenuInput ReadMenuInput() {
    MenuInput m = {}; m.padAccept = -1;
    m.up = KeyPressed(VK_UP); m.down = KeyPressed(VK_DOWN);
    m.left = KeyPressed(VK_LEFT); m.right = KeyPressed(VK_RIGHT);
    m.kbAccept = KeyPressed(VK_RETURN); m.accept = m.kbAccept;
    m.back = KeyPressed(VK_ESCAPE);
    m.clear = KeyPressed(VK_DELETE) | KeyPressed(VK_BACK);
    for (int i = 0; i < 4; i++) {
        WORD b = g_padConn[i] ? g_padNow[i].Gamepad.wButtons : 0;
        WORD pr = b & ~g_padPrev[i]; g_padPrev[i] = b;
        int ax = 0;
        if (g_padConn[i]) {
            SHORT ly = g_padNow[i].Gamepad.sThumbLY, lx = g_padNow[i].Gamepad.sThumbLX;
            if (ly > 20000) ax = 1; else if (ly < -20000) ax = 2; else if (lx < -20000) ax = 3; else if (lx > 20000) ax = 4;
        }
        int axp = ax != g_padAxisPrev[i] ? ax : 0; g_padAxisPrev[i] = ax;
        if ((pr & XINPUT_GAMEPAD_DPAD_UP) || axp == 1) m.up = true;
        if ((pr & XINPUT_GAMEPAD_DPAD_DOWN) || axp == 2) m.down = true;
        if ((pr & XINPUT_GAMEPAD_DPAD_LEFT) || axp == 3) m.left = true;
        if ((pr & XINPUT_GAMEPAD_DPAD_RIGHT) || axp == 4) m.right = true;
        if (pr & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START)) { m.accept = true; if (m.padAccept < 0) m.padAccept = i; }
        if (pr & XINPUT_GAMEPAD_B) m.back = true;
        if (pr & XINPUT_GAMEPAD_X) m.clear = true;
    }
    return m;
}
static void SyncInputState() {   // swallow anything currently held so it doesn't trigger next frame
    for (int vk = 1; vk < 256; vk++) g_keyPrev[vk] = GameHasFocus() && KeyDown(vk);
    for (int i = 0; i < 4; i++) g_padPrev[i] = g_padConn[i] ? g_padNow[i].Gamepad.wButtons : 0;
}

static void QueueSwitch(const char* pkg, int arg, uint32_t mask) {
    void* fe = *(void**)0x91CADC;
    if (fe && g_qpsTramp) Hook_QueuePackageSwitch(fe, nullptr, pkg, arg, mask, 0);
}

static const DWORD COL_PANEL = 0xE0101418, COL_TITLE = 0xFFFFB400, COL_TEXT = 0xFFE8E8E8,
                   COL_DIM = 0xFF8A8A8A, COL_SEL = 0xFFFFB400, COL_SELBG = 0x60FFB400, COL_OK = 0xFF63D25A;

// ================================================================ join screen
// The first device that presses joins as Player 1, the next one as Player 2. Each player then
// drives with their own saved controls, whichever controller (or the keyboard) they picked.
static bool g_joinActive = false;
static int g_join[2] = { -2, -2 };          // -2 not joined, -1 keyboard, 0..3 controller
static DWORD g_joinDoneTick = 0;

static float* GameDeviceValues() {
    uint8_t* dev = At<uint8_t*>(G::pIOModule, 8);
    return dev ? At<float*>(dev, 0x20) : nullptr;
}

static void StartJoinScreen() {
    g_joinActive = true; g_join[0] = g_join[1] = -2; g_joinDoneTick = 0;
    PollPads(); SyncInputState();
    Log("Join screen opened");
}

static void JoinScreenCancel() {
    g_joinActive = false;
    uint8_t* db = *G::pFEDatabase;
    bool trackSel = db && (At<uint32_t>(db, G::FEDB_GAMEMODE) & 4) && db[0] == 2 && At<int>(db, 0x18) == 2;
    QueueSwitch(trackSel ? "Track_Select.fng" : "Track_Options.fng", 0, 0);
}

static void JoinDevice(int dev) {
    for (int p = 0; p < 2; p++) if (g_join[p] == dev && dev >= 0) return;   // controller already in
    for (int p = 0; p < 2; p++) if (g_join[p] == -2) { g_join[p] = dev; return; }
}

static void UpdateJoinScreen() {
    PollPads();
    bool esc = KeyPressed(VK_ESCAPE), enter = KeyPressed(VK_RETURN), tab = KeyPressed(VK_TAB);
    DWORD now = GetTickCount();
    if (g_joinDoneTick) {
        if (now - g_joinDoneTick > 700) {
            g_joinActive = false;
            g_dev[0] = g_join[0]; g_dev[1] = g_join[1]; cfg.p2pad = g_dev[1];
            g_lastPadState[0] = g_lastPadState[1] = -1;
            SaveBindings();
            QueueSwitch("Car_Select.fng", 0, 7);
            Log("Players joined: P1 device %d, P2 device %d", g_join[0], g_join[1]);
        }
        return;
    }
    // controllers: A / START joins, B leaves
    for (int i = 0; i < 4; i++) {
        WORD b = g_padConn[i] ? g_padNow[i].Gamepad.wButtons : 0;
        WORD pr = b & ~g_padPrev[i]; g_padPrev[i] = b;
        if (pr & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START)) JoinDevice(i);
        if (pr & XINPUT_GAMEPAD_B) {
            bool was = false;
            for (int p = 0; p < 2; p++) if (g_join[p] == i) { g_join[p] = -2; was = true; }
            if (was && g_join[0] == -2 && g_join[1] != -2) { g_join[0] = g_join[1]; g_join[1] = -2; }
            if (!was && g_join[0] == -2 && g_join[1] == -2) { JoinScreenCancel(); return; }
        }
    }
    // keyboard: ENTER joins; TAB adds a second keyboard player (shared keyboard, different keys)
    if (enter && g_join[0] != -1 && g_join[1] != -1) JoinDevice(-1);
    else if (tab && (g_join[0] == -1 || g_join[1] == -1)) { for (int p = 0; p < 2; p++) if (g_join[p] == -2) { g_join[p] = -1; break; } }
    if (esc) {
        if (g_join[0] == -2 && g_join[1] == -2) { JoinScreenCancel(); return; }
        g_join[0] = g_join[1] = -2;
    }
    if (g_join[0] != -2 && g_join[1] != -2) g_joinDoneTick = now;
}

static void DeviceLabel(int dev, char* b, int n) {
    if (dev == -1) snprintf(b, n, "Keyboard");
    else snprintf(b, n, "Controller %d", dev + 1);
}

static void DrawJoinScreen() {
    float S = g_uiS, W = g_uiW, H = g_uiH;
    float pw = 1100 * S, ph = 560 * S, px = (W - pw) / 2, py = (H - ph) / 2;
    UIRect(0, 0, W, H, 0x90000000);
    UIRect(px, py, pw, ph, COL_PANEL);
    UIRect(px, py, pw, 6 * S, COL_TITLE);
    UIText(W / 2, py + 30 * S, 52 * S, COL_TITLE, "SPLIT SCREEN", 1);
    UIText(W / 2, py + 95 * S, 28 * S, COL_DIM, "Press A on a controller or ENTER on the keyboard to join. First to press is Player 1.", 1);
    char b[96];
    int next = g_join[0] == -2 ? 0 : 1;
    for (int p = 0; p < 2; p++) {
        int st = g_join[p];
        float y = py + (180 + p * 150) * S;
        bool waiting = st == -2 && p == next;
        UIRect(px + 60 * S, y, pw - 120 * S, 120 * S, waiting ? 0x30FFB400 : 0x20FFFFFF);
        snprintf(b, sizeof b, "PLAYER %d", p + 1);
        UIText(px + 90 * S, y + 18 * S, 44 * S, waiting ? COL_SEL : COL_TEXT, b);
        if (st != -2) {
            char dl[32]; DeviceLabel(st, dl, sizeof dl);
            snprintf(b, sizeof b, "READY  -  %s", dl);
            UIText(px + 90 * S, y + 70 * S, 30 * S, COL_OK, b);
        } else if (waiting) {
            UIText(px + 90 * S, y + 70 * S, 28 * S, COL_TEXT,
                   p == 1 && g_join[0] == -1 ? "Press A on a controller, or TAB to share the keyboard"
                                             : "Press A / START on a controller, or ENTER on the keyboard");
        } else UIText(px + 90 * S, y + 70 * S, 28 * S, COL_DIM, "Waiting...");
    }
    UIText(W / 2, py + ph - 55 * S, 26 * S, COL_DIM, g_joinDoneTick ? "Get ready!" : "B = leave    ESC = back", 1);
}

// ================================================================ controls screen (both players)
// Only Player 1 / Player 2 is chosen here; which controller each player uses is decided on the
// join screen. Every action has a controller binding and a keyboard binding (two columns).
static bool g_ctlActive = false;
static int g_ctlRow = 0, g_ctlPlayer = 0, g_ctlCol = 0;      // col 0 = controller, 1 = keyboard
static bool g_ctlListening = false; static DWORD g_ctlListenTick = 0; static bool g_ctlArmed = false;
static char g_ctlReturnPkg[64]; static int g_ctlReturnArg = 1;
static const int CTL_ROWS = NUM_BIND + 3;   // player, actions..., reset, done
static const int CTL_FIRST_ACTION = 1;

static void OpenControlsScreen(const char* returnPkg, int arg) {
    g_ctlActive = true; g_ctlRow = 0; g_ctlPlayer = 0; g_ctlCol = 0; g_ctlListening = false;
    snprintf(g_ctlReturnPkg, sizeof g_ctlReturnPkg, "%s", returnPkg ? returnPkg : "Pause_Main.fng");
    g_ctlReturnArg = arg;
    PollPads(); SyncInputState();
    Log("Controls screen opened (return to %s %d)", g_ctlReturnPkg, arg);
}
static void CloseControlsScreen() {
    g_ctlActive = false;
    SaveBindings();
    QueueSwitch(g_ctlReturnPkg, g_ctlReturnArg, 0);
}

// first input on any connected controller
static int CapturePadInputAny() {
    for (int dev = 0; dev < 4; dev++) {
        if (!g_padConn[dev]) continue;
        const XINPUT_GAMEPAD& p = g_padNow[dev].Gamepad;
        for (int c = PI_A; c <= PI_DRIGHT; c++) if (p.wButtons & kPadButtonMask[c]) return c;
        if (p.bLeftTrigger > 150) return PI_LT;
        if (p.bRightTrigger > 150) return PI_RT;
        int t = 24000;
        if (p.sThumbLX < -t) return PI_LSL; if (p.sThumbLX > t) return PI_LSR;
        if (p.sThumbLY > t) return PI_LSU; if (p.sThumbLY < -t) return PI_LSD;
        if (p.sThumbRX < -t) return PI_RSL; if (p.sThumbRX > t) return PI_RSR;
        if (p.sThumbRY > t) return PI_RSU; if (p.sThumbRY < -t) return PI_RSD;
    }
    return 0;
}
static bool AnyPadInputHeldAny() {
    for (int dev = 0; dev < 4; dev++) {
        if (!g_padConn[dev]) continue;
        const XINPUT_GAMEPAD& p = g_padNow[dev].Gamepad;
        if (p.wButtons || p.bLeftTrigger > 60 || p.bRightTrigger > 60 || abs(p.sThumbLX) > 16000 || abs(p.sThumbLY) > 16000 ||
            abs(p.sThumbRX) > 16000 || abs(p.sThumbRY) > 16000) return true;
    }
    return false;
}

static void UpdateControlsScreen() {
    PollPads();
    int pl = g_ctlPlayer;
    if (g_ctlListening) {
        int a = g_ctlRow - CTL_FIRST_ACTION;
        if (GetTickCount() - g_ctlListenTick < 250) { SyncInputState(); return; }
        if (g_ctlCol == 1) {                                  // keyboard
            for (int vk = 8; vk < 256; vk++) {
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;   // use the left/right variants
                if (KeyPressed(vk)) {
                    if (vk != VK_ESCAPE || a == NUM_BIND - 1) g_keyBind[pl][a] = vk;     // ESC may be bound to Pause
                    g_ctlListening = false; SyncInputState(); return;
                }
            }
        } else {                                              // controller
            if (!g_ctlArmed) { if (!AnyPadInputHeldAny()) g_ctlArmed = true; }
            else { int c = CapturePadInputAny(); if (c) { g_padBind[pl][a] = c; g_ctlListening = false; g_ctlArmed = false; SyncInputState(); return; } }
            if (KeyPressed(VK_ESCAPE)) { g_ctlListening = false; g_ctlArmed = false; SyncInputState(); }
        }
        return;
    }
    MenuInput m = ReadMenuInput();
    if (m.up) g_ctlRow = (g_ctlRow + CTL_ROWS - 1) % CTL_ROWS;
    if (m.down) g_ctlRow = (g_ctlRow + 1) % CTL_ROWS;
    if (m.back) { CloseControlsScreen(); return; }
    if (g_ctlRow == 0) { if (m.left || m.right || m.accept) g_ctlPlayer ^= 1; return; }
    if (g_ctlRow >= CTL_FIRST_ACTION && g_ctlRow < CTL_FIRST_ACTION + NUM_BIND) {
        int a = g_ctlRow - CTL_FIRST_ACTION;
        if (m.left) g_ctlCol = 0;
        if (m.right) g_ctlCol = 1;
        if (m.clear) { if (g_ctlCol == 1) g_keyBind[pl][a] = 0; else g_padBind[pl][a] = PI_NONE; }
        else if (m.accept) { g_ctlListening = true; g_ctlArmed = false; g_ctlListenTick = GetTickCount(); }
    } else if (g_ctlRow == CTL_FIRST_ACTION + NUM_BIND && m.accept) { ResetBindings(pl); }
    else if (g_ctlRow == CTL_FIRST_ACTION + NUM_BIND + 1 && m.accept) { CloseControlsScreen(); }
}

static void DrawControlsScreen() {
    float S = g_uiS, W = g_uiW, H = g_uiH;
    float pw = 1300 * S, ph = 1000 * S, px = (W - pw) / 2, py = (H - ph) / 2;
    int pl = g_ctlPlayer;
    UIRect(0, 0, W, H, 0xA0000000);
    UIRect(px, py, pw, ph, COL_PANEL);
    UIRect(px, py, pw, 6 * S, COL_TITLE);
    UIText(px + 50 * S, py + 24 * S, 50 * S, COL_TITLE, "SPLIT SCREEN CONTROLS");
    char b[96];
    float rowH = 48 * S, y = py + 100 * S, lx = px + 50 * S;
    float c1 = px + 560 * S, c2 = px + 940 * S, cw = 340 * S;        // column centres
    for (int r = 0; r < CTL_ROWS; r++) {
        bool sel = r == g_ctlRow;
        float ts = 30 * S;
        if (r == 0) {
            if (sel) UIRect(px + 30 * S, y - 4 * S, pw - 60 * S, rowH - 6 * S, COL_SELBG);
            snprintf(b, sizeof b, "<  PLAYER %d  >", pl + 1);
            UIText(px + pw / 2, y + 2 * S, 34 * S, sel ? COL_SEL : COL_TITLE, b, 1);
            y += rowH + 10 * S;
            UIText(c1, y, 26 * S, COL_DIM, "CONTROLLER", 1);
            UIText(c2, y, 26 * S, COL_DIM, "KEYBOARD", 1);
            y += 40 * S;
            continue;
        }
        if (r < CTL_FIRST_ACTION + NUM_BIND) {
            int a = r - CTL_FIRST_ACTION;
            if (sel) UIRect((g_ctlCol ? c2 : c1) - cw / 2, y - 4 * S, cw, rowH - 6 * S, COL_SELBG);
            UIText(lx, y + 4 * S, ts, sel ? 0xFFFFFFFF : COL_TEXT, kActions[a].name);
            for (int col = 0; col < 2; col++) {
                const char* val;
                bool here = sel && g_ctlCol == col;
                if (here && g_ctlListening) val = col ? "Press a key..." : "Press a button...";
                else if (col) val = KeyName(g_keyBind[pl][a], b, sizeof b);
                else val = kPadNames[g_padBind[pl][a]];
                UIText(col ? c2 : c1, y + 4 * S, ts, here ? COL_SEL : COL_TEXT, val, 1);
            }
        } else {
            if (r == CTL_FIRST_ACTION + NUM_BIND) y += 14 * S;
            if (sel) UIRect(px + 30 * S, y - 4 * S, pw - 60 * S, rowH - 6 * S, COL_SELBG);
            UIText(lx, y + 4 * S, ts, sel ? COL_SEL : COL_TEXT, r == CTL_FIRST_ACTION + NUM_BIND ? "Reset to defaults" : "Done");
        }
        y += rowH;
    }
    UIText(px + pw / 2, py + ph - 70 * S, 24 * S, COL_DIM,
           "Up/Down select  -  Left/Right column  -  Enter / A rebind  -  Delete / X clear  -  Esc / B save and go back", 1);
    UIText(px + pw / 2, py + ph - 38 * S, 24 * S, COL_DIM,
           "Each player's controller or keyboard is chosen on the join screen before car selection.", 1);
}

// ================================================================ entry point from Hook_FERender
// Which XInput pad does the game's own input (DirectInput, port 0) read? When the game reports a
// menu press in the same moment one pad presses A/START, that's the one. Our devices then leave
// menu actions on that pad to the game, so menus don't move twice.
static void DetectGamePad() {
    static bool prevAcc = true; static WORD prevB[4]; static DWORD padTick[4];
    float* gv = GameDeviceValues();
    bool acc = gv && (gv[32] > .5f || gv[34] > .5f);
    DWORD now = GetTickCount();
    for (int i = 0; i < 4; i++) {
        XINPUT_STATE st; memset(&st, 0, sizeof st);
        WORD b = (pXInputGetState && pXInputGetState(i, &st) == ERROR_SUCCESS) ? st.Gamepad.wButtons : 0;
        if ((b & ~prevB[i]) & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START)) padTick[i] = now;
        prevB[i] = b;
    }
    if (acc && !prevAcc) {
        int found = -1, n = 0;
        for (int i = 0; i < 4; i++) if (padTick[i] && now - padTick[i] < 150) { found = i; n++; }
        if (n == 1 && found != g_gamePad) { g_gamePad = found; Log("Game's own controller is XInput pad %d", found); }
    }
    prevAcc = acc;
}

// "PLAYER 1/2 - CHOOSE YOUR CAR" banner on the split screen car select screens
static bool CarSelectBannerVisible() {
    uint8_t* db = *G::pFEDatabase;
    bool splitFE = db && (At<uint32_t>(db, G::FEDB_GAMEMODE) & 4) && db[0] == 2;
    return cfg.enabled && splitFE && *G::pGameFlowState == 3 && !_stricmp(g_lastPkg, "Car_Select.fng") &&
           (g_carSelectPlayer == 0 || g_carSelectPlayer == 1);
}
static void DrawCarSelectBanner() {
    float S = g_uiS, W = g_uiW;
    int pl = g_carSelectPlayer;
    DWORD accent = pl == 0 ? 0xFFFFB400 : 0xFF3FA9FF;
    char b[64], dl[32];
    snprintf(b, sizeof b, "PLAYER %d  -  CHOOSE YOUR CAR", pl + 1);
    float tw = UITextWidth(b, 44 * S), bw = tw + 120 * S, bh = 100 * S, bx = (W - bw) / 2, by = 18 * S;
    UIRect(bx, by, bw, bh, 0xD0101418);
    UIRect(bx, by, bw, 5 * S, accent);
    UIText(W / 2, by + 12 * S, 44 * S, accent, b, 1);
    int dev = g_dev[pl];
    if (dev == -1) snprintf(dl, sizeof dl, "Keyboard"); else snprintf(dl, sizeof dl, "Controller %d", dev + 1);
    UIText(W / 2, by + 62 * S, 24 * S, COL_DIM, dl, 1);
}

static void UpdateAndDrawMenus() {
    DetectGamePad();
    if (!g_joinActive && !g_ctlActive && CarSelectBannerVisible() && BuildFont()) {
        UIBegin(); DrawCarSelectBanner(); UIEnd();
    }
    bool any = g_joinActive || g_ctlActive;
    g_inputBlocked = any;
    if (!any) return;
    if (!BuildFont()) return;
    if (g_joinActive) UpdateJoinScreen();
    if (g_ctlActive) UpdateControlsScreen();
    if (!g_joinActive && !g_ctlActive) return;
    UIBegin();
    if (g_joinActive) DrawJoinScreen();
    if (g_ctlActive) DrawControlsScreen();
    UIEnd();
}
static void ReleaseUIResources() { if (g_uiSB) { g_uiSB->Release(); g_uiSB = nullptr; } }
