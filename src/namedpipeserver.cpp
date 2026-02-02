#include "namedpipeserver.h"
#include <QDebug>

NamedPipeServer::NamedPipeServer(Logger* pLogger, QObject* parent)
    : QObject(parent)
    , m_pLogger(pLogger)
{
    // Initialize pipe info structures
    m_controlScreensPipe.type = PipeType::ControlScreens;
    m_controlScreensPipe.name = PIPE_CONTROL_SCREENS;
    m_controlScreensPipe.maxClients = MAX_CLIENTS_CONTROL_SCREENS;

    m_csMonitorPipe.type = PipeType::CSMonitor;
    m_csMonitorPipe.name = PIPE_CSMONITOR;
    m_csMonitorPipe.maxClients = MAX_CLIENTS_CSMONITOR;
}

NamedPipeServer::~NamedPipeServer()
{
    stopAll();
}

bool NamedPipeServer::initialize()
{
    m_pLogger->log("NamedPipeServer: Initializing pipes...", Logger::Info);

    // Create Control Screens pipe server
    m_controlScreensPipe.server = new QLocalServer(this);
    m_controlScreensPipe.server->setMaxPendingConnections(m_controlScreensPipe.maxClients);
    connect(m_controlScreensPipe.server, &QLocalServer::newConnection,
            this, &NamedPipeServer::onNewConnection);
    m_serverToPipeType[m_controlScreensPipe.server] = PipeType::ControlScreens;

    // Create CSMonitor pipe server
    m_csMonitorPipe.server = new QLocalServer(this);
    m_csMonitorPipe.server->setMaxPendingConnections(m_csMonitorPipe.maxClients);
    connect(m_csMonitorPipe.server, &QLocalServer::newConnection,
            this, &NamedPipeServer::onNewConnection);
    m_serverToPipeType[m_csMonitorPipe.server] = PipeType::CSMonitor;

    m_pLogger->log("NamedPipeServer: Pipes initialized", Logger::Info);
    return true;
}

bool NamedPipeServer::startAll()
{
    bool success = true;

    if (!startControlScreensPipe()) {
        success = false;
    }

    if (!startCSMonitorPipe()) {
        success = false;
    }

    return success;
}

void NamedPipeServer::stopAll()
{
    m_pLogger->log("NamedPipeServer: Stopping all pipes...", Logger::Info);
    stopControlScreensPipe();
    stopCSMonitorPipe();
    m_pLogger->log("NamedPipeServer: All pipes stopped", Logger::Info);
}

bool NamedPipeServer::startControlScreensPipe()
{
    return startPipe(PipeType::ControlScreens);
}

bool NamedPipeServer::startCSMonitorPipe()
{
    return startPipe(PipeType::CSMonitor);
}

void NamedPipeServer::stopControlScreensPipe()
{
    stopPipe(PipeType::ControlScreens);
}

void NamedPipeServer::stopCSMonitorPipe()
{
    stopPipe(PipeType::CSMonitor);
}

bool NamedPipeServer::startPipe(PipeType type)
{
    PipeInfo* info = getPipeInfo(type);
    if (!info || !info->server) {
        m_pLogger->log(QString("NamedPipeServer: Cannot start %1 pipe - not initialized")
                           .arg(pipeTypeToString(type)), Logger::Error);
        return false;
    }

    if (info->running) {
        m_pLogger->log(QString("NamedPipeServer: %1 pipe already running")
                           .arg(pipeTypeToString(type)), Logger::Warning);
        return true;
    }

    // Remove any existing server with this name
    QLocalServer::removeServer(info->name);

    // Set world access for usermode clients
    info->server->setSocketOptions(QLocalServer::WorldAccessOption);

    if (!info->server->listen(info->name)) {
        QString error = QString("NamedPipeServer: Failed to start %1 pipe '%2': %3")
        .arg(pipeTypeToString(type))
            .arg(info->name)
            .arg(info->server->errorString());
        m_pLogger->log(error, Logger::Error);
        emit serverError(type, error);
        return false;
    }

    info->running = true;
    m_pLogger->log(QString("NamedPipeServer: Started %1 pipe '%2'")
                       .arg(pipeTypeToString(type)).arg(info->name), Logger::Info);
    emit pipeStarted(type);

    return true;
}

