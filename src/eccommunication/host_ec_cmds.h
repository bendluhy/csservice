#ifndef HOST_EC_CMDS_H
#define HOST_EC_CMDS_H

#include <QSharedPointer>
#include <QByteArray>
#include <QtGlobal>

//!!! THIS MUST MATCH WITH THE EC VALUES !!!

#define __packed

#define EMI_BUF_MAX_SIZE        256

static_assert(true); // dummy declaration to fix clang bug
#pragma pack(push, 1)

enum EC_HOST_CMD_STATUS {
    /** Host command was successful. */
    EC_HOST_CMD_SUCCESS = 0,
    /** The specified command id is not recognized or supported. */
    EC_HOST_CMD_INVALID_COMMAND = 1,
    /** Generic Error. */
    EC_HOST_CMD_ERROR = 2,
    /** One of more of the input request parameters is invalid. */
    EC_HOST_CMD_INVALID_PARAM = 3,
    /** Host command is not permitted. */
    EC_HOST_CMD_ACCESS_DENIED = 4,
    /** Response was invalid (e.g. not version 3 of header). */
    EC_HOST_CMD_INVALID_RESPONSE = 5,
    /** Host command id version unsupported. */
    EC_HOST_CMD_INVALID_VERSION = 6,
    /** Checksum did not match. */
    EC_HOST_CMD_INVALID_CHECKSUM = 7,
    /** A host command is currently being processed. */
    EC_HOST_CMD_IN_PROGRESS = 8,
    /** Requested information is currently unavailable. */
    EC_HOST_CMD_UNAVAILABLE = 9,
    /** Timeout during processing. */
    EC_HOST_CMD_TIMEOUT = 10,
    /** Data or table overflow. */
    EC_HOST_CMD_OVERFLOW = 11,
    /** Header is invalid or unsupported (e.g. not version 3 of header). */
    EC_HOST_CMD_INVALID_HEADER = 12,
    /** Did not receive all expected request data. */
    EC_HOST_CMD_REQUEST_TRUNCATED = 13,
    /** Response was too big to send within one response packet. */
    EC_HOST_CMD_RESPONSE_TOO_BIG = 14,
    /** Error on underlying communication bus. */
    EC_HOST_CMD_BUS_ERROR = 15,
    /** System busy. Should retry later. */
    EC_HOST_CMD_BUSY = 16,
    /** Header version invalid. */
    EC_HOST_CMD_INVALID_HEADER_VERSION = 17,
    /** Header CRC invalid. */
    EC_HOST_CMD_INVALID_HEADER_CRC = 18,
    /** Data CRC invalid. */
    EC_HOST_CMD_INVALID_DATA_CRC = 19,
    /** Can't resend response. */
    EC_HOST_CMD_DUP_UNAVAILABLE = 20,

    EC_HOST_CMD_MAX = UINT16_MAX /* Force enum to be 16 bits. */
} __packed;

struct ec_host_cmd_request_header {
    uint8_t prtcl_ver;
    uint8_t checksum;
    uint16_t cmd_id;
    uint8_t cmd_ver;
    uint8_t reserved;
    uint16_t data_len;
} __packed;

struct ec_host_cmd_response_header {
    uint8_t prtcl_ver;
    uint8_t checksum;
    uint16_t result;
    uint16_t data_len;
    uint16_t reserved;
} __packed;

// EC Bootloader structures
struct dfu_slot
{
    quint16 slottype;
    quint16 slot;
}__packed;

struct dfu_slot_info
{
    uint8_t verMaj;
    uint8_t verMin;
    uint16_t rev;
    uint32_t buildNum;
    uint32_t slotSize;
    uint32_t slotBase;
    uint32_t imageSize;
    char time[9];
    char date[12];
}__packed;

struct dfu_info
{
    uint8_t app_slot_cnt;
    uint8_t boot_slot_cnt;
    uint8_t app_run_slot;
    uint8_t boot_run_slot;
    uint32_t app_slot_size;
    uint32_t boot_slot_size;
}__packed;

#define SLOT_TYPE_APP   1
#define SLOT_TYPE_BOOT  0
struct dfu_new_slot
{
    uint32_t size;                  //Size of the whole image
    uint32_t crc;                   //CRC for the whole image
    uint8_t slot;                   //Slot we are saving it to
    uint8_t slottype;               //1 for app, 0 for bootload
}__packed;

