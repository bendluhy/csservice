#ifndef WMIACCESS_H
#define WMIACCESS_H

#include <windows.h>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QMap>
#include <QMutex>
#include "logger.h"
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

class WmiAccess
{
public:
    explicit WmiAccess(Logger* logger);
    ~WmiAccess();

    bool initialize();
    void deinitialize();
    bool isInitialized() const { return m_isInitialized; }

    // Query WMI - returns all properties if property is empty
    bool query(const QString &namespacePath,
               const QString &query,
               QVector<QMap<QString, QVariant>> &results,
               const QString &property = QString());

    // Execute WMI method
    bool execMethod(const QString &namespacePath,
                    const QString &className,
                    const QString &methodName,
                    const QMap<QString, QVariant> &params,
                    QMap<QString, QVariant> &results);

private:
    Logger* m_pLogger;
    IWbemLocator* m_pLoc;
    IWbemServices* m_pSvc;
    QString m_currentNamespace;
    bool m_isInitialized;
    bool m_comInitialized;
    QMutex m_mutex;

    bool initializeSecurity();
    bool connectServer(const QString &namespacePath);
    void disconnectServer();

    bool variantToQVariant(const VARIANT &variant, QVariant &qVariant);
    bool qVariantToVariant(const QVariant &qVariant, VARIANT &variant);

    QString getComErrorString(HRESULT hr);
};

#endif // WMIACCESS_H
