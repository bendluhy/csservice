#include "ECMemoryMirror.h"
#include <sddl.h>
#include <chrono>

// ============================================================================
// ECMemoryWriter (Service-side)
// ============================================================================

ECMemoryWriter::ECMemoryWriter(Logger* logger, QObject* parent)
    : QObject(parent), m_logger(logger)
{
}

ECMemoryWriter::~ECMemoryWriter()
{
    close();
}

bool ECMemoryWriter::createSecurityDescriptor(SECURITY_ATTRIBUTES& sa)
{
    // Allow Authenticated Users to read, Administrators full access
    const wchar_t* sddl = L"D:(A;OICI;GRGW;;;AU)(A;OICI;GA;;;BA)";

    PSECURITY_DESCRIPTOR pSD = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &pSD, nullptr))
    {
        m_logger->log(QString("Failed to create security descriptor: %1").arg(GetLastError()));
        return false;
    }

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pSD;
    sa.bInheritHandle = FALSE;

    return true;
}

bool ECMemoryWriter::create()
{
    SECURITY_ATTRIBUTES sa = {0};
    if (!createSecurityDescriptor(sa)) {
        return false;
    }

    // Create shared memory
    m_memoryHandle = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        &sa,
        PAGE_READWRITE,
        0,
        sizeof(ECMemoryData),
        EC_MEMORY_NAME);

    if (sa.lpSecurityDescriptor) {
        LocalFree(sa.lpSecurityDescriptor);
    }

    if (!m_memoryHandle) {
        m_logger->log(QString("Failed to create EC memory: %1").arg(GetLastError()));
        return false;
    }

    // Map the memory
    m_pData = static_cast<ECMemoryData*>(
        MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ECMemoryData)));

    if (!m_pData) {
        m_logger->log(QString("Failed to map EC memory: %1").arg(GetLastError()));
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        return false;
    }

    // Initialize memory
    ZeroMemory(m_pData, sizeof(ECMemoryData));
    m_pData->version = 0;

    // Create mutex
    if (!createSecurityDescriptor(sa)) {
        UnmapViewOfFile(m_pData);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        m_pData = nullptr;
        return false;
    }

    m_mutex = CreateMutexW(&sa, FALSE, EC_MUTEX_NAME);

    if (sa.lpSecurityDescriptor) {
        LocalFree(sa.lpSecurityDescriptor);
    }

    if (!m_mutex) {
        m_logger->log(QString("Failed to create EC mutex: %1").arg(GetLastError()));
        UnmapViewOfFile(m_pData);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        m_pData = nullptr;
        return false;
    }

    m_logger->log("EC Memory Writer created successfully");
    return true;
}

bool ECMemoryWriter::updateMemory(const QByteArray& newData)
{
    if (!m_pData || !m_mutex) {
        m_logger->log("EC Memory not initialized");
        return false;
    }

    if (newData.size() > sizeof(m_pData->data)) {
        m_logger->log(QString("Data too large: %1 bytes (max %2)")
                          .arg(newData.size()).arg(sizeof(m_pData->data)));
        return false;
    }

    // Acquire mutex with timeout
    DWORD result = WaitForSingleObject(m_mutex, 100);
    if (result != WAIT_OBJECT_0) {
        m_logger->log("Failed to acquire EC mutex for writing");
        return false;
    }

    // Update the memory
    m_pData->version++;
    m_pData->timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    m_pData->dataSize = static_cast<uint16_t>(newData.size());
    memcpy(m_pData->data, newData.constData(), newData.size());

    ReleaseMutex(m_mutex);

    m_logger->log(QString("Updated EC memory: version %1, size %2 bytes")
                      .arg(m_pData->version).arg(newData.size()));
    return true;
}

void ECMemoryWriter::close()
{
    if (m_mutex) {
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }

    if (m_pData) {
        UnmapViewOfFile(m_pData);
        m_pData = nullptr;
    }

    if (m_memoryHandle) {
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
    }

    m_logger->log("EC Memory Writer closed");
}

// ============================================================================
// ECMemoryReader (Client-side)
// ============================================================================

ECMemoryReader::ECMemoryReader(Logger* logger, QObject* parent)
    : QObject(parent), m_logger(logger)
{
}

ECMemoryReader::~ECMemoryReader()
{
    close();
}

bool ECMemoryReader::open()
{
    // Open existing shared memory (read-only)
    m_memoryHandle = OpenFileMappingW(FILE_MAP_READ, FALSE, EC_MEMORY_NAME);
    if (!m_memoryHandle) {
        m_logger->log(QString("Failed to open EC memory: %1").arg(GetLastError()));
        return false;
    }

    // Map with read-only access
    m_pData = static_cast<ECMemoryData*>(
        MapViewOfFile(m_memoryHandle, FILE_MAP_READ, 0, 0, sizeof(ECMemoryData)));

    if (!m_pData) {
        m_logger->log(QString("Failed to map EC memory: %1").arg(GetLastError()));
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        return false;
    }

    // Open mutex
    m_mutex = OpenMutexW(SYNCHRONIZE, FALSE, EC_MUTEX_NAME);
    if (!m_mutex) {
        m_logger->log(QString("Failed to open EC mutex: %1").arg(GetLastError()));
        UnmapViewOfFile(m_pData);
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        m_pData = nullptr;
        return false;
    }

    m_logger->log("EC Memory Reader opened successfully");
    return true;
}

QByteArray ECMemoryReader::readMemory(bool* success)
{
    if (success) *success = false;

    if (!m_pData || !m_mutex) {
        m_logger->log("EC Memory not initialized");
        return QByteArray();
    }

    // Local copy for atomic read
    ECMemoryData localCopy;
    uint32_t v1, v2;

    // Read with version checking to ensure consistency
    int retries = 0;
    do {
        // Acquire mutex with timeout
        DWORD result = WaitForSingleObject(m_mutex, 100);
        if (result != WAIT_OBJECT_0) {
            m_logger->log("Failed to acquire EC mutex for reading");
            return QByteArray();
        }

        // Read version, data, then version again
        v1 = m_pData->version;
        memcpy(&localCopy, m_pData, sizeof(ECMemoryData));
        v2 = m_pData->version;

        ReleaseMutex(m_mutex);

        retries++;
        if (retries > 3) {
            m_logger->log("Too many retries reading EC memory");
            return QByteArray();
        }

    } while (v1 != v2); // Retry if version changed during read

    if (success) *success = true;

    // Return the data as QByteArray
    return QByteArray(reinterpret_cast<char*>(localCopy.data), localCopy.dataSize);
}

uint32_t ECMemoryReader::getVersion()
{
    if (!m_pData) return 0;

    DWORD result = WaitForSingleObject(m_mutex, 100);
    if (result != WAIT_OBJECT_0) return 0;

    uint32_t version = m_pData->version;
    ReleaseMutex(m_mutex);

    return version;
}

void ECMemoryReader::close()
{
    if (m_mutex) {
        CloseHandle(m_mutex);
        m_mutex = nullptr;
    }

    if (m_pData) {
        UnmapViewOfFile(m_pData);
        m_pData = nullptr;
    }

    if (m_memoryHandle) {
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
    }

    m_logger->log("EC Memory Reader closed");
}
