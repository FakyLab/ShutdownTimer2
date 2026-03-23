#pragma once

// Pure-C bridge to the Objective-C++ Apple Event implementation.
// Callable from any .cpp file — no Carbon, no Objective-C headers needed.
//
// Sends a Core Event directly to the system process using NSAppleEventDescriptor
// (Foundation framework). This is the same path the Apple menu uses for
// Shut Down / Restart / Sleep — no TCC prompt, no root, no osascript.
// Works on macOS 10.6 → 15 Sequoia, including from headless LaunchAgents.

#ifdef __cplusplus
extern "C" {
#endif

// Event ID constants — mirrors the Carbon AE values so callers don't need
// to import Carbon headers. These are fixed ABI constants from the Apple
// Event spec and will never change.
//   kAEShutDown      = 'shut' = 0x73687574
//   kAERestart       = 'rest' = 0x72657374
//   kAESleep         = 'slep' = 0x736C6570
//   kAEReallyLogOut  = 'rlgo' = 0x726C676F
#define MACOS_AE_SHUTDOWN    0x73687574u
#define MACOS_AE_RESTART     0x72657374u
#define MACOS_AE_SLEEP       0x736C6570u
#define MACOS_AE_REALLY_LOGOUT 0x726C676Fu

// Send a Core Event to the system process (loginwindow / kernel power manager).
// eventID: one of the MACOS_AE_* constants above.
// Returns 1 on success (OSStatus noErr), 0 on failure.
int MacOSSendSystemAppleEvent(unsigned int eventID);

#ifdef __cplusplus
}
#endif
