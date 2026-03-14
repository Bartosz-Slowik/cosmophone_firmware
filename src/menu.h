#pragma once

enum class AppMode {
    SNAKE,
    FLAPPY,
    RAINBOW,
    COLOR_FLASH,
};

// Blocks until the user selects an item, then returns the chosen mode.
AppMode showMenu();
