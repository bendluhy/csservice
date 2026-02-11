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
    m_logger("C:\\ProgramData\\Patrol PC\\Service", this),
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
    m_logger.log("Initializing service components...");

    // Create Secure Command Handler
    m_secureHandler = new SecureCommandHandlerV2(&m_logger, &m_commandProc, this);
    if(!m_commandProc.initializeEc(0x220)) {  // Note the ! (NOT) operator
        m_logger.log("Failed to initialize ec, continuing without EC", Logger::Warning);
    } else {
        m_logger.log("EC subsystem initialized successfully", Logger::Info);
        clearEcState();
        testEcCommunication();
    }
    if (m_commandProc.isEcInitialized()) {
        m_bezelMonitor = new BezelMonitor(
            m_commandProc.getEcManager(),
            &m_commandProc,
            &m_logger,
            this
            );

        // Optional: log button presses at service level
        connect(m_bezelMonitor, &BezelMonitor::buttonPressed,
                this, [this](int button, quint32 eventId) {
                    m_logger.log(QString("Bezel button %1 pressed (event 0x%2)")
                                     .arg(button).arg(eventId, 4, 16, QChar('0')),
                                 Logger::Info);
                });

        m_bezelMonitor->start(50);  // 50ms poll = responsive button detection
    }
    // Create and initialize pipe server
    m_pipeServer = new NamedPipeServer(&m_logger, this);

    if (!m_pipeServer->initialize()) {
        m_logger.log("Failed to initialize pipe server", Logger::Error);
        return false;
    }



    // Connect pipe-specific command signals
    connect(m_pipeServer, &NamedPipeServer::controlScreensCommandReceived,
            this, &WindowsService::onControlScreensCommand, Qt::DirectConnection);

    connect(m_pipeServer, &NamedPipeServer::csMonitorCommandReceived,
            this, &WindowsService::onCSMonitorCommand, Qt::DirectConnection);

    // Connect client connection/disconnection signals
    connect(m_pipeServer, &NamedPipeServer::clientConnected,
            this, &WindowsService::onClientConnected, Qt::DirectConnection);

    connect(m_pipeServer, &NamedPipeServer::clientDisconnected,
            this, &WindowsService::onClientDisconnected, Qt::DirectConnection);

    // Connect error signal
    connect(m_pipeServer, &NamedPipeServer::serverError,
            this, [this](PipeType type, const QString& error) {
                m_logger.log(QString("Pipe error (type %1): %2")
                                 .arg(static_cast<int>(type)).arg(error), Logger::Error);
            }, Qt::DirectConnection);

    // Start both pipes
    if (!m_pipeServer->startAll()) {
        m_logger.log("Failed to start all pipes - some may be running", Logger::Warning);
    }

    m_logger.log(QString("Pipe status - ControlScreens: %1, CSMonitor: %2")
                     .arg(m_pipeServer->isControlScreensRunning() ? "Running" : "Stopped")
                     .arg(m_pipeServer->isCSMonitorRunning() ? "Running" : "Stopped"));

    // Create EC Memory Writer
    /*m_ecMemoryWriter = new ECMemoryWriter(&m_logger, this);
    if (!m_ecMemoryWriter->create()) {
        m_logger.log("Failed to create EC memory writer", Logger::Warning);
        // Non-fatal, continue
    }*/

    // Start Monitors
    m_monitor = new Monitor(&m_logger);

    // Create shutdown timer
    m_shutdownTimer = new QTimer(this);
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &WindowsService::onShutdownTimeout);

    m_logger.log("All service components initialized successfully");
    return true;
}
void WindowsService::clearEcState() {
    m_logger.log("Clearing any stale EC state...", Logger::Info);

    PortIo* port = PortIo::instance();
    if (!port || !port->IsLoaded()) return;

    quint16 emiBase = 0x220;

    // Read current state
    quint8 ecHost = 0;
    port->Read(emiBase + 1, &ecHost);  // EC_HOST

    if (ecHost != 0) {
        m_logger.log(QString("EC_HOST = 0x%1, clearing...").arg(ecHost, 2, 16, QChar('0')), Logger::Info);

        // Write 1 to EC_HOST to clear the response ready flag
        port->Write(emiBase + 1, 1);
        QThread::msleep(10);

        // Verify it cleared
        port->Read(emiBase + 1, &ecHost);
        m_logger.log(QString("EC_HOST after clear = 0x%1").arg(ecHost, 2, 16, QChar('0')), Logger::Info);
    }

    // Also make sure HOST_EC is in ready state
    quint8 hostEc = 0;
    port->Read(emiBase, &hostEc);
    if (hostEc != 0) {
        m_logger.log(QString("HOST_EC = 0x%1, resetting...").arg(hostEc, 2, 16, QChar('0')), Logger::Warning);
        port->Write(emiBase, 0);  // Reset to ready state
        QThread::msleep(10);
    }

    m_logger.log("EC state cleared", Logger::Info);
}
void WindowsService::testEcCommunication()
{
    m_logger.log("Testing EC communication...", Logger::Info);

    EcManager* ecMgr = m_commandProc.getEcManager();
    if (!ecMgr) {
        m_logger.log("EC Manager not available", Logger::Error);
        return;
    }

    // Test 1: Get DFU Info (simple command with response)
    dfu_info dfuInfo;
    EC_HOST_CMD_STATUS status = ecMgr->getDfuInfo(dfuInfo);

    // Test 2: Read ACPI0 (read a few bytes from offset 0)
    QByteArray acpiData;
    status = ecMgr->acpi0Read(0, 4, acpiData);

    if (status == EC_HOST_CMD_SUCCESS) {
        m_logger.log(QString("EC Test PASSED - ACPI0 Read: %1").arg(acpiData.toHex(' ').constData()), Logger::Info);
    } else {
        m_logger.log(QString("EC Test FAILED - ACPI0 Read status: %1").arg(status), Logger::Warning);
    }
}
// ============================================================================
// Pipe Command Handlers
// ============================================================================

