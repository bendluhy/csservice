#ifndef BEZELMONITOR_H
#define BEZELMONITOR_H

#include <QObject>
#include <QTimer>
#include "ecmanager.h"
#include "logger.h"

// ============================================================================
// ACPI register offsets for bezel state (must match your smcregdef.h)
// TODO: Verify these match your actual EC register map
// ============================================================================
#define ACPI_REG_BEZ_STATE      0x26    // Bezel state flags (uBezState)
#define ACPI_REG_BUT_POS        0x27    // Button state bitmask register
#define ACPI_REG_SLIDER_POS     0x28    // Slider position (0-255)
#define ACPI_REG_BEZ_DEV        0xEF    // Bezel device ID
#define ACPI_REG_BEZ_VER        0xF6    // Bezel firmware version  // Slider position (0-255)

// ============================================================================
// Bezel event IDs - must match ec_events_m3.h so ActionManager maps them
// TODO: Verify these match your actual event IDs
// ============================================================================
#ifndef ECEVENT_BUT1_DN
#define ECEVENT_BUT1_DN         0x00010000
#define ECEVENT_BUT2_DN         0x00010001
#define ECEVENT_BUT3_DN         0x00010002
#define ECEVENT_BUT4_DN         0x00010003
#define ECEVENT_BUT5_DN         0x00010004
#define ECEVENT_BUT6_DN         0x00010005
#define ECEVENT_SLIDER_CHG      0x00020002
#endif

class CommandProc;  // Forward declare - we just call triggerActionEvent()

/**
 * @brief BezelMonitor - Polls EC for bezel button presses and feeds them
 *        into CommandProc's action queue.
 *
 * Flow:
 *   EC ACPI regs → BezelMonitor (polls, detects rising edges)
 *       → CommandProc::triggerActionEvent(eventId)
 *       → m_actionQueue (already exists)
 *       → CSMonitor polls via PollActionCommandsRequest
 *       → ActionPoller → ActionManager::executeEvent()
 *
 * Lives in WindowsService, created after EC is initialized.
 */
class BezelMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BezelMonitor(EcManager* ecManager, CommandProc* commandProc,
                          Logger* logger, QObject* parent = nullptr);
    ~BezelMonitor();

    void start(int pollIntervalMs = 50);
    void stop();
    bool isRunning() const { return m_running; }

    // Debug/status
    quint8 currentButtonState() const { return m_lastButtonState; }
    quint8 currentSliderPos() const { return m_lastSliderPos; }
    quint8 deviceId() const { return m_deviceId; }
    bool isBezelPresent() const { return m_bezelPresent; }

signals:
    void buttonPressed(int buttonIndex, quint32 eventId);
    void sliderChanged(quint8 position);
    void bezelPresenceChanged(bool present);

private slots:
    void onPollTimer();

private:
    void log(const QString& message, Logger::LogLevel level = Logger::Info);
    void processButtonState(quint8 newState);
    void processSliderState(quint8 newPos);

    EcManager*    m_ecManager;
    CommandProc*  m_commandProc;
    Logger*       m_logger;
    QTimer*       m_pollTimer;

    bool    m_running;
    bool    m_bezelPresent;
    quint8  m_deviceId;
    quint8  m_firmwareVersion;
    quint8  m_lastButtonState;
    quint8  m_lastSliderPos;
    bool    m_firstPoll;

    static const quint32 s_buttonEventMap[6];
};

#endif // BEZELMONITOR_H
