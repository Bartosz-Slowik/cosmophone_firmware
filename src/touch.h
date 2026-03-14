#pragma once

void touchInit();

// Returns true and sets x, y (screen coordinates) when the screen is touched.
bool getTouch(int &x, int &y);
