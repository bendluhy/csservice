#include "WmiAccess.h"
#include <QMutexLocker>

WmiAccess::WmiAccess(Logger* logger)
    : m_pLogger(logger)
    , m_pLoc(nullptr)
    , m_pSvc(nullptr)
    , m_isInitialized(false)
    , m_comInitialized(false)
{
}

WmiAccess::~WmiAccess()
{
    if (m_pLogger) {
        m_pLogger->log("WmiAccess: Closing", Logger::Info);
    }
    deinitialize();
}

bool WmiAccess::initialize()
{
    QMutexLocker locker(&m_mutex);

    if (m_isInitialized) {
        if (m_pLogger) {
            m_pLogger->log("WmiAccess: Already initialized", Logger::Warning);
        }
        return true;
    }

    HRESULT hres;

    // Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (hres == RPC_E_CHANGED_MODE) {
        // COM already initialized in different mode, try to continue
        if (m_pLogger) {
            m_pLogger->log("WmiAccess: COM already initialized in different mode", Logger::Warning);
        }
        m_comInitialized = false; // Don't uninitialize in destructor
    } else if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Failed to initialize COM: %1").arg(getComErrorString(hres)), Logger::Error);
        }
        return false;
    } else {
        m_comInitialized = true;
    }

    // Set general COM security levels
    hres = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr);

    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        // RPC_E_TOO_LATE means security was already set, which is OK
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Failed to initialize COM security: %1").arg(getComErrorString(hres)), Logger::Error);
        }
        if (m_comInitialized) {
            CoUninitialize();
            m_comInitialized = false;
        }
        return false;
    }

    // Obtain the initial locator to WMI
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&m_pLoc);

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Failed to create IWbemLocator: %1").arg(getComErrorString(hres)), Logger::Error);
        }
        if (m_comInitialized) {
            CoUninitialize();
            m_comInitialized = false;
        }
        return false;
    }

    m_isInitialized = true;
    if (m_pLogger) {
        m_pLogger->log("WmiAccess: Initialized successfully", Logger::Info);
    }

    return true;
}

void WmiAccess::deinitialize()
{
    QMutexLocker locker(&m_mutex);

    if (!m_isInitialized) {
        return;
    }

    disconnectServer();

    if (m_pLoc) {
        m_pLoc->Release();
        m_pLoc = nullptr;
    }

    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }

    m_isInitialized = false;

    if (m_pLogger) {
        m_pLogger->log("WmiAccess: Deinitialized", Logger::Info);
    }
}

bool WmiAccess::connectServer(const QString &namespacePath)
{
    // If already connected to this namespace, return success
    if (m_pSvc && m_currentNamespace == namespacePath) {
        return true;
    }

    // Disconnect from current server if connected
    disconnectServer();

    if (!m_pLoc) {
        if (m_pLogger) {
            m_pLogger->log("WmiAccess: Not initialized", Logger::Error);
        }
        return false;
    }

    HRESULT hres = m_pLoc->ConnectServer(
        _bstr_t(namespacePath.toStdWString().c_str()),
        nullptr,  // User name (nullptr = current user)
        nullptr,  // Password (nullptr = current)
        0,        // Locale
        0L,       // Security flags
        0,        // Authority
        0,        // Context object
        &m_pSvc);

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Could not connect to namespace '%1': %2")
                               .arg(namespacePath).arg(getComErrorString(hres)), Logger::Error);
        }
        return false;
    }

    // Set security levels on the proxy
    hres = CoSetProxyBlanket(
        m_pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE);

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Could not set proxy blanket: %1").arg(getComErrorString(hres)), Logger::Error);
        }
        m_pSvc->Release();
        m_pSvc = nullptr;
        return false;
    }

    m_currentNamespace = namespacePath;
    return true;
}

void WmiAccess::disconnectServer()
{
    if (m_pSvc) {
        m_pSvc->Release();
        m_pSvc = nullptr;
    }
    m_currentNamespace.clear();
}

