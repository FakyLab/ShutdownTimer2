#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include "MacOSNotifier.h"

static int ensureNotificationAuthorizationInternal(void)
{
    UNUserNotificationCenter* center =
        [UNUserNotificationCenter currentNotificationCenter];

    dispatch_semaphore_t authSem = dispatch_semaphore_create(0);
    __block BOOL granted = NO;

    [center requestAuthorizationWithOptions:
        (UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
        completionHandler:^(BOOL g, NSError* __unused err) {
            granted = g;
            dispatch_semaphore_signal(authSem);
        }];

    dispatch_semaphore_wait(authSem,
        dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));

    return granted ? 1 : 0;
}

int MacOSEnsureNotificationAuthorization(void)
{
    @autoreleasepool {
        return ensureNotificationAuthorizationInternal();
    }
}

int MacOSPostNotification(const char* title, const char* body)
{
    @autoreleasepool {
        if (!ensureNotificationAuthorizationInternal())
            return 0;

        UNUserNotificationCenter* center =
            [UNUserNotificationCenter currentNotificationCenter];

        UNMutableNotificationContent* content =
            [[UNMutableNotificationContent alloc] init];

        if (title && strlen(title) > 0)
            content.title = [NSString stringWithUTF8String:title];
        if (body && strlen(body) > 0)
            content.body  = [NSString stringWithUTF8String:body];

        content.sound = [UNNotificationSound defaultSound];

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

        dispatch_semaphore_wait(deliverSem,
            dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));

        return delivered ? 1 : 0;
    }
}
