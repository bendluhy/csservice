#include "commandproc.h"
#include <windows.h>
#include "./os/os.h"
// Qt Protobuf generates enums inside Gadget wrapper classes
// Use these shortcuts for cleaner code
using ResultCode = patrol::ResultCodeGadget::ResultCode;
using RegValueType = patrol::RegValueTypeGadget::RegValueType;
using EcStatus = patrol::EcStatusGadget::EcStatus;

CommandProc::CommandProc(Logger* logger, QObject *parent)
    : QObject(parent)
    , m_pLogger(logger)
    , m_RegistryAccess(logger)
    , m_WmiAccess(logger)
    , m_pEcManager(nullptr)
{
    m_WmiAccess.initialize();
}

CommandProc::~CommandProc()
{
    if (m_pEcManager) {
        delete m_pEcManager;
        m_pEcManager = nullptr;
    }
}

bool CommandProc::initializeEc(quint16 emiOffset)
{
    if (m_pEcManager) {
        m_pLogger->log("CommandProc: EC already initialized", Logger::Warning);
        return m_pEcManager->isInitialized();
    }

    m_pEcManager = new EcManager(m_pLogger, this);

    if (!m_pEcManager->initialize(emiOffset)) {
        m_pLogger->log("CommandProc: Failed to initialize EC", Logger::Error);
        delete m_pEcManager;
        m_pEcManager = nullptr;
        return false;
    }

    m_pLogger->log(QString("CommandProc: EC initialized at offset 0x%1")
                       .arg(emiOffset, 4, 16, QChar('0')), Logger::Info);
    return true;
}

bool CommandProc::isEcInitialized() const
{
    return m_pEcManager && m_pEcManager->isInitialized();
}

patrol::Command CommandProc::processCommand(const patrol::Command& request)
{
    patrol::Command response;
    response.setSequenceNumber(request.sequenceNumber());

    // Route based on which payload field is set
    if (request.hasMsrReadReq()) {
        response.setMsrReadResp(handleMsrRead(request.msrReadReq()));
    }
    else if (request.hasMsrWriteReq()) {
        response.setMsrWriteResp(handleMsrWrite(request.msrWriteReq()));
    }
    else if (request.hasRegistryReadReq()) {
        response.setRegistryReadResp(handleRegistryRead(request.registryReadReq()));
    }
    else if (request.hasRegistryWriteReq()) {
        response.setRegistryWriteResp(handleRegistryWrite(request.registryWriteReq()));
    }
    else if (request.hasRegistryDeleteReq()) {
        response.setRegistryDeleteResp(handleRegistryDelete(request.registryDeleteReq()));
    }
    else if (request.hasWmiQueryReq()) {
        response.setWmiQueryResp(handleWmiQuery(request.wmiQueryReq()));
    }
    else if (request.hasFileDeleteReq()) {
        response.setFileDeleteResp(handleFileDelete(request.fileDeleteReq()));
    }
    else if (request.hasFileRenameReq()) {
        response.setFileRenameResp(handleFileRename(request.fileRenameReq()));
    }
    else if (request.hasFileCopyReq()) {
        response.setFileCopyResp(handleFileCopy(request.fileCopyReq()));
    }
    else if (request.hasFileMoveReq()) {
        response.setFileMoveResp(handleFileMove(request.fileMoveReq()));
    }
    else if (request.hasGetCapabilitiesReq()) {
        response.setGetCapabilitiesResp(handleGetCapabilities(request.getCapabilitiesReq()));
    }
    else if (request.hasGetSystemInfoReq()) {
        response.setGetSystemInfoResp(handleGetSystemInfo(request.getSystemInfoReq()));
    }
    // EC Commands
    else if (request.hasEcRawReq()) {
        response.setEcRawResp(handleEcRawCommand(request.ecRawReq()));
    }
    else if (request.hasEcAcpiReadReq()) {
        response.setEcAcpiReadResp(handleEcAcpiRead(request.ecAcpiReadReq()));
    }
    else if (request.hasEcAcpiWriteReq()) {
        response.setEcAcpiWriteResp(handleEcAcpiWrite(request.ecAcpiWriteReq()));
    }
    else if (request.hasEcRamReadReq()) {
        response.setEcRamReadResp(handleEcRamRead(request.ecRamReadReq()));
    }
    else if (request.hasEcDfuInfoReq()) {
        response.setEcDfuInfoResp(handleEcDfuInfo(request.ecDfuInfoReq()));
    }
    else if (request.hasEcBatteryHealthReq()) {
        response.setEcBatteryHealthResp(handleEcBatteryHealth(request.ecBatteryHealthReq()));
    }
    else if (request.hasEcPeciReadReq()) {
        response.setEcPeciReadResp(handleEcPeciRead(request.ecPeciReadReq()));
    }
    else if (request.hasEcPeciWriteReq()) {
        response.setEcPeciWriteResp(handleEcPeciWrite(request.ecPeciWriteReq()));
    }
    else if (request.hasEcSmbusReq()) {
        response.setEcSmbusResp(handleEcSmbus(request.ecSmbusReq()));
    }
    else if (request.hasEcShellReq()) {
        response.setEcShellResp(handleEcShellCommand(request.ecShellReq()));
    }
    else if (request.hasEcStatusReq()) {
        response.setEcStatusResp(handleEcGetStatus(request.ecStatusReq()));
    }
    else if (request.hasPowerReq()) {
        response.setPowerResp(handlePowerCommand(request.powerReq()));
    }
    else {
        m_pLogger->log("Unknown command type received", Logger::Warning);
    }

    return response;
}

