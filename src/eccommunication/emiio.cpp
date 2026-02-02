#include <QRandomGenerator>
#include <QThread>
#include <QLoggingCategory>
#include "appstd.h"
#include "emiio.h"

#define IO_RANGE_SIZE   32
#define EMI_INST_MAX    3
#define EMI_SIMULATE    0

#define HOST_EC_IND     m_RegsList[0].ioadd
#define EC_HOST_IND     m_RegsList[1].ioadd
#define ADD0_IND        m_RegsList[2].ioadd
#define ADD1_IND        m_RegsList[3].ioadd
#define DAT0_IND        m_RegsList[4].ioadd
#define DAT1_IND        m_RegsList[5].ioadd
#define DAT2_IND        m_RegsList[6].ioadd
#define DAT3_IND        m_RegsList[7].ioadd
#define INTSL_IND       m_RegsList[8].ioadd
#define INTSH_IND       m_RegsList[9].ioadd
#define INTML_IND       m_RegsList[0xA].ioadd
#define INTMH_IND       m_RegsList[0xB].ioadd
#define APPID_IND       m_RegsList[0xC].ioadd



EmiIo::EmiIo(QObject *parent)
    : QObject{parent}
{
    m_Inst = -1;
    m_Name = "EMI?";

    //Register the debug class

    //Build the list of regs
    const struct EmiRegs cregsinit[] = {
        {"HOST-EC",0xFF,0},
        {"EC-HOST",0xFF,1},
        {"ADD0",0xFF,2},
        {"ADD1",0xFF,3},
        {"DAT0",0xFF,4},
        {"DAT1",0xFF,5},
        {"DAT2",0xFF,6},
        {"DAT3",0xFF,7},
        {"INTSL",0xFF,8},
        {"INTSH",0xFF,9},
        {"INTML",0xFF,0xA},
        {"INTMH",0xFF,0xB},
        {"APPID",0xFF,0xC},
    };

    //Load them into our regs
    for (int i=0;i<13;i++)
    {
        m_RegsList.append(cregsinit[i]);
    }

    connect(&m_Thread,&EmiThread::CommandDone,this,&EmiIo::CommandDone);
    m_Thread.start();
}

EmiIo::~EmiIo()
{
}

int EmiIo::readRegs()
{
    m_Mutex.lock();

    //Read all the regs
    for (int i=0;i<m_RegsList.count();i++)
    {
#if EMI_SIMULATE
        m_RegsList[i].value = QRandomGenerator::global()->generate();
#else
        //Get access to the port io
        PortIo* pPort = PortIo::instance();

        quint8 val;
        pPort->Read(m_RegsList[i].ioadd,&val);
        m_RegsList[i].value = val;
#endif
    }

    m_Mutex.unlock();

    emit regListChanged();

    return 0;
}

void EmiIo::setIoOffset(quint16 iooffset)
{
    //Mask the lower bits
    iooffset &= ~(IO_RANGE_SIZE-1);

    QMutexLocker locker(&m_Mutex);
    for (int i=0;i<m_RegsList.count();i++)
    {
        //Remap the offsets
        quint16 io = m_RegsList[i].ioadd;
        io &= (IO_RANGE_SIZE-1);
        io += iooffset;
        m_RegsList[i].ioadd = io;
    }

    m_IoOffset = iooffset;
    emit memIoOffsetChanged();
    emit regListChanged();
}

void EmiIo::setInstance(quint8 inst)
{
    if (inst >= EMI_INST_MAX)
    {
        m_Inst = -1;
        m_Name = "EMI?";
    }
    else
    {
        m_Inst = inst;
        m_Name = QString("EMI%1").arg(inst);
    }
}

QList<EmiRegs> EmiIo::getRegList()
{
    QMutexLocker locker(&m_Mutex);
    return m_RegsList;
}

int EmiIo::SendCmd(QSharedPointer<EmiCmd> pCmd)
{
    //Mark the result in an unknown state
    pCmd->result = -1;

    //Let the thread process the command
    return m_Thread.addCmdToQueue(pCmd);
}

void EmiIo::CommandDone(QSharedPointer<EmiCmd> pCmd)
{
    if (pCmd->FuncDone)
    {
        pCmd->FuncDone(pCmd);
    }
}

