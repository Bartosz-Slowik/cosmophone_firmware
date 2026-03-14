#pragma once
#include <Arduino_GFX_Library.h>

// Global display pointer — use Arduino_GFX base class so other
// files don't need to know the concrete panel type.
extern Arduino_GFX *gfx;

void      displayInit();

// ── Direct framebuffer access (used by full-screen demos) ────────────────────
// Layout: fb[y * 480 + x] = RGB565 pixel.
// After a bulk write call displayFlushFramebuffer() to make the DMA engine
// see the updated data (flushes CPU D-cache → PSRAM).
uint16_t *displayGetFramebuffer();
void      displayFlushFramebuffer();

// ── Double-buffer / diff-flush (used by games) ────────────────────────────────
// Call displayBeginFrame() before any gfx draw calls for one logical frame.
// All gfx-library writes are silently redirected to a PSRAM back buffer.
// Call displayEndFrame() when the frame is complete: it compares the back
// buffer against the display framebuffer row-by-row, copies only rows that
// changed, flushes the CPU D-cache to PSRAM in contiguous batches, then
// restores gfx to the real display framebuffer.
// The display therefore never sees a partially-drawn intermediate state.
void displayBeginFrame();
void displayEndFrame();
