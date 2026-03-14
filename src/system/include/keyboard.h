#pragma once

#include <Arduino.h>

namespace sys {
namespace keyboard {

// On-screen keyboard. Returns true if user confirmed (Done), false if cancelled.
// out: filled with entered text on success; can be pre-filled as initial value.
// title: main heading shown above the text field.
// subtitle: optional second line below the title (e.g. signal strength).
bool run(String& out, const char* title = "Enter text", const char* subtitle = nullptr);

}  // namespace keyboard
}  // namespace sys
