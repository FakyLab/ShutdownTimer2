#include "SingleInstanceGuard.h"
#include <QLocalSocket>

SingleInstanceGuard::SingleInstanceGuard(const QString& appId, QObject* parent)
    : QObject(parent)
    , m_appId(appId)
{
    m_isFirst = tryBecomeFirst();

    if (m_isFirst) {
        // First instance - start the server so future second instances
        // can send us the "activate" message to bring us to front.
        startServer();
    } else {
        // Another instance is running - tell it to come forward.
        QLocalSocket socket;
        socket.connectToServer(m_appId);
        if (socket.waitForConnected(3000)) {
            socket.write(kActivateMessage);
            socket.flush();
            socket.waitForBytesWritten(3000);
            socket.waitForDisconnected(1000);
        }
    }
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

#if defined(Q_OS_WIN)
    if (m_mutex) {
        ReleaseMutex(m_mutex);
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }
#endif
}

// -- tryBecomeFirst --

bool SingleInstanceGuard::tryBecomeFirst()
{
#if defined(Q_OS_WIN)
    // Windows: use CreateMutex for detection.
    //
    // QLocalServer::listen() is NOT used for detection on Windows because
    // it has a known issue where it silently succeeds even when a pipe with
    // the same name already exists, due to elevated vs non-elevated processes
    // using different pipe namespaces. This would let two instances both
    // think they are first.
    //
    // CreateMutex is atomic and OS-guaranteed. ERROR_ALREADY_EXISTS means
    // another instance owns the mutex, regardless of elevation level.
    // The mutex is auto-released when the process exits or crashes.
    // "Local\" scopes it to the current login session.

    QString mutexName = "Local\\ShutdownTimer_" + m_appId;
    m_mutex = CreateMutexW(
        nullptr,
        TRUE,  // request ownership immediately
        reinterpret_cast<LPCWSTR>(mutexName.utf16())
    );

    if (m_mutex == nullptr)
        return true; // CreateMutex failed — fail safe, treat as first

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return false; // Another instance owns the mutex — we are second

    return true; // We own the mutex — we are first

#else
    // Linux / macOS: QLocalServer is reliable here (no pipe namespace issue).
    //
    // We create the permanent server directly - detection and server startup
    // happen atomically in one listen() call, eliminating any race window
    // between "check if first" and "start server".
    //
    // Socket files can survive crashes, so if listen() fails we probe to
    // distinguish a live server from a stale file.

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    if (m_server->listen(m_appId)) {
        // Successfully claimed the socket name - we are the first instance.
        // Server is ready; startServer() will connect the newConnection signal.
        return true;
    }

    // listen() failed - name already taken. Probe to check if alive.
    QLocalSocket probe;
    probe.connectToServer(m_appId);
    if (probe.waitForConnected(500)) {
        // Live server responded - we are genuinely the second instance.
        probe.disconnectFromServer();
        return false;
    }

    // No live server - stale socket file from a crash. Remove and retry.
    QLocalServer::removeServer(m_appId);

    if (m_server->listen(m_appId))
        return true;

    // Couldn't claim even after cleanup. Fail safe - don't lock user out.
    return true;
#endif
}

// -- startServer --

void SingleInstanceGuard::startServer()
{
#if defined(Q_OS_WIN)
    // Windows: create the server now (detection was via mutex, not server).
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    if (!m_server->listen(m_appId)) {
        // Failed - app works fine, just no "bring to front" on second launch.
        delete m_server;
        m_server = nullptr;
        return;
    }
#endif

    // Linux/macOS: m_server already created and listening in tryBecomeFirst().
    // Just connect the signal here.
    if (m_server) {
        connect(m_server, &QLocalServer::newConnection,
                this,     &SingleInstanceGuard::onNewConnection);
    }
}

// -- onNewConnection --

void SingleInstanceGuard::onNewConnection()
{
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        if (socket->readAll().trimmed() == QByteArray(kActivateMessage))
            emit activateRequested();
        socket->disconnectFromServer();
    });

    connect(socket, &QLocalSocket::disconnected,
            socket, &QLocalSocket::deleteLater);
}