// Memory Structures
struct mem_region_info
{
    uint32_t start;
    uint32_t size;
    uint32_t sector_size;
}__packed;

struct mem_region_w
{
    uint32_t start;
    uint32_t size;
    uint8_t data[];
}__packed;

struct mem_region_r_e
{
    uint32_t start;
    uint32_t size;
}__packed;

struct peci_wr_pkg
{
    uint8_t hostid;
    uint8_t index;
    uint8_t parmL;
    uint8_t parmH;
    uint32_t data;
}__packed;

struct peci_rd_pkg
{
    uint8_t hostid;
    uint8_t index;
    uint8_t parmL;
    uint8_t parmH;
}__packed;

struct peci_rd_pkg_resp
{
    uint32_t data;
}__packed;

struct smbus_cmd
{
    uint8_t bus;					//Bus (0=DSW,1=A1,2=DOCK,3=S1,4=RES)
    uint8_t prot;					//Use acpi defined smbus protocol number
    uint8_t add;
    uint8_t cmd;
    uint8_t cnt;
    uint8_t data[32];
}__packed;

struct dock_eedata_cmd {
    uint8_t version;
    uint8_t size;
    uint8_t ckksum;
    uint8_t loadtstat;
    uint8_t antswitch;
    uint8_t vshutdown;
    uint8_t vcritical;
    uint8_t docktime;
    uint8_t gpiopwrendef;
    uint8_t gpiopwrs0;
    uint8_t gpiopwrs3;
    uint8_t gpiopwrs5;
    uint8_t gpiodir;
    uint8_t shutgpio;
    uint8_t fancfg;
    uint8_t hdmiGain;
}__packed;

struct bat_health
{
    uint8_t struct_ver;
    uint8_t HealthStat;
    uint8_t Status1;
    uint8_t Faults;
    uint16_t cell1_V;
    uint16_t cell2_V;
    uint16_t cell3_V;
    uint16_t cellDiff;
    int16_t RaIncPer_1;
    int16_t RaDecPer_1;
    int16_t RaIncPer_2;
    int16_t RaDecPer_2;
    int16_t RaIncPer_3;
    int16_t RaDecPer_3;
    uint32_t TimeRest;
    uint32_t TimeTempBad;
    uint32_t TimeRun;
    uint32_t safetyAlert;
    uint32_t safetyStatus;
    uint32_t pfalert;
    uint32_t pfstatus;
    uint16_t DischgLim;
    uint16_t ChgLim;
    uint8_t SOH;
} __packed;

#define MAX_SHELL_CMD_SIZE  100
struct shell_cmd
{
    uint8_t size;
    char str[MAX_SHELL_CMD_SIZE];
}__packed;


#pragma pack(pop)

//EMI_1 arbitration
#define HOST2EC_CMD_CONSOLE_HALT    0x00    //Clears the buffer to the host
#define HOST2EC_CMD_CONSOLE_RUN     0x01    //Allows the buffer to be loader

#define EC2HOST_CMD_BUFFER_EMPTY    0x00
#define EC2HOST_CMD_BUFFER_READY    0x01


//EMI_0 arbitraction
#define HOST2EC_CMD_READY           0x00
#define HOST2EC_CMD_PROC            0x01

#define EC2HOST_RESP_NONE           0x00
#define EC2HOST_RESP_READY          0x01

//Not supported commands
#define ECCMD_NONE                  0x0000

//IO PORT Access
#define ECCMD_GET_STATUS            0x0000  //Get the status of the port
#define ECCMD_GET_RESULT            0x0001  //Gets data from any result that were not processed immediately
#define ECCMD_RESET                 0x0002  //Reset the port from lockups

//EC Memory Operations +
#define ECCMD_ECMEM_INFO            0x0010
#define ECCMD_ECMEM_READ            0x0011
#define ECCMD_ECRAM_INFO            0x0012
#define ECCMD_ECRAM_READ            0x0013

//Main Flash Operations
#define ECCMD_BT_FLASH_INFO         0x0020
#define ECCMD_BT_FLASH_READ         0x0021
#define ECCMD_BT_FLASH_WRITE        0x0022
#define ECCMD_BT_FLASH_ERASE        0x0023

//Private Flash Operations +
#define ECCMD_PVT_FLASH_INFO        0x0030
#define ECCMD_PVT_FLASH_READ        0x0031
#define ECCMD_PVT_FLASH_WRITE       0x0032
#define ECCMD_PVT_FLASH_ERASE       0x0033

