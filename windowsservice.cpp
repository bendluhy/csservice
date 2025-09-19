#include "WindowsService.h"
#include <QCoreApplication>
#include <QDebug>
#include <memory>

WindowsService* g_service = nullptr;
QMutex WindowsService::s_globalMutex;

WindowsService::WindowsService(const QString &serviceName, QObject *parent)
    : QObject(parent),
    m_serviceName(serviceName),
    m_serviceStatusHandle(nullptr),
    m_running(false),
    m_shuttingDown(false),
    m_logger("C:\\Logs", this),
    m_commandProc(&m_logger),
    m_pipeServer(nullptr),
    m_secureHandler(nullptr),
    m_monitor(nullptr),
    m_ecMemoryWriter(nullptr),
    m_shutdownTimer(nullptr)
{
    m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    m_serviceStatus.dwWin32ExitCode = NO_ERROR;
    m_serviceStatus.dwServiceSpecificExitCode = 0;
    m_serviceStatus.dwCheckPoint = 0;
    m_serviceStatus.dwWaitHint = 0;

    QMutexLocker locker(&s_globalMutex);
    if (g_service) {
        m_logger.log("WARNING: Multiple WindowsService instances - replacing global pointer");
    }
    g_service = this;
}

WindowsService::~WindowsService()
{
    cleanup();

    QMutexLocker locker(&s_globalMutex);
    if (g_service == this) {
        g_service = nullptr;
    }
}

bool WindowsService::install()
{
    m_logger.log("Service install requested");
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!schSCManager) {
        m_logger.log("OpenSCManager failed: " + QString::number(GetLastError()));
        return false;
    }

    SC_HANDLE schService = CreateService(
        schSCManager,
        m_serviceName.toStdWString().c_str(),
        m_serviceName.toStdWString().c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        QCoreApplication::applicationFilePath().toStdWString().c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    bool success = (schService != nullptr);
    DWORD lastError = GetLastError();

    if (schService) {
        CloseServiceHandle(schService);
        m_logger.log("Service installed successfully");
    } else {
        m_logger.log("CreateService failed: " + QString::number(lastError));
    }

    CloseServiceHandle(schSCManager);
    return success;
}

bool WindowsService::uninstall()
{
    m_logger.log("Service uninstall requested");
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!schSCManager) {
        m_logger.log("OpenSCManager failed: " + QString::number(GetLastError()));
        return false;
    }

    SC_HANDLE schService = OpenService(schSCManager, m_serviceName.toStdWString().c_str(), DELETE);
    bool success = false;

    if (schService) {
        if (DeleteService(schService)) {
            m_logger.log("Service uninstalled successfully");
            success = true;
        } else {
            m_logger.log("DeleteService failed: " + QString::number(GetLastError()));
        }
        CloseServiceHandle(schService);
    } else {
        m_logger.log("OpenService failed: " + QString::number(GetLastError()));
    }

    CloseServiceHandle(schSCManager);
    return success;
}

void WindowsService::start()
{
    m_logger.log("Service start requested");
    std::wstring serviceNameW = m_serviceName.toStdWString();
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<LPWSTR>(serviceNameW.c_str()), serviceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        m_logger.log("StartServiceCtrlDispatcher error: " + QString::number(GetLastError()));
    }
}

void WindowsService::stop()
{
    QMutexLocker locker(&m_mutex);
    m_logger.log("Service stop requested");
    m_running = false;
}

bool WindowsService::initializeService()
{
    m_logger.log("Initializing service components with secure usermode access");

    // Create Secure Command Handler
    m_secureHandler = new SecureCommandHandlerV2(&m_logger, &m_commandProc, this);

    // Start Pipe server with usermode access
    m_pipeServer = new NamedPipeServer(&m_logger, PIPE_NAME, this);

    // Connect pipe server to secure handler
    connect(m_pipeServer, &NamedPipeServer::clientConnected,
        this, [this](QLocalSocket* client) {
            m_logger.log("New client connected - registering with secure handler");
            m_secureHandler->registerClient(client);
        }, Qt::DirectConnection);  // ADD THIS

    connect(m_pipeServer, &NamedPipeServer::clientDisconnected,
        this, [this](QLocalSocket* client) {
            m_logger.log("Client disconnected - unregistering from secure handler");
            m_secureHandler->unregisterClient(client);
        }, Qt::DirectConnection);  // ADD THIS

    connect(m_pipeServer, &NamedPipeServer::commandReceived,
        this, [this](const QByteArray& data, QLocalSocket* client) {
            QMutexLocker locker(&m_mutex);
            if (m_shuttingDown || !m_pipeServer || !m_secureHandler) {
                m_logger.log("Ignoring command - service is shutting down");
                return;
            }
            m_logger.log(QString("Secure command received: %1 bytes").arg(data.size()));
            // Process through secure handler
            QByteArray response = m_secureHandler->processCommand(data, client);
            if (!response.isEmpty() && m_pipeServer) {
                m_pipeServer->sendResponse(client, response);
                m_logger.log(QString("Sent secure response: %1 bytes").arg(response.size()));
            } else {
                m_logger.log("Authentication failed or invalid command - no response sent");
            }
        }, Qt::DirectConnection);  // ADD THIS

    connect(m_pipeServer, &NamedPipeServer::serverError,
        this, [this](const QString& error) {
            m_logger.log("Pipe server error: " + error);
        }, Qt::DirectConnection);  // ADD THIS

    if (!m_pipeServer->start()) {
        m_logger.log("Failed to start pipe server");
        return false;
    }

    // Create EC Memory Writer
    m_ecMemoryWriter = new ECMemoryWriter(&m_logger, this);
    if (!m_ecMemoryWriter->create()) {
        m_logger.log("Failed to create EC memory writer");
        return false;
    }

    // Start Monitors
    m_monitor = new Monitor(&m_logger);

    // Create shutdown timer
    m_shutdownTimer = new QTimer(this);
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &WindowsService::onShutdownTimeout);

    m_logger.log("All service components initialized successfully with secure usermode access");
    return true;
}

