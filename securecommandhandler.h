#ifndef SECURECOMMANDHANDLERV2_H
#define SECURECOMMANDHANDLERV2_H

#include <QObject>
#include <QLocalSocket>
#include <QByteArray>
#include <QMap>
#include <QDateTime>
#include "Logger.h"
#include "CommandProc.h"
#include "SecureProtocol.h"

struct ClientSession {
    uint32_t token;
    QLocalSocket* socket;
    QDateTime connectedAt;
    QDateTime lastActivity;
    uint32_t lastSequence;
    QString clientIdentifier;
    bool isAuthenticated;
};

class SecureCommandHandlerV2 : public QObject
{
    Q_OBJECT

public:
    explicit SecureCommandHandlerV2(Logger* logger, CommandProc* cmdProc, QObject* parent = nullptr);
    ~SecureCommandHandlerV2();

    // Process incoming command
    QByteArray processCommand(const QByteArray& data, QLocalSocket* client);

    // Client management
    void registerClient(QLocalSocket* client);
    void unregisterClient(QLocalSocket* client);
    bool isClientAuthenticated(QLocalSocket* client);

private:
    Logger* m_pLogger;
    CommandProc* m_pCmdProc;
    QMap<QLocalSocket*, ClientSession> m_clients;

    // Authentication
    bool authenticateClient(const QByteArray& authData, QLocalSocket* client);

    // Sequence validation
    bool validateSequence(QLocalSocket* client, uint32_t sequence);
};

#endif // SECURECOMMANDHANDLERV2_H
