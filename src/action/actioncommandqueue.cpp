#include "ActionCommandQueue.h"

ActionCommandQueue::ActionCommandQueue(QObject* parent)
    : QObject(parent)
{
}

uint32_t ActionCommandQueue::queueCommand(const patrol::ActionCommand& cmd)
{
    QMutexLocker lock(&m_mutex);

    patrol::ActionCommand cmdCopy = cmd;
    uint32_t id = m_nextCommandId++;
    cmdCopy.setCommandId(id);

    m_queue.enqueue(cmdCopy);
    return id;
}

QList<patrol::ActionCommand> ActionCommandQueue::takePending()
{
    QMutexLocker lock(&m_mutex);

    QList<patrol::ActionCommand> result;
    while (!m_queue.isEmpty()) {
        result.append(m_queue.dequeue());
    }
    return result;
}

bool ActionCommandQueue::hasPending() const
{
    QMutexLocker lock(&m_mutex);
    return !m_queue.isEmpty();
}

uint32_t ActionCommandQueue::triggerEvent(uint32_t eventId)
{
    patrol::ActionCommand cmd;
    cmd.setType(patrol::ActionCommand::Type::TRIGGER_EVENT);
    cmd.setEventId(eventId);
    return queueCommand(cmd);
}
void ActionCommandQueue::storeResult(uint32_t commandId, const patrol::ActionCommandResultRequest& result)
{
    QMutexLocker lock(&m_mutex);
    m_results[commandId] = result;
    m_resultWait.wakeAll();
}

bool ActionCommandQueue::waitForResult(uint32_t commandId, patrol::ActionCommandResultRequest& outResult, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);

    QElapsedTimer timer;
    timer.start();

    while (!m_results.contains(commandId) && timer.elapsed() < timeoutMs) {
        m_resultWait.wait(&m_mutex, 100);
    }

    if (m_results.contains(commandId)) {
        outResult = m_results.take(commandId);
        return true;
    }
    return false;
}
