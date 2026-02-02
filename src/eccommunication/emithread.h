#ifndef EMITHREAD_H
#define EMITHREAD_H

#include <QObject>
#include <QLoggingCategory>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QThread>
#include <QPointer>
#include "host_ec_cmds.h"
#include "portio.h"

class EmiThread : public QThread
{
    Q_OBJECT

public:
    explicit EmiThread(QObject *parent = nullptr);
    ~EmiThread();
    void run() override;
    void stop();
    int addCmdToQueue(QSharedPointer<EmiCmd>);

signals:
    void CommandDone(QSharedPointer<EmiCmd>);
    void TxOut(int bytes);
    void RxIn(int bytes);

private:
    quint16 m_EmiOffset = 0x220;
    PortIo* m_pPort;
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
