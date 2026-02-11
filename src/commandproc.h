#ifndef COMMANDPROC_H
#define COMMANDPROC_H

#include <QObject>
#include <QVariant>
#include "logger.h"
#include "RegistryAccess.h"
#include "WmiAccess.h"
#include "eccommunication/ecmanager.h"
#include "action/actioncommandqueue.h"
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

    void triggerActionEvent(uint32_t eventId);

    uint32_t queueAddAction(uint32_t eventId, const QString& name, const QString& qmlPath, const QStringList& params, int position = -1);
    uint32_t queueEditAction(uint32_t eventId, int index, const QString& name, const QString& qmlPath, const QStringList& params);
    uint32_t queueRemoveAction(uint32_t eventId, int index);
    uint32_t queueGetActions(uint32_t eventId);
    uint32_t queueGetAllEvents();
    uint32_t queueGetAvailableActions();
    uint32_t queueSaveActions();

    // Store results from Monitor
    void storeActionResult(uint32_t commandId, const patrol::ActionCommandResultRequest& result);
    bool getActionResult(uint32_t commandId, patrol::ActionCommandResultRequest& outResult, int timeoutMs = 5000);
    patrol::PollActionCommandsResponse handlePollActionCommands(const patrol::PollActionCommandsRequest& req);
    patrol::ActionCommandResultResponse handleActionCommandResult(const patrol::ActionCommandResultRequest& req);
    patrol::QueueActionCommandResponse handleQueueActionCommand(const patrol::QueueActionCommandRequest& req);

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
    patrol::EcAcpiQueueWriteResponse handleEcAcpiQueueWrite(const patrol::EcAcpiQueueWriteRequest& req);
    patrol::PowerCommandResponse handlePowerCommand(const patrol::PowerCommandRequest& request);
    patrol::DisplayBrightnessResponse handleDisplayBrightness(
        const patrol::DisplayBrightnessRequest& req);
    patrol::DisplayAutoBrightnessResponse handleDisplayAutoBrightness(
        const patrol::DisplayAutoBrightnessRequest& req);
    Logger* m_pLogger;
    RegistryAccess m_RegistryAccess;
    WmiAccess m_WmiAccess;
    EcManager* m_pEcManager;
    ActionCommandQueue m_actionQueue;

};

#endif // COMMANDPROC_H
