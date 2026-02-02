#include "RegistryAccess.h"
#include <QDebug>

RegistryAccess::RegistryAccess(Logger* logger)
    : m_pLogger(logger)
    , m_rootKey(HKEY_LOCAL_MACHINE)
{
    if (!m_pLogger) {
        qWarning() << "RegistryAccess: Logger is null!";
    }
}

RegistryAccess::~RegistryAccess()
{
}

HKEY RegistryAccess::openKey(const QString &keyPath, REGSAM accessRights, bool createIfMissing)
{
    if (keyPath.isEmpty()) {
        if (m_pLogger) m_pLogger->log("Registry: Empty key path", Logger::Error);
        return NULL;
    }

    HKEY hKey = NULL;
    std::wstring wKeyPath = keyPath.toStdWString();

    LONG result = RegOpenKeyExW(m_rootKey, wKeyPath.c_str(), 0, accessRights, &hKey);

    if (result != ERROR_SUCCESS && createIfMissing) {
        result = RegCreateKeyExW(
            m_rootKey,
            wKeyPath.c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            accessRights,
            NULL,
            &hKey,
            NULL);

        if (result == ERROR_SUCCESS && m_pLogger) {
            m_pLogger->log(QString("Registry: Created key: %1").arg(keyPath), Logger::Info);
        }
    }

    if (result != ERROR_SUCCESS) {
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Failed to open key '%1': %2")
                               .arg(keyPath).arg(getErrorString(result)), Logger::Error);
        }
        return NULL;
    }

    return hKey;
}

void RegistryAccess::closeKey(HKEY key)
{
    if (key != NULL) {
        RegCloseKey(key);
    }
}

bool RegistryAccess::readValue(const QString &keyPath, const QString &valueName, QVariant &value, DWORD valueType)
{
    HKEY hKey = openKey(keyPath, KEY_READ, false);
    if (!hKey) {
        return false;
    }

    bool success = false;
    QString valueString;

    switch (valueType) {
    case REG_SZ:
    case REG_EXPAND_SZ: {
        QString stringValue;
        success = readStringValue(hKey, valueName, stringValue);
        if (success) {
            value = stringValue;
            valueString = stringValue;
        }
        break;
    }
    case REG_DWORD: {
        DWORD dwordValue;
        success = readDwordValue(hKey, valueName, dwordValue);
        if (success) {
            value = static_cast<uint>(dwordValue);
            valueString = QString::number(dwordValue);
        }
        break;
    }
    case REG_QWORD: {
        ULONGLONG qwordValue;
        success = readQwordValue(hKey, valueName, qwordValue);
        if (success) {
            value = static_cast<qulonglong>(qwordValue);
            valueString = QString::number(qwordValue);
        }
        break;
    }
    case REG_BINARY: {
        QByteArray binaryValue;
        success = readBinaryValue(hKey, valueName, binaryValue);
        if (success) {
            value = binaryValue;
            valueString = QString("Binary[%1 bytes]").arg(binaryValue.size());
        }
        break;
    }
    default:
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Unsupported value type: %1").arg(valueType), Logger::Error);
        }
        break;
    }

    closeKey(hKey);

    if (success && m_pLogger) {
        m_pLogger->log(QString("Registry: Read '%1\\%2' = %3")
                           .arg(keyPath, valueName, valueString), Logger::Info);
    }

    return success;
}

