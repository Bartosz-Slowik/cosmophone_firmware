#include "flapbird.h"
#include "display.h"
#include "touch.h"
#include <Arduino.h>

// ── Layout ────────────────────────────────────────────────────────────────────
static const int   SCR_W      = 480;
static const int   GROUND_Y   = 430;   // y-coord where ground begins

static const int   BIRD_X     = 100;   // fixed horizontal position
static const int   BIRD_W     = 38;
static const int   BIRD_H     = 28;
static const int   BEAK_W     = 12;    // beak extends right of body

static const float GRAVITY    = 0.45f;
static const float FLAP_VEL   = -8.5f;
static const float MAX_FALL   = 11.0f;

static const int   PIPE_W     = 64;
static const int   PIPE_GAP   = 150;   // vertical gap between pipes
static const int   PIPE_SPEED = 3;
static const int   PIPE_SPACE = 215;   // horizontal spacing between pipes
static const int   N_PIPES    = 3;
static const int   GAP_MIN    = 80;
static const int   GAP_MAX    = GROUND_Y - PIPE_GAP - 80;  // 200

static const unsigned long FRAME_MS = 30; // ~33 fps

// ── Colours ───────────────────────────────────────────────────────────────────
static const uint16_t C_SKY    = 0x6D9E;  // cornflower blue
static const uint16_t C_GND    = 0xAB00;  // brown ground
static const uint16_t C_GRASS  = 0x25A0;  // bright green strip
static const uint16_t C_PIPE   = 0x05A0;  // green pipe body
static const uint16_t C_PIPELT = 0x07E0;  // lighter highlight stripe on pipe
static const uint16_t C_BIRD   = 0xFFE0;  // yellow
static const uint16_t C_BEAK   = 0xFB80;  // orange beak
static const uint16_t C_EYEW   = 0xFFFF;  // eye white
static const uint16_t C_EYEP   = 0x0000;  // pupil
static const uint16_t C_TEXT   = 0xFFFF;
static const uint16_t C_HUD_BG = 0x0821;  // semi-dark HUD background

// ── Pipe ──────────────────────────────────────────────────────────────────────
struct Pipe {
    float x;
    int   gapY;   // top of the opening
    bool  passed;
};

// ── State ─────────────────────────────────────────────────────────────────────
static float birdY, birdVel;
static int   prevBirdY;
static Pipe  pipes[N_PIPES];
static int   score;
static int   bestScore = 0;
static bool  dead, waitStart;
static unsigned long lastFrameMs;

// ── Pipe helpers ──────────────────────────────────────────────────────────────
static float rightmostPipeX() {
    float mx = (float)SCR_W;
    for (int i = 0; i < N_PIPES; i++) mx = max(mx, pipes[i].x);
    return mx;
}

static void resetPipe(Pipe &p, float x) {
    p.x      = x;
    p.gapY   = random(GAP_MIN, GAP_MAX + 1);
    p.passed = false;
}

static void initPipes() {
    for (int i = 0; i < N_PIPES; i++)
        resetPipe(pipes[i], (float)SCR_W + i * PIPE_SPACE);
}

// ── Low-level draw ────────────────────────────────────────────────────────────

// Draw or erase the bird at y.  The beak sticks out BEAK_W to the right.
static void drawBirdAt(int y) {
    int cy = max(0, y);
    int ch = min(GROUND_Y, y + BIRD_H) - cy;
    if (ch <= 0) return;
    gfx->fillRoundRect(BIRD_X, y, BIRD_W, BIRD_H, 8, C_BIRD);
    // Eye
    gfx->fillCircle(BIRD_X + BIRD_W - 10, y + 9, 5, C_EYEW);
    gfx->fillCircle(BIRD_X + BIRD_W - 9,  y + 9, 3, C_EYEP);
    // Beak
    gfx->fillRect(BIRD_X + BIRD_W, y + BIRD_H / 2 - 4, BEAK_W, 8, C_BEAK);
}

// Restore pipe pixels that fall inside the rectangle (rx,ry,rw,rh).
static void restorePipesInRegion(int rx, int ry, int rw, int rh) {
    for (int i = 0; i < N_PIPES; i++) {
        int px = (int)pipes[i].x;
        int ox = max(rx, px);
        int ow = min(rx + rw, px + PIPE_W) - ox;
        if (ow <= 0) continue;

        // Top pipe section
        int oh1 = min(ry + rh, pipes[i].gapY) - max(ry, 0);
        if (oh1 > 0) gfx->fillRect(ox, max(ry, 0), ow, oh1, C_PIPE);

        // Bottom pipe section
        int botTop = pipes[i].gapY + PIPE_GAP;
        int oy2    = max(ry, botTop);
        int oh2    = min(ry + rh, GROUND_Y) - oy2;
        if (oh2 > 0) gfx->fillRect(ox, oy2, ow, oh2, C_PIPE);
    }
}

