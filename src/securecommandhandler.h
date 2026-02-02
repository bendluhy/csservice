#ifndef SECURECOMMANDHANDLER_H
#define SECURECOMMANDHANDLER_H

#include <QObject>
#include <QLocalSocket>
#include <QByteArray>
#include <QMap>
#include <QDateTime>
#include <QProtobufSerializer>
#include "Logger.h"
#include "CommandProc.h"
#include "command.qpb.h"

// Use the shared protocol - this ensures client and server match
#include "../../Shared/Src/secureprotocol.h"

struct ClientSession {
    uint32_t token;
    QLocalSocket* socket;
    QDateTime connectedAt;
    QDateTime lastActivity;
    uint32_t lastSequence;
    QString clientIdentifier;
    bool isAuthenticated;
};

class SecureCommandHandler : public QObject
{
    Q_OBJECT

public:
    explicit SecureCommandHandler(Logger* logger, CommandProc* cmdProc, QObject* parent = nullptr);
    ~SecureCommandHandler();

    // Process incoming command - returns response packet
    QByteArray processCommand(const QByteArray& data, QLocalSocket* client);

    // Client management
    void registerClient(QLocalSocket* client);
    void unregisterClient(QLocalSocket* client);
    bool isClientAuthenticated(QLocalSocket* client);

private:
    Logger* m_pLogger;
    CommandProc* m_pCmdProc;
    QMap<QLocalSocket*, ClientSession> m_clients;
    QProtobufSerializer m_serializer;

    // Authentication
    bool authenticateClient(const QByteArray& authData, QLocalSocket* client);

    // Sequence validation
    bool validateSequence(QLocalSocket* client, uint32_t sequence);
};

// Keep the old name as alias for compatibility with existing code
using SecureCommandHandlerV2 = SecureCommandHandler;

#endif // SECURECOMMANDHANDLER_H
