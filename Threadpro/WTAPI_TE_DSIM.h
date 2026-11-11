/******************** (C) COPYRIGHT 2026 WATERTEK ******************************
* File Name          : WTAPI_TE_DSIM.h
* Author             :
* Version            : V1.0.0
* Date               : 05/12/2026
* Description        : This file contains all the functions prototypes for the
*                      WTAPI_TE_DSIM library.
*******************************************************************************/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef WTAPI_TE_DSIM
#define WTAPI_TE_DSIM
#include <QtCore/qglobal.h>
#include <stdio.h>
#include <Windows.h>
#include <QCoreApplication>
#include <QVariant>
#include <QMetaType>

#if defined(FC_API_LIBRARY)
#  define FC_APISHARED_EXPORT Q_DECL_EXPORT
#else
#  define FC_APISHARED_EXPORT typedef
#endif

typedef unsigned char UINT8;
typedef unsigned char UCHAR;

typedef signed int INT32;
typedef unsigned int UINT32;
typedef unsigned short UINT16;

typedef unsigned __int64  UINT64;
typedef __int64  INT64;

#pragma pack(4)
typedef struct _WTAPI_TE_DSIM_DEV_VERSION
{
    unsigned int uiAPIVer       : 32;               //API version API版本号
    unsigned int uiFPGAVer      : 32;               //FPGA version FPGA版本号
}WTAPI_TE_DSIM_DEV_VERSION, *PWTAPI_TE_DSIM_DEV_VERSION;//DSIM FC version struct DSIM FC版本结构体

typedef struct
{
    unsigned int uiSHTime       : 16;               //Send time high 16bit(100ns) 发送时标高16位(精度:100纳秒)
    unsigned int uiFrameNum     : 16;               //Send frame number 发送帧序号
    unsigned int uiSLTime       : 32;               //Send time low 32bit(100ns) 发送时标低32位(精度:100纳秒)
    unsigned int uiLength       : 12;               //Send length max:535*4B 发送数据长度最大 535*4字节
    unsigned int uiReserve      : 20;               //Reserve 保留
    unsigned int puiData[0];                        //Data buffer 发送数据缓冲区
}WTAPI_TE_DSIM_SEND_HEAD, *PWTAPI_TE_DSIM_SEND_HEAD;//DSIM FC send header DSIM FC发送帧头

typedef struct
{
    unsigned int uiSHTime       : 16;               //Send time high 16bit(100ms) 发送时标高16位(精度:100毫秒)
    unsigned int uiFrameNum     : 16;               //Send frame number 发送帧序号
    unsigned int uiSLTime       : 32;               //Send time low 32bit(100ms) 发送时标低32位(精度:100毫秒)
}WTAPI_TE_DSIM_SEND_TIME, *PWTAPI_TE_DSIM_SEND_TIME;//DSIM FC real send time DSIM FC真实发送时标
Q_DECLARE_METATYPE(PWTAPI_TE_DSIM_SEND_TIME)

typedef struct
{
    unsigned int D_ID           : 24;               //Destination address identifiers
    unsigned int R_CTL          : 8;                //Routing control
    unsigned int S_ID           : 24;               //Source address identifiers
    unsigned int CS_CTL         : 8;                //Class specific control
    unsigned int F_CTL          : 24;               //Frame control
    unsigned int TYPE           : 8;                //Data structure type
    unsigned int SEQ_CNT        : 16;               //Sequence count
    unsigned int DF_CTL         : 8;                //Data field control
    unsigned int SEQ_ID         : 8;                //Sequence ID
    unsigned int RX_ID          : 16;               //Responder exchange ID
    unsigned int OX_ID          : 16;               //Originator exchange ID
    unsigned int Parameter      : 32;               //Parameter
//ASMHeader
    unsigned int MSG_ID         : 32;               //Message ID
    unsigned int RES0           : 32;               //Reserve 0
    unsigned int RES1           : 32;               //Reserve 1
    unsigned int MSG_LEN        : 24;               //Message length
    unsigned int MSG_PRIORITY   : 7;                //Message priority
    unsigned int MSG_L          : 1;                //Message L
}WTAPI_TE_DSIM_FCHEAD, *PWTAPI_TE_DSIM_FCHEAD;//FC header FC帧头
#pragma pack()