// ============================================================================
// MSR Handlers
// ============================================================================

patrol::MsrReadResponse CommandProc::handleMsrRead(const patrol::MsrReadRequest& req)
{
    patrol::MsrReadResponse resp;

    quint32 msrAddr = static_cast<quint32>(req.msrAddress());
    m_pLogger->log(QString("MSR Read request - MSR: 0x%1").arg(msrAddr, 0, 16));

    // TODO: Implement actual MSR read via WinRing0 driver
    resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setDataLow(0);
    resp.setDataHigh(0);

    return resp;
}

patrol::MsrWriteResponse CommandProc::handleMsrWrite(const patrol::MsrWriteRequest& req)
{
    patrol::MsrWriteResponse resp;

    quint32 msrAddr = static_cast<quint32>(req.msrAddress());
    quint32 dataL = static_cast<quint32>(req.dataLow());
    quint32 dataH = static_cast<quint32>(req.dataHigh());

    m_pLogger->log(QString("MSR Write request - MSR: 0x%1, Low: 0x%2, High: 0x%3")
                       .arg(msrAddr, 0, 16).arg(dataL, 0, 16).arg(dataH, 0, 16));

    // TODO: Implement actual MSR write via WinRing0 driver
    resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));

    return resp;
}

// ============================================================================
// Registry Handlers
// ============================================================================

patrol::RegistryReadResponse CommandProc::handleRegistryRead(const patrol::RegistryReadRequest& req)
{
    patrol::RegistryReadResponse resp;

    QString keyPath = req.keyPath();
    QString valueName = req.valueName();
    auto valueType = req.valueType();

    m_pLogger->log(QString("Registry Read - Key: %1, Value: %2").arg(keyPath, valueName));

    QVariant value;
    quint16 regType = 0;

    switch (valueType) {
    case RegValueType::REG_TYPE_DWORD:
        regType = REG_DWORD;
        break;
    case RegValueType::REG_TYPE_QWORD:
        regType = REG_QWORD;
        break;
    case RegValueType::REG_TYPE_SZ:
        regType = REG_SZ;
        break;
    case RegValueType::REG_TYPE_EXPAND_SZ:
        regType = REG_EXPAND_SZ;
        break;
    case RegValueType::REG_TYPE_BINARY:
        regType = REG_BINARY;
        break;
    case RegValueType::REG_TYPE_MULTI_SZ:
        regType = REG_MULTI_SZ;
        break;
    default:
        regType = REG_SZ;
        break;
    }

    bool success = m_RegistryAccess.readValue(keyPath, valueName, value, regType);

    if (success) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        resp.setValueType(valueType);

        switch (valueType) {
        case RegValueType::REG_TYPE_DWORD:
            resp.setDwordValue(value.toUInt());
            break;
        case RegValueType::REG_TYPE_QWORD:
            resp.setQwordValue(value.toULongLong());
            break;
        case RegValueType::REG_TYPE_SZ:
        case RegValueType::REG_TYPE_EXPAND_SZ:
        case RegValueType::REG_TYPE_MULTI_SZ:
            resp.setStringValue(value.toString());
            break;
        case RegValueType::REG_TYPE_BINARY:
            resp.setData(value.toByteArray());
            break;
        default:
            resp.setStringValue(value.toString());
            break;
        }

        m_pLogger->log(QString("Registry Read success - Value: %1").arg(value.toString()));
    } else {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log("Registry Read failed", Logger::Warning);
    }

    return resp;
}

