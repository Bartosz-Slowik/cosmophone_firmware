#include "display.h"
#include <string.h>   // memcmp, memcpy
#include "esp_heap_caps.h"

#define GFX_BL 38
static const int FB_W = 480;
static const int FB_H = 480;
static const int ROW_BYTES = FB_W * 2;

static Arduino_ESP32RGBPanel *_bus = new Arduino_ESP32RGBPanel(
    39, 48, 47, 18, 17, 16, 21,
    11, 12, 13, 14, 0,
    8, 20, 3, 46, 9, 10,
    4, 5, 6, 7, 15);

static Arduino_ST7701_RGBPanel *_panel = new Arduino_ST7701_RGBPanel(
    _bus, GFX_NOT_DEFINED, 0, true, FB_W, FB_H,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations), true,
    10, 8, 50, 10, 8, 20);

Arduino_GFX *gfx = _panel;

static uint16_t *_displayFb  = nullptr;   // DMA reads from here → LCD
static uint16_t *_backbuffer = nullptr;   // gfx draws here

namespace sys {
namespace display {

void init() {
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    gfx->begin(16000000);
    _displayFb = _panel->getFramebuffer();

    // Allocate backbuffer and redirect all gfx drawing to it
    _backbuffer = (uint16_t *)heap_caps_malloc(FB_W * FB_H * 2, MALLOC_CAP_SPIRAM);
    memset(_backbuffer, 0, FB_W * FB_H * 2);
    memset(_displayFb, 0, FB_W * FB_H * 2);
    Cache_WriteBack_Addr((uint32_t)_displayFb, FB_W * FB_H * 2);
    _panel->setFramebuffer(_backbuffer);
}

// ── Flush: diff-copy changed rows from backbuffer to display FB ─────────────
void flush() {
    uint16_t *back  = _backbuffer;
    uint16_t *front = _displayFb;
    int batchStart = -1;

    for (int y = 0; y <= FB_H; y++) {
        bool dirty = (y < FB_H) && (memcmp(
            back  + (int32_t)y * FB_W,
            front + (int32_t)y * FB_W,
            ROW_BYTES) != 0);

        if (dirty) {
            memcpy(front + (int32_t)y * FB_W,
                   back  + (int32_t)y * FB_W,
                   ROW_BYTES);
            if (batchStart < 0) batchStart = y;
        } else if (batchStart >= 0) {
            Cache_WriteBack_Addr(
                (uint32_t)(front + (int32_t)batchStart * FB_W),
                (uint32_t)(y - batchStart) * ROW_BYTES);
            batchStart = -1;
        }
    }
}

// ── Back button ─────────────────────────────────────────────────────────────
static const int BACK_BTN_X = 4;
static const int BACK_BTN_Y = 4;
static const int BACK_BTN_W = 44;
static const int BACK_BTN_H = 44;

void drawBackButton(uint16_t bg, uint16_t border, uint16_t textColor) {
    gfx->fillRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 8, bg);
    gfx->drawRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 8, border);
    gfx->setTextColor(textColor);
    gfx->setTextSize(3);
    gfx->setCursor(BACK_BTN_X + 12, BACK_BTN_Y + 10);
    gfx->print("<");
}

bool backButtonTapped(int tx, int ty) {
    return tx >= BACK_BTN_X && tx < BACK_BTN_X + BACK_BTN_W &&
           ty >= BACK_BTN_Y && ty < BACK_BTN_Y + BACK_BTN_H;
}

// ── Direct framebuffer access (for rainbow / raw pixel demos) ────────────────
uint16_t *getDirectFramebuffer() {
    return _displayFb;
}

void flushDirect() {
    Cache_WriteBack_Addr((uint32_t)_displayFb, FB_W * FB_H * 2);
}

// ── Game helpers (backward compat) ───────────────────────────────────────────
void beginFrame() {
    // gfx already draws to backbuffer — nothing to do
}

void endFrame() {
    flush();
}

}  // namespace display
}  // namespace sys
