#include "ecmanager.h"
#include "logger.h"
#include <QElapsedTimer>

EcManager::EcManager(Logger* logger, QObject* parent)
    : QObject(parent)
    , m_logger(logger)
    , m_thread(nullptr)
    , m_portIo(nullptr)
    , m_initialized(false)
    , m_emiOffset(0x220)
    , m_packetIdCounter(1)
    , m_totalBytesTx(0)
    , m_totalBytesRx(0)
    , m_commandCount(0)
    , m_errorCount(0)
{
}

EcManager::~EcManager()
{
    if (m_thread) {
        m_thread->stop();
        delete m_thread;
        m_thread = nullptr;
    }
    m_initialized = false;
}

bool EcManager::initialize(quint16 emiOffset)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        log("EcManager already initialized");
        return true;
    }

    m_emiOffset = emiOffset;

    // Get port IO instance
    m_portIo = PortIo::instance();
    if (!m_portIo || !m_portIo->IsLoaded()) {
        log("Failed to load PortIO driver", 2);
        emit communicationError("PortIO driver not loaded");
        return false;
    }

    log(QString("PortIO driver loaded, EMI offset: 0x%1").arg(m_emiOffset, 4, 16, QChar('0')));

    // Create and start the EMI thread
    m_thread = new EmiThread(this);

    // Connect signals
    connect(m_thread, &EmiThread::CommandDone, this, &EcManager::onCommandDone, Qt::QueuedConnection);
    connect(m_thread, &EmiThread::TxOut, this, &EcManager::onTxOut, Qt::QueuedConnection);
    connect(m_thread, &EmiThread::RxIn, this, &EcManager::onRxIn, Qt::QueuedConnection);

    // Start the thread
    m_thread->start();

    m_initialized = true;
    log("EcManager initialized successfully");

    return true;
}

bool EcManager::isPortIoLoaded() const
{
    return m_portIo && m_portIo->IsLoaded();
}

void EcManager::setEmiOffset(quint16 offset)
{
    QMutexLocker locker(&m_mutex);
    m_emiOffset = offset;
    log(QString("EMI offset set to 0x%1").arg(m_emiOffset, 4, 16, QChar('0')));
}

// ============================================================================
// Synchronous API
// ============================================================================

EC_HOST_CMD_STATUS EcManager::sendCommandSync(quint16 cmd,
                                              const QByteArray& payloadOut,
                                              QByteArray& payloadIn,
                                              int timeoutMs)
{
    auto pCmd = QSharedPointer<EmiCmd>::create();
    pCmd->cmd = cmd;
    pCmd->payloadout = payloadOut;
    pCmd->result = EC_HOST_CMD_TIMEOUT;

    EC_HOST_CMD_STATUS status = sendCommandSync(pCmd, timeoutMs);
    payloadIn = pCmd->payloadin;

    return status;
}

EC_HOST_CMD_STATUS EcManager::sendCommandSync(QSharedPointer<EmiCmd> pCmd, int timeoutMs)
{
    if (!m_initialized || !m_thread) {
        log("EcManager not initialized", 2);
        return EC_HOST_CMD_UNAVAILABLE;
    }

    QMutexLocker locker(&m_mutex);

    // Assign packet ID
    pCmd->packetid = nextPacketId();
    pCmd->result = EC_HOST_CMD_TIMEOUT;

    // Track this command for synchronous wait
    m_pendingCommands.insert(pCmd->packetid, pCmd);

    // Set up completion callback that signals our wait condition.
    // This callback is invoked directly by EmiThread (not via Qt signal),
    // so it runs in the EMI thread's context while we're blocked in wait().
    // QWaitCondition::wait() releases the mutex, so the callback can acquire it.
    bool completed = false;
    pCmd->FuncDone = [this, &completed](QSharedPointer<EmiCmd>) {
        // Note: We need the mutex to safely set 'completed' and wake the waiter.
        // The waiter is in QWaitCondition::wait() which has released the mutex.
        QMutexLocker lock(&m_mutex);
        completed = true;
        m_waitCondition.wakeAll();
    };

    // Queue the command
    if (m_thread->addCmdToQueue(pCmd) != 0) {
        pCmd->FuncDone = nullptr;  // Clear callback before removing
        m_pendingCommands.remove(pCmd->packetid);
        log("Failed to queue command", 2);
        return EC_HOST_CMD_ERROR;
    }

    m_commandCount++;
    /*log(QString("Queued sync command 0x%1, packet %2")
            .arg(pCmd->cmd, 4, 16, QChar('0'))
            .arg(pCmd->packetid), 3);
    */
    // Wait for completion - wait() atomically releases mutex and waits
    QElapsedTimer timer;
    timer.start();

    while (!completed && timer.elapsed() < timeoutMs) {
        // wait() releases m_mutex, allowing FuncDone callback to acquire it
        m_waitCondition.wait(&m_mutex, qMin(100, timeoutMs - (int)timer.elapsed()));
    }

    // Clear callback to avoid dangling reference to 'completed' local variable
    pCmd->FuncDone = nullptr;
    m_pendingCommands.remove(pCmd->packetid);

    if (!completed) {
        log(QString("Command 0x%1 timed out after %2ms")
                .arg(pCmd->cmd, 4, 16, QChar('0'))
                .arg(timeoutMs), 1);
        m_errorCount++;
        return EC_HOST_CMD_TIMEOUT;
    }

    if (pCmd->result != EC_HOST_CMD_SUCCESS) {
        log(QString("Command 0x%1 failed with status %2")
                .arg(pCmd->cmd, 4, 16, QChar('0'))
                .arg(pCmd->result), 1);
        m_errorCount++;
    }

    return static_cast<EC_HOST_CMD_STATUS>(pCmd->result);
}

