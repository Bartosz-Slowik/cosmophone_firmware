#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

constexpr uint8_t GFX_BL = 38;
constexpr int FB_W = SCREEN_W;
constexpr int FB_H = SCREEN_H;
constexpr int ROW_BYTES = FB_W * 2;

static Arduino_ESP32RGBPanel *_rgb_panel = new Arduino_ESP32RGBPanel(
    18, 17, 16, 21,

    11, 12, 13, 14, 0,
    8,  20, 3,  46, 9, 10,
    4,  5,  6,  7,  15,

    1,  10, 8,  50,
    1,  10, 8,  20,
    
    0, 16000000, false
);

//Arduino_SWSPI
static Arduino_DataBus *_databus = new Arduino_SWSPI(//new Arduino_ESP32SPIDMA(
    GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED
); 

static Arduino_RGB_Display *_display = new Arduino_RGB_Display(
    FB_W, FB_H, _rgb_panel, 0, false,
    _databus, GFX_NOT_DEFINED,
    st7701_type9_init_operations, sizeof(st7701_type9_init_operations),
    0, 0, 0, 0
);

Arduino_Canvas *canvas = new Arduino_Canvas(FB_W, FB_H, _display);

Arduino_GFX *gfx = canvas;


namespace sys {
namespace display {

void init() {
    Serial0.begin(115200);

    // backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    Serial0.println("gfx->begin()");
    if(!gfx->begin(16000000)) {
        Serial0.println("[error] Cannot initialize gfx");
        while(1);
    }

    Serial0.println("gfx started");
}

// ----- Back button -----
constexpr int BACK_BTN_X = 4;
constexpr int BACK_BTN_Y = 4;
constexpr int BACK_BTN_W = 44;
constexpr int BACK_BTN_H = 44;

void drawBackButton(uint16_t bg, uint16_t border, uint16_t textColor) {
    gfx->fillRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 8, bg);
    gfx->drawRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 8, border);
    gfx->setTextColor(textColor);
    gfx->setTextSize(3);
    gfx->setCursor(BACK_BTN_X + 12, BACK_BTN_Y + 10);
    gfx->print("<");
}

bool backButtonTapped(int tx, int ty) {
    return tx >= BACK_BTN_X && tx < BACK_BTN_X + BACK_BTN_W &&
           ty >= BACK_BTN_Y && ty < BACK_BTN_Y + BACK_BTN_H;
}

uint16_t *getDirectFramebuffer() {
    return _display->getFramebuffer();
}

uint16_t *getBufferedFramebuffer() {
    return canvas->getFramebuffer();
}

void flushDirect() {
    Cache_WriteBack_Addr((uint32_t)getDirectFramebuffer(), FB_W * FB_H * 2);
}

void flush() {
    gfx->flush();
}

}  // namespace display
}  // namespace sys
