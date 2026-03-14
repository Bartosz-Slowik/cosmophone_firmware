#include <Arduino.h>
#include "display.h"
#include "touch.h"
#include "menu.h"
#include "snake.h"
#include "flapbird.h"
#include "demos.h"

void setup() {
    Serial.begin(115200);
    displayInit();
    touchInit();
}

void loop() {
    switch (showMenu()) {
        case AppMode::SNAKE:       runSnake();       break;
        case AppMode::FLAPPY:      runFlappyBird();  break;
        case AppMode::RAINBOW:     runRainbow();     break;
        case AppMode::COLOR_FLASH: runColorFlash();  break;
    }
}
