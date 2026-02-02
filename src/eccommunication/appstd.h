#ifdef INCLUDE_COMMON_HEADER

#define BYTE0(x) ((quint8) (x & 0xFF))
#define BYTE1(x) ((quint8) ((x >> 8) & 0xFF))
#define BYTE2(x) ((quint8) ((x >> 16) & 0xFF))
#define BYTE3(x) ((quint8) ((x >> 24) & 0xFF))
/*
#ifndef MAKEWORD
#define MAKEWORD(l,h)   (l + ((h << 8) & 0xFF00))
//#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#endif
*/
#define DISABLE_HW_ACCESS   0
#define SHOW_POLE_HW_ERR    0
#define SIMULATE_HARDWARE   0
#define REDIRECT_DEBUG      1   //Redirects the debugging to the debug output window
#ifdef qDebug
#undef qDebug
#endif
#define qDebug qInfo            //Get Full Debug in release build
#endif // APPSTD_H