/* Includes ------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

/* Exported macro ------------------------------------------------------------*/
#pragma warning(disable:4200)

//DSIM card type DSIM板卡类型
#define WTAPI_PCIE_TE_DSIM                  0x6E
//DSIM max channel DSIM卡最大通道数
#define WTAPI_TE_DSIM_MAX_CHANNEL           2
//API Version API版本号
#define WTAPI_TE_DSIM_API_VER_1_0_0         0x2605120A

/* Exported types ------------------------------------------------------------*/


/* Error define --------------------------------------------------------------*/
// Error enum 错误类型定义

//错误返回值
typedef enum
{
    TE_DSIM_SUCCESS = 0,                            //SUCCESS 成功
    TE_DSIM_PARAM_ERROR = 0x80000001,               //param error 参数错误
    TE_DSIM_MALLOC_DMA_ERROR,                       //Malloc dma buffer error 申请DMA空间错误
    TE_DSIM_MAX_BUFFER_ERROR,                       //Send buffer max error 发送缓冲区超过512帧错误
    TE_DSIM_SEND_EMPTY_ERROR,                       //Get send time empty error 获取发送时标空错误
    TE_DSIM_RECV_EMPTY_ERROR,                       //Get recv data empty error 获取接收数据空错误
    TE_DSIM_NO_DEVICE                               //Find device error 查找设备错误
}WTAPI_TE_DSIM_ERROR_TYPE;


/* Exported constants --------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */
///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_FindDevice
//  DESCRIPTION:Open the device 打开设备
//  INPUT:
//      phDevice:Device handle pointer
//      cardType:WTAPI_PCIE_TE_DSIM
//      Instance:Card instance [1..8]
//  OUTPUT:phDevice:Device handle pointer
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR/TE_DSIM_NO_DEVICE
///////////////////////////////
    FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_FindDevice)(HANDLE *phDevice, unsigned int cardType, unsigned int Instance);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_CloseDevice
//  DESCRIPTION:Close device 关闭板卡
//  INPUT:
//      hDevice:Device handle
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
    FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_CloseDevice)(HANDLE hDevice);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_Init