static void eraseBird(int y) {
    int ey = max(0, y);
    int eh = min(GROUND_Y, y + BIRD_H) - ey;
    if (eh <= 0) return;
    // Fill with sky over the whole bird+beak area
    gfx->fillRect(BIRD_X, ey, BIRD_W + BEAK_W, eh, C_SKY);
    // Restore any pipe pixels that were hiding behind the bird
    restorePipesInRegion(BIRD_X, ey, BIRD_W + BEAK_W, eh);
}

// Move a single pipe one step and draw only the changed pixel strips.
static void stepPipe(Pipe &p) {
    int oldX = (int)p.x;
    p.x -= PIPE_SPEED;
    int newX = (int)p.x;

    // Score: pipe centre passed bird centre
    if (!p.passed && newX + PIPE_W < BIRD_X + BIRD_W / 2) {
        p.passed = true;
        score++;
    }

    // Recycle when fully off-screen left
    if (newX + PIPE_W < 0) {
        resetPipe(p, rightmostPipeX() + PIPE_SPACE);
        return;
    }
    // Still entirely off-screen right — nothing to draw yet
    if (newX >= SCR_W) return;

    int gapY   = p.gapY;
    int botTop = gapY + PIPE_GAP;
    int botH   = GROUND_Y - botTop;

    // ── Trailing strip: right edge of pipe now exposes sky ───────────────────
    {
        int tx = max(0, newX + PIPE_W);
        int tw = min(SCR_W, oldX + PIPE_W) - tx;
        if (tw > 0) {
            if (gapY  > 0) gfx->fillRect(tx, 0,      tw, gapY, C_SKY);
            if (botH  > 0) gfx->fillRect(tx, botTop,  tw, botH, C_SKY);
        }
    }

    // ── Leading strip: left edge of pipe moves into fresh sky ────────────────
    {
        int lx = max(0, newX);
        // Pixels newly covered = from newX to min(oldX, SCR_W)
        int lw = min(SCR_W, (oldX >= SCR_W ? newX + PIPE_W : oldX)) - lx;
        if (lw > 0) {
            if (gapY > 0) gfx->fillRect(lx, 0,      lw, gapY, C_PIPE);
            if (botH > 0) gfx->fillRect(lx, botTop,  lw, botH, C_PIPE);
        }
    }
}

static void stepPipes() {
    for (int i = 0; i < N_PIPES; i++) stepPipe(pipes[i]);
}

// ── HUD (score) ───────────────────────────────────────────────────────────────
static void drawHUD() {
    // Dark pill behind score so it's always readable over pipes/sky
    gfx->fillRoundRect(SCR_W / 2 - 55, 6, 110, 42, 10, C_HUD_BG);
    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(3);
    // Centre the digit(s)
    int digits = score < 10 ? 1 : score < 100 ? 2 : 3;
    gfx->setCursor(SCR_W / 2 - digits * 9, 12);
    gfx->printf("%d", score);
}

// ── Static background ─────────────────────────────────────────────────────────
static void drawBackground() {
    gfx->fillRect(0, 0,        SCR_W, GROUND_Y,            C_SKY);
    gfx->fillRect(0, GROUND_Y, SCR_W, SCR_W - GROUND_Y,    C_GND);
    gfx->fillRect(0, GROUND_Y, SCR_W, 12,                  C_GRASS);
}

// ── Collision detection ───────────────────────────────────────────────────────
static bool collides() {
    if (birdY + BIRD_H >= GROUND_Y) return true;
    if (birdY          <= 0)        return true;

    // Shrink hitbox slightly for fairness
    int bLeft  = BIRD_X + 4;
    int bRight = BIRD_X + BIRD_W - 4;
    int bTop   = (int)birdY + 4;
    int bBot   = (int)birdY + BIRD_H - 4;

    for (int i = 0; i < N_PIPES; i++) {
        int px = (int)pipes[i].x;
        int pr = px + PIPE_W;
        if (bRight <= px || bLeft >= pr) continue;
        if (bTop < pipes[i].gapY || bBot > pipes[i].gapY + PIPE_GAP) return true;
    }
    return false;
}

