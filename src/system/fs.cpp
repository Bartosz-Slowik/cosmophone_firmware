#include "fs.h"
#include <LittleFS.h>
#include <climits>
#include <cstdio>

namespace sys::fs {

bool is_mounted = false;

namespace {

// Partition name column in cosmophone_16MB.csv (subtype 0x83).
const char littlefs_partition[] = "littlefs";
const char littlefs_mount[] = "/littlefs";

bool joinFsPath(char *out, size_t cap, const char *relative) {
  if (relative == nullptr || relative[0] == '/') return false;
  const int n = snprintf(out, cap, "/%s", relative);
  return n > 0 && n < (int)cap;
}

}  // namespace

const char *mountPoint() { return littlefs_mount; }

bool mount(bool format_if_mount_failed) {
  if (is_mounted) return true;
  if (!LittleFS.begin(format_if_mount_failed, littlefs_mount, 10, littlefs_partition)) {
    return false;
  }
  is_mounted = true;
  return true;
}

bool init(bool format_if_mount_failed) { return mount(format_if_mount_failed); }

void unmount() {
  if (!is_mounted) return;
  LittleFS.end();
  is_mounted = false;
}

bool readWholeFile(const char *path, void *dest, size_t expected_bytes) {
  if (!path || !dest || expected_bytes == 0) return false;
  if (expected_bytes > (size_t)INT_MAX) return false;
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  const int fsize = f.size();
  if (fsize < 0 || (size_t)fsize != expected_bytes) {
    f.close();
    return false;
  }
  const size_t n = f.read((uint8_t *)dest, expected_bytes);
  f.close();
  return n == expected_bytes;
}

bool writeWholeFile(const char *path, const void *src, size_t bytes) {
  if (!path || !src || bytes == 0) return false;
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const size_t n = f.write((const uint8_t *)src, bytes);
  f.close();
  return n == bytes;
}

bool readRelative(const char *relative_path, void *dest, size_t expected_bytes) {
  char path[80];
  if (!joinFsPath(path, sizeof(path), relative_path)) return false;
  return readWholeFile(path, dest, expected_bytes);
}

bool writeRelative(const char *relative_path, const void *src, size_t bytes) {
  char path[80];
  if (!joinFsPath(path, sizeof(path), relative_path)) return false;
  return writeWholeFile(path, src, bytes);
}

}
