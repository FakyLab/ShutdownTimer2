#pragma once

// Pure-C interface to the Objective-C++ notification implementation.
// Callable from any .cpp file without needing Objective-C++ headers.
//
// Uses UNUserNotificationCenter (macOS 10.14+) to post a native desktop
// notification attributed to "Shutdown Timer" — not to osascript.
// Requests notification permission on first call if not yet granted.

#ifdef __cplusplus
extern "C" {
#endif

// Post a notification with the given title and body.
// Requests permission automatically if not yet granted.
// Blocks until the notification is delivered (or fails) — suitable for
// headless LaunchAgent use where the process exits immediately after.
// Returns 1 on success, 0 on failure.
int MacOSPostNotification(const char* title, const char* body);

#ifdef __cplusplus
}
#endif