patrol::RegistryWriteResponse CommandProc::handleRegistryWrite(const patrol::RegistryWriteRequest& req)
{
    patrol::RegistryWriteResponse resp;

    QString keyPath = req.keyPath();
    QString valueName = req.valueName();
    auto valueType = req.valueType();

    m_pLogger->log(QString("Registry Write - Key: %1, Value: %2").arg(keyPath, valueName));

    QVariant value;
    quint16 regType = 0;

    switch (valueType) {
    case RegValueType::REG_TYPE_DWORD:
        regType = REG_DWORD;
        value = static_cast<quint32>(req.dwordValue());
        break;
    case RegValueType::REG_TYPE_QWORD:
        regType = REG_QWORD;
        value = static_cast<quint64>(req.qwordValue());
        break;
    case RegValueType::REG_TYPE_SZ:
        regType = REG_SZ;
        value = req.stringValue();
        break;
    case RegValueType::REG_TYPE_EXPAND_SZ:
        regType = REG_EXPAND_SZ;
        value = req.stringValue();
        break;
    case RegValueType::REG_TYPE_BINARY:
        regType = REG_BINARY;
        value = req.data();
        break;
    default:
        regType = REG_SZ;
        value = req.stringValue();
        break;
    }

    bool success = m_RegistryAccess.writeValue(keyPath, valueName, value, regType);

    if (success) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("Registry Write success");
    } else {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log("Registry Write failed", Logger::Warning);
    }

    return resp;
}

patrol::RegistryDeleteResponse CommandProc::handleRegistryDelete(const patrol::RegistryDeleteRequest& req)
{
    patrol::RegistryDeleteResponse resp;

    QString keyPath = req.keyPath();
    QString valueName = req.valueName();

    m_pLogger->log(QString("Registry Delete - Key: %1, Value: %2").arg(keyPath, valueName));

    bool success;
    if (valueName.isEmpty()) {
        success = m_RegistryAccess.deleteKey(keyPath);
    } else {
        success = m_RegistryAccess.deleteValue(keyPath, valueName);
    }

    if (success) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("Registry Delete success");
    } else {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log("Registry Delete failed", Logger::Warning);
    }

    return resp;
}

// ============================================================================
// WMI Handler
// ============================================================================

patrol::WmiQueryResponse CommandProc::handleWmiQuery(const patrol::WmiQueryRequest& req)
{
    patrol::WmiQueryResponse resp;

    QString namespacePath = req.namespacePath();
    QString query = req.query();
    QString property = req.property_proto();

    if (namespacePath.isEmpty()) {
        namespacePath = "ROOT\\CIMV2";
    }

    m_pLogger->log(QString("WMI Query - Namespace: %1, Query: %2, Property: %3")
                       .arg(namespacePath, query, property));

    QVector<QMap<QString, QVariant>> queryResults;
    bool success = m_WmiAccess.query(namespacePath, query, queryResults, property);

    if (success) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));

        QList<patrol::WmiQueryResult> results;
        for (const auto& resultMap : queryResults) {
            patrol::WmiQueryResult queryResult;
            QList<patrol::WmiPropertyValue> properties;

            for (auto it = resultMap.begin(); it != resultMap.end(); ++it) {
                patrol::WmiPropertyValue propValue;
                propValue.setName(it.key());
                propValue.setValue(it.value().toString());
                propValue.setType(QString::number(it.value().typeId()));
                properties.append(propValue);
            }

            queryResult.setProperties(properties);
            results.append(queryResult);
        }

        resp.setResults(results);
        m_pLogger->log(QString("WMI Query success - %1 results").arg(results.size()));
    } else {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log("WMI Query failed", Logger::Warning);
    }

    return resp;
}

