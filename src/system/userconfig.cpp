#include "userconfig.h"
#include "fs.h"
#include <cstring>

namespace sys::userconfig {

namespace {

UserConfig g_user{};
const char user_profile_file[] = "userconfig.bin";

}  // namespace

bool init() {
  if (!sys::fs::is_mounted) {
    memset(&g_user, 0, sizeof(g_user));
    return false;
  }
  if (!sys::fs::readRelative(user_profile_file, &g_user, sizeof(UserConfig))) {
    memset(&g_user, 0, sizeof(g_user));
    return sys::fs::writeRelative(user_profile_file, &g_user, sizeof(UserConfig));
  }
  return true;
}

UserConfig &userConfig() { return g_user; }

bool save() {
  return sys::fs::writeRelative(user_profile_file, &g_user, sizeof(UserConfig));
}

}  // namespace sys::userconfig
