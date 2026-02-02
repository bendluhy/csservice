#ifndef EMIIO_H
#define EMIIO_H

#include <QObject>
#include <QMutex>
#include "host_ec_cmds.h"
#include "emithread.h"


struct EmiRegs {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(quint8 value MEMBER value)
    Q_PROPERTY(quint16 ioadd MEMBER ioadd)

public:
    QString name;
    quint8 value;
    quint16 ioadd;
};

class EmiIo : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QList<EmiRegs> regsList READ getRegList NOTIFY regListChanged)
    Q_PROPERTY(bool enabled READ getEnabled NOTIFY memEnabledChanged)
    Q_PROPERTY(quint16 iooffset READ getIoOffset WRITE setIoOffset NOTIFY memIoOffsetChanged)
    Q_PROPERTY(quint8 txrate READ getTxRate  NOTIFY txRateChanged)
    Q_PROPERTY(quint8 rxrate READ getRxRate  NOTIFY rxRateChanged)

public:
    explicit EmiIo(QObject *parent = nullptr);
    ~EmiIo();

    Q_INVOKABLE int readRegs(void);
    int getTxRate(void) {return m_TxRate;};
    int getRxRate(void) {return m_RxRate;};
    bool getEnabled(void)  const {return m_bEnabled;}
    quint16 getIoOffset(void)  const {return m_IoOffset;}
    void setIoOffset(quint16 iooffset);
    void setInstance(quint8 inst);
    QList<EmiRegs> getRegList();
    int SendCmd(QSharedPointer<EmiCmd>);

signals:
    void regListChanged();
    void memEnabledChanged();
    void memIoOffsetChanged();
    void txRateChanged(void);
    void rxRateChanged(void);

public slots:
    void CommandDone(QSharedPointer<EmiCmd> pCmd);

private:
    int m_TxRate = 0;
    int m_RxRate = 0;
    int m_TxTotal = 0;
    int m_RxTotal = 0;
    EmiThread m_Thread;
    QMutex m_Mutex;
    QList<EmiRegs> m_RegsList;
    QString m_Name;
    int m_Inst = -1;
    bool m_bEnabled = false;
    quint16 m_IoOffset = 0x220;
};

#endif // EMIIO_H