// ============================================================================
// File Operation Handlers
// ============================================================================

patrol::FileDeleteResponse CommandProc::handleFileDelete(const patrol::FileDeleteRequest& req)
{
    patrol::FileDeleteResponse resp;

    QString filePath = req.filePath();
    m_pLogger->log(QString("File Delete - Path: %1").arg(filePath));

    if (DeleteFileW(reinterpret_cast<LPCWSTR>(filePath.utf16()))) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("File Delete success");
    } else {
        DWORD error = GetLastError();
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log(QString("File Delete failed - Error: %1").arg(error), Logger::Warning);
    }

    return resp;
}

patrol::FileRenameResponse CommandProc::handleFileRename(const patrol::FileRenameRequest& req)
{
    patrol::FileRenameResponse resp;

    QString oldPath = req.oldPath();
    QString newPath = req.newPath();
    m_pLogger->log(QString("File Rename - Old: %1, New: %2").arg(oldPath, newPath));

    if (MoveFileW(reinterpret_cast<LPCWSTR>(oldPath.utf16()),
                  reinterpret_cast<LPCWSTR>(newPath.utf16()))) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("File Rename success");
    } else {
        DWORD error = GetLastError();
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log(QString("File Rename failed - Error: %1").arg(error), Logger::Warning);
    }

    return resp;
}

patrol::FileCopyResponse CommandProc::handleFileCopy(const patrol::FileCopyRequest& req)
{
    patrol::FileCopyResponse resp;

    QString sourcePath = req.sourcePath();
    QString destPath = req.destPath();
    m_pLogger->log(QString("File Copy - Source: %1, Dest: %2").arg(sourcePath, destPath));

    if (CopyFileW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()),
                  reinterpret_cast<LPCWSTR>(destPath.utf16()), FALSE)) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("File Copy success");
    } else {
        DWORD error = GetLastError();
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log(QString("File Copy failed - Error: %1").arg(error), Logger::Warning);
    }

    return resp;
}

patrol::FileMoveResponse CommandProc::handleFileMove(const patrol::FileMoveRequest& req)
{
    patrol::FileMoveResponse resp;

    QString sourcePath = req.sourcePath();
    QString destPath = req.destPath();
    m_pLogger->log(QString("File Move - Source: %1, Dest: %2").arg(sourcePath, destPath));

    if (MoveFileW(reinterpret_cast<LPCWSTR>(sourcePath.utf16()),
                  reinterpret_cast<LPCWSTR>(destPath.utf16()))) {
        resp.setResult(static_cast<int>(ResultCode::RES_OK));
        m_pLogger->log("File Move success");
    } else {
        DWORD error = GetLastError();
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        m_pLogger->log(QString("File Move failed - Error: %1").arg(error), Logger::Warning);
    }

    return resp;
}

// ============================================================================
// Capabilities Handler
// ============================================================================

patrol::GetCapabilitiesResponse CommandProc::handleGetCapabilities(const patrol::GetCapabilitiesRequest& req)
{
    Q_UNUSED(req)

    patrol::GetCapabilitiesResponse resp;
    resp.setResult(static_cast<int>(ResultCode::RES_OK));

    patrol::HardwareCapabilities caps;
    caps.setHasNightMode(false);
    caps.setHasMsrAccess(false);
    caps.setHasEcControl(isEcInitialized());
    caps.setHasFanControl(false);
    caps.setHasDisplayControl(false);
    caps.setHasRgbLighting(false);
    caps.setHasBatteryInfo(isEcInitialized());
    caps.setDisplayCount(1);
    caps.setFanCount(0);
    caps.setTemperatureSensorCount(0);

    resp.setCapabilities(caps);

    m_pLogger->log("GetCapabilities processed");
    return resp;
}

// ============================================================================
// System Info Handler
// ============================================================================

