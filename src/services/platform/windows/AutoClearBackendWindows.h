#pragma once

#include "../../interfaces/IAutoClearBackend.h"

class AutoClearBackendWindows : public IAutoClearBackend
{
    Q_OBJECT
public:
    explicit AutoClearBackendWindows(QObject* parent = nullptr);
    ~AutoClearBackendWindows() override;

    bool schedule() override;
    bool cancel()   override;
    bool exists()   override;

    QString lastError() const override { return m_lastError; }

private:
    // Initializes COM on first use - never called at construction.
    // Subsequent calls are a no-op (m_comInitialized guard).
    // This means COM is only initialized when the user actually
    // interacts with the Startup Message tab, not at app startup.
    bool ensureCOM();
    void uninitCOM();

    bool    m_comInitialized = false;
    QString m_lastError;

    static constexpr wchar_t kTaskName[]   = L"ShutdownTimerAutoClearMsg";
    static constexpr wchar_t kTaskFolder[] = L"\\";
};
