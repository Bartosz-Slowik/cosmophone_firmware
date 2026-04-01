#pragma once
#include <Arduino.h>

namespace sys {
namespace touch {

constexpr uint8_t MAX_TOUCHES = 5;

struct Point {
    uint16_t x;
    uint16_t y;
};

void init();

// call every frame
void update();

// Returns true if screen is touched by 'num' fingers
bool isTouched(uint8_t num = 0);

// Returns true if screen is touched.
// Sets 'x' and 'y' to last known touch position (current touch position if returned true)
bool getTouch(Point &p, uint8_t index = 0);

uint8_t getTouchCount();

// Returns true only in frame in which touch was released
bool isJustReleased(uint8_t index);
// Returns true only in frame in which touch was pressed
bool isJustPressed(uint8_t index);

}  // namespace touch
}  // namespace sys