bool RegistryAccess::writeValue(const QString &keyPath, const QString &valueName, const QVariant &value, DWORD valueType)
{
    HKEY hKey = openKey(keyPath, KEY_WRITE, true);
    if (!hKey) {
        return false;
    }

    bool success = false;
    QString valueString;

    switch (valueType) {
    case REG_SZ:
    case REG_EXPAND_SZ: {
        success = writeStringValue(hKey, valueName, value.toString(), valueType);
        valueString = value.toString();
        break;
    }
    case REG_DWORD: {
        success = writeDwordValue(hKey, valueName, value.toUInt());
        valueString = QString::number(value.toUInt());
        break;
    }
    case REG_QWORD: {
        success = writeQwordValue(hKey, valueName, value.toULongLong());
        valueString = QString::number(value.toULongLong());
        break;
    }
    case REG_BINARY: {
        success = writeBinaryValue(hKey, valueName, value.toByteArray());
        valueString = QString("Binary[%1 bytes]").arg(value.toByteArray().size());
        break;
    }
    default:
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Unsupported value type: %1").arg(valueType), Logger::Error);
        }
        break;
    }

    closeKey(hKey);

    if (success && m_pLogger) {
        m_pLogger->log(QString("Registry: Wrote '%1\\%2' = %3")
                           .arg(keyPath, valueName, valueString), Logger::Info);
    }

    return success;
}

bool RegistryAccess::del(const QString &keyPath, const QString &valueName)
{
    if (valueName.isEmpty()) {
        return deleteKey(keyPath);
    } else {
        return deleteValue(keyPath, valueName);
    }
}

bool RegistryAccess::deleteValue(const QString &keyPath, const QString &valueName)
{
    HKEY hKey = openKey(keyPath, KEY_WRITE, false);
    if (!hKey) {
        return false;
    }

    LONG result = RegDeleteValueW(hKey, valueName.toStdWString().c_str());
    closeKey(hKey);

    if (result == ERROR_SUCCESS) {
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Deleted value '%1\\%2'")
                               .arg(keyPath, valueName), Logger::Info);
        }
        return true;
    } else {
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Failed to delete value '%1\\%2': %3")
                               .arg(keyPath, valueName, getErrorString(result)), Logger::Error);
        }
        return false;
    }
}

bool RegistryAccess::deleteKey(const QString &keyPath)
{
    std::wstring wKeyPath = keyPath.toStdWString();

    LONG result = RegDeleteKeyExW(m_rootKey, wKeyPath.c_str(), KEY_WOW64_64KEY, 0);

    if (result == ERROR_SUCCESS) {
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Deleted key '%1'").arg(keyPath), Logger::Info);
        }
        return true;
    } else {
        if (m_pLogger) {
            m_pLogger->log(QString("Registry: Failed to delete key '%1': %2")
                               .arg(keyPath, getErrorString(result)), Logger::Error);
        }
        return false;
    }
}

bool RegistryAccess::keyExists(const QString &keyPath)
{
    HKEY hKey = openKey(keyPath, KEY_READ, false);
    if (hKey) {
        closeKey(hKey);
        return true;
    }
    return false;
}

bool RegistryAccess::valueExists(const QString &keyPath, const QString &valueName)
{
    HKEY hKey = openKey(keyPath, KEY_READ, false);
    if (!hKey) {
        return false;
    }

    DWORD type, size = 0;
    LONG result = RegQueryValueExW(hKey, valueName.toStdWString().c_str(), NULL, &type, NULL, &size);
    closeKey(hKey);

    return (result == ERROR_SUCCESS);
}

bool RegistryAccess::readStringValue(HKEY key, const QString &valueName, QString &value)
{
    DWORD dataSize = 0;
    DWORD type = 0;
    std::wstring wValueName = valueName.toStdWString();

    // First call to get the size
    LONG result = RegQueryValueExW(key, wValueName.c_str(), NULL, &type, NULL, &dataSize);

    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return false;
    }

    if (dataSize == 0) {
        value = QString();
        return true;
    }

    // Allocate buffer (dataSize is in bytes for wide strings)
    QByteArray buffer(dataSize, 0);

    // Second call to get the actual data
    result = RegQueryValueExW(key, wValueName.c_str(), NULL, &type,
                              reinterpret_cast<LPBYTE>(buffer.data()), &dataSize);

    if (result == ERROR_SUCCESS) {
        // Convert from wide string (buffer contains wchar_t data)
        value = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.constData()),
                                        (dataSize / sizeof(wchar_t)) - 1); // -1 to exclude null terminator
        return true;
    }

    return false;
}

