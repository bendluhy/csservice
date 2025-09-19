#ifndef COMMANDPROC_H
#define COMMANDPROC_H

#include <QObject>
#include <QVariant>
#include "logger.h"
#include "winring0.h"
#include "RegistryAccess.h"
#include "WmiAccess.h"
#include "CommandMessage.h"

class CommandProc : public QObject
{
    Q_OBJECT
public:
    explicit CommandProc(Logger* logger, QObject *parent = nullptr);

    // Process command message (renamed from SharedMemoryData)
    int ProcessCommand(CommandMessage* pMsg);

private:
    int handleMsrRead(CommandMessage* pMsg);
    int handleMsrWrite(CommandMessage* pMsg);
    int handleRegRead(CommandMessage* pMsg);
    int handleRegWrite(CommandMessage* pMsg);
    int handleRegDel(CommandMessage* pMsg);
    int handleWmiQuery(CommandMessage* pMsg);
    int handleWmiMethod(CommandMessage* pMsg);
    int handleInvalidCmd(CommandMessage* pMsg);
    int handleFileDelete(CommandMessage* pMsg);
    int handleFileRename(CommandMessage* pMsg);
    int handleFileCopy(CommandMessage* pMsg);
    int handleFileMove(CommandMessage* pMsg);

    Logger* m_pLogger;
    WinRing0 m_WinRing0;
    RegistryAccess m_RegistryAccess;
    WmiAccess m_WmiAccess;
};

#endif // COMMANDPROC_H
