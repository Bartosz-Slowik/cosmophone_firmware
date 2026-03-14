#pragma once

namespace sys {
namespace touch {

void init();

// Returns true and sets x, y (screen coordinates) when the screen is touched.
bool getTouch(int &x, int &y);

// Returns true once when the finger lifts off the screen.
// x, y are set to the last known touch position before release.
bool getReleased(int &x, int &y);

}  // namespace touch
}  // namespace sys
