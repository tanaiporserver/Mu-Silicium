/** @file
  Definitions for NP05J USB CDC-ACM Virtual Serial DXE Driver.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _NP05J_USB_SERIAL_DXE_H_
#define _NP05J_USB_SERIAL_DXE_H_

#include <Uefi.h>

#include <Guid/EventGroup.h>
#include <IndustryStandard/Usb.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/SerialIo.h>
#include <Protocol/UsbFunctionIo.h>

#include <Np05jLogRing.h>

//
// USB Identity (pid.codes open-source test VID/PID, standard CDC-ACM class)
//
#define NP05J_USB_VID                 0x1209U
#define NP05J_USB_PID                 0x0505U
#define NP05J_USB_BCD_DEVICE          0x0100U

//
// Endpoints
//
#define NP05J_EP_CONTROL              0U
#define NP05J_EP_BULK_INDEX           1U
#define NP05J_EP_BULK_OUT_ADDR        0x01U
#define NP05J_EP_BULK_IN_ADDR         0x81U
#define NP05J_EP_INT_INDEX            2U
#define NP05J_EP_INT_IN_ADDR          0x82U

//
// Additional USB 2.0/3.0 Standard Request & Descriptor Constants
//
#define NP05J_USB_REQ_SET_SEL         0x30U
#define NP05J_USB_REQ_SET_ISOCH_DELAY 0x31U

#define NP05J_USB_DESC_DEV_QUALIFIER  0x06U
#define NP05J_USB_DESC_OTHER_SPEED    0x07U
#define NP05J_USB_DESC_SS_EP_COMP     0x30U

#define NP05J_USB_DEV_CAP_USB20_EXT   0x02U
#define NP05J_USB_DEV_CAP_SS_USB      0x03U

//
// CDC-ACM Class Constants
//
#define USB_CLASS_CDC_COMM            0x02U
#define USB_SUBCLASS_CDC_ACM          0x02U
#define USB_PROTO_CDC_AT              0x01U
#define USB_CLASS_CDC_DATA            0x0AU
#define USB_CDC_SUBTYPE_HEADER        0x00U
#define USB_CDC_SUBTYPE_CALL_MGMT     0x01U
#define USB_CDC_SUBTYPE_ACM           0x02U
#define USB_CDC_SUBTYPE_UNION         0x06U

#define USB_CDC_REQ_SET_LINE_CODING         0x20U
#define USB_CDC_REQ_GET_LINE_CODING         0x21U
#define USB_CDC_REQ_SET_CONTROL_LINE_STATE  0x22U
#define USB_CDC_REQ_SEND_BREAK              0x23U

//
// Transfer & Ring Buffer Sizes
//
#define NP05J_EP0_BUF_SIZE            1024U
#define NP05J_BULK_RX_BUF_SIZE        1024U
#define NP05J_BULK_TX_BUF_SIZE        2048U
#define NP05J_RX_RING_SIZE            4096U
#define NP05J_TX_RING_SIZE            16384U
#define NP05J_POLL_INTERVAL_100NS     50000ULL // 5 ms

//
// Qualcomm UsbfnIo SuperSpeed Extension (Revision >= 0x00010002)
//
#define NP05J_USBFN_IO_REV_SS         0x00010002U

//
// Unique Vendor Hardware Device Path GUID for NP05J USB UART
//
#define NP05J_USB_SERIAL_DEVICE_GUID \
  { \
    0xA26B1F4C, 0x9E8D, 0x47A3, \
    { 0x91, 0x52, 0x3E, 0x7A, 0xC4, 0x0B, 0x8D, 0x61 } \
  }

#pragma pack(1)

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT16  BcdUSB;
  UINT8   DeviceClass;
  UINT8   DeviceSubClass;
  UINT8   DeviceProtocol;
  UINT8   MaxPacketSize0;
  UINT8   NumConfigurations;
  UINT8   Reserved;
} NP05J_USB_DEVICE_QUALIFIER_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DevCapabilityType;
  UINT32  Attributes;
} NP05J_USB_20_EXT_CAP_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DevCapabilityType;
  UINT8   Attributes;
  UINT16  SpeedsSupported;
  UINT8   FunctionalitySupport;
  UINT8   U1DevExitLat;
  UINT16  U2DevExitLat;
} NP05J_USB_SS_CAP_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   MaxBurst;
  UINT8   Attributes;
  UINT16  BytesPerInterval;
} NP05J_USB_SS_EP_COMP_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DescriptorSubType;
  UINT16  BcdCdc;
} USB_CDC_HEADER_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DescriptorSubType;
  UINT8   Capabilities;
  UINT8   DataInterface;
} USB_CDC_CALL_MGMT_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DescriptorSubType;
  UINT8   Capabilities;
} USB_CDC_ACM_DESC;

typedef struct {
  UINT8   Length;
  UINT8   DescriptorType;
  UINT8   DescriptorSubType;
  UINT8   ControlInterface;
  UINT8   SubordinateInterface0;
} USB_CDC_UNION_DESC;

typedef struct {
  UINT32  DteRate;
  UINT8   CharFormat;
  UINT8   ParityType;
  UINT8   DataBits;
} USB_CDC_LINE_CODING;

typedef struct {
  EFI_USB_CONFIG_DESCRIPTOR     Config;
  EFI_USB_INTERFACE_DESCRIPTOR  CommIfc;
  USB_CDC_HEADER_DESC           CdcHeader;
  USB_CDC_CALL_MGMT_DESC        CdcCallMgmt;
  USB_CDC_ACM_DESC              CdcAcm;
  USB_CDC_UNION_DESC            CdcUnion;
  EFI_USB_ENDPOINT_DESCRIPTOR   IntInEp;
  EFI_USB_INTERFACE_DESCRIPTOR  DataIfc;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkOutEp;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkInEp;
} NP05J_CDC_HS_CONFIG_BUNDLE;

typedef struct {
  EFI_USB_CONFIG_DESCRIPTOR     Config;
  EFI_USB_INTERFACE_DESCRIPTOR  CommIfc;
  USB_CDC_HEADER_DESC           CdcHeader;
  USB_CDC_CALL_MGMT_DESC        CdcCallMgmt;
  USB_CDC_ACM_DESC              CdcAcm;
  USB_CDC_UNION_DESC            CdcUnion;
  EFI_USB_ENDPOINT_DESCRIPTOR   IntInEp;
  NP05J_USB_SS_EP_COMP_DESC     IntInComp;
  EFI_USB_INTERFACE_DESCRIPTOR  DataIfc;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkOutEp;
  NP05J_USB_SS_EP_COMP_DESC     BulkOutComp;
  EFI_USB_ENDPOINT_DESCRIPTOR   BulkInEp;
  NP05J_USB_SS_EP_COMP_DESC     BulkInComp;
} NP05J_CDC_SS_CONFIG_BUNDLE;

typedef struct {
  USB_BOS_DESCRIPTOR         Bos;
  NP05J_USB_20_EXT_CAP_DESC  Usb20Ext;
  NP05J_USB_SS_CAP_DESC      SsCap;
} NP05J_USB_BOS_BUNDLE;

typedef struct {
  VENDOR_DEVICE_PATH        VendorNode;
  UART_DEVICE_PATH          UartNode;
  EFI_DEVICE_PATH_PROTOCOL  End;
} NP05J_SERIAL_DEVICE_PATH;

typedef struct {
  VENDOR_DEVICE_PATH        VendorNode;
  UART_DEVICE_PATH          UartNode;
  VENDOR_DEVICE_PATH        TerminalNode;
  EFI_DEVICE_PATH_PROTOCOL  End;
} NP05J_TERMINAL_DEVICE_PATH;

#pragma pack()

//
// SuperSpeed DeviceInfo structures matching Qualcomm UsbfnDwc3Dxe ABI
//
typedef struct {
  EFI_USB_ENDPOINT_DESCRIPTOR  *EndpointDescriptor;
  NP05J_USB_SS_EP_COMP_DESC    *EndpointCompanionDescriptor;
} NP05J_USB_SS_ENDPOINT_DESC;

typedef struct {
  EFI_USB_INTERFACE_DESCRIPTOR  *InterfaceDescriptor;
  NP05J_USB_SS_ENDPOINT_DESC    **EndpointDescriptorTable;
} NP05J_USB_SS_INTERFACE_INFO;

typedef struct {
  EFI_USB_CONFIG_DESCRIPTOR    *ConfigDescriptor;
  NP05J_USB_SS_INTERFACE_INFO  **InterfaceInfoTable;
} NP05J_USB_SS_CONFIG_INFO;

typedef struct {
  EFI_USB_DEVICE_DESCRIPTOR  *DeviceDescriptor;
  NP05J_USB_SS_CONFIG_INFO   **ConfigInfoTable;
  USB_BOS_DESCRIPTOR         *BosDescriptor;
} NP05J_USB_SS_DEVICE_INFO;

typedef
EFI_STATUS
(EFIAPI *NP05J_USBFN_IO_CONFIGURE_ENABLE_ENDPOINTS_EX)(
  IN EFI_USBFN_IO_PROTOCOL     *This,
  IN EFI_USB_DEVICE_INFO       *DeviceInfo,
  IN NP05J_USB_SS_DEVICE_INFO  *SSDeviceInfo
  );

typedef struct {
  EFI_USBFN_IO_PROTOCOL                         Base;
  NP05J_USBFN_IO_CONFIGURE_ENABLE_ENDPOINTS_EX  ConfigureEnableEndpointsEx;
} NP05J_USBFN_IO_PROTOCOL_EX;

STATIC_ASSERT (
  OFFSET_OF (NP05J_USBFN_IO_PROTOCOL_EX, ConfigureEnableEndpointsEx) == 0x90,
  "ConfigureEnableEndpointsEx ABI offset mismatch with UsbfnDwc3Dxe"
  );

typedef enum {
  Ep0StageIdle = 0,
  Ep0StageDataIn,
  Ep0StageDataOut,
  Ep0StageStatusIn,
  Ep0StageStatusOut
} NP05J_EP0_STAGE;

typedef struct {
  //
  // Persistent UEFI handles / events
  //
  EFI_HANDLE              ImageHandle;
  EFI_HANDLE              SerialHandle;
  EFI_HANDLE              UsbfnControllerHandle;
  EFI_USBFN_IO_PROTOCOL   *UsbfnIo;
  EFI_EVENT               PollTimer;
  EFI_EVENT               DeferredConnectTimer;
  EFI_EVENT               UsbfnNotifyEvent;
  VOID                    *UsbfnNotifyReg;
  EFI_EVENT               EndOfDxeEvent;
  EFI_EVENT               ExitBootServicesEvent;

  //
  // State flags
  //
  BOOLEAN                 InPoll;
  BOOLEAN                 ExitedBootServices;
  BOOLEAN                 Attached;
  BOOLEAN                 Configured;
  BOOLEAN                 Dtr;
  BOOLEAN                 Rts;
  BOOLEAN                 HostReady;
  UINT8                   ConfigValue;
  EFI_USB_BUS_SPEED       BusSpeed;
  UINT16                  BulkMaxPacket;

  //
  // EP0 transfer state
  //
  NP05J_EP0_STAGE         Ep0Stage;
  EFI_USB_DEVICE_REQUEST  Ep0PendingReq;
  VOID                    *Ep0Buf;

  //
  // Bulk transfer state
  //
  VOID                    *BulkRxBuf;
  VOID                    *BulkTxBuf;
  BOOLEAN                 BulkRxPending;
  BOOLEAN                 BulkTxPending;

  //
  // CDC-ACM Line Coding
  //
  USB_CDC_LINE_CODING     LineCoding;

  //
  // Console RX / TX rings (UEFI SerialIo)
  //
  UINT8                   RxRing[NP05J_RX_RING_SIZE];
  UINT32                  RxHead;
  UINT32                  RxTail;
  UINT32                  RxCount;

  UINT8                   TxRing[NP05J_TX_RING_SIZE];
  UINT32                  TxHead;
  UINT32                  TxTail;
  UINT32                  TxCount;
  UINT64                  TxDropped;

  //
  // Shared RAM boot log ring reader cursor
  //
  NP05J_LOG_RING_HEADER   *LogRingHeader;
  UINT8                   *LogRingData;
  UINT64                  LogReadPos;
  UINT8                   LogLastChar;

  //
  // EFI_SERIAL_IO_PROTOCOL instance
  //
  EFI_SERIAL_IO_MODE      SerialMode;
  EFI_SERIAL_IO_PROTOCOL  SerialIo;
} NP05J_USB_SERIAL_DEV;

#endif // _NP05J_USB_SERIAL_DXE_H_
