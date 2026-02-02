#include "Monitor.h"

Monitor::Monitor(Logger* logger, QObject *parent)
    : QObject(parent), m_logger(logger)
{
    m_logger->log("Monitor initialized");

    connect(&m_timer, &QTimer::timeout, this, &Monitor::onTimeout);
    m_timer.start(MONITOR_TIME);

    m_logger->log("Monitor timer started");
}

void Monitor::onTimeout()
{
    //m_logger->log("Monitor timeout triggered");
}

void Monitor::settingsChanged()
{
    m_logger->log("Monitor received settingsChanged notification");
}

void Monitor::shutdown()
{
    m_logger->log("Monitor shutting down");
    m_timer.stop();
}
