#include "WinRing0.h"
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>

WinRing0::WinRing0(Logger* pLogger, QObject* parent)
    : QObject(parent)
    , m_hModule(NULL)
    , m_pLogger(pLogger)
    , m_isLoaded(false)
{
    clearFunctions();
}

WinRing0::~WinRing0()
{
    unload();
}

bool WinRing0::load(const QString& customPath)
{
    QMutexLocker locker(&m_mutex);

    if (m_isLoaded) {
        if (m_pLogger) {
            m_pLogger->log("WinRing0: Already loaded", Logger::Warning);
        }
        return true;
    }

    // Determine DLL path
    QString dllPath;
    if (!customPath.isEmpty()) {
        dllPath = customPath;
    } else {
        // Use default path - WinRing0x64.dll in current directory or system path
        dllPath = "WinRing0x64.dll";
    }

    // Log the attempt
    if (m_pLogger) {
        m_pLogger->log(QString("WinRing0: Loading DLL from: %1").arg(dllPath), Logger::Info);
    }

    // Load the DLL
    m_hModule = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!m_hModule) {
        DWORD error = GetLastError();
        if (m_pLogger) {
            m_pLogger->log(QString("WinRing0: Failed to load DLL (Error: %1)").arg(error), Logger::Error);
        }
        return false;
    }

    // Load all function pointers
    if (!loadFunctions()) {
        if (m_pLogger) {
            m_pLogger->log("WinRing0: Failed to load function pointers", Logger::Error);
        }
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        clearFunctions();
        return false;
    }

    // Initialize OLS
    if (!_InitializeOls || !_InitializeOls()) {
        DWORD error = GetLastError();
        if (m_pLogger) {
            m_pLogger->log(QString("WinRing0: Failed to initialize OLS (Error: %1)").arg(error), Logger::Error);
        }
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        clearFunctions();
        return false;
    }

    m_isLoaded = true;
    if (m_pLogger) {
        m_pLogger->log("WinRing0: Successfully loaded and initialized", Logger::Info);
    }

    return true;
}

void WinRing0::unload()
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded) {
        return;
    }

    // Deinitialize OLS
    if (_DeinitializeOls) {
        _DeinitializeOls();
    }

    // Free the library
    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }

    clearFunctions();
    m_isLoaded = false;

    if (m_pLogger) {
        m_pLogger->log("WinRing0: Unloaded", Logger::Info);
    }
}

bool WinRing0::loadFunctions()
{
    _InitializeOls = reinterpret_cast<InitializeOls>(GetProcAddress(m_hModule, "InitializeOls"));
    _DeinitializeOls = reinterpret_cast<DeinitializeOls>(GetProcAddress(m_hModule, "DeinitializeOls"));
    _Rdmsr = reinterpret_cast<Rdmsr>(GetProcAddress(m_hModule, "Rdmsr"));
    _Wrmsr = reinterpret_cast<Wrmsr>(GetProcAddress(m_hModule, "Wrmsr"));
    _ReadPciConfigByte = reinterpret_cast<ReadPciConfigByte>(GetProcAddress(m_hModule, "ReadPciConfigByte"));
    _ReadPciConfigWord = reinterpret_cast<ReadPciConfigWord>(GetProcAddress(m_hModule, "ReadPciConfigWord"));
    _ReadPciConfigDword = reinterpret_cast<ReadPciConfigDword>(GetProcAddress(m_hModule, "ReadPciConfigDword"));
    _WritePciConfigByte = reinterpret_cast<WritePciConfigByte>(GetProcAddress(m_hModule, "WritePciConfigByte"));
    _WritePciConfigWord = reinterpret_cast<WritePciConfigWord>(GetProcAddress(m_hModule, "WritePciConfigWord"));
    _WritePciConfigDword = reinterpret_cast<WritePciConfigDword>(GetProcAddress(m_hModule, "WritePciConfigDword"));
    _ReadIoPortByte = reinterpret_cast<ReadIoPortByte>(GetProcAddress(m_hModule, "ReadIoPortByte"));
    _ReadIoPortWord = reinterpret_cast<ReadIoPortWord>(GetProcAddress(m_hModule, "ReadIoPortWord"));
    _ReadIoPortDword = reinterpret_cast<ReadIoPortDword>(GetProcAddress(m_hModule, "ReadIoPortDword"));
    _WriteIoPortByte = reinterpret_cast<WriteIoPortByte>(GetProcAddress(m_hModule, "WriteIoPortByte"));
    _WriteIoPortWord = reinterpret_cast<WriteIoPortWord>(GetProcAddress(m_hModule, "WriteIoPortWord"));
    _WriteIoPortDword = reinterpret_cast<WriteIoPortDword>(GetProcAddress(m_hModule, "WriteIoPortDword"));

    // Verify all critical functions loaded
    if (!_InitializeOls || !_DeinitializeOls || !_Rdmsr || !_Wrmsr) {
        return false;
    }

    return true;
}

