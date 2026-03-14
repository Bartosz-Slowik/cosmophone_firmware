#include "menu.h"
#include "display.h"
#include "touch.h"
#include <cmath>
#include <WiFi.h>

namespace sys {
namespace menu {

// ── Palette ───────────────────────────────────────────────────────────────────
static const uint16_t M_BG      = 0x0841;  // very dark blue-grey
static const uint16_t M_TITLE   = 0x07FF;  // cyan
static const uint16_t M_SUB     = 0x7BEF;  // light grey
static const uint16_t M_BTN     = 0x2145;  // dark button
static const uint16_t M_BTN_HL  = 0x4A6F;  // highlighted button
static const uint16_t M_BTN_BDR = 0x07FF;  // cyan border
static const uint16_t M_TEXT    = 0xFFFF;  // white

// ── Menu item descriptor ──────────────────────────────────────────────────────
struct MenuItem {
    const char  *label;
    const char  *subtitle;
    uint16_t     iconColor;
    AppMode      mode;
};

static const MenuItem ITEMS[] = {
    { "SNAKE",       "Classic snake game",       0x07E0, AppMode::SNAKE       },
    { "FLAPPY BIRD", "Tap to flap!",             0xFFE0, AppMode::FLAPPY      },
    { "RAINBOW",     "Scrolling rainbow demo",   0xF81F, AppMode::RAINBOW     },
};
static const int N_ITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);

// ── Layout ────────────────────────────────────────────────────────────────────
static const int HEADER_H    = 90;
static const int BTN_W       = 400;
static const int BTN_H       = 70;
static const int BTN_X       = (480 - BTN_W) / 2;
static const int BTN_GAP     = 10;
static const int BTN_START_Y = HEADER_H + 10;

// Settings gear icon (top-right)
static const int GEAR_X = 420;
static const int GEAR_Y = 12;
static const int GEAR_SZ = 56;

static void drawGearIcon(int cx, int cy, uint16_t color, uint16_t bg) {
    const int bodyR = 10;
    const int tipR  = 16;
    const int holeR = 5;
    const int nTeeth = 8;
    const float PI2 = 6.28318530f;
    const float toothHalf = PI2 / nTeeth / 3.5f;

    gfx->fillCircle(cx, cy, bodyR, color);

    for (int i = 0; i < nTeeth; i++) {
        float a = i * PI2 / nTeeth;
        float a1 = a - toothHalf;
        float a2 = a + toothHalf;
        int ix1 = cx + (int)(bodyR * std::cos(a1));
        int iy1 = cy + (int)(bodyR * std::sin(a1));
        int ix2 = cx + (int)(bodyR * std::cos(a2));
        int iy2 = cy + (int)(bodyR * std::sin(a2));
        int ox1 = cx + (int)(tipR  * std::cos(a1));
        int oy1 = cy + (int)(tipR  * std::sin(a1));
        int ox2 = cx + (int)(tipR  * std::cos(a2));
        int oy2 = cy + (int)(tipR  * std::sin(a2));
        gfx->fillTriangle(ix1, iy1, ix2, iy2, ox1, oy1, color);
        gfx->fillTriangle(ix2, iy2, ox1, oy1, ox2, oy2, color);
    }

    gfx->fillCircle(cx, cy, holeR, bg);
}

static void drawHeader() {
    gfx->fillRect(0, 0, 480, HEADER_H, 0x1082);
    gfx->drawFastHLine(0, HEADER_H, 480, M_TITLE);

    gfx->setTextColor(M_TITLE);
    gfx->setTextSize(4);
    gfx->setCursor((480 - 10 * 24) / 2, 12);
    gfx->print("COSMOPHONE");

    gfx->setTextColor(M_SUB);
    gfx->setTextSize(2);
    gfx->setCursor((480 - 12 * 12) / 2, 58);
    gfx->print("Game Console");

    // Settings gear icon (top-right)
    int gearCx = GEAR_X + GEAR_SZ / 2;
    int gearCy = GEAR_Y + GEAR_SZ / 2;
    gfx->fillRoundRect(GEAR_X, GEAR_Y, GEAR_SZ, GEAR_SZ, 8, M_BTN);
    gfx->drawRoundRect(GEAR_X, GEAR_Y, GEAR_SZ, GEAR_SZ, 8, M_BTN_BDR);
    drawGearIcon(gearCx, gearCy, M_TITLE, M_BTN);
}

static void drawItem(int idx, bool highlighted) {
    const MenuItem &item = ITEMS[idx];
    int y = BTN_START_Y + idx * (BTN_H + BTN_GAP);

    uint16_t bg  = highlighted ? M_BTN_HL : M_BTN;
    gfx->fillRoundRect(BTN_X, y, BTN_W, BTN_H, 14, bg);
    gfx->drawRoundRect(BTN_X, y, BTN_W, BTN_H, 14, M_BTN_BDR);

    gfx->fillRoundRect(BTN_X + 12, y + 10, 50, 50, 8, item.iconColor);

    gfx->setTextColor(M_TEXT);
    gfx->setTextSize(3);
    gfx->setCursor(BTN_X + 78, y + 10);
    gfx->print(item.label);

    gfx->setTextColor(M_SUB);
    gfx->setTextSize(1);
    gfx->setCursor(BTN_X + 80, y + 48);
    gfx->print(item.subtitle);
}

static void drawWifiStatus() {
    int y = 462;
    gfx->fillRect(0, y, 480, 18, M_BG);
    gfx->setTextSize(1);

    if (WiFi.status() == WL_CONNECTED) {
        gfx->setTextColor(0x07E0);  // green
        gfx->setCursor(4, y + 4);
        gfx->print("WiFi: ");
        gfx->print(WiFi.SSID());
        gfx->print("  ");
        gfx->print(WiFi.localIP().toString());
    } else {
        gfx->setTextColor(0x7BEF);  // grey
        gfx->setCursor(4, y + 4);
        gfx->print("WiFi: Not connected");
    }
}

static void drawMenu() {
    gfx->fillScreen(M_BG);
    drawHeader();
    for (int i = 0; i < N_ITEMS; i++) drawItem(i, false);
    drawWifiStatus();
    sys::display::flush();
}

static bool gearHit(int tx, int ty) {
    return tx >= GEAR_X && tx < GEAR_X + GEAR_SZ &&
           ty >= GEAR_Y && ty < GEAR_Y + GEAR_SZ;
}

static int itemHit(int tx, int ty) {
    for (int i = 0; i < N_ITEMS; i++) {
        int y = BTN_START_Y + i * (BTN_H + BTN_GAP);
        if (tx >= BTN_X && tx < BTN_X + BTN_W && ty >= y && ty < y + BTN_H)
            return i;
    }
    return -1;
}

static void highlightGear(bool hl) {
    int gearCx = GEAR_X + GEAR_SZ / 2;
    int gearCy = GEAR_Y + GEAR_SZ / 2;
    uint16_t bg = hl ? M_BTN_HL : M_BTN;
    gfx->fillRoundRect(GEAR_X, GEAR_Y, GEAR_SZ, GEAR_SZ, 8, bg);
    gfx->drawRoundRect(GEAR_X, GEAR_Y, GEAR_SZ, GEAR_SZ, 8, M_BTN_BDR);
    drawGearIcon(gearCx, gearCy, M_TITLE, bg);
}

AppMode showMenu() {
    drawMenu();

    // Ignore any lingering touch from the previous screen
    unsigned long entryMs = millis();
    while (millis() - entryMs < 100) {
        int tx, ty;
        sys::touch::getTouch(tx, ty);
        sys::touch::getReleased(tx, ty);
        delay(10);
    }

    int hlItem = -1;
    bool hlGear = false;

    while (true) {
        int tx, ty;

        if (sys::touch::getTouch(tx, ty)) {
            int item = itemHit(tx, ty);
            bool gear = gearHit(tx, ty);

            if (item != hlItem || gear != hlGear) {
                if (item != hlItem) {
                    if (hlItem >= 0) drawItem(hlItem, false);
                    if (item >= 0) drawItem(item, true);
                    hlItem = item;
                }
                if (gear != hlGear) {
                    highlightGear(gear);
                    hlGear = gear;
                }
                sys::display::flush();
            }
            delay(10);
            continue;
        }

        int rx, ry;
        if (!sys::touch::getReleased(rx, ry)) { delay(10); continue; }

        if (hlItem >= 0 || hlGear) {
            if (hlItem >= 0) { drawItem(hlItem, false); hlItem = -1; }
            if (hlGear) { highlightGear(false); hlGear = false; }
            sys::display::flush();
        }

        if (gearHit(rx, ry)) return AppMode::SETTINGS;

        int idx = itemHit(rx, ry);
        if (idx >= 0) return ITEMS[idx].mode;
    }
}

}  // namespace menu
}  // namespace sys
