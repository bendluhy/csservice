#ifndef NAMEDPIPESERVER_H
#define NAMEDPIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QHash>
#include <QPointer>
#include "logger.h"

// Predefined pipe names
#define PIPE_CONTROL_SCREENS    "PPC_SERV"        // Control Screens pipe
#define PIPE_CSMONITOR          "PPC_MON"       // CSMonitor pipe

// Pipe identifiers for routing
enum class PipeType {
    Unknown,
    ControlScreens,
    CSMonitor
};

class NamedPipeServer : public QObject
{
    Q_OBJECT

public:
    explicit NamedPipeServer(Logger* pLogger, QObject* parent = nullptr);
    ~NamedPipeServer();

    // Initialize both pipes
    bool initialize();

    // Start/stop all pipes
    bool startAll();
    void stopAll();

    // Start/stop individual pipes
    bool startControlScreensPipe();
    bool startCSMonitorPipe();
    void stopControlScreensPipe();
    void stopCSMonitorPipe();

    // Check status
    bool isControlScreensRunning() const;
    bool isCSMonitorRunning() const;
    bool isAnyRunning() const;

    // Send response to a specific client
    void sendResponse(QLocalSocket* client, const QByteArray& response);

    // Get which pipe type a client is connected to
    PipeType getClientPipeType(QLocalSocket* client) const;
    QString getClientPipeName(QLocalSocket* client) const;

    // Get client counts
    int controlScreensClientCount() const;
    int csMonitorClientCount() const;

signals:
    // Command received with pipe type for routing
    void commandReceived(PipeType pipeType, const QByteArray& data, QLocalSocket* client);

    // Convenience signals for specific pipes
    void controlScreensCommandReceived(const QByteArray& data, QLocalSocket* client);
    void csMonitorCommandReceived(const QByteArray& data, QLocalSocket* client);

    // Client connection events
    void clientConnected(PipeType pipeType, QLocalSocket* client);
    void clientDisconnected(PipeType pipeType, QLocalSocket* client);

    // Server errors
    void serverError(PipeType pipeType, const QString& error);

    // Pipe lifecycle events
    void pipeStarted(PipeType pipeType);
    void pipeStopped(PipeType pipeType);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QLocalSocket::LocalSocketError socketError);

private:
    struct PipeInfo {
        QLocalServer* server = nullptr;
        QMap<QLocalSocket*, QPointer<QLocalSocket>> clients;
        QString name;
        PipeType type = PipeType::Unknown;
        int maxClients = 5;
        bool running = false;
    };

    bool addPipe(const QString& pipeName, PipeType type, int maxClients);
    bool startPipe(PipeType type);
    void stopPipe(PipeType type);
    PipeInfo* getPipeInfo(PipeType type);
    const PipeInfo* getPipeInfo(PipeType type) const;
    QString pipeTypeToString(PipeType type) const;

    Logger* m_pLogger;

    // Two pipes: one for ControlScreens, one for CSMonitor
    PipeInfo m_controlScreensPipe;
    PipeInfo m_csMonitorPipe;

    // Client tracking
    QHash<QLocalSocket*, PipeType> m_clientToPipeType;
    QHash<QLocalServer*, PipeType> m_serverToPipeType;

    static const int MAX_CLIENTS_CONTROL_SCREENS = 5;
    static const int MAX_CLIENTS_CSMONITOR = 10;
};

#endif // NAMEDPIPESERVER_H