void WinRing0::clearFunctions()
{
    _InitializeOls = NULL;
    _DeinitializeOls = NULL;
    _Rdmsr = NULL;
    _Wrmsr = NULL;
    _ReadPciConfigByte = NULL;
    _ReadPciConfigWord = NULL;
    _ReadPciConfigDword = NULL;
    _WritePciConfigByte = NULL;
    _WritePciConfigWord = NULL;
    _WritePciConfigDword = NULL;
    _ReadIoPortByte = NULL;
    _ReadIoPortWord = NULL;
    _ReadIoPortDword = NULL;
    _WriteIoPortByte = NULL;
    _WriteIoPortWord = NULL;
    _WriteIoPortDword = NULL;
}

// MSR Operations
bool WinRing0::readMsr(DWORD msrAddress, DWORD &low, DWORD &high)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_Rdmsr) {
        if (m_pLogger) {
            m_pLogger->log("WinRing0: readMsr called but not loaded", Logger::Error);
        }
        return false;
    }

    bool result = _Rdmsr(msrAddress, &low, &high);

    if (m_pLogger) {
        if (result) {
            m_pLogger->log(QString("WinRing0: Read MSR 0x%1: High=0x%2 Low=0x%3")
                               .arg(msrAddress, 0, 16).arg(high, 0, 16).arg(low, 0, 16), Logger::Debug);
        } else {
            m_pLogger->log(QString("WinRing0: Failed to read MSR 0x%1").arg(msrAddress, 0, 16), Logger::Error);
        }
    }

    return result;
}

bool WinRing0::writeMsr(DWORD msrAddress, DWORD low, DWORD high)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_Wrmsr) {
        if (m_pLogger) {
            m_pLogger->log("WinRing0: writeMsr called but not loaded", Logger::Error);
        }
        return false;
    }

    bool result = _Wrmsr(msrAddress, low, high);

    if (m_pLogger) {
        if (result) {
            m_pLogger->log(QString("WinRing0: Wrote MSR 0x%1: High=0x%2 Low=0x%3")
                               .arg(msrAddress, 0, 16).arg(high, 0, 16).arg(low, 0, 16), Logger::Debug);
        } else {
            m_pLogger->log(QString("WinRing0: Failed to write MSR 0x%1").arg(msrAddress, 0, 16), Logger::Error);
        }
    }

    return result;
}

// PCI Configuration Read Operations
bool WinRing0::readPciConfigByte(DWORD pciAddress, DWORD regAddress, BYTE &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadPciConfigByte) {
        return false;
    }

    value = _ReadPciConfigByte(pciAddress, regAddress);
    return true;
}

bool WinRing0::readPciConfigWord(DWORD pciAddress, DWORD regAddress, WORD &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadPciConfigWord) {
        return false;
    }

    value = _ReadPciConfigWord(pciAddress, regAddress);
    return true;
}

bool WinRing0::readPciConfigDword(DWORD pciAddress, DWORD regAddress, DWORD &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadPciConfigDword) {
        return false;
    }

    value = _ReadPciConfigDword(pciAddress, regAddress);
    return true;
}

// PCI Configuration Write Operations
bool WinRing0::writePciConfigByte(DWORD pciAddress, DWORD regAddress, BYTE value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WritePciConfigByte) {
        return false;
    }

    return _WritePciConfigByte(pciAddress, regAddress, value);
}

bool WinRing0::writePciConfigWord(DWORD pciAddress, DWORD regAddress, WORD value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WritePciConfigWord) {
        return false;
    }

    return _WritePciConfigWord(pciAddress, regAddress, value);
}

bool WinRing0::writePciConfigDword(DWORD pciAddress, DWORD regAddress, DWORD value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WritePciConfigDword) {
        return false;
    }

    return _WritePciConfigDword(pciAddress, regAddress, value);
}

// IO Port Read Operations
bool WinRing0::readIoPortByte(WORD port, BYTE &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadIoPortByte) {
        return false;
    }

    value = _ReadIoPortByte(port);
    return true;
}

bool WinRing0::readIoPortWord(WORD port, WORD &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadIoPortWord) {
        return false;
    }

    value = _ReadIoPortWord(port);
    return true;
}

bool WinRing0::readIoPortDword(WORD port, DWORD &value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_ReadIoPortDword) {
        return false;
    }

    value = _ReadIoPortDword(port);
    return true;
}

// IO Port Write Operations
bool WinRing0::writeIoPortByte(WORD port, BYTE value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WriteIoPortByte) {
        return false;
    }

    return _WriteIoPortByte(port, value);
}

bool WinRing0::writeIoPortWord(WORD port, WORD value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WriteIoPortWord) {
        return false;
    }

    return _WriteIoPortWord(port, value);
}

bool WinRing0::writeIoPortDword(WORD port, DWORD value)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isLoaded || !_WriteIoPortDword) {
        return false;
    }

    return _WriteIoPortDword(port, value);
}
