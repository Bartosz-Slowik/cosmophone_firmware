#include <Arduino.h>
#include "system/display.h"
#include "system/touch.h"
#include "system/menu.h"
#include "apps/rainbow/rainbow.h"

void setup() {
    Serial0.begin(115200);
    sys::display::init();
    sys::touch::init();
}

void loop() {
    using namespace sys::menu;
    AppMode selectedMode = showMenu();
    switch (selectedMode) {
        case AppMode::RAINBOW: apps::rainbow::run(); break;
        default: break;
    }
}