void WindowsService::cleanup()
{
    QMutexLocker locker(&m_mutex);
    m_shuttingDown = true;
    m_logger.log("Starting secure cleanup");

    if (m_shutdownTimer) {
        m_shutdownTimer->stop();
        delete m_shutdownTimer;
        m_shutdownTimer = nullptr;
    }

    if (m_pipeServer) {
        m_pipeServer->stop();
        delete m_pipeServer;
        m_pipeServer = nullptr;
    }

    if (m_secureHandler) {
        delete m_secureHandler;
        m_secureHandler = nullptr;
    }

    if (m_ecMemoryWriter) {
        m_ecMemoryWriter->close();
        delete m_ecMemoryWriter;
        m_ecMemoryWriter = nullptr;
    }

    if (m_monitor) {
        delete m_monitor;
        m_monitor = nullptr;
    }

    m_logger.log("Secure cleanup completed");
}

void WindowsService::onShutdownTimeout()
{
    m_logger.log("Shutdown timeout - forcing exit");
    QCoreApplication::exit(1);
}

void WINAPI WindowsService::serviceMain(DWORD argc, LPTSTR *argv)
{
    WindowsService* service = nullptr;

    {
        QMutexLocker locker(&s_globalMutex);
        service = g_service;
    }

    if (!service) {
        return;
    }

    service->m_serviceStatusHandle = RegisterServiceCtrlHandler(
        service->m_serviceName.toStdWString().c_str(),
        serviceCtrlHandler);

    if (!service->m_serviceStatusHandle) {
        service->m_logger.log("RegisterServiceCtrlHandler failed: " + QString::number(GetLastError()));
        return;
    }

    service->setServiceStatus(SERVICE_START_PENDING);

    int argcQt = 0;
    char* argvQt[] = { nullptr };
    QCoreApplication app(argcQt, argvQt);

    bool initSuccess = service->initializeService();

    if (initSuccess) {
        service->m_running = true;
        service->setServiceStatus(SERVICE_RUNNING);
        service->m_logger.log("Secure service running - entering event loop");

        app.exec();

        service->m_logger.log("Exited Qt event loop");
    } else {
        service->m_logger.log("Service initialization failed");
    }

    // Cleanup
    service->setServiceStatus(SERVICE_STOP_PENDING);
    service->cleanup();
    service->setServiceStatus(SERVICE_STOPPED);
}

void WINAPI WindowsService::serviceCtrlHandler(DWORD ctrlCode)
{
    WindowsService* service = nullptr;

    {
        QMutexLocker locker(&s_globalMutex);
        service = g_service;
    }

    if (!service) {
        return;
    }

    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        service->m_logger.log("Service stop/shutdown requested");
        service->setServiceStatus(SERVICE_STOP_PENDING);

        {
            QMutexLocker locker(&service->m_mutex);
            service->m_running = false;

            if (service->m_shutdownTimer) {
                service->m_shutdownTimer->start(SHUTDOWN_TIMEOUT_MS);
            }
        }

        QCoreApplication::quit();
        break;

    case SERVICE_CONTROL_INTERROGATE:
        service->setServiceStatus(service->m_serviceStatus.dwCurrentState);
        break;

    default:
        service->m_logger.log("Unknown control code: " + QString::number(ctrlCode));
        break;
    }
}

void WindowsService::setServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint)
{
    static DWORD dwCheckPoint = 1;

    m_serviceStatus.dwCurrentState = currentState;
    m_serviceStatus.dwWin32ExitCode = win32ExitCode;
    m_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING ||
        currentState == SERVICE_STOP_PENDING) {
        m_serviceStatus.dwCheckPoint = dwCheckPoint++;
    } else {
        m_serviceStatus.dwCheckPoint = 0;
    }

    if (m_serviceStatusHandle) {
        if (!SetServiceStatus(m_serviceStatusHandle, &m_serviceStatus)) {
            m_logger.log("SetServiceStatus failed: " + QString::number(GetLastError()));
        }
    }
}

void WindowsService::runAsService()
{
    start();
}

void WindowsService::runAsApp()
{
    int argcQt = 0;
    char* argvQt[] = { nullptr };
    QCoreApplication app(argcQt, argvQt);

    m_logger.log("Running as secure application");

    if (!initializeService()) {
        m_logger.log("Failed to initialize secure application");
        return;
    }

    m_running = true;
    m_logger.log("Entering Qt event loop (secure runAsApp)");

    int result = app.exec();

    m_logger.log("Exited Qt event loop (runAsApp) with code: " + QString::number(result));

    cleanup();
}
