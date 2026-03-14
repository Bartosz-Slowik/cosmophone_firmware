#pragma once

namespace sys {
namespace wifi_settings {

// Shows WiFi setup UI: scan, list networks, select SSID, enter password with
// on-screen keyboard, connect and save to NVS. Returns when user goes back.
void run();

}  // namespace wifi_settings
}  // namespace sys