// ============================================================================
// Asynchronous API
// ============================================================================

quint32 EcManager::sendCommandAsync(quint16 cmd,
                                    const QByteArray& payloadOut,
                                    CommandCallback callback)
{
    auto pCmd = QSharedPointer<EmiCmd>::create();
    pCmd->cmd = cmd;
    pCmd->payloadout = payloadOut;

    QMutexLocker locker(&m_mutex);
    pCmd->packetid = nextPacketId();

    if (callback) {
        m_asyncCallbacks.insert(pCmd->packetid, callback);
    }

    locker.unlock();

    quint32 packetId = sendCommandAsync(pCmd);
    if (packetId == 0 && callback) {
        QMutexLocker lock(&m_mutex);
        m_asyncCallbacks.remove(pCmd->packetid);
    }

    return packetId;
}

quint32 EcManager::sendCommandAsync(QSharedPointer<EmiCmd> pCmd)
{
    if (!m_initialized || !m_thread) {
        log("EcManager not initialized", 2);
        return 0;
    }

    QMutexLocker locker(&m_mutex);

    if (pCmd->packetid == 0) {
        pCmd->packetid = nextPacketId();
    }

    // Set up the done callback to route through our handler
    quint32 packetId = pCmd->packetid;
    pCmd->FuncDone = [this](QSharedPointer<EmiCmd> cmd) {
        // This will be called from the EMI thread
        // Route to our slot via signal
        emit commandCompleted(cmd->packetid, static_cast<EC_HOST_CMD_STATUS>(cmd->result));
    };

    if (m_thread->addCmdToQueue(pCmd) != 0) {
        log("Failed to queue async command", 2);
        return 0;
    }

    m_commandCount++;
    log(QString("Queued async command 0x%1, packet %2")
            .arg(pCmd->cmd, 4, 16, QChar('0'))
            .arg(packetId), 3);

    return packetId;
}

// ============================================================================
// Convenience Methods
// ============================================================================

EC_HOST_CMD_STATUS EcManager::acpi0Read(quint32 offset, quint32 size, QByteArray& data)
{
    mem_region_r_e req;
    req.start = offset;
    req.size = size;

    QByteArray payload(reinterpret_cast<const char*>(&req), sizeof(req));
    return sendCommandSync(ECCMD_ACPI0_READ, payload, data);
}

EC_HOST_CMD_STATUS EcManager::acpi0Write(quint32 offset, const QByteArray& data)
{
    QByteArray payload;
    payload.reserve(sizeof(mem_region_w) + data.size());

    mem_region_w hdr;
    hdr.start = offset;
    hdr.size = data.size();

    payload.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    payload.append(data);

    QByteArray response;
    return sendCommandSync(ECCMD_ACPI0_WRITE, payload, response);
}

EC_HOST_CMD_STATUS EcManager::ecRamRead(quint32 offset, quint32 size, QByteArray& data)
{
    mem_region_r_e req;
    req.start = offset;
    req.size = size;

    QByteArray payload(reinterpret_cast<const char*>(&req), sizeof(req));
    return sendCommandSync(ECCMD_ECRAM_READ, payload, data);
}

EC_HOST_CMD_STATUS EcManager::getDfuInfo(dfu_info& info)
{
    QByteArray response;
    EC_HOST_CMD_STATUS status = sendCommandSync(ECCMD_DFU_INFO, QByteArray(), response);

    if (status == EC_HOST_CMD_SUCCESS && response.size() >= (int)sizeof(dfu_info)) {
        memcpy(&info, response.constData(), sizeof(dfu_info));
    }

    return status;
}

