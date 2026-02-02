#ifndef MONITOR_H
#define MONITOR_H

#include <QObject>
#include <QTimer>
#include "Logger.h"

#define MONITOR_TIME    1000

class Monitor : public QObject
{
    Q_OBJECT
public:
    explicit Monitor(Logger* logger, QObject *parent = nullptr);

    void shutdown();
    void settingsChanged(); // Call when settings are updated

private slots:
    void onTimeout(); // Called periodically

private:
    Logger* m_logger;
    QTimer m_timer;
};

#endif // MONITOR_H
