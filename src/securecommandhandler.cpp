#include "securecommandhandler.h"
#include <QDataStream>

SecureCommandHandler::SecureCommandHandler(Logger* logger, CommandProc* cmdProc, QObject* parent)
    : QObject(parent)
    , m_pLogger(logger)
    , m_pCmdProc(cmdProc)
{
}

SecureCommandHandler::~SecureCommandHandler()
{
    m_clients.clear();
}

void SecureCommandHandler::registerClient(QLocalSocket* client)
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
        m_pLogger->log(QString("SecureHandler: Registered client: %1").arg(session.clientIdentifier), Logger::Info);
    }
}

void SecureCommandHandler::unregisterClient(QLocalSocket* client)
{
    if (m_clients.contains(client)) {
        if (m_pLogger) {
            m_pLogger->log(QString("SecureHandler: Unregistered client: %1").arg(m_clients[client].clientIdentifier), Logger::Info);
        }
        m_clients.remove(client);
    }
}

bool SecureCommandHandler::isClientAuthenticated(QLocalSocket* client)
{
    return m_clients.contains(client) && m_clients[client].isAuthenticated;
}

QByteArray SecureCommandHandler::processCommand(const QByteArray& data, QLocalSocket* client)
{
    // Parse the secure packet using shared protocol
    SecurePacketHeaderV2 header;
    QByteArray payload;

    if (!SecurePacketBuilder::parsePacket(data, header, payload)) {
        if (m_pLogger) {
            m_pLogger->log("SecureHandler: Invalid packet format or HMAC verification failed", Logger::Error);

            // Debug info
            if (data.size() >= 4) {
                uint32_t receivedMagic = *reinterpret_cast<const uint32_t*>(data.constData());
                m_pLogger->log(QString("SecureHandler: Received magic: 0x%1, expected: 0x%2")
                                   .arg(receivedMagic, 8, 16, QChar('0'))
                                   .arg(PROTOCOL_MAGIC, 8, 16, QChar('0')), Logger::Debug);
            }
            m_pLogger->log(QString("SecureHandler: Packet size: %1, expected header size: %2")
                               .arg(data.size())
                               .arg(sizeof(SecurePacketHeaderV2)), Logger::Debug);
        }
        return QByteArray();
    }

    // Check if this is an authentication request (token = 0)
    if (header.sessionToken == 0) {
        if (authenticateClient(payload, client)) {
            // Generate new session token using shared protocol
            uint32_t newToken = ProtocolSecurity::generateSessionToken();

            if (m_clients.contains(client)) {
                m_clients[client].token = newToken;
                m_clients[client].isAuthenticated = true;
                m_clients[client].lastActivity = QDateTime::currentDateTime();
                m_clients[client].lastSequence = 0;
            }

            // Build token response payload
            QByteArray responsePayload;
            QDataStream stream(&responsePayload, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream << newToken;

            if (m_pLogger) {
                m_pLogger->log(QString("SecureHandler: Client authenticated, token: %1").arg(newToken), Logger::Info);
            }

            // Build response using shared protocol (handles encryption + HMAC)
            return SecurePacketBuilder::buildPacket(newToken, 0, responsePayload);
        } else {
            if (m_pLogger) {
                m_pLogger->log("SecureHandler: Authentication failed", Logger::Warning);
            }
            return QByteArray();
        }
    }

    // Validate client registration
    if (!m_clients.contains(client)) {
        if (m_pLogger) {
            m_pLogger->log("SecureHandler: Unknown client", Logger::Error);
        }
        return QByteArray();
    }

    ClientSession& session = m_clients[client];

    // Validate authentication
    if (!session.isAuthenticated) {
        if (m_pLogger) {
            m_pLogger->log("SecureHandler: Client not authenticated", Logger::Warning);
        }
        return QByteArray();
    }

    // Validate token
    if (header.sessionToken != session.token) {
        if (m_pLogger) {
            m_pLogger->log(QString("SecureHandler: Token mismatch: expected %1, got %2")
                               .arg(session.token).arg(header.sessionToken), Logger::Warning);
        }
        return QByteArray();
    }

    // Validate sequence number (anti-replay)
    if (!validateSequence(client, header.sequenceNumber)) {
        if (m_pLogger) {
            m_pLogger->log("SecureHandler: Invalid sequence number", Logger::Warning);
        }
        return QByteArray();
    }

    // Update session
    session.lastActivity = QDateTime::currentDateTime();
    session.lastSequence = header.sequenceNumber;

    // Deserialize protobuf command (payload is already decrypted by parsePacket)
    patrol::Command request;
    if (!request.deserialize(&m_serializer, payload)) {
        if (m_pLogger) {
            m_pLogger->log("SecureHandler: Failed to deserialize protobuf command", Logger::Error);
        }
        return QByteArray();
    }

    if (m_pLogger) {
        m_pLogger->log(QString("SecureHandler: Processing command, sequence: %1").arg(header.sequenceNumber), Logger::Debug);
    }

    // Process command via CommandProc
    patrol::Command response = m_pCmdProc->processCommand(request);

    // Serialize response
    QByteArray responsePayload = response.serialize(&m_serializer);

    if (m_pLogger) {
        m_pLogger->log(QString("SecureHandler: Response size: %1 bytes").arg(responsePayload.size()), Logger::Debug);
    }

    // Build and return secure packet using shared protocol (handles encryption + HMAC)
    return SecurePacketBuilder::buildPacket(header.sessionToken, header.sequenceNumber, responsePayload);
}

bool SecureCommandHandler::authenticateClient(const QByteArray& authData, QLocalSocket* client)
{
    Q_UNUSED(client)

    if (authData.isEmpty() || authData.size() < 32) {
        if (m_pLogger) {
            m_pLogger->log(QString("SecureHandler: Auth data too small: %1 bytes").arg(authData.size()), Logger::Debug);
        }
        return false;
    }

    // Client sends: SHA256("AuthChallenge" + SHARED_SECRET)
    // SHARED_SECRET is now machine-specific via ProtocolSecurity::getSharedSecret()
    QByteArray expectedAuth = QCryptographicHash::hash(
        QByteArray("AuthChallenge") + SHARED_SECRET,
        QCryptographicHash::Sha256);

    QByteArray clientAuth = authData.left(32);

    bool matches = (clientAuth == expectedAuth);

    if (m_pLogger && !matches) {
        m_pLogger->log("SecureHandler: Auth hash mismatch", Logger::Debug);
    }

    return matches;
}

bool SecureCommandHandler::validateSequence(QLocalSocket* client, uint32_t sequence)
{
    if (!m_clients.contains(client)) {
        return false;
    }

    ClientSession& session = m_clients[client];

    // Sequence must always increase (with rollover handling)
    if (sequence <= session.lastSequence && sequence != 0) {
        // Check for uint32 rollover
        if (session.lastSequence > 0xFFFF0000 && sequence < 0x0000FFFF) {
            return true;
        }
        return false;
    }

    return true;
}
