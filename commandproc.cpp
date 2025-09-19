#include "CommandProc.h"

CommandProc::CommandProc(Logger* logger, QObject *parent)
    : QObject(parent), m_pLogger(logger), m_WinRing0(logger,parent),
    m_RegistryAccess(logger), m_WmiAccess(logger)
{
    m_WinRing0.load();
    m_WmiAccess.initialize();
}

int CommandProc::ProcessCommand(CommandMessage* pMsg)
{
    int result = -1;
    CMD_HDR* hdr = reinterpret_cast<CMD_HDR*>(pMsg->command);

    // Verify checksum (Checksum verification logic goes here)

    switch (hdr->CmdID) {
    case CMD_ID::MSR_READ: {
        result = handleMsrRead(pMsg);
        break;
    }
    case CMD_ID::MSR_WRITE: {
        result = handleMsrWrite(pMsg);
        break;
    }
    case CMD_ID::REG_READ: {
        result = handleRegRead(pMsg);
        break;
    }
    case CMD_ID::REG_WRITE: {
        result = handleRegWrite(pMsg);
        break;
    }
    case CMD_ID::REG_DEL: {
        result = handleRegDel(pMsg);
        break;
    }
    case CMD_ID::WMI_QUERY: {
        result = handleWmiQuery(pMsg);
        break;
    }
    case CMD_ID::WMI_METHOD: {
        result = handleWmiMethod(pMsg);
        break;
    }
    case CMD_ID::FILE_DELETE: {
        result = handleFileDelete(pMsg);
        break;
    }
    case CMD_ID::FILE_RENAME: {
        result = handleFileRename(pMsg);
        break;
    }
    case CMD_ID::FILE_COPY: {
        result = handleFileCopy(pMsg);
        break;
    }
    case CMD_ID::FILE_MOVE: {
        result = handleFileMove(pMsg);
        break;
    }
    default:
        result = handleInvalidCmd(pMsg);
        break;
    }

    return result;
}

