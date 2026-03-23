#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int MacOSPostNotification(const char* title, const char* body);
int MacOSEnsureNotificationAuthorization(void);

#ifdef __cplusplus
}
#endif