bool RegistryAccess::readDwordValue(HKEY key, const QString &valueName, DWORD &value)
{
    DWORD dataSize = sizeof(DWORD);
    DWORD type = 0;

    LONG result = RegQueryValueExW(key, valueName.toStdWString().c_str(), NULL, &type,
                                   reinterpret_cast<LPBYTE>(&value), &dataSize);

    return (result == ERROR_SUCCESS && type == REG_DWORD);
}

bool RegistryAccess::readQwordValue(HKEY key, const QString &valueName, ULONGLONG &value)
{
    DWORD dataSize = sizeof(ULONGLONG);
    DWORD type = 0;

    LONG result = RegQueryValueExW(key, valueName.toStdWString().c_str(), NULL, &type,
                                   reinterpret_cast<LPBYTE>(&value), &dataSize);

    return (result == ERROR_SUCCESS && type == REG_QWORD);
}

bool RegistryAccess::readBinaryValue(HKEY key, const QString &valueName, QByteArray &value)
{
    DWORD dataSize = 0;
    DWORD type = 0;
    std::wstring wValueName = valueName.toStdWString();

    // First call to get the size
    LONG result = RegQueryValueExW(key, wValueName.c_str(), NULL, &type, NULL, &dataSize);

    if (result != ERROR_SUCCESS || type != REG_BINARY) {
        return false;
    }

    if (dataSize == 0) {
        value = QByteArray();
        return true;
    }

    // Allocate buffer
    value.resize(static_cast<int>(dataSize));

    // Second call to get the actual data
    result = RegQueryValueExW(key, wValueName.c_str(), NULL, &type,
                              reinterpret_cast<LPBYTE>(value.data()), &dataSize);

    return (result == ERROR_SUCCESS);
}

bool RegistryAccess::writeStringValue(HKEY key, const QString &valueName, const QString &value, DWORD type)
{
    std::wstring wValue = value.toStdWString();
    DWORD dataSize = static_cast<DWORD>((wValue.length() + 1) * sizeof(wchar_t));

    LONG result = RegSetValueExW(key, valueName.toStdWString().c_str(), 0, type,
                                 reinterpret_cast<const BYTE*>(wValue.c_str()), dataSize);

    return (result == ERROR_SUCCESS);
}

bool RegistryAccess::writeDwordValue(HKEY key, const QString &valueName, DWORD value)
{
    LONG result = RegSetValueExW(key, valueName.toStdWString().c_str(), 0, REG_DWORD,
                                 reinterpret_cast<const BYTE*>(&value), sizeof(value));

    return (result == ERROR_SUCCESS);
}

bool RegistryAccess::writeQwordValue(HKEY key, const QString &valueName, ULONGLONG value)
{
    LONG result = RegSetValueExW(key, valueName.toStdWString().c_str(), 0, REG_QWORD,
                                 reinterpret_cast<const BYTE*>(&value), sizeof(value));

    return (result == ERROR_SUCCESS);
}

bool RegistryAccess::writeBinaryValue(HKEY key, const QString &valueName, const QByteArray &value)
{
    LONG result = RegSetValueExW(key, valueName.toStdWString().c_str(), 0, REG_BINARY,
                                 reinterpret_cast<const BYTE*>(value.constData()),
                                 static_cast<DWORD>(value.size()));

    return (result == ERROR_SUCCESS);
}

QString RegistryAccess::getErrorString(LONG errorCode)
{
    switch (errorCode) {
    case ERROR_SUCCESS: return "Success";
    case ERROR_FILE_NOT_FOUND: return "Key not found";
    case ERROR_ACCESS_DENIED: return "Access denied";
    case ERROR_INVALID_PARAMETER: return "Invalid parameter";
    case ERROR_MORE_DATA: return "More data available";
    case ERROR_NO_MORE_ITEMS: return "No more items";
    default: return QString("Error code %1").arg(errorCode);
    }
}
