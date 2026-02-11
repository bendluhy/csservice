#ifndef ACTIONCOMMANDQUEUE_H
#define ACTIONCOMMANDQUEUE_H

#include <QObject>
#include <QMutex>
#include <QQueue>
#include "command.qpb.h"
#include <QWaitCondition>

class ActionCommandQueue : public QObject
{
    Q_OBJECT

public:
    explicit ActionCommandQueue(QObject* parent = nullptr);

    // Queue a command (from Control Screens)
    uint32_t queueCommand(const patrol::ActionCommand& cmd);

    // Get pending commands (Monitor polls this)
    QList<patrol::ActionCommand> takePending();

    // Check if any pending
    bool hasPending() const;

    // Trigger event directly (from ACPI)
    uint32_t triggerEvent(uint32_t eventId);
    QMap<uint32_t, patrol::ActionCommandResultRequest> m_results;
    QWaitCondition m_resultWait;

    // Add methods:
    void storeResult(uint32_t commandId, const patrol::ActionCommandResultRequest& result);
    bool waitForResult(uint32_t commandId, patrol::ActionCommandResultRequest& outResult, int timeoutMs);

private:
    mutable QMutex m_mutex;
    QQueue<patrol::ActionCommand> m_queue;
    uint32_t m_nextCommandId = 1;
};

#endif
