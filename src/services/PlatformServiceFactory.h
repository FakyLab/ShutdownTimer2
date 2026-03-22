#pragma once

#include <QObject>
#include "interfaces/IShutdownBackend.h"
#include "interfaces/IMessageBackend.h"
#include "interfaces/IAutoClearBackend.h"

// Bundles all three platform-specific backend instances plus
// pre-queried hardware capability flags.
// Created once in main.cpp and injected into controllers.
struct PlatformServices {
    IShutdownBackend*  shutdown        = nullptr;
    IMessageBackend*   message         = nullptr;
    IAutoClearBackend* autoClear       = nullptr;

    // Queried once at factory creation time on the main thread.
    // Cached here so TimerController returns them instantly without
    // ever calling the backend again after startup.
    bool hibernateAvailable = false;
    bool sleepAvailable     = false;
};

class PlatformServiceFactory
{
public:
    // Creates backends and queries hardware capabilities.
    // All returned objects are parented to |parent| for lifetime management.
    // Asserts at runtime if called on an unsupported platform.
    static PlatformServices create(QObject* parent = nullptr);
};