// ── Game-over overlay ─────────────────────────────────────────────────────────
// Returns true = play again, false = back to menu.
static bool showGameOver() {
    if (score > bestScore) bestScore = score;

    static const int OX = 80, OY = 150, OW = 320, OH = 180;
    gfx->fillRoundRect(OX, OY, OW, OH, 18, 0x2945);
    gfx->drawRoundRect(OX, OY, OW, OH, 18, C_TEXT);

    gfx->setTextColor(0xF800);   // red
    gfx->setTextSize(4);
    gfx->setCursor(OX + 28, OY + 14);
    gfx->print("GAME OVER");

    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(2);
    gfx->setCursor(OX + 20, OY + 70);
    gfx->printf("Score: %d   Best: %d", score, bestScore);

    // Buttons
    static const int BW = 130, BH = 44;
    int by  = OY + OH - BH - 14;
    int bx1 = OX + 10;
    int bx2 = OX + OW - BW - 10;
    gfx->fillRoundRect(bx1, by, BW, BH, 8, 0x0400);
    gfx->fillRoundRect(bx2, by, BW, BH, 8, 0x0019);
    gfx->setTextSize(2);
    gfx->setTextColor(C_TEXT);
    gfx->setCursor(bx1 + 8,  by + 12); gfx->print("PLAY AGAIN");
    gfx->setCursor(bx2 + 30, by + 12); gfx->print("MENU");

    // Wait for tap
    bool prevTouched = false;
    while (true) {
        int tx, ty;
        bool touched = getTouch(tx, ty);
        if (touched && !prevTouched) {
            if (tx >= bx1 && tx < bx1 + BW && ty >= by && ty < by + BH) return true;
            if (tx >= bx2 && tx < bx2 + BW && ty >= by && ty < by + BH) return false;
        }
        prevTouched = touched;
        delay(10);
    }
}

// ── Init / reset ──────────────────────────────────────────────────────────────
static void initGame() {
    birdY     = 190.0f;
    birdVel   = 0.0f;
    prevBirdY = (int)birdY;
    score     = 0;
    dead      = false;
    waitStart = true;

    initPipes();
    drawBackground();
    drawHUD();

    // "TAP TO START" prompt
    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(3);
    gfx->setCursor(104, 330);
    gfx->print("TAP TO START");

    drawBirdAt((int)birdY);
    lastFrameMs = millis();
}

// ── Public entry point ────────────────────────────────────────────────────────
void runFlappyBird() {
    displayBeginFrame();
    initGame();
    displayEndFrame();

    bool prevTouched = false;

    while (true) {
        int  tx, ty;
        bool touched     = getTouch(tx, ty);
        bool justTouched = touched && !prevTouched;
        prevTouched      = touched;

        // ── Game-over screen ─────────────────────────────────────────────────
        // showGameOver() draws its overlay directly to the display FB
        // (gfx is back to display FB after the last displayEndFrame).
        if (dead) {
            bool restart = showGameOver();
            if (restart) {
                displayBeginFrame();
                initGame();
                displayEndFrame();
                prevTouched = false;
            } else return;   // back to menu
            continue;
        }

        // ── Waiting for first tap ─────────────────────────────────────────────
        if (waitStart) {
            if (justTouched) {
                waitStart = false;
                birdVel   = FLAP_VEL;
                // Erase the prompt text via the back buffer so the display
                // never shows a half-erased state.
                displayBeginFrame();
                gfx->fillRect(90, 320, 310, 44, C_SKY);
                displayEndFrame();
            }
            delay(16);
            continue;
        }

        // ── Frame throttle ────────────────────────────────────────────────────
        unsigned long now = millis();
        if (now - lastFrameMs < FRAME_MS) { delay(4); continue; }
        lastFrameMs = now;

        // ── Input ─────────────────────────────────────────────────────────────
        if (justTouched) birdVel = FLAP_VEL;

        // ── Physics ───────────────────────────────────────────────────────────
        birdVel += GRAVITY;
        if (birdVel > MAX_FALL) birdVel = MAX_FALL;
        birdY  += birdVel;

        int newBirdY = (int)birdY;

        // ── Render entire frame into back buffer, then diff-flush ─────────────
        displayBeginFrame();
        eraseBird(prevBirdY);   // clear old bird, restore pipe pixels
        stepPipes();            // scroll all pipes one step
        drawHUD();              // score always on top
        drawBirdAt(newBirdY);   // draw bird on top of everything
        displayEndFrame();

        prevBirdY = newBirdY;

        // ── Collision ─────────────────────────────────────────────────────────
        if (collides()) dead = true;
    }
}
