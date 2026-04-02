#pragma once

#include <stddef.h>

namespace sys::fs {

/// Set by `init`/`mount`, cleared by `unmount`.
extern bool is_mounted;

/// VFS path where LittleFS is mounted (partition `littlefs` in cosmophone_16MB.csv).
const char *mountPoint();

/// Mount LittleFS once at startup. Safe to call again if already mounted.
bool init(bool format_if_mount_failed = true);
bool mount(bool format_if_mount_failed = true);
void unmount();

bool readWholeFile(const char *path, void *dest, size_t expected_bytes);
bool writeWholeFile(const char *path, const void *src, size_t bytes);
bool readRelative(const char *relative_path, void *dest, size_t expected_bytes);
bool writeRelative(const char *relative_path, const void *src, size_t bytes);

}  // namespace sys::fs