int CommandProc::handleMsrRead(CommandMessage* pMsg)
{
    CMD_MSR_READ* pReadCmd = (CMD_MSR_READ*) &pMsg->command;

    // Init the response
    CMD_MSR_READ_RESP* pResp = (CMD_MSR_READ_RESP*) &pMsg->response;
    pResp->hdr.ChkSum = 0;
    pResp->hdr.CmdIndex = pReadCmd->hdr.CmdIndex;
    pResp->hdr.CmdID = pReadCmd->hdr.CmdID;
    pResp->hdr.PacketSize = sizeof(CMD_MSR_READ_RESP);  // This is correct - fixed size response

    if (pReadCmd->hdr.PacketSize != sizeof(CMD_MSR_READ))
    {
        m_pLogger->log(QString("!MSR Read size - %1").arg(pReadCmd->hdr.PacketSize));
        pResp->hdr.Result = CMD_RESULT::RES_INV_INPUTS;
        return -1;
    }

    DWORD low, high;
    bool success = m_WinRing0.readMsr(pReadCmd->MSR, low, high);
    if (success) {
        m_pLogger->log(QString("MSR Read - MSR: %1, Low: %2, High: %3").arg(pReadCmd->MSR).arg(low).arg(high));
        pResp->DataL = low;
        pResp->DataH = high;
        pResp->hdr.Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        m_pLogger->log(QString("!MSR Read Failed - MSR: %1").arg(pReadCmd->MSR));
        pResp->hdr.Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleMsrWrite(CommandMessage* pMsg)
{
    CMD_MSR_WRITE* pWriteCmd = (CMD_MSR_WRITE*) &pMsg->command;

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pWriteCmd->hdr.CmdIndex;
    pResp->CmdID = pWriteCmd->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    if (pWriteCmd->hdr.PacketSize != sizeof(CMD_MSR_WRITE))
    {
        m_pLogger->log(QString("!MSR Write size - %1").arg(pWriteCmd->hdr.PacketSize));
        pResp->Result = CMD_RESULT::RES_INV_INPUTS;
        return -1;
    }

    bool success = m_WinRing0.writeMsr(pWriteCmd->MSR, pWriteCmd->DataL, pWriteCmd->DataH);
    if (success) {
        m_pLogger->log(QString("MSR Write - MSR: %1, Low: %2, High: %3").arg(pWriteCmd->MSR).arg(pWriteCmd->DataL).arg(pWriteCmd->DataH));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        m_pLogger->log(QString("!MSR Write - MSR: %1, Low: %2, High: %3").arg(pWriteCmd->MSR).arg(pWriteCmd->DataL).arg(pWriteCmd->DataH));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleRegRead(CommandMessage* pMsg)
{
    CMD_REG_READ* pRead = (CMD_REG_READ*) &pMsg->command;

    // Extract the keyPath and valueName from the flexible array
    wchar_t* keyPath = pRead->data;
    wchar_t* valueName = pRead->data + pRead->keyPathSize / sizeof(wchar_t);

    // Init the response - FIXED: Only include header + datasize field initially
    CMD_REG_READ_RESP* pResp = (CMD_REG_READ_RESP*) &pMsg->response;
    pResp->hdr.ChkSum = 0;
    pResp->hdr.CmdIndex = pRead->hdr.CmdIndex;
    pResp->hdr.CmdID = pRead->hdr.CmdID;
    pResp->hdr.PacketSize = sizeof(CMD_RESP_HDR) + sizeof(uint32_t); // Header + datasize field
    pResp->datasize = 0;

    quint32 insize = sizeof(CMD_REG_READ) + pRead->valueNameSize + pRead->keyPathSize;
    if (pRead->hdr.PacketSize != insize)
    {
        m_pLogger->log(QString("!Read Reg size - %1,%2").arg(pRead->hdr.PacketSize).arg(insize));
        pResp->hdr.Result = CMD_RESULT::RES_INV_INPUTS;
        return -1;
    }

    QVariant value;
    QString keypath = QString::fromWCharArray(keyPath,pRead->keyPathSize / sizeof(wchar_t));
    QString valname = QString::fromWCharArray(valueName,pRead->valueNameSize / sizeof(wchar_t));
    bool success = m_RegistryAccess.readValue(keypath,
                                              valname,
                                              value,
                                              pRead->valueType);
    if (success) {
        m_pLogger->log(QString::asprintf("Registry Read - KeyPath: %s, ValueName: %s, Value: %s",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname),
                                         qUtf8Printable(value.toString())));
        pResp->hdr.Result = CMD_RESULT::RES_OK;

        if (pRead->valueType == (quint16) REG_VALUE_TYPE::REG_V_DWORD)
        {
            quint32 dword = value.toUInt();
            memcpy(pResp->data, &dword, sizeof(quint32));
            pResp->datasize = sizeof(quint32);
        }
        else if (pRead->valueType == (quint16) REG_VALUE_TYPE::REG_V_QWORD)
        {
            quint64 qword = value.toLongLong();
            memcpy(pResp->data, &qword, sizeof(quint64));
            pResp->datasize = sizeof(quint64);
        }
        else if (pRead->valueType == (quint16) REG_VALUE_TYPE::REG_V_SZ)
        {
            QString str = value.toString();
            str.toWCharArray((wchar_t*) pResp->data);
            pResp->datasize = (quint32) str.size() * sizeof(wchar_t);
        }

        // FIXED: Add actual data size to base packet size
        pResp->hdr.PacketSize = sizeof(CMD_RESP_HDR) + sizeof(uint32_t) + pResp->datasize;

        return 0;
    }
    else
    {
        m_pLogger->log(QString::asprintf("Registry Read Failed - KeyPath: %s, ValueName: %s",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname)));

        pResp->hdr.Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleRegWrite(CommandMessage* pMsg)
{
    CMD_REG_WRITE* pWrite = (CMD_REG_WRITE*) &pMsg->command;

    // Extract keyPath, valueName, and data from the flexible array
    wchar_t* keyPath = pWrite->data;
    wchar_t* valueName = pWrite->data + pWrite->keyPathSize / sizeof(wchar_t);
    wchar_t* dataPtr = pWrite->data + (pWrite->keyPathSize + pWrite->valueNameSize) / sizeof(wchar_t);

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pWrite->hdr.CmdIndex;
    pResp->CmdID = pWrite->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    quint32 insize = sizeof(CMD_REG_WRITE) + pWrite->valueNameSize + pWrite->keyPathSize + pWrite->dataSize;
    if (pWrite->hdr.PacketSize != insize)
    {
        m_pLogger->log(QString("!Write Reg size - %1,%2").arg(pWrite->hdr.PacketSize).arg(insize));
        pResp->Result = CMD_RESULT::RES_INV_INPUTS;
        return -1;
    }

    QVariant value;
    switch (static_cast<quint16>(pWrite->valueType))
    {
    case static_cast<quint16>(REG_VALUE_TYPE::REG_V_DWORD):
        value = *reinterpret_cast<quint32*>(dataPtr);
        break;
    case static_cast<quint16>(REG_VALUE_TYPE::REG_V_QWORD):
        value = *reinterpret_cast<quint64*>(dataPtr);
        break;
    default:
        QString wDataString = QString::fromWCharArray(dataPtr, pWrite->dataSize / sizeof(wchar_t));
        value = QVariant(wDataString);
        break;
    }

    QString keypath = QString::fromWCharArray(keyPath,pWrite->keyPathSize / sizeof(wchar_t));
    QString valname = QString::fromWCharArray(valueName,pWrite->valueNameSize / sizeof(wchar_t));
    bool success = m_RegistryAccess.writeValue(keypath,
                                               valname,
                                               value,
                                               pWrite->valueType);

    if (success)
    {
        m_pLogger->log(QString::asprintf("Registry Write - KeyPath: %s, ValueName: %s, ValueType: %u, DataSize: %u",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname),
                                         static_cast<quint16>(pWrite->valueType),
                                         pWrite->dataSize));

        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        m_pLogger->log(QString::asprintf("Registry Write Failed - KeyPath: %s, ValueName: %s",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname)));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleRegDel(CommandMessage *pMsg)
{
    CMD_REG_DEL* pDel = (CMD_REG_DEL*) &pMsg->command;

    // Extract keyPath and valueName from the flexible array
    wchar_t* keyPath = pDel->data;
    wchar_t* valueName = pDel->data + pDel->keyPathSize / sizeof(wchar_t);

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pDel->hdr.CmdIndex;
    pResp->CmdID = pDel->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    quint32 insize = sizeof(CMD_REG_DEL) + pDel->keyPathSize + pDel->valueNameSize;
    if (pDel->hdr.PacketSize != insize)
    {
        m_pLogger->log(QString("!Del Reg size  - %1,%2").arg(pDel->hdr.PacketSize).arg(insize));
        pResp->Result = CMD_RESULT::RES_INV_INPUTS;
        return -1;
    }

    QString keypath = QString::fromWCharArray(keyPath,pDel->keyPathSize / sizeof(wchar_t));
    QString valname = QString::fromWCharArray(valueName,pDel->valueNameSize / sizeof(wchar_t));
    bool success = m_RegistryAccess.del(keypath,valname);
    if (success)
    {
        m_pLogger->log(QString::asprintf("Registry Del - KeyPath: %s, ValueName: %s",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname)));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        m_pLogger->log(QString::asprintf("!Registry Del - KeyPath: %s, ValueName: %s",
                                         qUtf8Printable(keypath),
                                         qUtf8Printable(valname)));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleFileDelete(CommandMessage *pMsg)
{
    CMD_FILE_DEL* pDel = (CMD_FILE_DEL*) &pMsg->command;

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pDel->hdr.CmdIndex;
    pResp->CmdID = pDel->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    QString filepath = QString::fromWCharArray(pDel->data,pDel->filePathSize / sizeof(wchar_t));

    // Use Windows API to delete the file
    if (DeleteFileW((LPCWSTR) filepath.utf16()))
    {
        m_pLogger->log(QString("File deleted successfully: %1").arg(filepath));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        DWORD error = GetLastError();
        m_pLogger->log(QString("Failed to delete file: %1, Error: %2").arg(filepath).arg(error));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleFileRename(CommandMessage *pMsg)
{
    CMD_FILE_RENAME* pRename = (CMD_FILE_RENAME*) &pMsg->command;

    // Extract file paths from the flexible array
    wchar_t* oldPath = pRename->data;
    wchar_t* newPath = pRename->data + pRename->fileOldPathSize / sizeof(wchar_t);

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pRename->hdr.CmdIndex;
    pResp->CmdID = pRename->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    QString oldpath = QString::fromWCharArray(oldPath,pRename->fileOldPathSize / sizeof(wchar_t));
    QString newpath = QString::fromWCharArray(newPath,pRename->fileNewPathSize / sizeof(wchar_t));

    // Use Windows API to rename the file
    if (MoveFileW((LPCWSTR)oldpath.utf16(), (LPCWSTR) newpath.utf16()))
    {
        m_pLogger->log(QString::asprintf("File renamed successfully: %s -> %s",
                                         qUtf8Printable(oldpath),
                                         qUtf8Printable(newpath)));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        DWORD error = GetLastError();
        m_pLogger->log(QString::asprintf("Failed to rename file: %s -> %s, Error: %u",
                                         qUtf8Printable(oldpath),
                                         qUtf8Printable(newpath),
                                         error));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleFileCopy(CommandMessage *pMsg)
{
    CMD_FILE_COPY* pCopy = (CMD_FILE_COPY*) &pMsg->command;

    // Extract file paths from the flexible array
    wchar_t* fromPath = pCopy->data;
    wchar_t* toPath = pCopy->data + pCopy->fileFromPathSize / sizeof(wchar_t);

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pCopy->hdr.CmdIndex;
    pResp->CmdID = pCopy->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    QString frompath = QString::fromWCharArray(fromPath,pCopy->fileFromPathSize / sizeof(wchar_t));
    QString topath = QString::fromWCharArray(toPath,pCopy->fileToPathSize / sizeof(wchar_t));

    // Use Windows API to copy the file
    if (CopyFileW((LPCWSTR)frompath.utf16(), (LPCWSTR)topath.utf16(), FALSE))
    {
        m_pLogger->log(QString::asprintf("File copied successfully: %s -> %s",
                                         qUtf8Printable(frompath),
                                         qUtf8Printable(topath)));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        DWORD error = GetLastError();
        m_pLogger->log(QString::asprintf("Failed to copy file: %s -> %s, Error: %u",
                                         qUtf8Printable(frompath),
                                         qUtf8Printable(topath),
                                         error));
        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleFileMove(CommandMessage *pMsg)
{
    CMD_FILE_MOVE* pMove = (CMD_FILE_MOVE*) &pMsg->command;

    // Extract file paths from the flexible array
    wchar_t* fromPath = pMove->data;
    wchar_t* toPath = pMove->data + pMove->fileFromPathSize / sizeof(wchar_t);

    // Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pMove->hdr.CmdIndex;
    pResp->CmdID = pMove->hdr.CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);

    QString frompath = QString::fromWCharArray(fromPath,pMove->fileFromPathSize / sizeof(wchar_t));
    QString topath = QString::fromWCharArray(toPath,pMove->fileToPathSize / sizeof(wchar_t));

    // Use Windows API to move the file
    if (MoveFileW((LPCWSTR)frompath.utf16(), (LPCWSTR)topath.utf16()))
    {
        m_pLogger->log(QString::asprintf("File moved successfully: %s -> %s",
                                         qUtf8Printable(frompath),
                                         qUtf8Printable(topath)));
        pResp->Result = CMD_RESULT::RES_OK;
        return 0;
    }
    else
    {
        DWORD error = GetLastError();
        m_pLogger->log(QString::asprintf("Failed to move file: %s -> %s, Error: %u",
                                         qUtf8Printable(frompath),
                                         qUtf8Printable(topath),
                                         error));

        pResp->Result = CMD_RESULT::RES_FAILED_OP;
        return -1;
    }
}

int CommandProc::handleWmiQuery(CommandMessage* pMsg)
{
    CMD_WMI_QUERY* pQuery = (CMD_WMI_QUERY*) &pMsg->command;

    // Initialize the response
    CMD_WMI_QUERY_RESP* pResp = (CMD_WMI_QUERY_RESP*) &pMsg->response;
    pResp->hdr.ChkSum = 0;
    pResp->hdr.CmdIndex = pQuery->hdr.CmdIndex;
    pResp->hdr.CmdID = pQuery->hdr.CmdID;
    pResp->hdr.PacketSize = sizeof(CMD_RESP_HDR); // FIXED: Start with just header

    // Extract the namespace path, class name, query, and property from the flexible array member
    wchar_t* namespacePath = pQuery->data;
    wchar_t* className = pQuery->data + pQuery->namespacePathSize / sizeof(wchar_t);
    wchar_t* query = pQuery->data + (pQuery->namespacePathSize + pQuery->classNameSize) / sizeof(wchar_t);
    wchar_t* property = pQuery->data + (pQuery->namespacePathSize + pQuery->classNameSize + pQuery->querySize) / sizeof(wchar_t);

    // Convert wchar_t to QString
    QString namespacePathStr = QString::fromWCharArray(namespacePath,pQuery->namespacePathSize / sizeof(wchar_t));
    QString classNameStr = QString::fromWCharArray(className,pQuery->classNameSize / sizeof(wchar_t));
    QString wmiQueryStr = QString::fromWCharArray(query,pQuery->querySize / sizeof(wchar_t));
    QString propertyStr = QString::fromWCharArray(property,pQuery->propertySize / sizeof(wchar_t));

    // Log the WMI query details
    m_pLogger->log(QString("WMI Query - Namespace: %1, Class: %2, Query: %3, Property: %4")
                       .arg(namespacePathStr, classNameStr, wmiQueryStr, propertyStr));

    // Prepare a QVector of QMap to hold the query results
    QVector<QMap<QString, QVariant>> queryResults;

    // Perform the WMI query using m_WmiAccess
    bool success = m_WmiAccess.query(namespacePathStr, wmiQueryStr, queryResults, propertyStr);
    if (success)
    {
        // Log the success
        m_pLogger->log("WMI Query executed successfully");

        // Serialize the query results into the response buffer
        QByteArray responseData;
        QDataStream responseStream(&responseData, QIODevice::WriteOnly);
        for (const auto& resultMap : queryResults)
        {
            responseStream << resultMap;
        }

        // Ensure the response fits within the buffer
        if (responseData.size() <= RESP_MAX_SIZE - sizeof(CMD_RESP_HDR))
        {
            memcpy(pResp->data, responseData.data(), responseData.size());
            pResp->hdr.PacketSize = sizeof(CMD_RESP_HDR) + responseData.size(); // FIXED: Add data size
            pResp->hdr.Result = CMD_RESULT::RES_OK;
            m_pLogger->log(QString("WMI Query results serialized successfully. %1").arg(pResp->hdr.PacketSize));
        }
        else
        {
            m_pLogger->log("WMI Query results too large to fit into response buffer");
            pResp->hdr.Result = CMD_RESULT::RES_FAILED_OP;
        }
    }
    else
    {
        m_pLogger->log("Failed to execute WMI query");
        pResp->hdr.Result = CMD_RESULT::RES_FAILED_OP;
    }

    return (pResp->hdr.Result == CMD_RESULT::RES_OK) ? 0 : -1;
}

int CommandProc::handleWmiMethod(CommandMessage* pMsg)
{
    return -1;
}

int CommandProc::handleInvalidCmd(CommandMessage *pMsg)
{
    CMD_HDR* pHdr = (CMD_HDR*) &pMsg->command;

    //Init the response
    CMD_RESP_HDR* pResp = (CMD_RESP_HDR*) &pMsg->response;
    pResp->ChkSum = 0;
    pResp->CmdIndex = pHdr->CmdIndex;
    pResp->CmdID = pHdr->CmdID;
    pResp->PacketSize = sizeof(CMD_RESP_HDR);
    pResp->Result = CMD_RESULT::RES_INV_CMD;
    return -1;
}
