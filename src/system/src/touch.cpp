#include "touch.h"
#include <Wire.h>
#include "Touch_GT911.h"

namespace sys {
namespace touch {

static Touch_GT911 _ts(19, 45, -1, -1, 480, 480);

static bool _prevTouching = false;
static bool _released     = false;
static int  _lastX = 0;
static int  _lastY = 0;

void init() {
    Wire.begin(19, 45);
    _ts.begin();
    _ts.setRotation(ROTATION_NORMAL);
}

bool getTouch(int &x, int &y) {
    _ts.read();
    bool touching = _ts.isTouched && _ts.touches > 0;

    // Detect release transition: was touching, now not
    _released = (_prevTouching && !touching);
    _prevTouching = touching;

    if (touching) {
        x = 479 - _ts.points[0].x;
        y = 479 - _ts.points[0].y;
        _lastX = x;
        _lastY = y;
        return true;
    }
    return false;
}

bool getReleased(int &x, int &y) {
    if (_released) {
        _released = false;
        x = _lastX;
        y = _lastY;
        return true;
    }
    return false;
}

}  // namespace touch
}  // namespace sys
