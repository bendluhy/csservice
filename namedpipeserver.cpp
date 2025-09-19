#include "NamedPipeServer.h"
#include <QDebug>

NamedPipeServer::NamedPipeServer(Logger* pLogger, const QString& pipeName, QObject* parent)
    : QObject(parent)
    , m_pLogger(pLogger)
    , m_server(nullptr)
    , m_pipeName(pipeName)
    , m_running(false)
{
    m_server = new QLocalServer(this);
    m_server->setMaxPendingConnections(MAX_CLIENTS);
    connect(m_server, &QLocalServer::newConnection, this, &NamedPipeServer::onNewConnection);
}

NamedPipeServer::~NamedPipeServer()
{
    stop();
}

bool NamedPipeServer::start()
{
    if (m_running) {
        m_pLogger->log("Server is already running");
        return true;
    }

    QLocalServer::removeServer(m_pipeName);
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);

    if (!m_server->listen(m_pipeName)) {
        QString error = QString("Failed to start server on pipe '%1': %2")
                            .arg(m_pipeName)
                            .arg(m_server->errorString());
        m_pLogger->log(error);
        emit serverError(error);
        return false;
    }

    m_running = true;
    m_pLogger->log(QString("NamedPipeServer started with usermode access on pipe: %1").arg(m_pipeName));
    return true;
}

void NamedPipeServer::stop()
{
    if (!m_running) {
        return;
    }

    m_pLogger->log("Stopping NamedPipeServer...");
    m_running = false;

    if (m_server->isListening()) {
        m_server->close();
    }

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        QLocalSocket* client = it.key();
        if (client) {
            client->disconnectFromServer();
            client->deleteLater();
        }
    }
    m_clients.clear();

    m_pLogger->log("NamedPipeServer stopped");
}

bool NamedPipeServer::isRunning() const
{
    return m_running && m_server->isListening();
}

void NamedPipeServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QLocalSocket* client = m_server->nextPendingConnection();

        if (!client) {
            continue;
        }

        if (m_clients.size() >= MAX_CLIENTS) {
            m_pLogger->log("Maximum client limit reached, rejecting new connection");
            client->disconnectFromServer();
            client->deleteLater();
            continue;
        }

        m_clients.insert(client, QPointer<QLocalSocket>(client));

        connect(client, &QLocalSocket::readyRead,
                this, &NamedPipeServer::onClientReadyRead);
        connect(client, &QLocalSocket::disconnected,
                this, &NamedPipeServer::onClientDisconnected);
        connect(client, &QLocalSocket::errorOccurred,
                this, &NamedPipeServer::onClientError);

        m_pLogger->log(QString("Client connected to pipe: %1 (Total clients: %2)")
                           .arg(m_pipeName)
                           .arg(m_clients.size()));

        emit clientConnected(client);
    }
}

void NamedPipeServer::onClientReadyRead()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    while (client->bytesAvailable() > 0) {
        QByteArray data = client->readAll();

        if (!data.isEmpty()) {
            m_pLogger->log(QString("Received %1 bytes from client").arg(data.size()));
            emit commandReceived(data, client);
        }
    }
}

void NamedPipeServer::onClientDisconnected()
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    m_pLogger->log(QString("Client disconnected (Remaining clients: %1)")
                       .arg(m_clients.size() - 1));

    m_clients.remove(client);
    emit clientDisconnected(client);
    client->deleteLater();
}

void NamedPipeServer::onClientError(QLocalSocket::LocalSocketError socketError)
{
    QLocalSocket* client = qobject_cast<QLocalSocket*>(sender());
    if (!client) {
        return;
    }

    QString errorStr = QString("Client socket error: %1").arg(client->errorString());
    m_pLogger->log(errorStr);
}

void NamedPipeServer::sendResponse(QLocalSocket* client, const QByteArray& response)
{
    if (!client) {
        m_pLogger->log("Cannot send response: null client pointer");
        return;
    }

    if (!m_clients.contains(client) || m_clients[client].isNull()) {
        m_pLogger->log("Cannot send response: client no longer valid");
        return;
    }

    if (client->state() != QLocalSocket::ConnectedState) {
        m_pLogger->log("Cannot send response: client not connected");
        return;
    }

    qint64 bytesWritten = client->write(response);

    if (bytesWritten == -1) {
        m_pLogger->log(QString("Failed to send response: %1").arg(client->errorString()));
    } else if (bytesWritten != response.size()) {
        m_pLogger->log(QString("Partial write: sent %1 of %2 bytes")
                           .arg(bytesWritten)
                           .arg(response.size()));
    } else {
        client->flush();
        m_pLogger->log(QString("Sent response of %1 bytes").arg(bytesWritten));
    }
}
