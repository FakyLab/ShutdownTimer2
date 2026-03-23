#import <Foundation/Foundation.h>
#include "MacOSSystemEvents.h"

// -- MacOSSendSystemAppleEvent ------------------------------------------------
//
// Sends a Core Event to the system process using NSAppleEventDescriptor.
// This uses Foundation only — no Carbon, no ProcessSerialNumber, no
// kSystemProcess constant from CarbonCore/Processes.h.
//
// The target is PSN {highLong=0, lowLong=1} — this is the reserved Process
// Serial Number for "the system process" (loginwindow / kernel power manager).
// lowLongOfPSN=1 is kSystemProcess, a fixed ABI constant since System 7.
// We use the literal value 1 to avoid importing Carbon headers.
//
// NSAppleEventDescriptor is available on macOS 10.2+ (Foundation.framework).
// It is the recommended API per Apple DTS engineer "Quinn the Eskimo" in:
//   https://developer.apple.com/forums/thread/90702
//
// kCoreEventClass = 'aevt' = 0x61657674 — fixed Apple Event constant.

int MacOSSendSystemAppleEvent(unsigned int eventID)
{
    @autoreleasepool {
        // Build the target descriptor: PSN { 0, kSystemProcess=1 }
        // typeProcessSerialNumber = 'psn ' — fixed ABI, safe to hardcode.
        struct { uint32_t high; uint32_t low; } psn = { 0, 1 };

        NSAppleEventDescriptor* target =
            [NSAppleEventDescriptor
                descriptorWithDescriptorType: 0x70736E20  // 'psn '
                bytes: &psn
                length: sizeof(psn)];

        // kCoreEventClass = 'aevt' = 0x61657674
        NSAppleEventDescriptor* event =
            [NSAppleEventDescriptor
                appleEventWithEventClass: 0x61657674
                eventID:               eventID
                targetDescriptor:      target
                returnID:              (AEReturnID)kAutoGenerateReturnID
                transactionID:         (AETransactionID)kAnyTransactionID];

        // kAENoReply = 1: fire and forget — we don't wait for a response.
        // The machine may shut down before a reply arrives.
        NSError* err = nil;
        [event sendEventWithOptions: NSAppleEventSendNoReply
                            timeout: kAEDefaultTimeout
                              error: &err];

        return (err == nil) ? 1 : 0;
    }
}
