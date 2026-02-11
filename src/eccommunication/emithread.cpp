#include "emithread.h"
#include "emiio.h"
#include "appstd.h"

#define HOST_EC_IND     m_EmiOffset
#define EC_HOST_IND     m_EmiOffset + 1
#define ADD0_IND        m_EmiOffset + 2
#define ADD1_IND        m_EmiOffset + 3
#define DAT0_IND        m_EmiOffset + 4
#define DAT1_IND        m_EmiOffset + 5
#define DAT2_IND        m_EmiOffset + 6
#define DAT3_IND        m_EmiOffset + 7
#define INTSL_IND       m_EmiOffset + 8
#define INTSH_IND       m_EmiOffset + 9
#define INTML_IND       m_EmiOffset + 0x0A
#define INTMH_IND       m_EmiOffset + 0x0B
#define APPID_IND       m_EmiOffset + 0x0C

EmiThread::EmiThread(QObject *parent)
    : QThread{parent}
{
    m_pPort = PortIo::instance();
}

EmiThread::~EmiThread()
{
    stop();
}

void EmiThread::log(const QString& message, Logger::LogLevel level)
{
    if (m_pLogger) {
        m_pLogger->log(QString("EmiThread: %1").arg(message), level);
    }
}

void EmiThread::run()
{
    QMutexLocker locker(&m_Mutex);

    log("Thread started");

    while (true)
    {
        //Wait for something to do
        while (!m_StopFlag && m_pCmdQueue.isEmpty())
        {
            m_WaitCondition.wait(&m_Mutex);
        }

        //Exit thread if requested
        if (m_StopFlag)
        {
            break;
        }

        //Get the next command
        QSharedPointer<EmiCmd> pCmd = m_pCmdQueue.dequeue();

        locker.unlock();

        //Process the command
        ProcCmd(pCmd);

        // Call the completion callback directly from this thread.
        // This is CRITICAL for synchronous waiters who are blocked on QWaitCondition.
        // The Qt::QueuedConnection signal won't be processed until the caller's
        // event loop runs, but the caller is blocked waiting - classic deadlock.
        // By calling FuncDone here, we wake the synchronous waiter immediately.
        if (pCmd->FuncDone) {
            pCmd->FuncDone(pCmd);
        }

        //Notify EMI controller that we are done (for async/signal-based handling)
        emit CommandDone(pCmd);

        locker.relock();
    }

    log("Thread stopped");
}

void EmiThread::stop()
{
    m_StopFlag = true;
    m_WaitCondition.wakeAll();
    wait();
}

int EmiThread::addCmdToQueue(QSharedPointer<EmiCmd> pCmd)
{
    Q_ASSERT(pCmd);
    if (!m_pPort) return -1;

    QMutexLocker locker(&m_Mutex);
    m_pCmdQueue.enqueue(pCmd);
    m_WaitCondition.wakeOne();

    return 0;
}

EC_HOST_CMD_STATUS EmiThread::ProcCmd(QSharedPointer<EmiCmd> pCmd)
{
    Q_ASSERT(pCmd);
    struct ec_host_cmd_response_header* pHdr;
    EC_HOST_CMD_STATUS stat;

    quint8 data;

    //Convert the payload to an full packet
    QByteArray packetout;
    stat = PayloadToOutPack(pCmd->cmd, pCmd->payloadout, packetout);
    if (stat == EC_HOST_CMD_SUCCESS)
    {

        //Send the intial command
        int retry = 10;
        while (retry--)
        {
            stat = SendCmdOut(packetout, pCmd->payloadin);
            if (stat == EC_HOST_CMD_SUCCESS || stat == EC_HOST_CMD_IN_PROGRESS) break;
        }

        //Check if command takes a while, if so keep poling for it to finish
        if (stat == EC_HOST_CMD_IN_PROGRESS)
        {
            log("Slow transfer in progress", Logger::Warning);
            stat = SendCmdGetResults(pCmd->payloadin);
        }
    }

    pCmd->result = stat;
    return stat;
}