patrol::GetSystemInfoResponse CommandProc::handleGetSystemInfo(const patrol::GetSystemInfoRequest& req)
{
    Q_UNUSED(req)

    patrol::GetSystemInfoResponse resp;
    resp.setResult(static_cast<int>(ResultCode::RES_OK));

    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(computerName, &size)) {
        resp.setMachineName(QString::fromWCharArray(computerName));
    }

    resp.setOsVersion(QStringLiteral("Windows"));

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        resp.setTotalMemory(memInfo.ullTotalPhys);
        resp.setAvailableMemory(memInfo.ullAvailPhys);
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    resp.setCpuCount(sysInfo.dwNumberOfProcessors);

    m_pLogger->log("GetSystemInfo processed");
    return resp;
}

// ============================================================================
// EC Command Handlers
// ============================================================================

patrol::EcRawCommandResponse CommandProc::handleEcRawCommand(const patrol::EcRawCommandRequest& req)
{
    patrol::EcRawCommandResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        m_pLogger->log("EC Raw Command failed - EC not initialized", Logger::Warning);
        return resp;
    }

    quint16 cmdId = static_cast<quint16>(req.commandId());
    QByteArray payloadOut = req.payload();
    int timeout = req.timeoutMs() > 0 ? req.timeoutMs() : 5000;

    m_pLogger->log(QString("EC Raw Command 0x%1, payload %2 bytes")
                       .arg(cmdId, 4, 16, QChar('0')).arg(payloadOut.size()), Logger::Debug);

    QByteArray payloadIn;
    EC_HOST_CMD_STATUS status = m_pEcManager->sendCommandSync(cmdId, payloadOut, payloadIn, timeout);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));
    resp.setPayload(payloadIn);

    return resp;
}

patrol::EcAcpiReadResponse CommandProc::handleEcAcpiRead(const patrol::EcAcpiReadRequest& req)
{
    patrol::EcAcpiReadResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    quint32 nsId = req.namespaceId();
    quint32 offset = req.offset();
    quint32 size = req.size();

    m_pLogger->log(QString("EC ACPI%1 Read offset=0x%2, size=%3")
                       .arg(nsId).arg(offset, 4, 16, QChar('0')).arg(size), Logger::Debug);

    // Build request payload
    mem_region_r_e memReq;
    memReq.start = offset;
    memReq.size = size;
    QByteArray payload(reinterpret_cast<const char*>(&memReq), sizeof(memReq));

    QByteArray data;
    quint16 cmd = (nsId == 0) ? ECCMD_ACPI0_READ : ECCMD_ACPI1_READ;
    EC_HOST_CMD_STATUS status = m_pEcManager->sendCommandSync(cmd, payload, data);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));
    resp.setData(data);

    return resp;
}

patrol::EcAcpiWriteResponse CommandProc::handleEcAcpiWrite(const patrol::EcAcpiWriteRequest& req)
{
    patrol::EcAcpiWriteResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    quint32 nsId = req.namespaceId();
    quint32 offset = req.offset();
    QByteArray data = req.data();

    m_pLogger->log(QString("EC ACPI%1 Write offset=0x%2, size=%3")
                       .arg(nsId).arg(offset, 4, 16, QChar('0')).arg(data.size()), Logger::Debug);

    // Build payload with header + data
    QByteArray payload;
    payload.reserve(sizeof(mem_region_w) + data.size());

    mem_region_w hdr;
    hdr.start = offset;
    hdr.size = data.size();
    payload.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    payload.append(data);

    QByteArray response;
    quint16 cmd = (nsId == 0) ? ECCMD_ACPI0_WRITE : ECCMD_ACPI1_WRITE;
    EC_HOST_CMD_STATUS status = m_pEcManager->sendCommandSync(cmd, payload, response);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    return resp;
}

patrol::EcRamReadResponse CommandProc::handleEcRamRead(const patrol::EcRamReadRequest& req)
{
    patrol::EcRamReadResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    QByteArray data;
    EC_HOST_CMD_STATUS status = m_pEcManager->ecRamRead(req.offset(), req.size(), data);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));
    resp.setData(data);

    return resp;
}