//  DESCRIPTION:Initialize the device 初始化板卡
//  INPUT:
//      hDevice:Device handle
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR/TE_DSIM_MALLOC_DMA_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_Init)(HANDLE hDevice);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_SoftReset
//  DESCRIPTION:Reset FPGA 软件复位FPGA
//  INPUT:
//      hDevice:Device handle
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_SoftReset)(HANDLE hDevice);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_GetVersion
//  DESCRIPTION:Get DSIM FPGA and API version 获取FPGA、API版本信息
//  INPUT:
//      hHandle:Device handle 设备句柄
//    pVersion:WTAPI_TE_DSIM_DEV_VERSION struct pointer 版本结构体指针
//  OUTPUT:pVersion:WTAPI_TE_DSIM_DEV_VERSION struct
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_GetVersion)(HANDLE hHandle, PWTAPI_TE_DSIM_DEV_VERSION pVersion);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_SETRate
//  DESCRIPTION:Set FC rate 设置FC线速率
//  INPUT:
//      hDevice:Device handle
//		uRate:FC rate 1:1G  2:2G  4:4G  Default:4G
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_SETRate)(HANDLE hDevice, unsigned int uRate);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_SyncMasterSlaveMode
//  DESCRIPTION:Set DSIM FC SYNC master or slave mode 设置DSIM FC同步为主模式或从模式
//  INPUT:
//      hDevice:Device handle
//		uMode:DSIM FC SYNC master or slave mode 0:Slave  1:Master  Default:0(Slave)
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_SyncMasterSlaveMode)(HANDLE hDevice, unsigned int uMode);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_LinkStatus
//  DESCRIPTION:DSIM FC link status DSIM FC链接状态
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      puiLink:Link status pointer 0:Disconnect 1:Link 链接状态指针 0:断开 1:链接
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_LinkStatus)(HANDLE hHandle, unsigned int uiChannel, unsigned int *puiLink);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_SendControl
//  DESCRIPTION:DSIM FC send control DSIM FC发送控制
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      uiControl:Send control 0:Disable 1:Enable 发送控制使能或关闭
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_SendControl)(HANDLE hHandle, unsigned int uiChannel, unsigned int uiControl);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_RecvControl
//  DESCRIPTION:DSIM FC receive control DSIM FC接收控制
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      uiControl:Receive control 0:Disable 1:Enable 接收控制使能或关闭
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_RecvControl)(HANDLE hHandle, unsigned int uiChannel, unsigned int uiControl);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_SendData
//  DESCRIPTION:DSIM FC send data DSIM FC发送数据
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0:1] 通道号
//      pSENDData:WTAPI_TE_DSIM_SEND_HEAD struct pointer WTAPI_TE_DSIM_SEND_HEAD结构体指针
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR/TE_DSIM_MAX_BUFFER_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_SendData)(HANDLE hHandle, unsigned int uiChannel, PWTAPI_TE_DSIM_SEND_HEAD pSENDData);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_GetSendTime
//  DESCRIPTION:Get DSIM FC frame send time 获取DSIM FC帧发送时间
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      pSENDTime:WTAPI_TE_DSIM_SEND_TIME struct pointer WTAPI_TE_DSIM_SEND_TIME结构体指针
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR/TE_DSIM_SEND_EMPTY_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_GetSendTime)(HANDLE hHandle, unsigned int uiChannel, PWTAPI_TE_DSIM_SEND_TIME pSENDTime);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_RecvData
//  DESCRIPTION:DSIM FC receive data DSIM FC接收数据
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      puiLength:Data length bytes pointer MAX:2148B 数据长度字节缓冲区(最大2148B)
//      pucData:Data buffer pointer 数据缓冲区
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR/TE_DSIM_RECV_EMPTY_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_RecvData)(HANDLE hHandle, unsigned int uiChannel, unsigned int *puiLength, unsigned char *pucData);


///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_OnlineReq
//  DESCRIPTION:DSIM FC online request DSIM FC上网请求
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_OnlineReq)(HANDLE hHandle, unsigned int uiChannel);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_LoginReq
//  DESCRIPTION:DSIM FC login request DSIM FC登录请求
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      uiMode:Mode is 0:A or 1:C  default:1 模式为A或C 0为A 1为C  默认为1
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_LoginReq)(HANDLE hHandle, unsigned int uiChannel, unsigned int uiMode);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_BBCredit
//  DESCRIPTION:DSIM FC BB credit value DSIM FC BBCredit值
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      uiValue:BBCredit value default:6 BBCredit值 默认为6
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_BBCredit)(HANDLE hHandle, unsigned int uiChannel, unsigned int uiValue);


///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_I2CRead
//  DESCRIPTION:DSIM FC I2C read DSIM FC I2C读
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      usAddr:I2C read address I2C读取数据地址
//      pucData:I2C read data I2C读取数据
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_I2CRead)(HANDLE hHandle, unsigned int uiChannel, unsigned short usAddr, unsigned char *pucData);

///////////////////////////////
//  FUNCTION:WTAPI_TE_DSIM_I2CWrite
//  DESCRIPTION:DSIM FC I2C write DSIM FC I2C写
//  INPUT:
//      hHandle:Device handle 设备句柄
//      uiChannel:Channel number [0..1] 通道号
//      usAddr:I2C write address I2C写入数据地址
//      ucData:I2C write data I2C写入数据
//  OUTPUT:NULL
//  RETURN:TE_DSIM_SUCCESS or TE_DSIM_PARAM_ERROR
///////////////////////////////
FC_APISHARED_EXPORT INT32 (__stdcall *lpWTAPI_TE_DSIM_I2CWrite)(HANDLE hHandle, unsigned int uiChannel, unsigned short usAddr, unsigned char ucData);

#ifdef __cplusplus
}
#endif

#endif //WTAPI_TE_DSIM

/******************** (C) COPYRIGHT 2026 WATERTEK *****************************/