//Internal EE +
#define ECCMD_IEE_INFO              0x0040
#define ECCMD_IEE_READ              0x0041
#define ECCMD_IEE_WRITE             0x0042

//External EE
#define ECCMD_XEE_FLASH_INFO        0x0050
#define ECCMD_XEE_FLASH_READ        0x0051
#define ECCMD_XEE_FLASH_WRITE       0x0052

//Internal BBRAM +
#define ECCMD_BRAM_FLASH_INFO       0x0060
#define ECCMD_BRAM_FLASH_READ       0x0061
#define ECCMD_BRAM_FLASH_WRITE      0x0062

//Peci +
#define ECCMD_PECI_INFO             0x0070
#define ECCMD_PECI_RD_PKG           0x0071
#define ECCMD_PECI_WR_PKG           0x0072

//Smbus
#define ECCMD_SMBUS_INFO            0x0080
#define ECCMD_SMBUS_PROC            0x0081

//Acpi0 +
#define ECCMD_ACPI0_INFO            0x0090
#define ECCMD_ACPI0_READ            0x0091
#define ECCMD_ACPI0_WRITE           0x0092
#define ECCMD_ACPI0_READ_CHANGED    0x0093
#define ECCMD_ACPI0_READ_EVENTS     0x0094

//Acpi1 +
#define ECCMD_ACPI1_INFO            0x00A0
#define ECCMD_ACPI1_READ            0x00A1
#define ECCMD_ACPI1_WRITE           0x00A2

//Acpi Queue
#define ECCMD_ACPI_QUEUE_WRITE      0x00B1
#define ECCMD_ACPI_QUEUE_READ       0x00B2

//Bezel DFU
#define ECCMD_BEZ_DFU_WRITE         0x00C0
#define ECCMD_BEZ_DFU_READ          0x00C1

//Image Update
#define ECCMD_DFU_INFO              0x00D0
#define ECCMD_DFU_SLOT_INFO         0x00D1
#define ECCMD_DFU_OPEN_SLOT         0x00D2
#define ECCMD_DFU_ERASE             0x00D3
#define ECCMD_DFU_READ              0x00D4
#define ECCMD_DFU_WRITE             0x00D5
#define ECCMD_DFU_CRC               0x00D6
#define ECCMD_DFU_SET_NEW_IMAGE     0x00D7

//EC Console Debugger
#define ECCMD_SHELL_CMD             0x00E0

//IO Port Host Routine
#define ECCMD_IOPORT_READ           0xF000
#define ECCMD_IOPORT_WRITE          0xF001

//MEM Host Routine
#define ECCMD_MEM_READ              0xF010
#define ECCMD_MEM_WRITE             0xF011

//Dock
#define ECCMD_DOCK_GET_EE           0x00F0
#define ECCMD_DOCK_SET_EE           0x00F1

//Battery
#define ECCMD_BAT_SET_INFO          0x0100
#define ECCMD_BAT_GET_INFO          0x0101
#define ECCMD_BAT_GET_HEALTH        0x0102
#define ECCMD_BAT_SET_DATAFLASH     0x0103
#define ECCMD_BAT_GET_DATAFLASH     0x0104
#define ECCMD_BAT_GET_RA_TABLE      0x0105

class EmiCmdParam
{
public:
    virtual ~EmiCmdParam(){}
};

class EmiCmdReadParam : public EmiCmdParam
{
public:
    quint32 startAdd;
    quint32 totalSize;
    quint32 currentAdd;
    quint32 currentSize;
};

class EmiDfuCmd : public EmiCmdParam
{
public:
    quint16 Cmd;
    quint16 Row;
    int datpos;
};

class EmiCmdGenericParam : public EmiCmdParam
{
public:
    quint32 quint32;
};

#define RESP_VAR_SIZE(x)   (x | 0x8000)
#define RESP_VAR_SIZE_MASK(x) (x & 0x7FFF)

class EmiCmd {
public:
    EmiCmd(){};
    ~EmiCmd(){
        if (pParam) delete pParam;
    };
    quint32 packetid;
    quint16 result;
    quint16 cmd;
    quint16 reqrespsize;
    QByteArray payloadout;
    QByteArray payloadin;
    int waittime;
    std::function<void(QSharedPointer<EmiCmd>)> FuncDone;
    EmiCmdParam* pParam = NULL;
};

#endif // HOST_EC_CMDS_H


