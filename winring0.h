#ifndef WINRING0_H
#define WINRING0_H

#include <windows.h>
#include <QString>
#include <QMutex>
#include "Logger.h"

#define WINRING_PATH "Sys/Drivers/WinRing0x64.dll"

// WinRing0 function type definitions
typedef BOOL (WINAPI *InitializeOls)();
typedef VOID (WINAPI *DeinitializeOls)();
typedef BOOL (WINAPI *Rdmsr)(DWORD, PDWORD, PDWORD);
typedef BOOL (WINAPI *Wrmsr)(DWORD, DWORD, DWORD);
typedef BYTE (WINAPI *ReadPciConfigByte)(DWORD, DWORD);
typedef WORD (WINAPI *ReadPciConfigWord)(DWORD, DWORD);
typedef DWORD (WINAPI *ReadPciConfigDword)(DWORD, DWORD);
typedef BOOL (WINAPI *WritePciConfigByte)(DWORD, DWORD, BYTE);
typedef BOOL (WINAPI *WritePciConfigWord)(DWORD, DWORD, WORD);
typedef BOOL (WINAPI *WritePciConfigDword)(DWORD, DWORD, DWORD);
typedef BYTE (WINAPI *ReadIoPortByte)(WORD);
typedef WORD (WINAPI *ReadIoPortWord)(WORD);
typedef DWORD (WINAPI *ReadIoPortDword)(WORD);
typedef BOOL (WINAPI *WriteIoPortByte)(WORD, BYTE);
typedef BOOL (WINAPI *WriteIoPortWord)(WORD, WORD);
typedef BOOL (WINAPI *WriteIoPortDword)(WORD, DWORD);

class WinRing0 : public QObject
{
    Q_OBJECT

public:
    explicit WinRing0(Logger *logger, QObject *parent = nullptr);
    ~WinRing0();

    bool load(const QString& customPath = QString());
    void unload();
    bool isLoaded() const { return m_isLoaded; }

    // MSR operations
    bool readMsr(DWORD msrAddress, DWORD &low, DWORD &high);
    bool writeMsr(DWORD msrAddress, DWORD low, DWORD high);

    // PCI Configuration operations
    bool readPciConfigByte(DWORD pciAddress, DWORD regAddress, BYTE &value);
    bool readPciConfigWord(DWORD pciAddress, DWORD regAddress, WORD &value);
    bool readPciConfigDword(DWORD pciAddress, DWORD regAddress, DWORD &value);
    bool writePciConfigByte(DWORD pciAddress, DWORD regAddress, BYTE value);
    bool writePciConfigWord(DWORD pciAddress, DWORD regAddress, WORD value);
    bool writePciConfigDword(DWORD pciAddress, DWORD regAddress, DWORD value);

    // IO Port operations
    bool readIoPortByte(WORD port, BYTE &value);
    bool readIoPortWord(WORD port, WORD &value);
    bool readIoPortDword(WORD port, DWORD &value);
    bool writeIoPortByte(WORD port, BYTE value);
    bool writeIoPortWord(WORD port, WORD value);
    bool writeIoPortDword(WORD port, DWORD value);

private:
    bool loadFunctions();
    void clearFunctions();

    HMODULE m_hModule;
    Logger* m_pLogger;
    QMutex m_mutex;
    bool m_isLoaded;

    // Function pointers
    InitializeOls _InitializeOls;
    DeinitializeOls _DeinitializeOls;
    Rdmsr _Rdmsr;
    Wrmsr _Wrmsr;
    ReadPciConfigByte _ReadPciConfigByte;
    ReadPciConfigWord _ReadPciConfigWord;
    ReadPciConfigDword _ReadPciConfigDword;
    WritePciConfigByte _WritePciConfigByte;
    WritePciConfigWord _WritePciConfigWord;
    WritePciConfigDword _WritePciConfigDword;
    ReadIoPortByte _ReadIoPortByte;
    ReadIoPortWord _ReadIoPortWord;
    ReadIoPortDword _ReadIoPortDword;
    WriteIoPortByte _WriteIoPortByte;
    WriteIoPortWord _WriteIoPortWord;
    WriteIoPortDword _WriteIoPortDword;
};

#endif // WINRING0_H
