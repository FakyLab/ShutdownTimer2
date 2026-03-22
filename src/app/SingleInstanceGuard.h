#pragma once

#include <QObject>
#include <QLocalServer>
#include <QString>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

// -- SingleInstanceGuard --
// Guarantees only one instance of the application runs at a time.
//
// Detection strategy (platform-specific):
//
//   Windows:
//     Uses CreateMutex() with a unique name. The OS guarantees this is
//     atomic and works correctly regardless of UAC elevation level.
//     If the mutex already exists (ERROR_ALREADY_EXISTS), another instance
//     is running. The mutex is automatically released when the process exits.
//
//   Linux / macOS:
//     Uses QLocalServer. If listen() fails, tries to connect as a client
//     to distinguish a live server from a stale socket file after a crash.
//
// "Bring to front" communication (all platforms):
//     The first instance runs a QLocalServer.
//     The second instance connects and sends "activate", then exits.
//     The first instance emits activateRequested() which MainWindow
//     connects to bringToFront().

class SingleInstanceGuard : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstanceGuard(const QString& appId,
                                 QObject*       parent = nullptr);
    ~SingleInstanceGuard() override;

    bool isFirstInstance() const { return m_isFirst; }

signals:
    void activateRequested();

private slots:
    void onNewConnection();

private:
    bool tryBecomeFirst();
    void startServer();

    QString       m_appId;
    bool          m_isFirst = false;
    QLocalServer* m_server  = nullptr;

#if defined(Q_OS_WIN)
    HANDLE m_mutex = nullptr;
#endif

    static constexpr char kActivateMessage[] = "activate";
};