patrol::EcDfuInfoResponse CommandProc::handleEcDfuInfo(const patrol::EcDfuInfoRequest& req)
{
    Q_UNUSED(req)
    patrol::EcDfuInfoResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    dfu_info info;
    EC_HOST_CMD_STATUS status = m_pEcManager->getDfuInfo(info);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    if (status == EC_HOST_CMD_SUCCESS) {
        patrol::EcDfuInfo dfuInfo;
        dfuInfo.setAppSlotCount(info.app_slot_cnt);
        dfuInfo.setBootSlotCount(info.boot_slot_cnt);
        dfuInfo.setAppRunSlot(info.app_run_slot);
        dfuInfo.setBootRunSlot(info.boot_run_slot);
        dfuInfo.setAppSlotSize(info.app_slot_size);
        dfuInfo.setBootSlotSize(info.boot_slot_size);
        resp.setInfo(dfuInfo);
    }

    return resp;
}

patrol::EcBatteryHealthResponse CommandProc::handleEcBatteryHealth(const patrol::EcBatteryHealthRequest& req)
{
    Q_UNUSED(req)
    patrol::EcBatteryHealthResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    bat_health health;
    EC_HOST_CMD_STATUS status = m_pEcManager->getBatteryHealth(health);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    if (status == EC_HOST_CMD_SUCCESS) {
        patrol::EcBatteryHealth batHealth;
        batHealth.setHealthStatus(health.HealthStat);
        batHealth.setStatus1(health.Status1);
        batHealth.setFaults(health.Faults);
        batHealth.setCell1Voltage(health.cell1_V);
        batHealth.setCell2Voltage(health.cell2_V);
        batHealth.setCell3Voltage(health.cell3_V);
        batHealth.setCellDiff(health.cellDiff);
        batHealth.setRaIncPer1(health.RaIncPer_1);
        batHealth.setRaDecPer1(health.RaDecPer_1);
        batHealth.setRaIncPer2(health.RaIncPer_2);
        batHealth.setRaDecPer2(health.RaDecPer_2);
        batHealth.setRaIncPer3(health.RaIncPer_3);
        batHealth.setRaDecPer3(health.RaDecPer_3);
        batHealth.setTimeRest(health.TimeRest);
        batHealth.setTimeTempBad(health.TimeTempBad);
        batHealth.setTimeRun(health.TimeRun);
        batHealth.setSafetyAlert(health.safetyAlert);
        batHealth.setSafetyStatus(health.safetyStatus);
        batHealth.setPfAlert(health.pfalert);
        batHealth.setPfStatus(health.pfstatus);
        batHealth.setDischargeLimit(health.DischgLim);
        batHealth.setChargeLimit(health.ChgLim);
        batHealth.setStateOfHealth(health.SOH);
        resp.setHealth(batHealth);
    }

    return resp;
}

patrol::EcPeciReadResponse CommandProc::handleEcPeciRead(const patrol::EcPeciReadRequest& req)
{
    patrol::EcPeciReadResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    quint32 data = 0;
    EC_HOST_CMD_STATUS status = m_pEcManager->peciReadPackage(
        static_cast<quint8>(req.hostId()),
        static_cast<quint8>(req.index()),
        static_cast<quint8>(req.paramLow()),
        static_cast<quint8>(req.paramHigh()),
        data);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));
    resp.setData(data);

    return resp;
}

patrol::EcPeciWriteResponse CommandProc::handleEcPeciWrite(const patrol::EcPeciWriteRequest& req)
{
    patrol::EcPeciWriteResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    EC_HOST_CMD_STATUS status = m_pEcManager->peciWritePackage(
        static_cast<quint8>(req.hostId()),
        static_cast<quint8>(req.index()),
        static_cast<quint8>(req.paramLow()),
        static_cast<quint8>(req.paramHigh()),
        req.data());

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    return resp;
}

patrol::EcSmbusResponse CommandProc::handleEcSmbus(const patrol::EcSmbusRequest& req)
{
    patrol::EcSmbusResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    smbus_cmd cmd;
    cmd.bus = static_cast<quint8>(req.bus());
    cmd.prot = static_cast<quint8>(req.protocol());
    cmd.add = static_cast<quint8>(req.address());
    cmd.cmd = static_cast<quint8>(req.command());

    QByteArray reqData = req.data();
    cmd.cnt = qMin(static_cast<int>(reqData.size()), 32);
    memset(cmd.data, 0, 32);
    if (cmd.cnt > 0) {
        memcpy(cmd.data, reqData.constData(), cmd.cnt);
    }

    smbus_cmd response;
    EC_HOST_CMD_STATUS status = m_pEcManager->smbusCommand(cmd, response);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    if (status == EC_HOST_CMD_SUCCESS) {
        resp.setProtocol(response.prot);
        resp.setData(QByteArray(reinterpret_cast<const char*>(response.data), response.cnt));
    }

    return resp;
}

