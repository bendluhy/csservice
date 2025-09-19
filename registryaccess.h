#ifndef REGISTRYACCESS_H
#define REGISTRYACCESS_H

#include <windows.h>
#include <QString>
#include <QVariant>
#include <QByteArray>
#include "logger.h"

class RegistryAccess
{
public:
    explicit RegistryAccess(Logger* logger);
    ~RegistryAccess();

    // Main interface methods
    bool readValue(const QString &keyPath, const QString &valueName, QVariant &value, DWORD valueType);
    bool writeValue(const QString &keyPath, const QString &valueName, const QVariant &value, DWORD valueType);
    bool deleteValue(const QString &keyPath, const QString &valueName);
    bool deleteKey(const QString &keyPath);

    // Convenience method that handles both
    bool del(const QString &keyPath, const QString &valueName = QString());

    // Check if a key exists
    bool keyExists(const QString &keyPath);
    bool valueExists(const QString &keyPath, const QString &valueName);

    // Set which root key to use (default is HKEY_LOCAL_MACHINE)
    void setRootKey(HKEY rootKey) { m_rootKey = rootKey; }
    HKEY getRootKey() const { return m_rootKey; }

private:
    // Internal helper methods
    HKEY openKey(const QString &keyPath, REGSAM accessRights, bool createIfMissing = false);
    void closeKey(HKEY key);

    // Type-specific read methods
    bool readStringValue(HKEY key, const QString &valueName, QString &value);
    bool readDwordValue(HKEY key, const QString &valueName, DWORD &value);
    bool readQwordValue(HKEY key, const QString &valueName, ULONGLONG &value);
    bool readBinaryValue(HKEY key, const QString &valueName, QByteArray &value);

    // Type-specific write methods
    bool writeStringValue(HKEY key, const QString &valueName, const QString &value, DWORD type);
    bool writeDwordValue(HKEY key, const QString &valueName, DWORD value);
    bool writeQwordValue(HKEY key, const QString &valueName, ULONGLONG value);
    bool writeBinaryValue(HKEY key, const QString &valueName, const QByteArray &value);

    // Helper to convert Windows error codes to strings
    QString getErrorString(LONG errorCode);

    Logger* m_pLogger;
    HKEY m_rootKey;
};

#endif // REGISTRYACCESS_H