void NamedPipeServer::stopPipe(PipeType type)
{
    PipeInfo* info = getPipeInfo(type);
    if (!info) {
        return;
    }

    if (!info->running) {
        return;
    }

    m_pLogger->log(QString("NamedPipeServer: Stopping %1 pipe...")
                       .arg(pipeTypeToString(type)), Logger::Info);

    info->running = false;

    if (info->server && info->server->isListening()) {
        info->server->close();
    }

    // Disconnect and cleanup all clients for this pipe
    for (auto it = info->clients.begin(); it != info->clients.end(); ++it) {
        QLocalSocket* client = it.key();
        if (client) {
            m_clientToPipeType.remove(client);
            client->disconnectFromServer();
            client->deleteLater();
        }
    }
    info->clients.clear();

    m_pLogger->log(QString("NamedPipeServer: %1 pipe stopped")
                       .arg(pipeTypeToString(type)), Logger::Info);
    emit pipeStopped(type);
}

bool NamedPipeServer::isControlScreensRunning() const
{
    return m_controlScreensPipe.running &&
           m_controlScreensPipe.server &&
           m_controlScreensPipe.server->isListening();
}

bool NamedPipeServer::isCSMonitorRunning() const
{
    return m_csMonitorPipe.running &&
           m_csMonitorPipe.server &&
           m_csMonitorPipe.server->isListening();
}

bool NamedPipeServer::isAnyRunning() const
{
    return isControlScreensRunning() || isCSMonitorRunning();
}

int NamedPipeServer::controlScreensClientCount() const
{
    return m_controlScreensPipe.clients.size();
}

int NamedPipeServer::csMonitorClientCount() const
{
    return m_csMonitorPipe.clients.size();
}

PipeType NamedPipeServer::getClientPipeType(QLocalSocket* client) const
{
    return m_clientToPipeType.value(client, PipeType::Unknown);
}

QString NamedPipeServer::getClientPipeName(QLocalSocket* client) const
{
    PipeType type = getClientPipeType(client);
    const PipeInfo* info = getPipeInfo(type);
    return info ? info->name : QString();
}

NamedPipeServer::PipeInfo* NamedPipeServer::getPipeInfo(PipeType type)
{
    switch (type) {
    case PipeType::ControlScreens:
        return &m_controlScreensPipe;
    case PipeType::CSMonitor:
        return &m_csMonitorPipe;
    default:
        return nullptr;
    }
}

const NamedPipeServer::PipeInfo* NamedPipeServer::getPipeInfo(PipeType type) const
{
    switch (type) {
    case PipeType::ControlScreens:
        return &m_controlScreensPipe;
    case PipeType::CSMonitor:
        return &m_csMonitorPipe;
    default:
        return nullptr;
    }
}

QString NamedPipeServer::pipeTypeToString(PipeType type) const
{
    switch (type) {
    case PipeType::ControlScreens:
        return "ControlScreens";
    case PipeType::CSMonitor:
        return "CSMonitor";
    default:
        return "Unknown";
    }
}

void NamedPipeServer::onNewConnection()
{
    QLocalServer* server = qobject_cast<QLocalServer*>(sender());
    if (!server) {
        return;
    }

    // Determine which pipe this connection is for
    PipeType pipeType = m_serverToPipeType.value(server, PipeType::Unknown);
    PipeInfo* info = getPipeInfo(pipeType);

    if (!info) {
        m_pLogger->log("NamedPipeServer: Connection from unknown server", Logger::Error);
        return;
    }

    while (server->hasPendingConnections()) {
        QLocalSocket* client = server->nextPendingConnection();

        if (!client) {
            continue;
        }

        // Check client limit
        if (info->clients.size() >= info->maxClients) {
            m_pLogger->log(QString("NamedPipeServer: Max clients reached for %1 pipe, rejecting")
                               .arg(pipeTypeToString(pipeType)), Logger::Warning);
            client->disconnectFromServer();
            client->deleteLater();
            continue;
        }

        // Track client
        info->clients.insert(client, QPointer<QLocalSocket>(client));
        m_clientToPipeType[client] = pipeType;

        // Connect client signals
        connect(client, &QLocalSocket::readyRead,
                this, &NamedPipeServer::onClientReadyRead);
        connect(client, &QLocalSocket::disconnected,
                this, &NamedPipeServer::onClientDisconnected);
        connect(client, &QLocalSocket::errorOccurred,
                this, &NamedPipeServer::onClientError);

        m_pLogger->log(QString("NamedPipeServer: Client connected to %1 pipe (total: %2)")
                           .arg(pipeTypeToString(pipeType)).arg(info->clients.size()), Logger::Info);

        emit clientConnected(pipeType, client);
    }
}

