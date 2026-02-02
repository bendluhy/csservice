#ifndef PORTIO_H
#define PORTIO_H

#include <QObject>

#define PORTIO_PATH_EXT     "Deploy/inpoutx64.dll"

class PortIo
{
public:
    static PortIo* instance()
    {
        //This is doen this way so that it can call the register after other code has come up
        static PortIo inst;
        static bool init = false;

        if (!init)
        {
            init = true;
            inst.Init();
        }
        return &inst;
    }
    void Init();
    int IsLoaded();
    int Load();
    void UnLoad();
    int Write(quint16 port, quint8 byte);
    int Write(quint16 port, QByteArray& ar);
    int Read(quint16 port, quint8* pByte);
    int Read(quint16 port, QByteArray& ar);
private:
    bool m_bLoaded = false;
    PortIo();
};

#endif // PORTIO_H
