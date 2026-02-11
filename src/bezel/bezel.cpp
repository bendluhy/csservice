#include "bezel.h"
#include "commandproc.h"
#include <QDebug>

// Button bit → event ID mapping
// Button state register is a bitmask: bit0=But1, bit1=But2, ... bit5=But6
// We detect rising edges (0→1 transitions) as button presses.
const quint32 BezelMonitor::s_buttonEventMap[6] = {
    ECEVENT_BUT1_DN,    // bit 0
    ECEVENT_BUT2_DN,    // bit 1
    ECEVENT_BUT3_DN,    // bit 2
    ECEVENT_BUT4_DN,    // bit 3
    ECEVENT_BUT5_DN,    // bit 4
    ECEVENT_BUT6_DN,    // bit 5
};

BezelMonitor::BezelMonitor(EcManager* ecManager, CommandProc* commandProc,
                           Logger* logger, QObject* parent)
    : QObject(parent)
    , m_ecManager(ecManager)
    , m_commandProc(commandProc)
    , m_logger(logger)
    , m_pollTimer(new QTimer(this))
    , m_running(false)
    , m_bezelPresent(false)
    , m_deviceId(0xFF)
    , m_firmwareVersion(0)
    , m_lastButtonState(0)
    , m_lastSliderPos(0)
    , m_firstPoll(true)
{
    m_pollTimer->setTimerType(Qt::PreciseTimer);
    connect(m_pollTimer, &QTimer::timeout, this, &BezelMonitor::onPollTimer);
}

BezelMonitor::~BezelMonitor()
{
    stop();
}

void BezelMonitor::start(int pollIntervalMs)
{
    if (m_running) {
        log("Already running");
        return;
    }

    if (!m_ecManager || !m_ecManager->isInitialized()) {
        log("Cannot start - EcManager not initialized", Logger::Error);
        return;
    }

    if (!m_commandProc) {
        log("Cannot start - CommandProc is null", Logger::Error);
        return;
    }

    // Read initial bezel device info
    QByteArray devData;
    if (m_ecManager->acpi0Read(ACPI_REG_BEZ_DEV, 1, devData) == EC_HOST_CMD_SUCCESS
        && devData.size() >= 1)
    {
        m_deviceId = static_cast<quint8>(devData.at(0));
    }

    QByteArray verData;
    if (m_ecManager->acpi0Read(ACPI_REG_BEZ_VER, 1, verData) == EC_HOST_CMD_SUCCESS
        && verData.size() >= 1)
    {
        m_firmwareVersion = static_cast<quint8>(verData.at(0));
    }

    m_bezelPresent = (m_deviceId != 0xFF && m_deviceId != 0x00);

    if (m_bezelPresent) {
        log(QString("Bezel detected: deviceId=%1, firmware=0x%2")
                .arg(m_deviceId)
                .arg(m_firmwareVersion, 2, 16, QChar('0')));
    } else {
        log(QString("Bezel not detected (deviceId=0x%1), will keep polling")
                .arg(m_deviceId, 2, 16, QChar('0')), Logger::Warning);
    }

    m_firstPoll = true;
    m_running = true;
    m_pollTimer->start(pollIntervalMs);

    log(QString("Started, polling every %1ms").arg(pollIntervalMs));
}

void BezelMonitor::stop()
{
    if (!m_running) return;

    m_pollTimer->stop();
    m_running = false;
    log("Stopped");
}

// ============================================================================
// Poll Timer
// ============================================================================

void BezelMonitor::onPollTimer()
{
    if (!m_ecManager || !m_ecManager->isInitialized()) {
        return;
    }

    // --- Read button state ---
    QByteArray btnData;
    EC_HOST_CMD_STATUS status = m_ecManager->acpi0Read(ACPI_REG_BUT_POS, 1, btnData);

    if (status != EC_HOST_CMD_SUCCESS || btnData.size() < 1) {
        // Don't spam logs - only log every 100th failure
        static int failCount = 0;
        if (++failCount % 100 == 1) {
            log(QString("Failed to read button state (status=%1, fails=%2)")
                    .arg(status).arg(failCount), Logger::Warning);
        }
        return;
    }

    quint8 buttonState = static_cast<quint8>(btnData.at(0));

    // --- Read slider ---
    QByteArray sliderData;
    quint8 sliderPos = m_lastSliderPos;
    if (m_ecManager->acpi0Read(ACPI_REG_SLIDER_POS, 1, sliderData) == EC_HOST_CMD_SUCCESS
        && sliderData.size() >= 1)
    {
        sliderPos = static_cast<quint8>(sliderData.at(0));
    }

    // --- Periodic bezel presence check (every ~5s at 50ms poll) ---
    static int presenceCounter = 0;
    if (++presenceCounter >= 100) {
        presenceCounter = 0;

        QByteArray devData;
        if (m_ecManager->acpi0Read(ACPI_REG_BEZ_DEV, 1, devData) == EC_HOST_CMD_SUCCESS
            && devData.size() >= 1)
        {
            quint8 newDevId = static_cast<quint8>(devData.at(0));
            bool newPresent = (newDevId != 0xFF && newDevId != 0x00);

            if (newPresent != m_bezelPresent) {
                m_bezelPresent = newPresent;
                m_deviceId = newDevId;
                log(QString("Bezel %1 (deviceId=0x%2)")
                        .arg(m_bezelPresent ? "connected" : "disconnected")
                        .arg(m_deviceId, 2, 16, QChar('0')));
                emit bezelPresenceChanged(m_bezelPresent);
            }
        }
    }

    // --- First poll: just capture baseline, don't fire events ---
    if (m_firstPoll) {
        m_lastButtonState = buttonState;
        m_lastSliderPos = sliderPos;
        m_firstPoll = false;
        return;
    }

    // --- Detect and process changes ---
    if (buttonState != m_lastButtonState) {
        processButtonState(buttonState);
    }

    if (sliderPos != m_lastSliderPos) {
        processSliderState(sliderPos);
    }
}

// ============================================================================
// Change Detection
// ============================================================================

void BezelMonitor::processButtonState(quint8 newState)
{
    quint8 oldState = m_lastButtonState;
    m_lastButtonState = newState;

    // Rising edges = button presses (bits that went 0→1)
    quint8 pressed = (newState & ~oldState);

    for (int i = 0; i < 6; i++) {
        if (pressed & (1 << i)) {
            quint32 eventId = s_buttonEventMap[i];

            log(QString("Button %1 pressed → event 0x%2 (state 0x%3→0x%4)")
                    .arg(i + 1)
                    .arg(eventId, 4, 16, QChar('0'))
                    .arg(oldState, 2, 16, QChar('0'))
                    .arg(newState, 2, 16, QChar('0')));

            // Push into CommandProc's existing action queue
            // This is the same queue that handlePollActionCommands() drains
            m_commandProc->triggerActionEvent(eventId);

            emit buttonPressed(i + 1, eventId);
        }
    }
}

void BezelMonitor::processSliderState(quint8 newPos)
{
    quint8 oldPos = m_lastSliderPos;
    m_lastSliderPos = newPos;

    log(QString("Slider changed: %1 → %2").arg(oldPos).arg(newPos), Logger::Debug);

    m_commandProc->triggerActionEvent(ECEVENT_SLIDER_CHG);

    emit sliderChanged(newPos);
}

// ============================================================================
// Logging
// ============================================================================

void BezelMonitor::log(const QString& message, Logger::LogLevel level)
{
    if (m_logger) {
        m_logger->log(QString("BezelMonitor: %1").arg(message), level);
    }
}