EC_HOST_CMD_STATUS EmiThread::SendCmdGetResults(QByteArray &payloadin)
{
    EC_HOST_CMD_STATUS stat;
    QByteArray payloadout;
    QByteArray packetout;

    PayloadToOutPack(ECCMD_GET_RESULT,payloadout,packetout);

    int i=0;
    while (i < 1000)
    {
        //Send the command to get the results
        stat = SendCmdOut(packetout, payloadin);
        if (stat == EC_HOST_CMD_SUCCESS)
        {
            log(QString("Results ready after %1ms").arg(i), Logger::Debug);
            return stat;
        }
        else if (stat != EC_HOST_CMD_IN_PROGRESS)
        {
            log(QString("Result fail response %1 at %2ms").arg(stat).arg(i), Logger::Warning);
            return stat;
        }

        if (i < 10){
            i++;
        }
        else if (i < 30){
            QThread::msleep(1);
            i++;
        }
        else
        {
            QThread::msleep(20);
            i+=20;
        }
    }

    log(QString("Results timeout after %1ms").arg(i), Logger::Warning);

    return EC_HOST_CMD_TIMEOUT;
}

EC_HOST_CMD_STATUS EmiThread::SendCmdOut(QByteArray &packetout, QByteArray &payloadin)
{
#if SIMULATE_HARDWARE || DISABLE_HW_ACCESS
    return EC_HOST_CMD_SUCCESS;
#else
    quint8 data;
    EC_HOST_CMD_STATUS resp;
    const struct ec_host_cmd_response_header* pHdr;

    // Wait for the emi interface to be open
    resp = WaitBusReady();
    if (resp != EC_HOST_CMD_SUCCESS)
    {
        return EC_HOST_CMD_BUS_ERROR;
    }

    // Send out the data
    SendPacketOut(packetout);

    // Tell the command to process
    m_pPort->Write(EC_HOST_IND, 1);  // Register is write 1 to clear
    m_pPort->Write(HOST_EC_IND, HOST2EC_CMD_PROC);

    // Wait for the response
    int waittime = -5;

    while (1)
    {
        m_pPort->Read(EC_HOST_IND, &data);

        if (data == EC2HOST_RESP_READY) {
            break;
        }

        if (waittime >= 5000)
        {
            log(QString("Send cmd timeout, EC_HOST=0x%1").arg(data, 2, 16, QChar('0')), Logger::Warning);
            resp = EC_HOST_CMD_TIMEOUT;

            // Reset the bus
            QThread::msleep(1000);
            m_pPort->Write(EC_HOST_IND, 1);
            goto done;
        }
        else if (waittime >= 10)
        {
            QThread::msleep(10);
            waittime += 10;
        }
        else if (waittime >= 0)
        {
            QThread::msleep(1);
            waittime++;
        }
        else waittime++;
    }

    if (waittime > 10)
    {
        log(QString("Slow EC response: %1ms").arg(waittime), Logger::Debug);
    }

    //Read the input data packet
    resp = GetPayloadIn(payloadin);

done:
    return resp;
#endif
}

EC_HOST_CMD_STATUS EmiThread::PayloadToOutPack(quint16 cmd, QByteArray &payloadout, QByteArray &packetout)
{
    //Check the size of the packet
    if ((payloadout.size() + sizeof(struct ec_host_cmd_request_header)) > EMI_BUF_MAX_SIZE)
    {
        log("Payload too big to send", Logger::Warning);
        return EC_HOST_CMD_INVALID_PARAM;
    }

    //Load int the header
    struct ec_host_cmd_request_header hdr;
    hdr.prtcl_ver = 3;
    hdr.checksum = 0;
    hdr.cmd_id = cmd;
    hdr.cmd_ver = 1;
    hdr.data_len = payloadout.size();
    hdr.reserved = 0;
    packetout.clear();
    packetout.append(reinterpret_cast<const char*>(&hdr),sizeof(struct ec_host_cmd_request_header));

    //Load the payload
    packetout.append(payloadout);

    //Calc the checksum
    uint8_t chk = 0;
    for (int i=0;i<packetout.size();i++)
    {
        chk += packetout.at(i);
    }

    //Add in the checksum
    packetout[1] = 0 - chk;

    return EC_HOST_CMD_SUCCESS;
}

