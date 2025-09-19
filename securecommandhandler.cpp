#include "SecureCommandHandler.h"
#include <QDataStream>

SecureCommandHandlerV2::SecureCommandHandlerV2(Logger* logger, CommandProc* cmdProc, QObject* parent)
    : QObject(parent)
    , m_pLogger(logger)
    , m_pCmdProc(cmdProc)
{
}

SecureCommandHandlerV2::~SecureCommandHandlerV2()
{
    m_clients.clear();
}

void SecureCommandHandlerV2::registerClient(QLocalSocket* client)
{
    if (!client) return;

    ClientSession session;
    session.socket = client;
    session.connectedAt = QDateTime::currentDateTime();
    session.lastActivity = session.connectedAt;
    session.token = 0;
    session.lastSequence = 0;
    session.clientIdentifier = QString::number(reinterpret_cast<quint64>(client));
    session.isAuthenticated = false;

    m_clients[client] = session;

    if (m_pLogger) {
        m_pLogger->log(QString("SecureV2: Registered client: %1").arg(session.clientIdentifier), Logger::Info);
    }
}

void SecureCommandHandlerV2::unregisterClient(QLocalSocket* client)
{
    if (m_clients.contains(client)) {
        if (m_pLogger) {
            m_pLogger->log(QString("SecureV2: Unregistered client: %1").arg(m_clients[client].clientIdentifier), Logger::Info);
        }
        m_clients.remove(client);
    }
}

bool SecureCommandHandlerV2::isClientAuthenticated(QLocalSocket* client)
{
    return m_clients.contains(client) && m_clients[client].isAuthenticated;
}

QByteArray SecureCommandHandlerV2::processCommand(const QByteArray& data, QLocalSocket* client)
{
    // Parse the secure packet
    SecurePacketHeaderV2 header;
    QByteArray payload;

    if (!SecurePacketBuilder::parsePacket(data, header, payload)) {
        if (m_pLogger) {
            m_pLogger->log("SecureV2: Invalid packet format or HMAC verification failed", Logger::Error);
        }
        return QByteArray(); // Don't respond to invalid packets
    }

    // Check if this is an authentication request (token = 0)
    if (header.sessionToken == 0) {
        if (authenticateClient(payload, client)) {
            // Generate new session token
            uint32_t newToken = ProtocolSecurity::generateSessionToken();

            if (m_clients.contains(client)) {
                m_clients[client].token = newToken;
                m_clients[client].isAuthenticated = true;
                m_clients[client].lastActivity = QDateTime::currentDateTime();
                m_clients[client].lastSequence = 0;
            }

            // Send token response
            QByteArray response;
            QDataStream stream(&response, QIODevice::WriteOnly);
            stream << newToken;

            if (m_pLogger) {
                m_pLogger->log(QString("SecureV2: Client authenticated, token: %1").arg(newToken), Logger::Info);
            }

            return SecurePacketBuilder::buildPacket(newToken, 0, response);
        } else {
            if (m_pLogger) {
                m_pLogger->log("SecureV2: Authentication failed", Logger::Warning);
            }
            return QByteArray();
        }
    }

    // Validate client registration
    if (!m_clients.contains(client)) {
        if (m_pLogger) {
            m_pLogger->log("SecureV2: Unknown client", Logger::Error);
        }
        return QByteArray();
    }

    ClientSession& session = m_clients[client];

    // Validate authentication
    if (!session.isAuthenticated) {
        if (m_pLogger) {
            m_pLogger->log("SecureV2: Client not authenticated", Logger::Warning);
        }
        return QByteArray();
    }

    // Validate token
    if (header.sessionToken != session.token) {
        if (m_pLogger) {
            m_pLogger->log(QString("SecureV2: Token mismatch: expected %1, got %2")
                               .arg(session.token).arg(header.sessionToken), Logger::Warning);
        }
        return QByteArray();
    }

    // Validate sequence number (anti-replay)
    if (!validateSequence(client, header.sequenceNumber)) {
        if (m_pLogger) {
            m_pLogger->log("SecureV2: Invalid sequence number", Logger::Warning);
        }
        return QByteArray();
    }

    // Update session
    session.lastActivity = QDateTime::currentDateTime();
    session.lastSequence = header.sequenceNumber;

    // Process command
    CommandMessage cmdMsg;
    memset(&cmdMsg, 0, sizeof(CommandMessage));

    // Copy command data (variable length!)
    size_t copySize = qMin(static_cast<size_t>(payload.size()), sizeof(cmdMsg.command));
    memcpy(cmdMsg.command, payload.constData(), copySize);

    // Execute command
    int result = m_pCmdProc->ProcessCommand(&cmdMsg);

    // Extract response (variable length!)
    QByteArray responsePayload;
    if (payload.size() >= sizeof(CMD_HDR)) {
        CMD_RESP_HDR* respHdr = reinterpret_cast<CMD_RESP_HDR*>(cmdMsg.response);
        size_t respSize = respHdr->PacketSize;
        if (respSize > 0 && respSize <= sizeof(cmdMsg.response)) {
            responsePayload = QByteArray(reinterpret_cast<char*>(cmdMsg.response), respSize);
        }
    }

    if (m_pLogger) {
        m_pLogger->log(QString("SecureV2: Command processed, result: %1, response size: %2")
                           .arg(result).arg(responsePayload.size()), Logger::Debug);
    }

    // Build secure response packet (variable length!)
    return SecurePacketBuilder::buildPacket(header.sessionToken, header.sequenceNumber, responsePayload);
}

bool SecureCommandHandlerV2::authenticateClient(const QByteArray& authData, QLocalSocket* client)
{
    if (authData.isEmpty() || authData.size() < 32) {
        return false;
    }

    // Match what the client sends: SHA256 hash, not HMAC
    QByteArray expectedAuth = QCryptographicHash::hash(
        QByteArray("AuthChallenge") + SHARED_SECRET,
        QCryptographicHash::Sha256);

    QByteArray clientAuth = authData.left(32);

    // Direct byte comparison
    if (clientAuth == expectedAuth) {
        if (m_pLogger) {
            m_pLogger->log("SecureV2: Client authenticated successfully", Logger::Info);
        }
        return true;
    }

    if (m_pLogger) {
        m_pLogger->log("SecureV2: Authentication failed - invalid credentials", Logger::Warning);
    }
    return false;
}

bool SecureCommandHandlerV2::validateSequence(QLocalSocket* client, uint32_t sequence)
{
    if (!m_clients.contains(client)) {
        return false;
    }

    ClientSession& session = m_clients[client];

    // Sequence must always increase (with rollover handling)
    if (sequence <= session.lastSequence && sequence != 0) {
        // Check for uint32 rollover
        if (session.lastSequence > 0xFFFF0000 && sequence < 0x0000FFFF) {
            // Acceptable rollover
            return true;
        }
        return false;
    }

    return true;
}
