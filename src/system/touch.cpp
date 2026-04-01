#include "touch.h"
#include <TAMC_GT911.h>

namespace sys {
namespace touch {

static TAMC_GT911 touch(19, 45, -1, -1, SCREEN_W, SCREEN_H);

static bool touched[MAX_TOUCHES]       = {false, false, false, false, false};
static bool justTouched[MAX_TOUCHES]   = {false, false, false, false, false};
static bool justReleased[MAX_TOUCHES]  = {false, false, false, false, false};
static Point lastPosition[MAX_TOUCHES] = {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}};
static uint64_t lastRead = 0;;


constexpr uint8_t MAX_UPDATE_PER_SEC = 60;

void init() {
    touch.begin();
    touch.setRotation((ROTATION_NORMAL + 2) % 4);
}

void update() {
    touch.read();

    for (int i = 0 ; i < MAX_TOUCHES ; i++) {
        if (touch.touches > i) {
            TP_Point point = touch.points[i];
            lastPosition[i] = {point.x, point.y};
            justTouched[i] = !touched[i];
            touched[i] = true;
            justReleased[i] = false;
        }
        else {
            justReleased[i] = touched[i];
            touched[i] = false;
            justTouched[i] = false;
        }   
    }
}

// does not update "just" state
void softUpdate() {
    if (lastRead + 1000 / MAX_UPDATE_PER_SEC < millis() ){
        touch.read();
        lastRead = millis();
    }
    for (int i = 0 ; i < touch.touches ; i++) {
        TP_Point point = touch.points[i];
        lastPosition[i] = {point.x, point.y};
    }
}

bool isTouched(uint8_t num) {
    softUpdate();
    return num < touch.touches;
}

uint8_t getTouchCount() {
    return touch.touches;
}

bool getTouch(Point &p, uint8_t index) {
    assert(index < 5);
    softUpdate();

    p = lastPosition[index];
    return touch.touches > index;
}

bool isJustReleased(uint8_t index) {
    assert(index < 5);
    return justReleased[index];
}

Point getLastPosition(uint8_t index) {
    assert(index < 5);
    return lastPosition[index];
}

}  // namespace touch
}  // namespace sys