EC_HOST_CMD_STATUS EmiThread::WaitBusReady()
{
    quint8 data;

    int retry = 0;

    /*The ec is designed to process cmds quickly. If the response takes a while it will queue it to
     * a thread and process it outside of the bus thread. The host will get a busy response from the command.
     * It then will read get results until it gets a valid response.
    */
    while (1)
    {
        m_pPort->Read(HOST_EC_IND,&data);
        if (data == HOST2EC_CMD_READY) break;

        //The first few checks we do quickly then slow it down
        if (retry > 4)
        {
            QThread::msleep(1);
        }

        if (retry > 10)
        {
            log(QString("Bus busy, HOST2EC=0x%1").arg(data, 2, 16, QChar('0')), Logger::Warning);
            return EC_HOST_CMD_BUS_ERROR;
        }

        retry++;
    }

    return EC_HOST_CMD_SUCCESS;
}

EC_HOST_CMD_STATUS EmiThread::GetPayloadIn(QByteArray &in)
{
    quint8 data;
    quint8 crc = 0;
    QByteArray packetin;
    EC_HOST_CMD_STATUS resp;
    const ec_host_cmd_response_header* pHdr;

    in.clear();

#if SIMULATE_HARDWARE
    return EC_HOST_CMD_INVALID_VERSION;
#else

    quint16 readbytes = 8;
    for (qint16 i=0;i<readbytes;i++)
    {
        switch (i % 4)
        {
        case 0:
            m_pPort->Write(ADD0_IND,i);
            m_pPort->Write(ADD1_IND,0);
            m_pPort->Read(DAT0_IND,&data);
            break;
        case 1:
            m_pPort->Read(DAT1_IND,&data);
            break;
        case 2:
            m_pPort->Read(DAT2_IND,&data);
            break;
        case 3:
            m_pPort->Read(DAT3_IND,&data);
            break;
        }

        crc += data;
        packetin.append(data);

        //We just received the header so we can check the byte size
        if (packetin.size() == 8)
        {
            pHdr = reinterpret_cast<const ec_host_cmd_response_header*>(packetin.constData());
            readbytes = sizeof(struct ec_host_cmd_request_header) + pHdr->data_len;

            //Validate the version
            if (pHdr->prtcl_ver != 3)
            {
#if SHOW_POLE_HW_ERR
                log(QString("Invalid protocol version %1").arg(pHdr->prtcl_ver), Logger::Warning);
#endif
                resp = EC_HOST_CMD_INVALID_VERSION;
                return resp;
            }

            //vallidate the read size
            if (readbytes > EMI_BUF_MAX_SIZE)
            {
                log(QString("Response too large: %1 bytes").arg(readbytes), Logger::Warning);
                resp = EC_HOST_CMD_RESPONSE_TOO_BIG;
                return resp;
            }
        }
    }

    //Validate the packet
    //Cast to the header
    pHdr = reinterpret_cast<const ec_host_cmd_response_header*>(packetin.constData());
    resp = (EC_HOST_CMD_STATUS) pHdr->result;

    if (crc)
    {
        log(QString("Packet CRC error: 0x%1").arg(crc, 2, 16, QChar('0')), Logger::Warning);
        resp = EC_HOST_CMD_INVALID_CHECKSUM;
    }

    //Remove the header
    in = packetin.mid(8);

    emit RxIn(packetin.size());

    return resp;
#endif
}

void EmiThread::SendPacketOut(QByteArray &packetOut)
{
    emit TxOut(packetOut.size());

    //Send the packet
    int cnt = 0;
    for (int i=0;i<packetOut.size();i++)
    {
        switch (cnt++)
        {
        case 0:
            m_pPort->Write(ADD0_IND,i);
            m_pPort->Write(ADD1_IND,0);
            m_pPort->Write(DAT0_IND,packetOut[i]);
            break;
        case 1:
            m_pPort->Write(DAT1_IND,packetOut[i]);
            break;
        case 2:
            m_pPort->Write(DAT2_IND,packetOut[i]);
            break;
        case 3:
            m_pPort->Write(DAT3_IND,packetOut[i]);
            break;
        }

        if (cnt > 3) cnt = 0;
    }
}