void WindowsService::onControlScreensCommand(const QByteArray& data, QLocalSocket* client)
{
    QMutexLocker locker(&m_mutex);

    if (m_shuttingDown || !m_pipeServer || !m_secureHandler) {
        m_logger.log("Ignoring ControlScreens command - service shutting down");
        return;
    }

    m_logger.log(QString("ControlScreens command received: %1 bytes").arg(data.size()));

    // Process through secure handler
    QByteArray response = m_secureHandler->processCommand(data, client);

    if (!response.isEmpty() && m_pipeServer) {
        m_pipeServer->sendResponse(client, response);
        m_logger.log(QString("Sent ControlScreens response: %1 bytes").arg(response.size()));
    } else {
        m_logger.log("No response for ControlScreens command (auth failed or invalid)");
    }
}

void WindowsService::onCSMonitorCommand(const QByteArray& data, QLocalSocket* client)
{
    QMutexLocker locker(&m_mutex);

    if (m_shuttingDown || !m_pipeServer || !m_secureHandler) {
        m_logger.log("Ignoring CSMonitor command - service shutting down");
        return;
    }

    m_logger.log(QString("CSMonitor command received: %1 bytes").arg(data.size()));

    // Process through secure handler
    // You could use a different handler for CSMonitor if needed
    QByteArray response = m_secureHandler->processCommand(data, client);

    if (!response.isEmpty() && m_pipeServer) {
        m_pipeServer->sendResponse(client, response);
        m_logger.log(QString("Sent CSMonitor response: %1 bytes").arg(response.size()));
    } else {
        m_logger.log("No response for CSMonitor command (auth failed or invalid)");
    }
}

void WindowsService::onClientConnected(PipeType pipeType, QLocalSocket* client)
{
    QString pipeName = (pipeType == PipeType::ControlScreens) ? "ControlScreens" : "CSMonitor";
    m_logger.log(QString("Client connected to %1 pipe - registering with secure handler").arg(pipeName));
    m_secureHandler->registerClient(client);
}

void WindowsService::onClientDisconnected(PipeType pipeType, QLocalSocket* client)
{
    QString pipeName = (pipeType == PipeType::ControlScreens) ? "ControlScreens" : "CSMonitor";
    m_logger.log(QString("Client disconnected from %1 pipe - unregistering").arg(pipeName));
    m_secureHandler->unregisterClient(client);
}

// ============================================================================
// Cleanup and Shutdown
// ============================================================================

void WindowsService::cleanup()
{
    QMutexLocker locker(&m_mutex);
    m_shuttingDown = true;
    m_logger.log("Starting cleanup");

    if (m_shutdownTimer) {
        m_shutdownTimer->stop();
        delete m_shutdownTimer;
        m_shutdownTimer = nullptr;
    }
    if (m_bezelMonitor) {
        m_bezelMonitor->stop();
        delete m_bezelMonitor;
        m_bezelMonitor = nullptr;
    }
    if (m_pipeServer) {
        m_pipeServer->stopAll();
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

    m_logger.log("Cleanup completed");
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
        service->m_logger.log("Service running with dual pipes - entering event loop");

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

    m_logger.log("Running as application with dual pipes");

    if (!initializeService()) {
        m_logger.log("Failed to initialize application");
        return;
    }

    m_running = true;
    m_logger.log("Entering Qt event loop (runAsApp)");

    int result = app.exec();

    m_logger.log("Exited Qt event loop (runAsApp) with code: " + QString::number(result));

    cleanup();
}
