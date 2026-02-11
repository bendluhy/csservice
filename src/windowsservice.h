#ifndef WINDOWSSERVICE_H
#define WINDOWSSERVICE_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <windows.h>
#include "Logger.h"
#include "namedpipeserver.h"
#include "commandproc.h"
#include "monitor.h"
#include "ecmemorymirror.h"
#include "securecommandhandler.h"
#include "bezel.h"

#define SHUTDOWN_TIMEOUT_MS 10000

class WindowsService : public QObject
{
    Q_OBJECT

public:
    WindowsService(const QString &serviceName, QObject *parent = nullptr);
    ~WindowsService();

    bool install();
    bool uninstall();
    void start();
    void stop();
    void runAsService();
    void runAsApp();
    //EC HELPERS
    void clearEcState();
    void testEcCommunication();
    static void WINAPI serviceMain(DWORD argc, LPTSTR *argv);
    static void WINAPI serviceCtrlHandler(DWORD ctrlCode);

private slots:
    void onShutdownTimeout();

    // Pipe event handlers
    void onControlScreensCommand(const QByteArray& data, QLocalSocket* client);
    void onCSMonitorCommand(const QByteArray& data, QLocalSocket* client);
    void onClientConnected(PipeType pipeType, QLocalSocket* client);
    void onClientDisconnected(PipeType pipeType, QLocalSocket* client);

private:
    void setServiceStatus(DWORD currentState, DWORD win32ExitCode = NO_ERROR, DWORD waitHint = 0);
    void cleanup();
    bool initializeService();

    QString m_serviceName;
    SERVICE_STATUS_HANDLE m_serviceStatusHandle;
    SERVICE_STATUS m_serviceStatus;

    QMutex m_mutex;
    volatile bool m_running;
    volatile bool m_shuttingDown;

    Logger m_logger;
    CommandProc m_commandProc;
    NamedPipeServer* m_pipeServer;
    SecureCommandHandler* m_secureHandler;  // Changed type name
    BezelMonitor* m_bezelMonitor;
    Monitor* m_monitor;
    ECMemoryWriter* m_ecMemoryWriter;
    QTimer* m_shutdownTimer;

    static QMutex s_globalMutex;
};

#endif // WINDOWSSERVICE_H