EC_HOST_CMD_STATUS EcManager::getBatteryHealth(bat_health& health)
{
    QByteArray response;
    EC_HOST_CMD_STATUS status = sendCommandSync(ECCMD_BAT_GET_HEALTH, QByteArray(), response);

    if (status == EC_HOST_CMD_SUCCESS && response.size() >= (int)sizeof(bat_health)) {
        memcpy(&health, response.constData(), sizeof(bat_health));
    }

    return status;
}

EC_HOST_CMD_STATUS EcManager::sendShellCommand(const QString& command)
{
    shell_cmd cmd;
    QByteArray cmdBytes = command.toUtf8();

    if (cmdBytes.size() >= MAX_SHELL_CMD_SIZE) {
        return EC_HOST_CMD_OVERFLOW;
    }

    cmd.size = cmdBytes.size();
    memset(cmd.str, 0, MAX_SHELL_CMD_SIZE);
    memcpy(cmd.str, cmdBytes.constData(), cmdBytes.size());

    QByteArray payload(reinterpret_cast<const char*>(&cmd), sizeof(cmd));
    QByteArray response;

    return sendCommandSync(ECCMD_SHELL_CMD, payload, response);
}

EC_HOST_CMD_STATUS EcManager::peciReadPackage(quint8 hostId, quint8 index,
                                              quint8 paramL, quint8 paramH,
                                              quint32& data)
{
    peci_rd_pkg req;
    req.hostid = hostId;
    req.index = index;
    req.parmL = paramL;
    req.parmH = paramH;

    QByteArray payload(reinterpret_cast<const char*>(&req), sizeof(req));
    QByteArray response;

    EC_HOST_CMD_STATUS status = sendCommandSync(ECCMD_PECI_RD_PKG, payload, response);

    if (status == EC_HOST_CMD_SUCCESS && response.size() >= (int)sizeof(peci_rd_pkg_resp)) {
        const peci_rd_pkg_resp* resp = reinterpret_cast<const peci_rd_pkg_resp*>(response.constData());
        data = resp->data;
    }

    return status;
}

EC_HOST_CMD_STATUS EcManager::peciWritePackage(quint8 hostId, quint8 index,
                                               quint8 paramL, quint8 paramH,
                                               quint32 data)
{
    peci_wr_pkg req;
    req.hostid = hostId;
    req.index = index;
    req.parmL = paramL;
    req.parmH = paramH;
    req.data = data;

    QByteArray payload(reinterpret_cast<const char*>(&req), sizeof(req));
    QByteArray response;

    return sendCommandSync(ECCMD_PECI_WR_PKG, payload, response);
}

EC_HOST_CMD_STATUS EcManager::smbusCommand(const smbus_cmd& cmd, smbus_cmd& response)
{
    QByteArray payload(reinterpret_cast<const char*>(&cmd), sizeof(cmd));
    QByteArray respData;

    EC_HOST_CMD_STATUS status = sendCommandSync(ECCMD_SMBUS_PROC, payload, respData);

    if (status == EC_HOST_CMD_SUCCESS && respData.size() >= (int)sizeof(smbus_cmd)) {
        memcpy(&response, respData.constData(), sizeof(smbus_cmd));
    }

    return status;
}

// ============================================================================
// Private Slots
// ============================================================================

void EcManager::onCommandDone(QSharedPointer<EmiCmd> pCmd)
{
    QMutexLocker locker(&m_mutex);

    // Check if there's an async callback registered
    if (m_asyncCallbacks.contains(pCmd->packetid)) {
        CommandCallback callback = m_asyncCallbacks.take(pCmd->packetid);
        locker.unlock();

        if (callback) {
            callback(static_cast<EC_HOST_CMD_STATUS>(pCmd->result), pCmd->payloadin);
        }
    }

    // Wake any synchronous waiters
    m_waitCondition.wakeAll();
}

void EcManager::onTxOut(int bytes)
{
    m_totalBytesTx += bytes;
    emit dataTx(bytes);
}

void EcManager::onRxIn(int bytes)
{
    m_totalBytesRx += bytes;
    emit dataRx(bytes);
}

// ============================================================================
// Private Helpers
// ============================================================================

void EcManager::log(const QString& message, int level)
{
    if (m_logger) {
        Logger::LogLevel logLevel;
        switch (level) {
        case 1: logLevel = Logger::Warning; break;
        case 2: logLevel = Logger::Error; break;
        case 3: logLevel = Logger::Debug; break;
        default: logLevel = Logger::Info; break;
        }
        m_logger->log(QString("EcManager: %1").arg(message), logLevel);
    }
}

quint32 EcManager::nextPacketId()
{
    // Note: caller must hold m_mutex
    if (m_packetIdCounter == 0) {
        m_packetIdCounter = 1; // Skip 0 as it's used for "invalid"
    }
    return m_packetIdCounter++;
}
