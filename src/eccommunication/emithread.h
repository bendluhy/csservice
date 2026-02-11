#ifndef EMITHREAD_H
#define EMITHREAD_H

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QThread>
#include <QPointer>
#include "host_ec_cmds.h"
#include "portio.h"
#include "logger.h"

class EmiThread : public QThread
{
    Q_OBJECT

public:
    explicit EmiThread(QObject *parent = nullptr);
    ~EmiThread();
    void run() override;
    void stop();
    int addCmdToQueue(QSharedPointer<EmiCmd>);

    void setLogger(Logger* logger) { m_pLogger = logger; }

signals:
    void CommandDone(QSharedPointer<EmiCmd>);
    void TxOut(int bytes);
    void RxIn(int bytes);

private:
    void log(const QString& message, Logger::LogLevel level = Logger::Info);

    quint16 m_EmiOffset = 0x220;
    PortIo* m_pPort;
    Logger* m_pLogger = nullptr;
    QMutex m_Mutex;
    QQueue<QSharedPointer<EmiCmd>> m_pCmdQueue;
    QWaitCondition m_WaitCondition;
    bool m_StopFlag = false;
    EC_HOST_CMD_STATUS ProcCmd(QSharedPointer<EmiCmd> pCmd);
    EC_HOST_CMD_STATUS SendCmdGetResults(QByteArray& payloadin);
    EC_HOST_CMD_STATUS SendCmdOut(QByteArray& packetout, QByteArray& payloadin);
    EC_HOST_CMD_STATUS PayloadToOutPack(quint16 cmd, QByteArray &payloadout, QByteArray &packetout);
    EC_HOST_CMD_STATUS WaitBusReady();
    EC_HOST_CMD_STATUS GetPayloadIn(QByteArray &in);
    void SendPacketOut(QByteArray &packetOut);
};

#endif // EMITHREAD_H
