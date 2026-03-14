#include "snake.h"
#include "display.h"
#include "touch.h"

namespace apps {
namespace snake {

// ── Layout ────────────────────────────────────────────────────────────────────
static const int CELL    = 20;
static const int COLS    = 24;
static const int ROWS    = 17;
static const int SCORE_Y = ROWS * CELL;        // 340
static const int SCORE_H = 20;
static const int BTN_Y   = SCORE_Y + SCORE_H;  // 360
static const int BTN_H   = 480 - BTN_Y;        // 120

// ── Colours ───────────────────────────────────────────────────────────────────
static const uint16_t C_BG      = 0x0000;
static const uint16_t C_GRID    = 0x2104;
static const uint16_t C_SNAKE   = 0x07E0;
static const uint16_t C_HEAD    = 0xFFE0;
static const uint16_t C_FOOD    = 0xF800;
static const uint16_t C_BTN     = 0x2945;
static const uint16_t C_TEXT    = 0xFFFF;
static const uint16_t C_SCOREBG = 0x0841;

// ── State ─────────────────────────────────────────────────────────────────────
enum Dir { DRIGHT, DLEFT, DUP, DDOWN };

static const int MAX_LEN = COLS * ROWS;
static int  sx[MAX_LEN], sy[MAX_LEN];
static int  sLen;
static Dir  curDir, nextDir;
static int  foodX, foodY;
static int  score;
static int  bestScore = 0;   // persists across games
static bool gameOver;
static unsigned long lastMoveMs, moveMs;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void fillCell(int col, int row, uint16_t color) {
    gfx->fillRect(col * CELL + 1, row * CELL + 1, CELL - 2, CELL - 2, color);
}

static bool isOpposite(Dir a, Dir b) {
    return (a == DUP && b == DDOWN) || (a == DDOWN && b == DUP) ||
           (a == DLEFT && b == DRIGHT) || (a == DRIGHT && b == DLEFT);
}

static Dir touchToDir(int tx, int ty) {
    if (tx < 120)  return DLEFT;
    if (tx >= 360) return DRIGHT;
    return (ty < BTN_Y + BTN_H / 2) ? DUP : DDOWN;
}

// ── Drawing ───────────────────────────────────────────────────────────────────
static void drawGrid() {
    gfx->fillRect(0, 0, COLS * CELL, ROWS * CELL, C_BG);
    for (int x = 0; x < COLS; x++)
        for (int y = 0; y < ROWS; y++)
            gfx->drawRect(x * CELL, y * CELL, CELL, CELL, C_GRID);
    gfx->drawRect(0, 0, COLS * CELL, ROWS * CELL, 0x4208);
}

static void drawScore() {
    gfx->fillRect(0, SCORE_Y, 480, SCORE_H, C_SCOREBG);
    gfx->setTextSize(1);
    gfx->setTextColor(C_TEXT, C_SCOREBG);
    gfx->setCursor(4, SCORE_Y + 6);
    gfx->printf("  SCORE: %-4d   BEST: %-4d   SPEED: %lums/step  ",
                score, bestScore, moveMs);
}

static void drawButtons() {
    gfx->fillRect(0, BTN_Y, 480, BTN_H, 0x1082);
    gfx->fillRoundRect(4,   BTN_Y + 4,        112, BTN_H - 8, 12, C_BTN);
    gfx->fillRoundRect(364, BTN_Y + 4,        112, BTN_H - 8, 12, C_BTN);
    gfx->fillRoundRect(120, BTN_Y + 4,        240, BTN_H/2-6, 12, C_BTN);
    gfx->fillRoundRect(120, BTN_Y+BTN_H/2+2,  240, BTN_H/2-6, 12, C_BTN);
    gfx->setTextSize(4);
    gfx->setTextColor(C_TEXT);
    gfx->setCursor(28,  BTN_Y + BTN_H/2 - 14); gfx->print("<");
    gfx->setCursor(390, BTN_Y + BTN_H/2 - 14); gfx->print(">");
    gfx->setCursor(222, BTN_Y + BTN_H/4 - 14); gfx->print("^");
    gfx->setCursor(222, BTN_Y+3*BTN_H/4 - 14); gfx->print("v");
}

static void placeFood() {
    bool ok;
    do {
        ok = true;
        foodX = random(COLS);
        foodY = random(ROWS);
        for (int i = 0; i < sLen; i++)
            if (sx[i] == foodX && sy[i] == foodY) { ok = false; break; }
    } while (!ok);
    fillCell(foodX, foodY, C_FOOD);
}

// ── Game over overlay — returns true = restart, false = back to menu ──────────
static bool showGameOver() {
    if (score > bestScore) bestScore = score;

    const int OX = 60, OY = ROWS * CELL / 2 - 80, OW = 360, OH = 160;
    gfx->fillRect(OX, OY, OW, OH, 0x2945);
    gfx->drawRect(OX, OY, OW, OH, C_TEXT);

    gfx->setTextColor(C_FOOD);
    gfx->setTextSize(4);
    gfx->setCursor(OX + 40, OY + 12);
    gfx->print("GAME  OVER");

    gfx->setTextColor(C_TEXT);
    gfx->setTextSize(2);
    gfx->setCursor(OX + 30, OY + 62);
    gfx->printf("Score: %d   Best: %d", score, bestScore);

    const int BW = 140, BH = 44, BY = OY + OH - BH - 10;
    const int BX1 = OX + 10;
    const int BX2 = OX + OW - BW - 10;
    const uint16_t C_PLAY = 0x0400, C_PLAY_HL = 0x07E0;
    const uint16_t C_MENU = 0x0019, C_MENU_HL = 0x07FF;

    auto drawBtns = [&](int hl) {
        gfx->fillRoundRect(BX1, BY, BW, BH, 8, hl == 0 ? C_PLAY_HL : C_PLAY);
        gfx->fillRoundRect(BX2, BY, BW, BH, 8, hl == 1 ? C_MENU_HL : C_MENU);
        gfx->setTextSize(2);
        gfx->setTextColor(C_TEXT);
        gfx->setCursor(BX1 + 8,  BY + 12); gfx->print("PLAY AGAIN");
        gfx->setCursor(BX2 + 30, BY + 12); gfx->print("MENU");
    };
    drawBtns(-1);
    sys::display::flush();

    int hlBtn = -1;
    while (true) {
        int tx, ty;
        if (sys::touch::getTouch(tx, ty)) {
            int cur = -1;
            if (ty >= BY && ty < BY + BH) {
                if (tx >= BX1 && tx < BX1 + BW) cur = 0;
                else if (tx >= BX2 && tx < BX2 + BW) cur = 1;
            }
            if (cur != hlBtn) { hlBtn = cur; drawBtns(hlBtn); sys::display::flush(); }
            delay(10); continue;
        }
        int rx, ry;
        if (!sys::touch::getReleased(rx, ry)) { delay(10); continue; }
        if (hlBtn >= 0) { drawBtns(-1); sys::display::flush(); hlBtn = -1; }
        if (ry >= BY && ry < BY + BH) {
            if (rx >= BX1 && rx < BX1 + BW) return true;
            if (rx >= BX2 && rx < BX2 + BW) return false;
        }
    }
}

// ── Init ──────────────────────────────────────────────────────────────────────
static void initGame() {
    sLen   = 4;
    curDir = nextDir = DRIGHT;
    score  = 0;
    moveMs = 200;

    for (int i = 0; i < sLen; i++) {
        sx[i] = COLS / 2 - i;
        sy[i] = ROWS / 2;
    }

    drawGrid();
    for (int i = 1; i < sLen; i++) fillCell(sx[i], sy[i], C_SNAKE);
    fillCell(sx[0], sy[0], C_HEAD);
    placeFood();
    drawScore();
    drawButtons();
    lastMoveMs = millis();
    gameOver   = false;
}

void run() {
    sys::display::beginFrame();
    initGame();
    sys::display::endFrame();

    while (true) {
        int tx, ty;
        bool touched = sys::touch::getTouch(tx, ty);

        if (gameOver) {
            bool restart = showGameOver();
            if (restart) {
                sys::display::beginFrame();
                initGame();
                sys::display::endFrame();
            } else return;
            continue;
        }

        if (touched && ty >= BTN_Y) {
            Dir d = touchToDir(tx, ty);
            if (!isOpposite(d, curDir)) nextDir = d;
        }

        if (millis() - lastMoveMs < moveMs) { delay(5); continue; }
        lastMoveMs = millis();
        curDir = nextDir;

        int nx = sx[0] + (curDir == DRIGHT ? 1 : curDir == DLEFT ? -1 : 0);
        int ny = sy[0] + (curDir == DDOWN  ? 1 : curDir == DUP   ? -1 : 0);

        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
            gameOver = true; continue;
        }

        bool ate = (nx == foodX && ny == foodY);
        for (int i = 0; i < (ate ? sLen : sLen - 1); i++) {
            if (sx[i] == nx && sy[i] == ny) { gameOver = true; break; }
        }
        if (gameOver) continue;

        sys::display::beginFrame();

        if (!ate) {
            fillCell(sx[sLen - 1], sy[sLen - 1], C_BG);
        } else {
            score++;
            sLen = min(sLen + 1, MAX_LEN - 1);
            if (moveMs > 80) moveMs -= 4;
            drawScore();
            placeFood();
        }

        fillCell(sx[0], sy[0], C_SNAKE);
        for (int i = sLen - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
        sx[0] = nx;  sy[0] = ny;
        fillCell(nx, ny, C_HEAD);

        sys::display::endFrame();
    }
}

}  // namespace snake
}  // namespace apps
