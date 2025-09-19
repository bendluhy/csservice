#ifndef NAMEDPIPESERVER_H
#define NAMEDPIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QPointer>
#include "logger.h"

class NamedPipeServer : public QObject
{
    Q_OBJECT

public:
    explicit NamedPipeServer(Logger* pLogger, const QString& pipeName, QObject* parent = nullptr);
    ~NamedPipeServer();

    bool start();
    void stop();
    bool isRunning() const;

    // Send response to a specific client
    void sendResponse(QLocalSocket* client, const QByteArray& response);

signals:
    void commandReceived(const QByteArray& data, QLocalSocket* client);
    void clientConnected(QLocalSocket* client);
    void clientDisconnected(QLocalSocket* client);
    void serverError(const QString& error);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QLocalSocket::LocalSocketError socketError);

private:
    Logger* m_pLogger;
    QLocalServer* m_server;
    QString m_pipeName;
    QMap<QLocalSocket*, QPointer<QLocalSocket>> m_clients;
    bool m_running;

    static const int MAX_CLIENTS = 5;
    static const int BUFFER_SIZE = 4096;
};

#endif // NAMEDPIPESERVER_H
