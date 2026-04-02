#pragma once

namespace sys::userconfig {

struct UserConfig {
  char name[64];
  char wifi_ssid[32];
  char wifi_pass[64];
};

bool init();

UserConfig &userConfig();

bool save();

}  // namespace sys::userconfig
