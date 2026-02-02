#ifndef ECMEMORYMIRROR_H
#define ECMEMORYMIRROR_H

#include <QObject>
#include <QByteArray>
#include <windows.h>
#include "Logger.h"

#define EC_MEMORY_SIZE 512
#define EC_MEMORY_NAME L"Global\\ECMemoryMirror"
#define EC_MUTEX_NAME L"Global\\ECMemoryMutex"

#pragma pack(push, 1)
struct ECMemoryData {
    uint32_t version;           // Incremented on each update
    uint32_t timestamp;         // Milliseconds since epoch (optional)
    uint16_t dataSize;          // Actual data size (max EC_MEMORY_SIZE - 10)
    uint8_t  data[502];         // Payload data
};
#pragma pack(pop)

// Service-side: Writer
class ECMemoryWriter : public QObject
{
    Q_OBJECT

public:
    explicit ECMemoryWriter(Logger* logger, QObject* parent = nullptr);
    ~ECMemoryWriter();

    bool create();
    bool updateMemory(const QByteArray& newData);
    void close();

private:
    Logger* m_logger;
    HANDLE m_memoryHandle = nullptr;
    HANDLE m_mutex = nullptr;
    ECMemoryData* m_pData = nullptr;

    bool createSecurityDescriptor(SECURITY_ATTRIBUTES& sa);
};

// Client-side: Reader
class ECMemoryReader : public QObject
{
    Q_OBJECT

public:
    explicit ECMemoryReader(Logger* logger, QObject* parent = nullptr);
    ~ECMemoryReader();

    bool open();
    QByteArray readMemory(bool* success = nullptr);
    uint32_t getVersion();
    void close();

private:
    Logger* m_logger;
    HANDLE m_memoryHandle = nullptr;
    HANDLE m_mutex = nullptr;
    ECMemoryData* m_pData = nullptr;
};

#endif // ECMEMORYMIRROR_H
