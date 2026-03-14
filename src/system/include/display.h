#pragma once
#include <Arduino_GFX_Library.h>

namespace sys {
namespace display {

void init();

// ── Buffered rendering (used everywhere) ─────────────────────────────────────
// All gfx draw calls write to an offscreen backbuffer.
// Call flush() to push only the changed rows to the visible display.
// This prevents partial-draw flashing.
void flush();

// Standard back button (top-left). Draw it, then use backButtonTapped(tx, ty) in your touch loop.
void drawBackButton(uint16_t bg = 0x2145, uint16_t border = 0x07FF, uint16_t textColor = 0xFFFF);
bool backButtonTapped(int tx, int ty);

// ── Direct framebuffer access (used by raw pixel demos like rainbow) ─────────
// Returns the visible display framebuffer for direct pixel writes.
// After writing, call flushDirect() to make the DMA see updated data.
uint16_t *getDirectFramebuffer();
void      flushDirect();

// ── Game helpers (convenience wrappers) ──────────────────────────────────────
// beginFrame/endFrame are kept for backward compat with game code.
// beginFrame() is a no-op (gfx already draws to backbuffer).
// endFrame() just calls flush().
void beginFrame();
void endFrame();

}  // namespace display
}  // namespace sys

// Convenience: global gfx pointer for drawing.
extern Arduino_GFX *gfx;
