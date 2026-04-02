#include "menu.h"
#include "display.h"
#include "touch.h"
#include <cmath>

namespace sys {
namespace menu {

struct MenuItem {
    const char  *label;
    const char  *subtitle;
    uint16_t     iconColor;
    AppMode      mode;
};


// -- Palette -------------------------------------------------------------------
constexpr uint16_t M_BG      = 0x0841;  // very dark blue-grey
constexpr uint16_t M_TITLE   = 0x07FF;  // cyan
constexpr uint16_t M_SUB     = 0x7BEF;  // light grey
constexpr uint16_t M_BTN     = 0x2145;  // dark button
constexpr uint16_t M_BTN_HL  = 0x4A6F;  // highlighted button
constexpr uint16_t M_BTN_BDR = 0x07FF;  // cyan border
constexpr uint16_t M_TEXT    = 0xFFFF;  // white

// -- Menu item descriptor ------------------------------------------------------

static const MenuItem ITEMS[] = {
    { "SKETCH",      "Finger drawing pad",      0x07FF, AppMode::SKETCH    },
    { "RAINBOW",     "Scrolling rainbow demo",   0xF81F, AppMode::RAINBOW   },
};
constexpr int N_ITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);

// -- Layout --------------------------------------------------------------------
constexpr int HEADER_H    = 90;
constexpr int BTN_W       = 400;
constexpr int BTN_H       = 70;
constexpr int BTN_X       = (480 - BTN_W) / 2;
constexpr int BTN_GAP     = 10;
constexpr int BTN_START_Y = HEADER_H + 10;

// Settings gear icon (top-right)
constexpr int GEAR_X = 420;
constexpr int GEAR_Y = 12;
constexpr int GEAR_SZ = 56;

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

static void drawMenu() {
    gfx->fillScreen(M_BG);
    drawHeader();
    for (int i = 0; i < N_ITEMS; i++) drawItem(i, false);
    sys::display::flush();
}

static int itemHit(int tx, int ty) {
    for (int i = 0; i < N_ITEMS; i++) {
        int y = BTN_START_Y + i * (BTN_H + BTN_GAP);
        if (tx >= BTN_X && tx < BTN_X + BTN_W && ty >= y && ty < y + BTN_H)
            return i;
    }
    return -1;
}

AppMode showMenu() {
    drawMenu();

    // Ignore any lingering touch from the previous screen
    do {
        delay(10);
    } while (sys::touch::isTouched());

    int hlItem = -1;
    bool hlGear = false;

    while (true) {
        sys::touch::Point t;

        if (sys::touch::getTouch(t)) {
            int item = itemHit(t.x, t.y);

            if (item != hlItem) {
                if (item != hlItem) {
                    if (hlItem >= 0) drawItem(hlItem, false);
                    if (item >= 0) drawItem(item, true);
                    hlItem = item;
                }
                sys::display::flush();
            }
            delay(10);
            continue;
        }
        else 

        if (hlItem >= 0 || hlGear) {
            if (hlItem >= 0) { drawItem(hlItem, false); hlItem = -1; }
            sys::display::flush();
        }

        int idx = itemHit(t.x, t.y);
        if (idx >= 0) return ITEMS[idx].mode;
    }
}

}  // namespace menu
}  // namespace sys