bool WmiAccess::query(const QString &namespacePath,
                      const QString &query,
                      QVector<QMap<QString, QVariant>> &results,
                      const QString &property)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isInitialized) {
        if (m_pLogger) {
            m_pLogger->log("WmiAccess: Not initialized", Logger::Error);
        }
        return false;
    }

    // Ensure connection to the WMI namespace
    if (!connectServer(namespacePath)) {
        return false;
    }

    HRESULT hres;
    IEnumWbemClassObject* pEnumerator = nullptr;

    // Execute WMI query
    hres = m_pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(query.toStdWString().c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator);

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Query failed '%1': %2").arg(query).arg(getComErrorString(hres)), Logger::Error);
        }
        return false;
    }

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;

    // Iterate over the results
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (uReturn == 0) {
            break; // No more results
        }

        QMap<QString, QVariant> result;
        VARIANT vtProp;
        BSTR propName = nullptr;

        // Begin enumeration of the properties
        hr = pclsObj->BeginEnumeration(0);
        if (SUCCEEDED(hr)) {
            // Iterate over properties
            while (pclsObj->Next(0, &propName, &vtProp, nullptr, nullptr) == WBEM_S_NO_ERROR) {
                if (propName) {
                    QString propertyName = QString::fromWCharArray(propName);

                    // Filter by requested property if specified
                    bool shouldInclude = property.isEmpty() || propertyName == property;

                    // Filter out system properties (those starting with __)
                    if (shouldInclude && !propertyName.startsWith("__")) {
                        QVariant qValue;
                        if (variantToQVariant(vtProp, qValue)) {
                            result.insert(propertyName, qValue);
                        }
                    }

                    SysFreeString(propName);
                    propName = nullptr;
                }
                VariantClear(&vtProp);
            }

            pclsObj->EndEnumeration();
        }

        // Add non-empty results
        if (!result.isEmpty()) {
            results.append(result);
        }

        pclsObj->Release();
    }

    // Release enumerator
    pEnumerator->Release();

    if (m_pLogger) {
        m_pLogger->log(QString("WmiAccess: Query returned %1 results").arg(results.size()), Logger::Info);
    }

    return true;
}

bool WmiAccess::execMethod(const QString &namespacePath,
                           const QString &className,
                           const QString &methodName,
                           const QMap<QString, QVariant> &params,
                           QMap<QString, QVariant> &results)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isInitialized) {
        if (m_pLogger) {
            m_pLogger->log("WmiAccess: Not initialized", Logger::Error);
        }
        return false;
    }

    if (!connectServer(namespacePath)) {
        return false;
    }

    HRESULT hres;
    IWbemClassObject* pClass = nullptr;

    hres = m_pSvc->GetObject(_bstr_t(className.toStdWString().c_str()), 0, nullptr, &pClass, nullptr);
    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Could not get class '%1': %2")
                               .arg(className).arg(getComErrorString(hres)), Logger::Error);
        }
        return false;
    }

    IWbemClassObject* pInParamsDefinition = nullptr;
    IWbemClassObject* pOutParamsDefinition = nullptr;

    hres = pClass->GetMethod(_bstr_t(methodName.toStdWString().c_str()), 0, &pInParamsDefinition, &pOutParamsDefinition);

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Could not get method '%1': %2")
                               .arg(methodName).arg(getComErrorString(hres)), Logger::Error);
        }
        pClass->Release();
        return false;
    }

    IWbemClassObject* pClassInstance = nullptr;

    if (pInParamsDefinition) {
        hres = pInParamsDefinition->SpawnInstance(0, &pClassInstance);
        if (FAILED(hres)) {
            if (m_pLogger) {
                m_pLogger->log("WmiAccess: Could not spawn instance for method", Logger::Error);
            }
            if (pOutParamsDefinition) pOutParamsDefinition->Release();
            pInParamsDefinition->Release();
            pClass->Release();
            return false;
        }

        // Set input parameters
        for (auto it = params.cbegin(); it != params.cend(); ++it) {
            VARIANT var;
            VariantInit(&var);

            if (qVariantToVariant(it.value(), var)) {
                hres = pClassInstance->Put(it.key().toStdWString().c_str(), 0, &var, 0);
                if (FAILED(hres) && m_pLogger) {
                    m_pLogger->log(QString("WmiAccess: Failed to set parameter '%1'").arg(it.key()), Logger::Warning);
                }
            }
            VariantClear(&var);
        }
    }

    // Execute method
    IWbemClassObject* pOutParams = nullptr;
    hres = m_pSvc->ExecMethod(
        _bstr_t(className.toStdWString().c_str()),
        _bstr_t(methodName.toStdWString().c_str()),
        0,
        nullptr,
        pClassInstance,
        &pOutParams,
        nullptr);

    // Clean up input parameters
    if (pClassInstance) pClassInstance->Release();
    if (pInParamsDefinition) pInParamsDefinition->Release();
    if (pOutParamsDefinition) pOutParamsDefinition->Release();
    pClass->Release();

    if (FAILED(hres)) {
        if (m_pLogger) {
            m_pLogger->log(QString("WmiAccess: Method execution failed '%1': %2")
                               .arg(methodName).arg(getComErrorString(hres)), Logger::Error);
        }
        return false;
    }

    // Get output parameters
    if (pOutParams) {
        VARIANT vtProp;
        BSTR propName = nullptr;

        pOutParams->BeginEnumeration(0);

        // FIX: Correct enumeration loop
        while (pOutParams->Next(0, &propName, &vtProp, nullptr, nullptr) == WBEM_S_NO_ERROR) {
            if (propName) {
                QString propertyName = QString::fromWCharArray(propName);
                QVariant qValue;

                if (variantToQVariant(vtProp, qValue)) {
                    results.insert(propertyName, qValue);
                }

                SysFreeString(propName);
            }
            VariantClear(&vtProp);
        }

        pOutParams->EndEnumeration();
        pOutParams->Release();
    }

    return true;
}

