#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include "MacOSNotifier.h"

// -- MacOSPostNotification ----------------------------------------------------
//
// Posts a UNUserNotification attributed to this app (ShutdownTimer), not to
// osascript. This is the same API that Calendar, Reminders, etc. use for
// their alerts — the notification appears in Notification Center with the
// app icon and name.
//
// Permission flow:
//   First call: UNUserNotificationCenter requests authorization. On macOS 10.14+
//   this shows a one-time system dialog "Shutdown Timer wants to send you
//   notifications". After the user grants permission, all future calls work
//   silently. If denied, the notification is silently dropped (no crash).
//
// Threading:
//   UNUserNotificationCenter callbacks are async. We use a semaphore to block
//   until delivery is confirmed — required because the LaunchAgent process
//   exits immediately after this call, and an async delivery would be
//   cancelled when the process dies.

int MacOSPostNotification(const char* title, const char* body)
{
    @autoreleasepool {
        UNUserNotificationCenter* center =
            [UNUserNotificationCenter currentNotificationCenter];

        // --- Step 1: Request authorization (no-op if already granted/denied) ---
        dispatch_semaphore_t authSem = dispatch_semaphore_create(0);
        __block BOOL granted = NO;

        [center requestAuthorizationWithOptions:
            (UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
            completionHandler:^(BOOL g, NSError* __unused err) {
                granted = g;
                dispatch_semaphore_signal(authSem);
            }];

        // Wait up to 10 s for the user to respond to the permission dialog.
        // If this is not the first time (already granted/denied), this returns
        // immediately with the cached decision.
        dispatch_semaphore_wait(authSem,
            dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

        if (!granted)
            return 0;

        // --- Step 2: Build and post the notification ---
        UNMutableNotificationContent* content =
            [[UNMutableNotificationContent alloc] init];

        if (title && strlen(title) > 0)
            content.title = [NSString stringWithUTF8String:title];
        if (body && strlen(body) > 0)
            content.body  = [NSString stringWithUTF8String:body];

        content.sound = [UNNotificationSound defaultSound];

        // Trigger immediately — no time interval, no repeat.
        UNTimeIntervalNotificationTrigger* trigger =
            [UNTimeIntervalNotificationTrigger
                triggerWithTimeInterval:0.1
                repeats:NO];

        NSString* requestID = @"com.fakylab.shutdowntimer.startup-message";
        UNNotificationRequest* request =
            [UNNotificationRequest requestWithIdentifier:requestID
                                                 content:content
                                                 trigger:trigger];

        dispatch_semaphore_t deliverSem = dispatch_semaphore_create(0);
        __block BOOL delivered = NO;

        [center addNotificationRequest:request
            withCompletionHandler:^(NSError* err) {
                delivered = (err == nil);
                dispatch_semaphore_signal(deliverSem);
            }];

        // Wait up to 5 s for the notification to be queued by the system.
        dispatch_semaphore_wait(deliverSem,
            dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

        return delivered ? 1 : 0;
    }
}
