#include <QLibrary>
#include <QLoggingCategory>
#include "appstd.h"
#include "portio.h"
#include "../appresource.h"
typedef void	(__stdcall *lpDlPortWritePortUchar)(quint16, quint8);
typedef quint8	(__stdcall *lpDlPortReadPortUchar)(quint16);
typedef bool	(__stdcall *lpIsInpOutDriverOpen)(void);
typedef bool	(__stdcall *lpIsXP64Bit)(void);

static lpDlPortWritePortUchar gDlPortWritePortUchar = NULL;
static lpDlPortReadPortUchar gDlPortReadPortUchar = NULL;
static lpIsInpOutDriverOpen gfpIsInpOutDriverOpen = NULL;
static lpIsXP64Bit gfpIsXP64Bit = NULL;


PortIo::PortIo()
{
    Load();
}

void PortIo::Init()
{
    //Register the debug class
}

int PortIo::IsLoaded()
{
    if (!m_bLoaded) return 0;
    return 1;
}

int PortIo::Load()
{
    QString path = AppResource::getInstance()->getInstallFolder();;
    path += PORTIO_PATH_EXT;

    QLibrary myLib(path);
    if (myLib.load())
    {

        gDlPortWritePortUchar = reinterpret_cast<lpDlPortWritePortUchar>(myLib.resolve("DlPortWritePortUchar"));
        gDlPortReadPortUchar = reinterpret_cast<lpDlPortReadPortUchar>(myLib.resolve("DlPortReadPortUchar"));
        gfpIsInpOutDriverOpen = reinterpret_cast<lpIsInpOutDriverOpen>(myLib.resolve("IsInpOutDriverOpen"));
        gfpIsXP64Bit = reinterpret_cast<lpIsXP64Bit>(myLib.resolve("IsXP64Bit"));

        if (gDlPortWritePortUchar && gDlPortReadPortUchar) m_bLoaded = true;
        return 0;
    }
    else
    {
        return -1;
    }
}

void PortIo::UnLoad()
{
    m_bLoaded = false;
}

int PortIo::Write(quint16 port, quint8 byte)
{
    if (!m_bLoaded) return -1;
    gDlPortWritePortUchar(port,byte);

    return 0;
}

int PortIo::Write(quint16 port, QByteArray &ar)
{
    if (!m_bLoaded) return -1;
    if (ar.size() == 0) return -1;
    for (int i=0;i <ar.size();i++)
    {
        gDlPortWritePortUchar(port,ar[i]);
        port++;
    }
    return 0;
}

int PortIo::Read(quint16 port, quint8* pByte)
{
    if (!m_bLoaded || pByte == NULL) return -1;
    *pByte = gDlPortReadPortUchar(port);
    return 0;
}

int PortIo::Read(quint16 port, QByteArray &ar)
{
    if (!m_bLoaded) return -1;
    if (ar.size() == 0) return -1;
    for (int i=0;i <ar.size();i++)
    {
        ar[i] = gDlPortReadPortUchar(port);
        port++;
    }
    return 0;
}

