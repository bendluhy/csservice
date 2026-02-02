#ifndef COMMANDPROC_H
#define COMMANDPROC_H

#include <QObject>
#include <QVariant>
#include "logger.h"
#include "RegistryAccess.h"
#include "WmiAccess.h"
#include "eccommunication/ecmanager.h"
#include "command.qpb.h"

class CommandProc : public QObject
{
    Q_OBJECT
public:
    explicit CommandProc(Logger* logger, QObject *parent = nullptr);
    ~CommandProc();

    // Initialize EC subsystem
    bool initializeEc(quint16 emiOffset = 0x220);
    bool isEcInitialized() const;
    EcManager* getEcManager() {return m_pEcManager;};
    // Process protobuf command and return response
    patrol::Command processCommand(const patrol::Command& request);

private:
    // MSR
    patrol::MsrReadResponse handleMsrRead(const patrol::MsrReadRequest& req);
    patrol::MsrWriteResponse handleMsrWrite(const patrol::MsrWriteRequest& req);

    // Registry
    patrol::RegistryReadResponse handleRegistryRead(const patrol::RegistryReadRequest& req);
    patrol::RegistryWriteResponse handleRegistryWrite(const patrol::RegistryWriteRequest& req);
    patrol::RegistryDeleteResponse handleRegistryDelete(const patrol::RegistryDeleteRequest& req);

    // WMI
    patrol::WmiQueryResponse handleWmiQuery(const patrol::WmiQueryRequest& req);

    // File operations
    patrol::FileDeleteResponse handleFileDelete(const patrol::FileDeleteRequest& req);
    patrol::FileRenameResponse handleFileRename(const patrol::FileRenameRequest& req);
    patrol::FileCopyResponse handleFileCopy(const patrol::FileCopyRequest& req);
    patrol::FileMoveResponse handleFileMove(const patrol::FileMoveRequest& req);

    // Capabilities & System Info
    patrol::GetCapabilitiesResponse handleGetCapabilities(const patrol::GetCapabilitiesRequest& req);
    patrol::GetSystemInfoResponse handleGetSystemInfo(const patrol::GetSystemInfoRequest& req);

    // EC Commands
    patrol::EcRawCommandResponse handleEcRawCommand(const patrol::EcRawCommandRequest& req);
    patrol::EcAcpiReadResponse handleEcAcpiRead(const patrol::EcAcpiReadRequest& req);
    patrol::EcAcpiWriteResponse handleEcAcpiWrite(const patrol::EcAcpiWriteRequest& req);
    patrol::EcRamReadResponse handleEcRamRead(const patrol::EcRamReadRequest& req);
    patrol::EcDfuInfoResponse handleEcDfuInfo(const patrol::EcDfuInfoRequest& req);
    patrol::EcBatteryHealthResponse handleEcBatteryHealth(const patrol::EcBatteryHealthRequest& req);
    patrol::EcPeciReadResponse handleEcPeciRead(const patrol::EcPeciReadRequest& req);
    patrol::EcPeciWriteResponse handleEcPeciWrite(const patrol::EcPeciWriteRequest& req);
    patrol::EcSmbusResponse handleEcSmbus(const patrol::EcSmbusRequest& req);
    patrol::EcShellCommandResponse handleEcShellCommand(const patrol::EcShellCommandRequest& req);
    patrol::EcGetStatusResponse handleEcGetStatus(const patrol::EcGetStatusRequest& req);

    patrol::PowerCommandResponse handlePowerCommand(const patrol::PowerCommandRequest& request);
    Logger* m_pLogger;
    RegistryAccess m_RegistryAccess;
    WmiAccess m_WmiAccess;
    EcManager* m_pEcManager;
};

#endif // COMMANDPROC_H
