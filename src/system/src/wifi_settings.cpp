#include "wifi_settings.h"
#include "display.h"
#include "touch.h"
#include "keyboard.h"
#include <WiFi.h>
#include <Preferences.h>

namespace sys {
namespace wifi_settings {

// ── Palette (prefixed to avoid Arduino_GFX macro collisions) ─────────────────
static const uint16_t C_BG      = 0x0841;
static const uint16_t C_HEADER  = 0x1082;
static const uint16_t C_CYAN    = 0x07FF;
static const uint16_t C_BTN     = 0x2145;
static const uint16_t C_ROW     = 0x3186;
static const uint16_t C_ROWSEL  = 0x4A6F;
static const uint16_t C_WHITE   = 0xFFFF;
static const uint16_t C_GREY    = 0x7BEF;
static const uint16_t C_GREEN   = 0x07E0;
static const uint16_t C_RED     = 0xF800;

// ── Fixed layout zones (480 x 480) ──────────────────────────────────────────
//  [  0.. 55]  Header: back button + title
//  [ 56.. 59]  1px cyan line
//  [ 60..103]  Toolbar: [Scan] button + status label
//  [108..457]  Network list: 7 rows × 50px each = 350px
//  [460..479]  Footer status bar
static const int HDR_H       = 56;
static const int TOOL_Y      = 60;
static const int TOOL_H      = 44;
static const int LIST_Y      = 108;
static const int ROW_H       = 50;
static const int MAX_VISIBLE  = 7;
static const int LIST_HT     = MAX_VISIBLE * ROW_H;
static const int FOOTER_Y    = 460;

static const int SCAN_X = 12, SCAN_Y = TOOL_Y, SCAN_W = 120, SCAN_H = TOOL_H;
static const int STATUS_X = 148, STATUS_Y = TOOL_Y + 12;

static const char *PREFS_NS  = "wifi";
static const char *PKEY_SSID = "ssid";
static const char *PKEY_PASS = "pass";

// ── State ────────────────────────────────────────────────────────────────────
static int     nwCount = 0;
static String  nwSsids[16];
static int     nwRssi[16];
static bool    nwOpen[16];

// ── Drawing ──────────────────────────────────────────────────────────────────
static void drawHeader() {
    gfx->fillRect(0, 0, 480, HDR_H, C_HEADER);
    sys::display::drawBackButton(C_BTN, C_CYAN, C_WHITE);
    gfx->setTextColor(C_CYAN);
    gfx->setTextSize(3);
    gfx->setCursor(84, 16);
    gfx->print("Wi-Fi Setup");
    gfx->drawFastHLine(0, HDR_H, 480, C_CYAN);
}

static void drawScanButton(bool hl = false) {
    uint16_t bg = hl ? C_ROWSEL : C_BTN;
    gfx->fillRoundRect(SCAN_X, SCAN_Y, SCAN_W, SCAN_H, 8, bg);
    gfx->drawRoundRect(SCAN_X, SCAN_Y, SCAN_W, SCAN_H, 8, C_CYAN);
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(SCAN_X + 24, SCAN_Y + 12);
    gfx->print("Scan");
}

static void drawStatus(const char *msg, uint16_t color = C_GREY) {
    gfx->fillRect(STATUS_X, SCAN_Y, 480 - STATUS_X - 4, SCAN_H, C_BG);
    gfx->setTextColor(color);
    gfx->setTextSize(2);
    gfx->setCursor(STATUS_X, STATUS_Y);
    gfx->print(msg);
}

static void drawFooter(const char *msg, uint16_t color = C_GREY) {
    gfx->fillRect(0, FOOTER_Y, 480, 20, C_BG);
    gfx->setTextColor(color);
    gfx->setTextSize(1);
    gfx->setCursor(12, FOOTER_Y + 4);
    gfx->print(msg);
}

static void drawNetworkList(int selected) {
    gfx->fillRect(0, LIST_Y, 480, LIST_HT, C_BG);
    int visible = nwCount < MAX_VISIBLE ? nwCount : MAX_VISIBLE;
    for (int i = 0; i < visible; i++) {
        int y = LIST_Y + i * ROW_H;
        uint16_t bg = (selected == i) ? C_ROWSEL : C_ROW;
        gfx->fillRoundRect(12, y + 1, 456, ROW_H - 3, 6, bg);
        gfx->drawRoundRect(12, y + 1, 456, ROW_H - 3, 6, C_CYAN);

        gfx->setTextColor(nwOpen[i] ? C_GREEN : C_GREY);
        gfx->setTextSize(2);
        gfx->setCursor(22, y + 14);
        gfx->print(nwOpen[i] ? "o" : "*");

        gfx->setTextColor(C_WHITE);
        gfx->setTextSize(2);
        String ssid = nwSsids[i];
        if (ssid.length() == 0) ssid = "(hidden)";
        if ((int)ssid.length() > 20) ssid = ssid.substring(0, 19) + "..";
        gfx->setCursor(44, y + 14);
        gfx->print(ssid);

        gfx->setTextColor(C_GREY);
        gfx->setTextSize(1);
        char rssi[16];
        snprintf(rssi, sizeof(rssi), "%d dBm", nwRssi[i]);
        gfx->setCursor(400, y + 18);
        gfx->print(rssi);
    }
}

static void drawFullScreen(int selected) {
    gfx->fillScreen(C_BG);
    drawHeader();
    drawScanButton();
    drawNetworkList(selected);
    sys::display::flush();
}

// ── Touch helpers ────────────────────────────────────────────────────────────
static bool scanTapped(int tx, int ty) {
    return tx >= SCAN_X && tx < SCAN_X + SCAN_W &&
           ty >= SCAN_Y && ty < SCAN_Y + SCAN_H;
}

static int listTapped(int tx, int ty) {
    if (tx < 12 || tx >= 468 || ty < LIST_Y || ty >= LIST_Y + LIST_HT) return -1;
    int idx = (ty - LIST_Y) / ROW_H;
    return (idx < nwCount && idx < MAX_VISIBLE) ? idx : -1;
}

// ── Main loop ────────────────────────────────────────────────────────────────
void run() {
    nwCount = 0;
    drawFullScreen(-1);
    drawStatus("Tap Scan to find networks");
    drawFooter("Wi-Fi settings  v1.0");
    sys::display::flush();

    bool hlScan = false;
    int  hlRow  = -1;

    while (true) {
        int tx, ty;

        if (sys::touch::getTouch(tx, ty)) {
            bool scan = scanTapped(tx, ty);
            int  row  = listTapped(tx, ty);

            bool changed = false;
            if (scan != hlScan) { hlScan = scan; drawScanButton(hlScan); changed = true; }
            if (row != hlRow)   { hlRow = row; drawNetworkList(hlRow); changed = true; }
            if (changed) sys::display::flush();
            delay(10);
            continue;
        }

        int rx, ry;
        if (!sys::touch::getReleased(rx, ry)) { delay(10); continue; }

        int relRow = listTapped(rx, ry);
        bool relScan = scanTapped(rx, ry);

        bool changed2 = false;
        if (hlScan && !relScan) { hlScan = false; drawScanButton(false); changed2 = true; }
        if (hlRow >= 0 && hlRow != relRow) { hlRow = -1; drawNetworkList(-1); changed2 = true; }
        if (changed2) sys::display::flush();
        hlScan = false;
        hlRow = -1;

        if (sys::display::backButtonTapped(rx, ry)) return;

        if (relScan) {
            drawStatus("Scanning...", C_CYAN);
            sys::display::flush();
            WiFi.mode(WIFI_STA);
            WiFi.scanDelete();
            WiFi.scanNetworks(true);

            int found = WIFI_SCAN_RUNNING;
            while (found == WIFI_SCAN_RUNNING) {
                delay(50);
                found = WiFi.scanComplete();
            }

            if (found < 0) found = 0;
            nwCount = found > 16 ? 16 : found;
            for (int i = 0; i < nwCount; i++) {
                nwSsids[i]  = WiFi.SSID(i);
                nwRssi[i]   = WiFi.RSSI(i);
                nwOpen[i]   = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
            }
            WiFi.scanDelete();
            drawNetworkList(-1);
            if (nwCount == 0) drawStatus("No networks found", C_RED);
            else {
                char buf[32];
                snprintf(buf, sizeof(buf), "Found %d network%s", nwCount, nwCount == 1 ? "" : "s");
                drawStatus(buf, C_GREEN);
            }
            sys::display::flush();
            continue;
        }

        if (relRow >= 0) {
            int idx = relRow;

            String password;
            if (!nwOpen[idx]) {
                char rssiStr[32];
                snprintf(rssiStr, sizeof(rssiStr), "Signal: %d dBm", nwRssi[idx]);
                String kbTitle = "SSID: " + nwSsids[idx];
                if (!sys::keyboard::run(password, kbTitle.c_str(), rssiStr)) {
                    drawFullScreen(-1);
                    drawStatus("Cancelled");
                    sys::display::flush();
                    continue;
                }
                drawFullScreen(idx);
            }

            drawStatus("Connecting...", C_CYAN);
            sys::display::flush();
            WiFi.begin(nwSsids[idx].c_str(), password.c_str());
            int tries = 0;
            while (WiFi.status() != WL_CONNECTED && tries < 100) {
                delay(200);
                tries++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                Preferences prefs;
                prefs.begin(PREFS_NS, false);
                prefs.putString(PKEY_SSID, nwSsids[idx]);
                prefs.putString(PKEY_PASS, password);
                prefs.end();
                char buf[48];
                snprintf(buf, sizeof(buf), "Connected: %s", WiFi.localIP().toString().c_str());
                drawStatus(buf, C_GREEN);
                drawFooter("Saved to NVS", C_GREEN);
            } else {
                drawStatus("Connection failed", C_RED);
            }
            drawNetworkList(-1);
            sys::display::flush();
        }
    }
}

}  // namespace wifi_settings
}  // namespace sys
