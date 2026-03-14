#include "demos.h"
#include "display.h"
#include "touch.h"
#include <Arduino.h>
#include <string.h>  // memcpy

static const int W = 480;
static const int H = 480;

// ── HSV → RGB565 (full saturation and brightness) ────────────────────────────
// h: 0–359
static uint16_t hsv565(int h) {
    int region = h / 60;
    int t      = (h % 60) * 256 / 60;  // 0–255 linear ramp within sector
    uint8_t r, g, b;
    switch (region) {
        case 0:  r = 255;     g = t;       b = 0;       break;
        case 1:  r = 255 - t; g = 255;     b = 0;       break;
        case 2:  r = 0;       g = 255;     b = t;       break;
        case 3:  r = 0;       g = 255 - t; b = 255;     break;
        case 4:  r = t;       g = 0;       b = 255;     break;
        default: r = 255;     g = 0;       b = 255 - t; break;
    }
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

// ── Touch: rising-edge detection ─────────────────────────────────────────────
static bool justTouched(bool &prev) {
    int tx, ty;
    bool cur = getTouch(tx, ty);
    bool rising = cur && !prev;
    prev = cur;
    return rising;
}

// ── Demo 1: Rainbow scroll ────────────────────────────────────────────────────
// Strategy: pre-compute a 480-color palette; duplicate it to 960 entries so a
// window of 480 elements starting at any offset [0,479] needs no modulo.
// Each frame: memcpy that 960-byte window into all 480 rows → one PSRAM write
// pass; flush D-cache once at the end.
void runRainbow() {
    static uint16_t extPal[960];
    for (int i = 0; i < 480; i++) {
        extPal[i] = extPal[i + 480] = hsv565(i * 360 / 480);
    }

    uint16_t *fb   = displayGetFramebuffer();
    int       offs = 0;
    bool      prev = false;

    while (!justTouched(prev)) {
        uint16_t *row = &extPal[offs];       // 480-element window, no modulo
        for (int y = 0; y < H; y++) {
            memcpy(fb + y * W, row, W * 2);  // 960 bytes per row
        }
        displayFlushFramebuffer();
        if (++offs >= 480) offs = 0;
    }
}

// ── Demo 2: Color flash (hue cycle) ──────────────────────────────────────────
// Strategy: cycle through N_COLORS hues as fast as possible.
// Fill uses 32-bit stores (2 pixels per write) to halve memory transactions.
// One D-cache flush per frame.
static const int N_COLORS = 20;

void runColorFlash() {
    static uint16_t colors[N_COLORS];
    for (int i = 0; i < N_COLORS; i++) {
        colors[i] = hsv565(i * 360 / N_COLORS);
    }

    uint16_t *fb   = displayGetFramebuffer();
    int       ci   = 0;
    bool      prev = false;

    while (!justTouched(prev)) {
        uint16_t col = colors[ci];
        // Pack two RGB565 pixels into one 32-bit word for faster PSRAM writes
        uint32_t c32 = (uint32_t)col | ((uint32_t)col << 16);
        uint32_t *p  = reinterpret_cast<uint32_t *>(fb);
        for (int i = 0; i < W * H / 2; i++) *p++ = c32;
        displayFlushFramebuffer();
        if (++ci >= N_COLORS) ci = 0;
    }
}
