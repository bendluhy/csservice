#ifndef ECMANAGER_H
#define ECMANAGER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QSharedPointer>
#include <functional>
#include "host_ec_cmds.h"
#include "emithread.h"
#include "portio.h"
#include "../logger.h"

/**
 * @brief EcManager - Manages EC (Embedded Controller) communication for the service
 *
 * This class provides a synchronous and asynchronous interface to communicate
 * with the EC via the EMI (Embedded Memory Interface). It manages the underlying
 * EmiThread and provides thread-safe command execution.
 *
 * Usage:
 *   EcManager* ec = new EcManager(logger);
 *   if (ec->initialize(0x220)) {
 *       // Synchronous command
 *       QByteArray response;
 *       auto status = ec->sendCommandSync(ECCMD_ACPI0_READ, payload, response, 5000);
 *
 *       // Asynchronous command
 *       ec->sendCommandAsync(ECCMD_ACPI0_READ, payload, [](EC_HOST_CMD_STATUS status, const QByteArray& data) {
 *           // Handle response
 *       });
 *   }
 */
class EcManager : public QObject
{
    Q_OBJECT

public:
    explicit EcManager(Logger* logger, QObject* parent = nullptr);
    ~EcManager();

    /**
     * @brief Initialize the EC manager with the specified EMI IO offset
     * @param emiOffset The base IO port address for EMI (e.g., 0x220)
     * @return true if initialization successful
     */
    bool initialize(quint16 emiOffset = 0x220);

    /**
     * @brief Check if the EC manager is initialized and ready
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Check if the port IO driver is loaded
     */
    bool isPortIoLoaded() const;

    /**
     * @brief Get the current EMI IO offset
     */
    quint16 getEmiOffset() const { return m_emiOffset; }

    /**
     * @brief Set the EMI IO offset (must call before initialize or reinitialize after)
     */
    void setEmiOffset(quint16 offset);

    // ========================================================================
    // Synchronous API - blocks until command completes or times out
    // ========================================================================

    /**
     * @brief Send a command to the EC and wait for response
     * @param cmd The EC command ID (e.g., ECCMD_ACPI0_READ)
     * @param payloadOut Data to send with the command
     * @param payloadIn Buffer to receive response data
     * @param timeoutMs Timeout in milliseconds (default 5000)
     * @return EC_HOST_CMD_STATUS result code
     */
    EC_HOST_CMD_STATUS sendCommandSync(quint16 cmd,
                                       const QByteArray& payloadOut,
                                       QByteArray& payloadIn,
                                       int timeoutMs = 5000);

    /**
     * @brief Send a raw EmiCmd synchronously
     */
    EC_HOST_CMD_STATUS sendCommandSync(QSharedPointer<EmiCmd> pCmd, int timeoutMs = 5000);

    // ========================================================================
    // Asynchronous API - returns immediately, callback invoked when done
    // ========================================================================

    using CommandCallback = std::function<void(EC_HOST_CMD_STATUS status, const QByteArray& payloadIn)>;

    /**
     * @brief Send a command to the EC asynchronously
     * @param cmd The EC command ID
     * @param payloadOut Data to send with the command
     * @param callback Function called when command completes
     * @return Packet ID for tracking, or 0 on failure
     */
    quint32 sendCommandAsync(quint16 cmd,
                             const QByteArray& payloadOut,
                             CommandCallback callback);

    /**
     * @brief Send a raw EmiCmd asynchronously
     */
    quint32 sendCommandAsync(QSharedPointer<EmiCmd> pCmd);

    // ========================================================================
    // Convenience methods for common EC operations
    // ========================================================================

    /**
     * @brief Read from ACPI namespace 0
     */
    EC_HOST_CMD_STATUS acpi0Read(quint32 offset, quint32 size, QByteArray& data);

    /**
     * @brief Write to ACPI namespace 0
     */
    EC_HOST_CMD_STATUS acpi0Write(quint32 offset, const QByteArray& data);

    /**
     * @brief Read EC RAM
     */
    EC_HOST_CMD_STATUS ecRamRead(quint32 offset, quint32 size, QByteArray& data);

    /**
     * @brief Get DFU (firmware update) info
     */
    EC_HOST_CMD_STATUS getDfuInfo(dfu_info& info);

    /**
     * @brief Get battery health information
     */
    EC_HOST_CMD_STATUS getBatteryHealth(bat_health& health);

    /**
     * @brief Send shell command to EC console
     */
    EC_HOST_CMD_STATUS sendShellCommand(const QString& command);

    /**
     * @brief Read PECI package
     */
    EC_HOST_CMD_STATUS peciReadPackage(quint8 hostId, quint8 index,
                                       quint8 paramL, quint8 paramH,
                                       quint32& data);

    /**
     * @brief Write PECI package
     */
    EC_HOST_CMD_STATUS peciWritePackage(quint8 hostId, quint8 index,
                                        quint8 paramL, quint8 paramH,
                                        quint32 data);

    /**
     * @brief Execute SMBus command
     */
    EC_HOST_CMD_STATUS smbusCommand(const smbus_cmd& cmd, smbus_cmd& response);

signals:
    /**
     * @brief Emitted when a command completes (for async commands)
     */
    void commandCompleted(quint32 packetId, EC_HOST_CMD_STATUS status);

    /**
     * @brief Emitted when data is transmitted
     */
    void dataTx(int bytes);

    /**
     * @brief Emitted when data is received
     */
    void dataRx(int bytes);

    /**
     * @brief Emitted on EC communication error
     */
    void communicationError(const QString& error);

private slots:
    void onCommandDone(QSharedPointer<EmiCmd> pCmd);
    void onTxOut(int bytes);
    void onRxIn(int bytes);

private:
    void log(const QString& message, int level = 0);
    quint32 nextPacketId();

    Logger* m_logger;
    EmiThread* m_thread;
    PortIo* m_portIo;

    bool m_initialized;
    quint16 m_emiOffset;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;

    // For synchronous command tracking
    QMap<quint32, QSharedPointer<EmiCmd>> m_pendingCommands;
    QMap<quint32, CommandCallback> m_asyncCallbacks;

    quint32 m_packetIdCounter;

    // Statistics
    qint64 m_totalBytesTx;
    qint64 m_totalBytesRx;
    quint32 m_commandCount;
    quint32 m_errorCount;
};

#endif // ECMANAGER_H