void NamedPipeServer::onClientReadyRead()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    PipeType pipeType = m_clientToPipeType.value(client, PipeType::Unknown);
    if (pipeType == PipeType::Unknown) {
        m_pLogger->log("NamedPipeServer: Data from untracked client", Logger::Warning);
        return;
    }

    while (client->bytesAvailable() > 0) {
        QByteArray data = client->readAll();

        if (!data.isEmpty()) {
            m_pLogger->log(QString("NamedPipeServer: Received %1 bytes on %2 pipe")
                               .arg(data.size()).arg(pipeTypeToString(pipeType)), Logger::Debug);

            // Emit general signal
            emit commandReceived(pipeType, data, client);

            // Emit pipe-specific signal for convenience
            switch (pipeType) {
            case PipeType::ControlScreens:
                emit controlScreensCommandReceived(data, client);
                break;
            case PipeType::CSMonitor:
                emit csMonitorCommandReceived(data, client);
                break;
            default:
                break;
            }
        }
    }
}

void NamedPipeServer::onClientDisconnected()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    PipeType pipeType = m_clientToPipeType.value(client, PipeType::Unknown);
    PipeInfo* info = getPipeInfo(pipeType);

    if (info) {
        info->clients.remove(client);
        m_pLogger->log(QString("NamedPipeServer: Client disconnected from %1 pipe (remaining: %2)")
                           .arg(pipeTypeToString(pipeType)).arg(info->clients.size()), Logger::Info);
        emit clientDisconnected(pipeType, client);
    }

    m_clientToPipeType.remove(client);
    client->deleteLater();
}

void NamedPipeServer::onClientError(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError);

    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    PipeType pipeType = m_clientToPipeType.value(client, PipeType::Unknown);
    m_pLogger->log(QString("NamedPipeServer: Client error on %1 pipe: %2")
                       .arg(pipeTypeToString(pipeType)).arg(client->errorString()), Logger::Warning);
}

void NamedPipeServer::sendResponse(QLocalSocket* client, const QByteArray& response)
{
    if (!client) {
        m_pLogger->log("NamedPipeServer: Cannot send response - null client", Logger::Error);
        return;
    }

    PipeType pipeType = m_clientToPipeType.value(client, PipeType::Unknown);
    PipeInfo* info = getPipeInfo(pipeType);

    if (!info) {
        m_pLogger->log("NamedPipeServer: Cannot send response - client not tracked", Logger::Error);
        return;
    }

    if (!info->clients.contains(client) || info->clients[client].isNull()) {
        m_pLogger->log("NamedPipeServer: Cannot send response - client no longer valid", Logger::Error);
        return;
    }

    if (client->state() != QLocalSocket::ConnectedState) {
        m_pLogger->log("NamedPipeServer: Cannot send response - client not connected", Logger::Error);
        return;
    }

    qint64 bytesWritten = client->write(response);

    if (bytesWritten == -1) {
        m_pLogger->log(QString("NamedPipeServer: Failed to send on %1 pipe: %2")
                           .arg(pipeTypeToString(pipeType)).arg(client->errorString()), Logger::Error);
    } else if (bytesWritten != response.size()) {
        m_pLogger->log(QString("NamedPipeServer: Partial write on %1 pipe: %2 of %3 bytes")
                           .arg(pipeTypeToString(pipeType)).arg(bytesWritten).arg(response.size()), Logger::Warning);
    } else {
        client->flush();
        m_pLogger->log(QString("NamedPipeServer: Sent %1 bytes on %2 pipe")
                           .arg(bytesWritten).arg(pipeTypeToString(pipeType)), Logger::Debug);
    }
}