patrol::EcShellCommandResponse CommandProc::handleEcShellCommand(const patrol::EcShellCommandRequest& req)
{
    patrol::EcShellCommandResponse resp;

    if (!isEcInitialized()) {
        resp.setResult(static_cast<int>(ResultCode::RES_FAILED_OP));
        resp.setEcStatus(EcStatus::EC_STATUS_UNAVAILABLE);
        return resp;
    }

    QString command = req.command();
    m_pLogger->log(QString("EC Shell Command: %1").arg(command), Logger::Debug);

    EC_HOST_CMD_STATUS status = m_pEcManager->sendShellCommand(command);

    resp.setResult(status == EC_HOST_CMD_SUCCESS ?
                       static_cast<int>(ResultCode::RES_OK) :
                       static_cast<int>(ResultCode::RES_FAILED_OP));
    resp.setEcStatus(static_cast<EcStatus>(status));

    return resp;
}

patrol::EcGetStatusResponse CommandProc::handleEcGetStatus(const patrol::EcGetStatusRequest& req)
{
    Q_UNUSED(req)
    patrol::EcGetStatusResponse resp;

    resp.setResult(static_cast<int>(ResultCode::RES_OK));
    resp.setEcStatus(EcStatus::EC_STATUS_SUCCESS);

    if (m_pEcManager) {
        resp.setPortIoLoaded(m_pEcManager->isPortIoLoaded());
        resp.setEcInitialized(m_pEcManager->isInitialized());
        resp.setEmiOffset(m_pEcManager->getEmiOffset());
    } else {
        resp.setPortIoLoaded(false);
        resp.setEcInitialized(false);
        resp.setEmiOffset(0);
    }

    resp.setTotalCommands(0);
    resp.setTotalErrors(0);
    resp.setBytesTx(0);
    resp.setBytesRx(0);

    return resp;
}
patrol::PowerCommandResponse CommandProc::handlePowerCommand(const patrol::PowerCommandRequest& request)
{
    patrol::PowerCommandResponse response;

    bool success = false;
    QString errorMsg;

    switch (request.action()) {
    case patrol::PowerActionGadget::PowerAction::POWER_SHUTDOWN:
        m_pLogger->log(QString("Power: Shutdown requested (timeout=%1s, force=%2)")
                           .arg(request.timeoutSeconds()).arg(request.force()), Logger::Info);
        success = OS::shutdown(request.timeoutSeconds(), request.force(), request.reason());
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_RESTART:
        m_pLogger->log(QString("Power: Restart requested (timeout=%1s, force=%2)")
                           .arg(request.timeoutSeconds()).arg(request.force()), Logger::Info);
        success = OS::restart(request.timeoutSeconds(), request.force(), request.reason());
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_SLEEP:
        m_pLogger->log("Power: Sleep requested", Logger::Info);
        success = OS::sleep();
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_HIBERNATE:
        m_pLogger->log("Power: Hibernate requested", Logger::Info);
        success = OS::hibernate();
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_LOGOFF:
        m_pLogger->log(QString("Power: Logoff requested (force=%1)").arg(request.force()), Logger::Info);
        success = OS::logOff(request.force());
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_LOCK:
        m_pLogger->log("Power: Lock workstation requested", Logger::Info);
        success = OS::lockWorkstation();
        if (!success) errorMsg = OS::lastError();
        break;

    case patrol::PowerActionGadget::PowerAction::POWER_CANCEL:
        m_pLogger->log("Power: Cancel shutdown requested", Logger::Info);
        success = OS::cancelShutdown();
        if (!success) errorMsg = OS::lastError();
        break;

    default:
        errorMsg = "Unknown power action";
        m_pLogger->log(QString("Power: Unknown action %1").arg(static_cast<int>(request.action())), Logger::Warning);
        break;
    }

    response.setResult(success ? 0 : -1);  // RES_OK or RES_FAILED_OP
    response.setErrorMessage(errorMsg);

    return response;
}