bool WmiAccess::variantToQVariant(const VARIANT &variant, QVariant &qVariant)
{
    switch (variant.vt) {
    case VT_EMPTY:
    case VT_NULL:
        qVariant = QVariant();
        break;
    case VT_BSTR:
        if (variant.bstrVal) {
            qVariant = QString::fromWCharArray(variant.bstrVal);
        } else {
            qVariant = QString();
        }
        break;
    case VT_I1:
        qVariant = variant.cVal;
        break;
    case VT_I2:
        qVariant = variant.iVal;
        break;
    case VT_I4:
        qVariant = variant.intVal;
        break;
    case VT_I8:
        qVariant = static_cast<qint64>(variant.llVal);
        break;
    case VT_UI1:
        qVariant = variant.bVal;
        break;
    case VT_UI2:
        qVariant = variant.uiVal;
        break;
    case VT_UI4:
        qVariant = variant.uintVal;
        break;
    case VT_UI8:
        qVariant = static_cast<quint64>(variant.ullVal);
        break;
    case VT_R4:
        qVariant = variant.fltVal;
        break;
    case VT_R8:
        qVariant = variant.dblVal;
        break;
    case VT_BOOL:
        qVariant = (variant.boolVal == VARIANT_TRUE);
        break;
    case VT_DATE:
        // Convert DATE to QDateTime if needed
        qVariant = QVariant(); // Placeholder
        break;
    default:
        return false;
    }
    return true;
}

bool WmiAccess::qVariantToVariant(const QVariant &qVariant, VARIANT &variant)
{
    VariantInit(&variant);

    switch (qVariant.metaType().id()) {
    case QMetaType::QString:
        variant.vt = VT_BSTR;
        variant.bstrVal = SysAllocString(reinterpret_cast<const wchar_t*>(qVariant.toString().utf16()));
        break;
    case QMetaType::Int:
        variant.vt = VT_I4;
        variant.intVal = qVariant.toInt();
        break;
    case QMetaType::UInt:
        variant.vt = VT_UI4;
        variant.uintVal = qVariant.toUInt();
        break;
    case QMetaType::LongLong:
        variant.vt = VT_I8;
        variant.llVal = qVariant.toLongLong();
        break;
    case QMetaType::ULongLong:
        variant.vt = VT_UI8;
        variant.ullVal = qVariant.toULongLong();
        break;
    case QMetaType::Float:
        variant.vt = VT_R4;
        variant.fltVal = qVariant.toFloat();
        break;
    case QMetaType::Double:
        variant.vt = VT_R8;
        variant.dblVal = qVariant.toDouble();
        break;
    case QMetaType::Bool:
        variant.vt = VT_BOOL;
        variant.boolVal = qVariant.toBool() ? VARIANT_TRUE : VARIANT_FALSE;
        break;
    default:
        return false;
    }
    return true;
}

QString WmiAccess::getComErrorString(HRESULT hr)
{
    switch (hr) {
    case S_OK: return "Success";
    case E_ACCESSDENIED: return "Access denied";
    case E_OUTOFMEMORY: return "Out of memory";
    case WBEM_E_FAILED: return "WMI: Failed";
    case WBEM_E_NOT_FOUND: return "WMI: Not found";
    case WBEM_E_ACCESS_DENIED: return "WMI: Access denied";
    case WBEM_E_INVALID_PARAMETER: return "WMI: Invalid parameter";
    case WBEM_E_INVALID_CLASS: return "WMI: Invalid class";
    case WBEM_E_INVALID_QUERY: return "WMI: Invalid query";
    case RPC_E_CHANGED_MODE: return "COM: Already initialized in different mode";
    case RPC_E_TOO_LATE: return "COM: Security already initialized";
    default: return QString("HRESULT: 0x%1").arg(static_cast<uint>(hr), 0, 16);
    }
}
