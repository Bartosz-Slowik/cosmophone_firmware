#include "keyboard.h"
#include "display.h"
#include "touch.h"
#include <cstring>

namespace sys {
namespace keyboard {

// ── Screen geometry ──────────────────────────────────────────────────────────
static const int SCR_W  = 480;
static const int SCR_H  = 480;
static const int MARGIN = 4;
static const int GAP    = 3;
static const int N_ROWS = 4;
static const int KEY_H  = 48;
static const int KB_H   = N_ROWS * KEY_H + (N_ROWS - 1) * GAP;
static const int KB_Y   = SCR_H - KB_H;
static const int TF_H   = 42;
static const int TF_Y   = KB_Y - TF_H - 10;
static const int MAX_LEN = 64;

// ── Palette ──────────────────────────────────────────────────────────────────
static const uint16_t BG      = 0x0841;
static const uint16_t KEY_BG  = 0x3186;
static const uint16_t KEY_BDR = 0x07FF;
static const uint16_t CTRL_BG = 0x2145;
static const uint16_t TEXT_C  = 0xFFFF;
static const uint16_t C_SUB   = 0x7BEF;
static const uint16_t KEY_HL  = 0x07FF;
static const uint16_t SHIFT_ACTIVE = 0x07E0;  // green when shift/caps active

// ── Key definition ───────────────────────────────────────────────────────────
struct K { const char *lbl; int w; };

// Android lowercase
static const K LC0[] = {{"q",1},{"w",1},{"e",1},{"r",1},{"t",1},{"y",1},{"u",1},{"i",1},{"o",1},{"p",1},{0,0}};
static const K LC1[] = {{"a",1},{"s",1},{"d",1},{"f",1},{"g",1},{"h",1},{"j",1},{"k",1},{"l",1},{0,0}};
static const K LC2[] = {{"SHIFT",2},{"z",1},{"x",1},{"c",1},{"v",1},{"b",1},{"n",1},{"m",1},{"<<",2},{0,0}};
static const K LC3[] = {{"?123",2},{",",1},{" ",5},{".",1},{"OK",2},{0,0}};

// Android uppercase
static const K UC0[] = {{"Q",1},{"W",1},{"E",1},{"R",1},{"T",1},{"Y",1},{"U",1},{"I",1},{"O",1},{"P",1},{0,0}};
static const K UC1[] = {{"A",1},{"S",1},{"D",1},{"F",1},{"G",1},{"H",1},{"J",1},{"K",1},{"L",1},{0,0}};
static const K UC2[] = {{"SHIFT",2},{"Z",1},{"X",1},{"C",1},{"V",1},{"B",1},{"N",1},{"M",1},{"<<",2},{0,0}};
static const K UC3[] = {{"?123",2},{",",1},{" ",5},{".",1},{"OK",2},{0,0}};

// Symbols page 1
static const K SY0[] = {{"1",1},{"2",1},{"3",1},{"4",1},{"5",1},{"6",1},{"7",1},{"8",1},{"9",1},{"0",1},{0,0}};
static const K SY1[] = {{"@",1},{"#",1},{"$",1},{"_",1},{"&",1},{"-",1},{"+",1},{"(",1},{")",1},{"/",1},{0,0}};
static const K SY2[] = {{"=\\<",2},{"*",1},{"\"",1},{"'",1},{":",1},{";",1},{"!",1},{"?",1},{"<<",2},{0,0}};
static const K SY3[] = {{"ABC",2},{",",1},{" ",5},{".",1},{"OK",2},{0,0}};

// Symbols page 2
static const K S20[] = {{"~",1},{"`",1},{"|",1},{"^",1},{"%",1},{"\\",1},{"[",1},{"]",1},{"{",1},{"}",1},{0,0}};
static const K S21[] = {{"<",1},{">",1},{"=",1},{"@",1},{"#",1},{"$",1},{"&",1},{"*",1},{"+",1},{"-",1},{0,0}};
static const K S22[] = {{"?123",2},{"!",1},{"?",1},{"/",1},{"_",1},{"(",1},{")",1},{":",1},{"<<",2},{0,0}};
static const K S23[] = {{"ABC",2},{",",1},{" ",5},{".",1},{"OK",2},{0,0}};

enum Mode { LOWER, UPPER, SYM1, SYM2 };
enum ShiftState { SHIFT_OFF, SHIFT_ONCE, SHIFT_LOCK };

static const K *const LAYOUTS[4][N_ROWS] = {
    {LC0, LC1, LC2, LC3},
    {UC0, UC1, UC2, UC3},
    {SY0, SY1, SY2, SY3},
    {S20, S21, S22, S23},
};

// ── Helpers ──────────────────────────────────────────────────────────────────
static int rowKeys(const K *r) { int n=0; while(r[n].lbl) n++; return n; }
static int rowWeight(const K *r) { int w=0; for(int i=0;r[i].lbl;i++) w+=r[i].w; return w; }

static bool isCtrl(const char *l) {
    return !strcmp(l,"SHIFT") || !strcmp(l,"<<") || !strcmp(l,"OK") ||
           !strcmp(l,"?123") || !strcmp(l,"ABC") || !strcmp(l,"=\\<");
}

static int keyPx(const K *r, int i) {
    int nk = rowKeys(r);
    int tw = rowWeight(r);
    int usable = SCR_W - 2*MARGIN - (nk-1)*GAP;
    return r[i].w * usable / tw;
}

static int keyX(const K *r, int i) {
    int x = MARGIN;
    for (int j = 0; j < i; j++) x += keyPx(r, j) + GAP;
    return x;
}

static ShiftState shiftState = SHIFT_OFF;

static uint16_t keyBgFor(const char *lbl) {
    if (!strcmp(lbl, "SHIFT")) {
        if (shiftState != SHIFT_OFF) return SHIFT_ACTIVE;
        return CTRL_BG;
    }
    return isCtrl(lbl) ? CTRL_BG : KEY_BG;
}

static void drawKeyWithBg(int x, int y, int w, int h, const char *lbl, uint16_t bg) {
    gfx->fillRoundRect(x, y, w, h, 6, bg);
    gfx->drawRoundRect(x, y, w, h, 6, KEY_BDR);

    if (!strcmp(lbl, " ")) return;

    // Shift key: draw an arrow icon instead of text
    if (!strcmp(lbl, "SHIFT")) {
        int cx = x + w / 2;
        int cy = y + h / 2;
        uint16_t col = TEXT_C;
        // Arrow pointing up
        gfx->fillTriangle(cx, cy - 10, cx - 8, cy + 2, cx + 8, cy + 2, col);
        gfx->fillRect(cx - 4, cy + 2, 8, 8, col);
        if (shiftState == SHIFT_LOCK) {
            gfx->drawFastHLine(cx - 8, cy + 13, 16, col);
            gfx->drawFastHLine(cx - 8, cy + 14, 16, col);
        }
        return;
    }

    gfx->setTextColor(TEXT_C);
    int len = (int)strlen(lbl);
    if (len == 1) {
        gfx->setTextSize(3);
        gfx->setCursor(x + w/2 - 9, y + h/2 - 11);
    } else {
        int ts = (len <= 3) ? 2 : 1;
        int cw = ts * 6;
        gfx->setTextSize(ts);
        gfx->setCursor(x + (w - len * cw) / 2, y + h/2 - ts * 4);
    }
    gfx->print(lbl);
}

static void drawKey(int x, int y, int w, int h, const char *lbl) {
    drawKeyWithBg(x, y, w, h, lbl, keyBgFor(lbl));
}

static void drawKeys(Mode m) {
    gfx->fillRect(0, KB_Y, SCR_W, KB_H, BG);
    for (int r = 0; r < N_ROWS; r++) {
        int y = KB_Y + r * (KEY_H + GAP);
        const K *row = LAYOUTS[m][r];
        for (int i = 0; row[i].lbl; i++)
            drawKey(keyX(row, i), y, keyPx(row, i), KEY_H, row[i].lbl);
    }
}

static void drawTextField(const String &s, int cursor) {
    gfx->fillRoundRect(16, TF_Y, SCR_W-32, TF_H, 8, KEY_BG);
    gfx->drawRoundRect(16, TF_Y, SCR_W-32, TF_H, 8, KEY_BDR);
    gfx->setTextColor(TEXT_C);
    gfx->setTextSize(2);
    int maxChars = (SCR_W - 48) / 12;
    int start = (int)s.length() > maxChars ? (int)s.length() - maxChars : 0;
    gfx->setCursor(24, TF_Y + 12);
    gfx->print(s.c_str() + start);

    // Draw cursor blink line
    int visPos = cursor - start;
    if (visPos >= 0 && visPos <= maxChars) {
        int cx = 24 + visPos * 12;
        gfx->drawFastVLine(cx, TF_Y + 8, TF_H - 16, KEY_BDR);
    }
}

static void drawScreen(const String &s, int cursor, const char *title, const char *subtitle, Mode m) {
    gfx->fillScreen(BG);
    sys::display::drawBackButton(CTRL_BG, KEY_BDR, TEXT_C);

    gfx->setTextColor(0x07FF);
    gfx->setTextSize(2);
    gfx->setCursor(56, 8);
    gfx->print(title);

    if (subtitle) {
        gfx->setTextColor(C_SUB);
        gfx->setTextSize(2);
        gfx->setCursor(56, 32);
        gfx->print(subtitle);
    }

    gfx->setTextColor(TEXT_C);
    gfx->setTextSize(2);
    gfx->setCursor(16, TF_Y - 22);
    gfx->print("Enter password:");

    drawTextField(s, cursor);
    drawKeys(m);
    sys::display::flush();
}

// Hit-test: returns label or nullptr
static const char *hitTest(int tx, int ty, Mode m) {
    for (int r = 0; r < N_ROWS; r++) {
        int y = KB_Y + r * (KEY_H + GAP);
        if (ty < y || ty >= y + KEY_H) continue;
        const K *row = LAYOUTS[m][r];
        for (int i = 0; row[i].lbl; i++) {
            int kx = keyX(row, i);
            int kw = keyPx(row, i);
            if (tx >= kx && tx < kx + kw) return row[i].lbl;
        }
    }
    return nullptr;
}

static void highlightAt(int tx, int ty, Mode m) {
    for (int r = 0; r < N_ROWS; r++) {
        int y = KB_Y + r * (KEY_H + GAP);
        if (ty < y || ty >= y + KEY_H) continue;
        const K *row = LAYOUTS[m][r];
        for (int i = 0; row[i].lbl; i++) {
            int kx = keyX(row, i);
            int kw = keyPx(row, i);
            if (tx >= kx && tx < kx + kw) {
                drawKeyWithBg(kx, y, kw, KEY_H, row[i].lbl, KEY_HL);
                return;
            }
        }
    }
}

static void unhighlightAt(int tx, int ty, Mode m) {
    for (int r = 0; r < N_ROWS; r++) {
        int y = KB_Y + r * (KEY_H + GAP);
        if (ty < y || ty >= y + KEY_H) continue;
        const K *row = LAYOUTS[m][r];
        for (int i = 0; row[i].lbl; i++) {
            int kx = keyX(row, i);
            int kw = keyPx(row, i);
            if (tx >= kx && tx < kx + kw) {
                drawKey(kx, y, kw, KEY_H, row[i].lbl);
                return;
            }
        }
    }
}

static int cursorPos = 0;

// ── Public API ───────────────────────────────────────────────────────────────
bool run(String &out, const char *title, const char *subtitle) {
    String s = out;
    cursorPos = (int)s.length();
    Mode mode = LOWER;
    shiftState = SHIFT_OFF;
    drawScreen(s, cursorPos, title, subtitle, mode);

    const char *prevHit = nullptr;
    int prevTx = 0, prevTy = 0;
    unsigned long lastShiftTap = 0;

    while (true) {
        int tx, ty;

        if (sys::touch::getTouch(tx, ty)) {
            const char *cur = hitTest(tx, ty, mode);
            if (cur != prevHit || tx != prevTx || ty != prevTy) {
                if (prevHit) unhighlightAt(prevTx, prevTy, mode);
                if (cur) highlightAt(tx, ty, mode);
                prevHit = cur;
                prevTx = tx;
                prevTy = ty;
                sys::display::flush();
            }
            delay(10);
            continue;
        }

        int rx, ry;
        if (!sys::touch::getReleased(rx, ry)) { delay(10); continue; }

        const char *hit = prevHit;
        if (prevHit) { unhighlightAt(prevTx, prevTy, mode); prevHit = nullptr; }

        if (sys::display::backButtonTapped(rx, ry)) return false;

        if (!hit) continue;

        // Shift key
        if (!strcmp(hit, "SHIFT")) {
            unsigned long now = millis();
            if (shiftState == SHIFT_OFF) {
                if (now - lastShiftTap < 400) {
                    shiftState = SHIFT_LOCK;
                    mode = UPPER;
                } else {
                    shiftState = SHIFT_ONCE;
                    mode = UPPER;
                }
            } else if (shiftState == SHIFT_ONCE) {
                if (now - lastShiftTap < 400) {
                    shiftState = SHIFT_LOCK;
                } else {
                    shiftState = SHIFT_OFF;
                    mode = LOWER;
                }
            } else {
                shiftState = SHIFT_OFF;
                mode = LOWER;
            }
            lastShiftTap = now;
            drawKeys(mode);
            sys::display::flush();
            continue;
        }

        // Mode switches
        if (!strcmp(hit, "?123")) { mode = SYM1; drawKeys(mode); sys::display::flush(); continue; }
        if (!strcmp(hit, "=\\<")) { mode = SYM2; drawKeys(mode); sys::display::flush(); continue; }
        if (!strcmp(hit, "ABC"))  {
            mode = (shiftState != SHIFT_OFF) ? UPPER : LOWER;
            drawKeys(mode); sys::display::flush();
            continue;
        }

        // OK
        if (!strcmp(hit, "OK")) { out = s; return true; }

        // Backspace
        if (!strcmp(hit, "<<")) {
            if (cursorPos > 0) { s.remove(cursorPos - 1, 1); cursorPos--; }
            drawTextField(s, cursorPos);
            sys::display::flush();
            continue;
        }

        // Regular character
        if ((int)s.length() < MAX_LEN) {
            String before = s.substring(0, cursorPos);
            String after  = s.substring(cursorPos);
            s = before + hit + after;
            cursorPos += (int)strlen(hit);
        }
        drawTextField(s, cursorPos);

        if (shiftState == SHIFT_ONCE) {
            shiftState = SHIFT_OFF;
            mode = LOWER;
            drawKeys(mode);
        }
        sys::display::flush();
    }
}

}  // namespace keyboard
}  // namespace sys
