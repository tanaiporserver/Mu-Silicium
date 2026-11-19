/** @file
  NP05J USB CDC-ACM Virtual Serial (UART over USB) DXE Driver.

  Exposes a standard USB CDC-ACM serial port (/dev/ttyACM0 / COMx) over
  Qualcomm UsbfnDwc3Dxe (EFI_USBFN_IO_PROTOCOL) providing:
    - Complete SEC/DXE/vendor low-level debug log streaming (from Np05jLogRing)
    - Bidirectional UEFI Console / Shell TTY (EFI_SERIAL_IO_PROTOCOL + TerminalDxe)

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Np05jUsbSerialDxe.h"

#define NP05J_BUS_SPEED_SUPER_PLUS  5U

STATIC_ASSERT (sizeof (NP05J_CDC_HS_CONFIG_BUNDLE) == 67, "HS CDC config bundle size mismatch");
STATIC_ASSERT (sizeof (NP05J_CDC_SS_CONFIG_BUNDLE) == 85, "SS CDC config bundle size mismatch");
STATIC_ASSERT (sizeof (NP05J_USB_BOS_BUNDLE) == 22, "BOS bundle size mismatch");

//
// Global singleton device context (persists across USB attach/detach cycles)
//
STATIC NP05J_USB_SERIAL_DEV  mDev;

//
// Persistent SerialIo Device Path (parent for TerminalDxe)
//
STATIC NP05J_SERIAL_DEVICE_PATH  mSerialDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8)
      }
    },
    NP05J_USB_SERIAL_DEVICE_GUID
  },
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_UART_DP,
      {
        (UINT8)(sizeof (UART_DEVICE_PATH)),
        (UINT8)(sizeof (UART_DEVICE_PATH) >> 8)
      }
    },
    0,
    115200,
    8,
    NoParity,
    OneStopBit
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)(END_DEVICE_PATH_LENGTH >> 8)
    }
  }
};

//
// Full Console Device Path registered into ConIn / ConOut / ErrOut (VT-UTF8)
//
STATIC NP05J_TERMINAL_DEVICE_PATH  mTerminalDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8)
      }
    },
    NP05J_USB_SERIAL_DEVICE_GUID
  },
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_UART_DP,
      {
        (UINT8)(sizeof (UART_DEVICE_PATH)),
        (UINT8)(sizeof (UART_DEVICE_PATH) >> 8)
      }
    },
    0,
    115200,
    8,
    NoParity,
    OneStopBit
  },
  {
    {
      MESSAGING_DEVICE_PATH,
      MSG_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8)
      }
    },
    EFI_VT_UTF8_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)(END_DEVICE_PATH_LENGTH >> 8)
    }
  }
};

//
// USB Descriptors (HS/FS + SS + BOS + Qualifier + Strings)
//
STATIC EFI_USB_DEVICE_DESCRIPTOR  mHsDeviceDesc = {
  sizeof (EFI_USB_DEVICE_DESCRIPTOR),
  USB_DESC_TYPE_DEVICE,
  0x0210,                 // USB 2.1 (BOS capable)
  USB_CLASS_CDC_COMM,     // 0x02 Communications (CDC)
  0x00,
  0x00,
  64,
  NP05J_USB_VID,
  NP05J_USB_PID,
  NP05J_USB_BCD_DEVICE,
  1,                      // iManufacturer
  2,                      // iProduct
  3,                      // iSerialNumber
  1                       // bNumConfigurations
};

STATIC EFI_USB_DEVICE_DESCRIPTOR  mSsDeviceDesc = {
  sizeof (EFI_USB_DEVICE_DESCRIPTOR),
  USB_DESC_TYPE_DEVICE,
  0x0300,                 // USB 3.0 (upgraded to 0x0310 on SSP)
  USB_CLASS_CDC_COMM,     // Must match mHsDeviceDesc
  0x00,
  0x00,
  9,                      // 2^9 = 512 bytes for SuperSpeed EP0
  NP05J_USB_VID,
  NP05J_USB_PID,
  NP05J_USB_BCD_DEVICE,
  1,
  2,
  3,
  1
};

STATIC CONST NP05J_USB_DEVICE_QUALIFIER_DESC  mDeviceQualifierDesc = {
  sizeof (NP05J_USB_DEVICE_QUALIFIER_DESC),
  NP05J_USB_DESC_DEV_QUALIFIER,
  0x0200,
  USB_CLASS_CDC_COMM,
  0x00,
  0x00,
  64,
  1,
  0
};

STATIC NP05J_USB_BOS_BUNDLE  mBosBundle = {
  {
    sizeof (USB_BOS_DESCRIPTOR),
    USB_DESC_TYPE_BOS,
    sizeof (NP05J_USB_BOS_BUNDLE),
    2
  },
  {
    sizeof (NP05J_USB_20_EXT_CAP_DESC),
    USB_DESC_TYPE_DEV_CAP,
    NP05J_USB_DEV_CAP_USB20_EXT,
    0x00000002            // LPM capable
  },
  {
    sizeof (NP05J_USB_SS_CAP_DESC),
    USB_DESC_TYPE_DEV_CAP,
    NP05J_USB_DEV_CAP_SS_USB,
    0x00,
    0x000E,               // FS | HS | SS supported
    0x01,                 // Lowest full-function speed: Full Speed
    0x0A,                 // U1 exit latency: 10 us
    0x07FF                // U2 exit latency: 2047 us
  }
};

STATIC NP05J_CDC_HS_CONFIG_BUNDLE  mHsConfigBundle = {
  {
    sizeof (EFI_USB_CONFIG_DESCRIPTOR),
    USB_DESC_TYPE_CONFIG,
    sizeof (NP05J_CDC_HS_CONFIG_BUNDLE),
    2,                    // bNumInterfaces (Comm + Data)
    1,                    // bConfigurationValue
    0,
    0xC0,                 // Self-powered
    50                    // 100 mA
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    0,                    // Interface 0: CDC Control
    0,
    1,                    // 1 Interrupt IN endpoint
    USB_CLASS_CDC_COMM,
    USB_SUBCLASS_CDC_ACM,
    USB_PROTO_CDC_AT,
    0
  },
  {
    sizeof (USB_CDC_HEADER_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_HEADER,
    0x0110                // CDC 1.10
  },
  {
    sizeof (USB_CDC_CALL_MGMT_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_CALL_MGMT,
    0x00,
    1                     // Data interface = 1
  },
  {
    sizeof (USB_CDC_ACM_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_ACM,
    0x06                  // Line coding + control line state + send break
  },
  {
    sizeof (USB_CDC_UNION_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_UNION,
    0,                    // Master: Interface 0
    1                     // Subordinate: Interface 1
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_INT_IN_ADDR,
    USB_ENDPOINT_INTERRUPT,
    64,
    9                     // 32 ms at HS
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    1,                    // Interface 1: CDC Data
    0,
    2,                    // Bulk OUT + Bulk IN
    USB_CLASS_CDC_DATA,
    0x00,
    0x00,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_OUT_ADDR,
    USB_ENDPOINT_BULK,
    512,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_IN_ADDR,
    USB_ENDPOINT_BULK,
    512,
    0
  }
};

STATIC NP05J_CDC_HS_CONFIG_BUNDLE  mFsConfigBundle = {
  {
    sizeof (EFI_USB_CONFIG_DESCRIPTOR),
    USB_DESC_TYPE_CONFIG,
    sizeof (NP05J_CDC_HS_CONFIG_BUNDLE),
    2,
    1,
    0,
    0xC0,
    50
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    0,
    0,
    1,
    USB_CLASS_CDC_COMM,
    USB_SUBCLASS_CDC_ACM,
    USB_PROTO_CDC_AT,
    0
  },
  {
    sizeof (USB_CDC_HEADER_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_HEADER,
    0x0110
  },
  {
    sizeof (USB_CDC_CALL_MGMT_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_CALL_MGMT,
    0x00,
    1
  },
  {
    sizeof (USB_CDC_ACM_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_ACM,
    0x06
  },
  {
    sizeof (USB_CDC_UNION_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_UNION,
    0,
    1
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_INT_IN_ADDR,
    USB_ENDPOINT_INTERRUPT,
    64,
    32                    // 32 ms at FS
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    1,
    0,
    2,
    USB_CLASS_CDC_DATA,
    0x00,
    0x00,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_OUT_ADDR,
    USB_ENDPOINT_BULK,
    64,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_IN_ADDR,
    USB_ENDPOINT_BULK,
    64,
    0
  }
};

STATIC NP05J_CDC_SS_CONFIG_BUNDLE  mSsConfigBundle = {
  {
    sizeof (EFI_USB_CONFIG_DESCRIPTOR),
    USB_DESC_TYPE_CONFIG,
    sizeof (NP05J_CDC_SS_CONFIG_BUNDLE),
    2,
    1,
    0,
    0xC0,
    12                    // 96 mA (in 8 mA units for SS)
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    0,
    0,
    1,
    USB_CLASS_CDC_COMM,
    USB_SUBCLASS_CDC_ACM,
    USB_PROTO_CDC_AT,
    0
  },
  {
    sizeof (USB_CDC_HEADER_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_HEADER,
    0x0110
  },
  {
    sizeof (USB_CDC_CALL_MGMT_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_CALL_MGMT,
    0x00,
    1
  },
  {
    sizeof (USB_CDC_ACM_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_ACM,
    0x06
  },
  {
    sizeof (USB_CDC_UNION_DESC),
    USB_DESC_TYPE_CS_INTERFACE,
    USB_CDC_SUBTYPE_UNION,
    0,
    1
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_INT_IN_ADDR,
    USB_ENDPOINT_INTERRUPT,
    64,
    9
  },
  {
    sizeof (NP05J_USB_SS_EP_COMP_DESC),
    NP05J_USB_DESC_SS_EP_COMP,
    0,
    0,
    64
  },
  {
    sizeof (EFI_USB_INTERFACE_DESCRIPTOR),
    USB_DESC_TYPE_INTERFACE,
    1,
    0,
    2,
    USB_CLASS_CDC_DATA,
    0x00,
    0x00,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_OUT_ADDR,
    USB_ENDPOINT_BULK,
    1024,
    0
  },
  {
    sizeof (NP05J_USB_SS_EP_COMP_DESC),
    NP05J_USB_DESC_SS_EP_COMP,
    0,
    0,
    0
  },
  {
    sizeof (EFI_USB_ENDPOINT_DESCRIPTOR),
    USB_DESC_TYPE_ENDPOINT,
    NP05J_EP_BULK_IN_ADDR,
    USB_ENDPOINT_BULK,
    1024,
    0
  },
  {
    sizeof (NP05J_USB_SS_EP_COMP_DESC),
    NP05J_USB_DESC_SS_EP_COMP,
    0,
    0,
    0
  }
};

//
// Pointer trees passed to UsbfnIo->ConfigureEnableEndpointsEx()
//
STATIC EFI_USB_ENDPOINT_DESCRIPTOR  *mHsCommEpTable[] = {
  &mHsConfigBundle.IntInEp
};

STATIC EFI_USB_ENDPOINT_DESCRIPTOR  *mHsDataEpTable[] = {
  &mHsConfigBundle.BulkOutEp,
  &mHsConfigBundle.BulkInEp
};

STATIC EFI_USB_INTERFACE_INFO  mHsCommIfcInfo = {
  &mHsConfigBundle.CommIfc,
  mHsCommEpTable
};

STATIC EFI_USB_INTERFACE_INFO  mHsDataIfcInfo = {
  &mHsConfigBundle.DataIfc,
  mHsDataEpTable
};

STATIC EFI_USB_INTERFACE_INFO  *mHsIfcInfoTable[] = {
  &mHsCommIfcInfo,
  &mHsDataIfcInfo
};

STATIC EFI_USB_CONFIG_INFO  mHsConfigInfo = {
  &mHsConfigBundle.Config,
  mHsIfcInfoTable
};

STATIC EFI_USB_CONFIG_INFO  *mHsConfigInfoTable[] = {
  &mHsConfigInfo
};

STATIC EFI_USB_DEVICE_INFO  mHsDeviceInfo = {
  &mHsDeviceDesc,
  mHsConfigInfoTable
};

// SuperSpeed tables
STATIC NP05J_USB_SS_ENDPOINT_DESC  mSsIntInEpDesc = {
  &mSsConfigBundle.IntInEp,
  &mSsConfigBundle.IntInComp
};

STATIC NP05J_USB_SS_ENDPOINT_DESC  mSsBulkOutEpDesc = {
  &mSsConfigBundle.BulkOutEp,
  &mSsConfigBundle.BulkOutComp
};

STATIC NP05J_USB_SS_ENDPOINT_DESC  mSsBulkInEpDesc = {
  &mSsConfigBundle.BulkInEp,
  &mSsConfigBundle.BulkInComp
};

STATIC NP05J_USB_SS_ENDPOINT_DESC  *mSsCommEpTable[] = {
  &mSsIntInEpDesc
};

STATIC NP05J_USB_SS_ENDPOINT_DESC  *mSsDataEpTable[] = {
  &mSsBulkOutEpDesc,
  &mSsBulkInEpDesc
};

STATIC NP05J_USB_SS_INTERFACE_INFO  mSsCommIfcInfo = {
  &mSsConfigBundle.CommIfc,
  mSsCommEpTable
};

STATIC NP05J_USB_SS_INTERFACE_INFO  mSsDataIfcInfo = {
  &mSsConfigBundle.DataIfc,
  mSsDataEpTable
};

STATIC NP05J_USB_SS_INTERFACE_INFO  *mSsIfcInfoTable[] = {
  &mSsCommIfcInfo,
  &mSsDataIfcInfo
};

STATIC NP05J_USB_SS_CONFIG_INFO  mSsConfigInfo = {
  &mSsConfigBundle.Config,
  mSsIfcInfoTable
};

STATIC NP05J_USB_SS_CONFIG_INFO  *mSsConfigInfoTable[] = {
  &mSsConfigInfo
};

STATIC NP05J_USB_SS_DEVICE_INFO  mSsDeviceInfo = {
  &mSsDeviceDesc,
  mSsConfigInfoTable,
  &mBosBundle.Bos
};

//
// USB String Descriptors
//
STATIC CONST UINT8  mStrDescLangId[] = {
  4, USB_DESC_TYPE_STRING,
  0x09, 0x04              // English (United States)
};

STATIC CONST CHAR16  *mUsbStrings[] = {
  NULL,                   // Index 0 handled by mStrDescLangId
  L"Project Silicium",
  L"NP05J UEFI USB UART",
  L"NP05J-SM8750-UART0"
};

//
// Forward declarations
//
STATIC
VOID
Np05jUsbSerialPoll (
  IN NP05J_USB_SERIAL_DEV  *Dev
  );

STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

STATIC EFI_DRIVER_BINDING_PROTOCOL  mDriverBinding = {
  Np05jUsbSerialBindingSupported,
  Np05jUsbSerialBindingStart,
  Np05jUsbSerialBindingStop,
  0x10,
  NULL,
  NULL
};

//
// Helper: locate shared RAM boot log ring buffer in Display_Reserved tail
//
STATIC
VOID
LocateLogRing (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  EFI_MEMORY_REGION_DESCRIPTOR  FbRegion;
  EFI_PHYSICAL_ADDRESS          RingAddr;

  if (Dev->LogRingHeader != NULL) {
    return;
  }

  ZeroMem (&FbRegion, sizeof (FbRegion));
  LocateMemoryRegionByName ("Display Reserved", &FbRegion);
  LocateMemoryRegionByName ("Display_Reserved", &FbRegion);
  if ((FbRegion.Address == 0) || (FbRegion.Length <= NP05J_LOG_RING_TAIL_OFFSET)) {
    return;
  }

  RingAddr           = FbRegion.Address + FbRegion.Length - NP05J_LOG_RING_TAIL_OFFSET;
  Dev->LogRingHeader = (NP05J_LOG_RING_HEADER *)(UINTN)RingAddr;
  Dev->LogRingData   = (UINT8 *)(UINTN)(RingAddr + NP05J_LOG_RING_HEADER_SIZE);
}

STATIC
BOOLEAN
IsLogRingValid (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  if (Dev->LogRingHeader == NULL) {
    LocateLogRing (Dev);
  }

  if (Dev->LogRingHeader == NULL) {
    return FALSE;
  }

  return ((Dev->LogRingHeader->Magic == NP05J_LOG_RING_MAGIC) &&
          (Dev->LogRingHeader->Version == NP05J_LOG_RING_VERSION) &&
          (Dev->LogRingHeader->Capacity == NP05J_LOG_RING_CAPACITY));
}

STATIC
VOID
RewindLogRingToOldest (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  UINT64  WritePos;

  if (!IsLogRingValid (Dev)) {
    return;
  }

  WritePos         = Dev->LogRingHeader->WritePos;
  Dev->LogReadPos  = (WritePos > NP05J_LOG_RING_CAPACITY) ? (WritePos - NP05J_LOG_RING_CAPACITY) : 0;
  Dev->LogLastChar = 0;
}

//
// Helper: ring buffer push/pop (callers hold TPL_NOTIFY lock)
//
STATIC
VOID
RxRingPushBytes (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN CONST UINT8           *Data,
  IN UINTN                 Len
  )
{
  EFI_TPL  OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  for (UINTN Index = 0; Index < Len; Index++) {
    if (Dev->RxCount >= NP05J_RX_RING_SIZE) {
      // Drop oldest byte to make room for newest keystroke
      Dev->RxTail = (Dev->RxTail + 1U) % NP05J_RX_RING_SIZE;
      Dev->RxCount--;
    }

    Dev->RxRing[Dev->RxHead] = Data[Index];
    Dev->RxHead              = (Dev->RxHead + 1U) % NP05J_RX_RING_SIZE;
    Dev->RxCount++;
  }

  gBS->RestoreTPL (OldTpl);
}

STATIC
UINTN
RxRingPopBytes (
  IN  NP05J_USB_SERIAL_DEV  *Dev,
  OUT UINT8                 *OutBuf,
  IN  UINTN                 MaxLen
  )
{
  EFI_TPL  OldTpl;
  UINTN    Copied;

  Copied = 0;
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  while ((Copied < MaxLen) && (Dev->RxCount > 0)) {
    OutBuf[Copied++] = Dev->RxRing[Dev->RxTail];
    Dev->RxTail      = (Dev->RxTail + 1U) % NP05J_RX_RING_SIZE;
    Dev->RxCount--;
  }

  gBS->RestoreTPL (OldTpl);
  return Copied;
}

STATIC
VOID
TxRingPushBytes (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN CONST UINT8           *Data,
  IN UINTN                 Len
  )
{
  EFI_TPL  OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  for (UINTN Index = 0; Index < Len; Index++) {
    if (Dev->TxCount >= NP05J_TX_RING_SIZE) {
      Dev->TxTail = (Dev->TxTail + 1U) % NP05J_TX_RING_SIZE;
      Dev->TxCount--;
      Dev->TxDropped++;
    }

    Dev->TxRing[Dev->TxHead] = Data[Index];
    Dev->TxHead              = (Dev->TxHead + 1U) % NP05J_TX_RING_SIZE;
    Dev->TxCount++;
  }

  gBS->RestoreTPL (OldTpl);
}

STATIC
VOID
TxRingPushAscii (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN CONST CHAR8           *Str
  )
{
  UINTN  Len;

  if (Str == NULL) {
    return;
  }

  Len = AsciiStrLen (Str);
  if (Len > 0) {
    TxRingPushBytes (Dev, (CONST UINT8 *)Str, Len);
  }
}

STATIC
UINTN
TxRingPopBytes (
  IN  NP05J_USB_SERIAL_DEV  *Dev,
  OUT UINT8                 *OutBuf,
  IN  UINTN                 MaxLen
  )
{
  EFI_TPL  OldTpl;
  UINTN    Copied;

  Copied = 0;
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  while ((Copied < MaxLen) && (Dev->TxCount > 0)) {
    OutBuf[Copied++] = Dev->TxRing[Dev->TxTail];
    Dev->TxTail      = (Dev->TxTail + 1U) % NP05J_TX_RING_SIZE;
    Dev->TxCount--;
  }

  gBS->RestoreTPL (OldTpl);
  return Copied;
}

STATIC
CONST CHAR8 *
BusSpeedName (
  IN EFI_USB_BUS_SPEED  Speed
  )
{
  switch ((UINT32)Speed) {
    case UsbBusSpeedLow:
      return "LS";
    case UsbBusSpeedFull:
      return "FS";
    case UsbBusSpeedHigh:
      return "HS";
    case UsbBusSpeedSuper:
      return "SS";
    case NP05J_BUS_SPEED_SUPER_PLUS:
      return "SSP";
    default:
      return "UNK";
  }
}

STATIC
VOID
OnHostConnected (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN BOOLEAN               IsBreakReplay
  )
{
  CHAR8   Banner[192];
  UINT64  LogBytes;

  LogBytes = IsLogRingValid (Dev) ? Dev->LogRingHeader->WritePos : 0;

  AsciiSPrint (
    Banner,
    sizeof (Banner),
    "\r\n=== NP05J UEFI USB-UART (%a, %a) | CapturedBootLog=%lu bytes | Break=ReplayLog ===\r\n",
    BusSpeedName (Dev->BusSpeed),
    IsBreakReplay ? "Replay" : "Connected",
    LogBytes
    );
  TxRingPushAscii (Dev, Banner);

  RewindLogRingToOldest (Dev);
}

//
// Helper: EP0 Transfer submission
//
STATIC
EFI_STATUS
Ep0SubmitStatusIn (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  UINTN       Size;
  EFI_STATUS  Status;

  Size          = 0;
  Dev->Ep0Stage = Ep0StageStatusIn;
  Status        = Dev->UsbfnIo->Transfer (
                                  Dev->UsbfnIo,
                                  NP05J_EP_CONTROL,
                                  EfiUsbEndpointDirectionDeviceTx,
                                  &Size,
                                  Dev->Ep0Buf
                                  );
  if (EFI_ERROR (Status)) {
    Dev->Ep0Stage = Ep0StageIdle;
  }

  return Status;
}

STATIC
EFI_STATUS
Ep0SubmitStatusOut (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  UINTN       Size;
  EFI_STATUS  Status;

  Size          = 0;
  Dev->Ep0Stage = Ep0StageStatusOut;
  Status        = Dev->UsbfnIo->Transfer (
                                  Dev->UsbfnIo,
                                  NP05J_EP_CONTROL,
                                  EfiUsbEndpointDirectionDeviceRx,
                                  &Size,
                                  Dev->Ep0Buf
                                  );
  if (EFI_ERROR (Status)) {
    Dev->Ep0Stage = Ep0StageIdle;
  }

  return Status;
}

STATIC
EFI_STATUS
Ep0SubmitDataIn (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN CONST VOID            *Data,
  IN UINTN                 DataLen,
  IN UINT16                HostMaxLen
  )
{
  UINTN       SendLen;
  EFI_STATUS  Status;

  SendLen = (DataLen < HostMaxLen) ? DataLen : HostMaxLen;
  if (SendLen > NP05J_EP0_BUF_SIZE) {
    SendLen = NP05J_EP0_BUF_SIZE;
  }

  if (SendLen == 0) {
    return Ep0SubmitStatusIn (Dev);
  }

  CopyMem (Dev->Ep0Buf, Data, SendLen);
  Dev->Ep0Stage = Ep0StageDataIn;
  Status        = Dev->UsbfnIo->Transfer (
                                  Dev->UsbfnIo,
                                  NP05J_EP_CONTROL,
                                  EfiUsbEndpointDirectionDeviceTx,
                                  &SendLen,
                                  Dev->Ep0Buf
                                  );
  if (EFI_ERROR (Status)) {
    Dev->Ep0Stage = Ep0StageIdle;
  }

  return Status;
}

STATIC
EFI_STATUS
Ep0SubmitDataOut (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN UINT16                Len
  )
{
  UINTN       RxLen;
  EFI_STATUS  Status;

  if ((Len == 0) || (Len > NP05J_EP0_BUF_SIZE)) {
    return EFI_INVALID_PARAMETER;
  }

  RxLen         = Len;
  Dev->Ep0Stage = Ep0StageDataOut;
  Status        = Dev->UsbfnIo->Transfer (
                                  Dev->UsbfnIo,
                                  NP05J_EP_CONTROL,
                                  EfiUsbEndpointDirectionDeviceRx,
                                  &RxLen,
                                  Dev->Ep0Buf
                                  );
  if (EFI_ERROR (Status)) {
    Dev->Ep0Stage = Ep0StageIdle;
  }

  return Status;
}

typedef
EFI_STATUS
(EFIAPI *NP05J_USBFN_SET_STALL_BY_VALUE)(
  IN EFI_USBFN_IO_PROTOCOL         *This,
  IN UINT8                         EndpointIndex,
  IN EFI_USBFN_ENDPOINT_DIRECTION  Direction,
  IN BOOLEAN                       State
  );

STATIC
EFI_STATUS
Np05jSetEndpointStallState (
  IN EFI_USBFN_IO_PROTOCOL         *UsbfnIo,
  IN UINT8                         EndpointIndex,
  IN EFI_USBFN_ENDPOINT_DIRECTION  Direction,
  IN BOOLEAN                       State
  )
{
  //
  // MdePkg UsbFunctionIo.h has a historic header typo (BOOLEAN *State), whereas
  // Qualcomm UsbfnDwc3Dxe (and EFIUsbfnIo.h) take BOOLEAN State by value in w3.
  //
  NP05J_USBFN_SET_STALL_BY_VALUE  SetStall;

  SetStall = (NP05J_USBFN_SET_STALL_BY_VALUE)(VOID *)UsbfnIo->SetEndpointStallState;
  return SetStall (UsbfnIo, EndpointIndex, Direction, State);
}

STATIC
VOID
Ep0Stall (
  IN NP05J_USB_SERIAL_DEV          *Dev,
  IN CONST EFI_USB_DEVICE_REQUEST  *Req
  )
{
  EFI_USBFN_ENDPOINT_DIRECTION  Dir;

  Dir           = ((Req->RequestType & 0x80U) != 0) ? EfiUsbEndpointDirectionDeviceTx : EfiUsbEndpointDirectionDeviceRx;
  Dev->Ep0Stage = Ep0StageIdle;
  Np05jSetEndpointStallState (Dev->UsbfnIo, NP05J_EP_CONTROL, Dir, TRUE);
}

//
// Helper: Bulk OUT RX & Bulk IN TX
//
STATIC
VOID
ArmBulkRx (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  UINTN       RxLen;
  EFI_STATUS  Status;

  if ((Dev->UsbfnIo == NULL) || !Dev->Configured || Dev->BulkRxPending || (Dev->BulkRxBuf == NULL)) {
    return;
  }

  // DWC3 OUT endpoint TRB length must be an integer multiple of wMaxPacketSize
  RxLen = Dev->BulkMaxPacket;
  if ((RxLen == 0) || (RxLen > NP05J_BULK_RX_BUF_SIZE)) {
    RxLen = 512;
  }

  Status = Dev->UsbfnIo->Transfer (
                           Dev->UsbfnIo,
                           NP05J_EP_BULK_INDEX,
                           EfiUsbEndpointDirectionDeviceRx,
                           &RxLen,
                           Dev->BulkRxBuf
                           );
  if (!EFI_ERROR (Status)) {
    Dev->BulkRxPending = TRUE;
  }
}

STATIC
UINTN
FillChunkFromLogRing (
  IN  NP05J_USB_SERIAL_DEV  *Dev,
  OUT UINT8                 *Dst,
  IN  UINTN                 MaxLen
  )
{
  UINT64  WritePos;
  UINT64  Oldest;
  UINTN   OutLen;
  UINT32  Mask;
  UINT8   Ch;

  if (!IsLogRingValid (Dev) || (MaxLen < 2)) {
    return 0;
  }

  WritePos = Dev->LogRingHeader->WritePos;
  if (WritePos > NP05J_LOG_RING_CAPACITY) {
    Oldest = WritePos - NP05J_LOG_RING_CAPACITY;
    if (Dev->LogReadPos < Oldest) {
      Dev->LogReadPos = Oldest;
    }
  }

  Mask   = NP05J_LOG_RING_CAPACITY - 1U;
  OutLen = 0;

  while ((Dev->LogReadPos < WritePos) && ((OutLen + 2) <= MaxLen)) {
    Ch = Dev->LogRingData[(UINTN)(Dev->LogReadPos & Mask)];
    Dev->LogReadPos++;

    // Convert bare '\n' to '\r\n' so terminals stay aligned at column 0
    if ((Ch == '\n') && (Dev->LogLastChar != '\r')) {
      Dst[OutLen++] = '\r';
    }

    Dst[OutLen++]    = Ch;
    Dev->LogLastChar = Ch;
  }

  return OutLen;
}

STATIC
VOID
PumpBulkTx (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  UINTN       MaxChunk;
  UINTN       ChunkLen;
  EFI_STATUS  Status;

  if ((Dev->UsbfnIo == NULL) || !Dev->Configured || !Dev->HostReady || Dev->BulkTxPending || (Dev->BulkTxBuf == NULL)) {
    return;
  }

  //
  // Keep every Bulk IN transfer strictly shorter than wMaxPacketSize (short packet)
  // so Linux cdc_acm, macOS, and Windows usbser.sys flush every URB immediately
  // without requiring separate zero-length packets.
  //
  MaxChunk = (Dev->BulkMaxPacket > 1U) ? (UINTN)(Dev->BulkMaxPacket - 1U) : 63U;
  if (MaxChunk > NP05J_BULK_TX_BUF_SIZE) {
    MaxChunk = NP05J_BULK_TX_BUF_SIZE;
  }

  // Prioritize interactive console / banner output in TxRing, then drain shared boot log ring
  ChunkLen = TxRingPopBytes (Dev, (UINT8 *)Dev->BulkTxBuf, MaxChunk);
  if (ChunkLen < MaxChunk) {
    ChunkLen += FillChunkFromLogRing (
                  Dev,
                  ((UINT8 *)Dev->BulkTxBuf) + ChunkLen,
                  MaxChunk - ChunkLen
                  );
  }

  if (ChunkLen == 0) {
    return;
  }

  Status = Dev->UsbfnIo->Transfer (
                           Dev->UsbfnIo,
                           NP05J_EP_BULK_INDEX,
                           EfiUsbEndpointDirectionDeviceTx,
                           &ChunkLen,
                           Dev->BulkTxBuf
                           );
  if (!EFI_ERROR (Status)) {
    Dev->BulkTxPending = TRUE;
  }
}

//
// EP0 Standard & CDC-ACM Class Request Handlers
//
STATIC
VOID
HandleGetDescriptor (
  IN NP05J_USB_SERIAL_DEV          *Dev,
  IN CONST EFI_USB_DEVICE_REQUEST  *Req
  )
{
  UINT8                        DescType;
  UINT8                        DescIndex;
  BOOLEAN                      IsSuperSpeed;
  NP05J_CDC_HS_CONFIG_BUNDLE   OtherCfg;
  UINT8                        StrBuf[128];
  CONST CHAR16                 *Str;
  UINTN                        StrChars;
  UINTN                        TotalBytes;

  DescType     = (UINT8)(Req->Value >> 8);
  DescIndex    = (UINT8)(Req->Value & 0xFFU);
  IsSuperSpeed = ((UINT32)Dev->BusSpeed >= (UINT32)UsbBusSpeedSuper);

  switch (DescType) {
    case USB_DESC_TYPE_DEVICE:
      if (IsSuperSpeed) {
        mSsDeviceDesc.BcdUSB = ((UINT32)Dev->BusSpeed == NP05J_BUS_SPEED_SUPER_PLUS) ? 0x0310 : 0x0300;
        Ep0SubmitDataIn (Dev, &mSsDeviceDesc, sizeof (mSsDeviceDesc), Req->Length);
      } else {
        Ep0SubmitDataIn (Dev, &mHsDeviceDesc, sizeof (mHsDeviceDesc), Req->Length);
      }

      return;

    case USB_DESC_TYPE_CONFIG:
      if (DescIndex != 0) {
        Ep0Stall (Dev, Req);
        return;
      }

      if (IsSuperSpeed) {
        Ep0SubmitDataIn (Dev, &mSsConfigBundle, sizeof (mSsConfigBundle), Req->Length);
      } else if (Dev->BusSpeed == UsbBusSpeedFull) {
        Ep0SubmitDataIn (Dev, &mFsConfigBundle, sizeof (mFsConfigBundle), Req->Length);
      } else {
        Ep0SubmitDataIn (Dev, &mHsConfigBundle, sizeof (mHsConfigBundle), Req->Length);
      }

      return;

    case USB_DESC_TYPE_STRING:
      if (DescIndex == 0) {
        Ep0SubmitDataIn (Dev, mStrDescLangId, sizeof (mStrDescLangId), Req->Length);
        return;
      }

      if (DescIndex >= ARRAY_SIZE (mUsbStrings)) {
        Ep0Stall (Dev, Req);
        return;
      }

      Str        = mUsbStrings[DescIndex];
      StrChars   = StrLen (Str);
      TotalBytes = 2U + (StrChars * 2U);
      if (TotalBytes > sizeof (StrBuf)) {
        TotalBytes = sizeof (StrBuf);
        StrChars   = (TotalBytes - 2U) / 2U;
      }

      StrBuf[0] = (UINT8)TotalBytes;
      StrBuf[1] = USB_DESC_TYPE_STRING;
      CopyMem (&StrBuf[2], Str, StrChars * 2U);
      Ep0SubmitDataIn (Dev, StrBuf, TotalBytes, Req->Length);
      return;

    case NP05J_USB_DESC_DEV_QUALIFIER:
      if (IsSuperSpeed) {
        Ep0Stall (Dev, Req);
      } else {
        Ep0SubmitDataIn (Dev, &mDeviceQualifierDesc, sizeof (mDeviceQualifierDesc), Req->Length);
      }

      return;

    case NP05J_USB_DESC_OTHER_SPEED:
      if (IsSuperSpeed || (DescIndex != 0)) {
        Ep0Stall (Dev, Req);
        return;
      }

      if (Dev->BusSpeed == UsbBusSpeedFull) {
        CopyMem (&OtherCfg, &mHsConfigBundle, sizeof (OtherCfg));
      } else {
        CopyMem (&OtherCfg, &mFsConfigBundle, sizeof (OtherCfg));
      }

      OtherCfg.Config.DescriptorType = NP05J_USB_DESC_OTHER_SPEED;
      Ep0SubmitDataIn (Dev, &OtherCfg, sizeof (OtherCfg), Req->Length);
      return;

    case USB_DESC_TYPE_BOS:
      Ep0SubmitDataIn (Dev, &mBosBundle, sizeof (mBosBundle), Req->Length);
      return;

    default:
      Ep0Stall (Dev, Req);
      return;
  }
}

STATIC
VOID
HandleStandardSetup (
  IN NP05J_USB_SERIAL_DEV          *Dev,
  IN CONST EFI_USB_DEVICE_REQUEST  *Req
  )
{
  UINT8                         Recipient;
  UINT16                        StatusWord;
  BOOLEAN                       Stalled;
  UINT8                         EpIdx;
  EFI_USBFN_ENDPOINT_DIRECTION  EpDir;
  UINT8                         ByteVal;

  Recipient = Req->RequestType & 0x1FU;

  switch (Req->Request) {
    case USB_REQ_GET_STATUS:
      StatusWord = 0;
      if (Recipient == USB_TARGET_DEVICE) {
        StatusWord = 0x0001; // Self-powered
      } else if (Recipient == USB_TARGET_ENDPOINT) {
        EpIdx   = (UINT8)(Req->Index & 0x0FU);
        EpDir   = ((Req->Index & 0x80U) != 0) ? EfiUsbEndpointDirectionDeviceTx : EfiUsbEndpointDirectionDeviceRx;
        Stalled = FALSE;
        if (!EFI_ERROR (Dev->UsbfnIo->GetEndpointStallState (Dev->UsbfnIo, EpIdx, EpDir, &Stalled)) && Stalled) {
          StatusWord = 0x0001;
        }
      }

      Ep0SubmitDataIn (Dev, &StatusWord, sizeof (StatusWord), Req->Length);
      return;

    case USB_REQ_CLEAR_FEATURE:
      if ((Recipient == USB_TARGET_ENDPOINT) && (Req->Value == 0)) {
        EpIdx = (UINT8)(Req->Index & 0x0FU);
        EpDir = ((Req->Index & 0x80U) != 0) ? EfiUsbEndpointDirectionDeviceTx : EfiUsbEndpointDirectionDeviceRx;
        Np05jSetEndpointStallState (Dev->UsbfnIo, EpIdx, EpDir, FALSE);
      }

      Ep0SubmitStatusIn (Dev);
      return;

    case USB_REQ_SET_FEATURE:
      if ((Recipient == USB_TARGET_ENDPOINT) && (Req->Value == 0)) {
        EpIdx = (UINT8)(Req->Index & 0x0FU);
        EpDir = ((Req->Index & 0x80U) != 0) ? EfiUsbEndpointDirectionDeviceTx : EfiUsbEndpointDirectionDeviceRx;
        Np05jSetEndpointStallState (Dev->UsbfnIo, EpIdx, EpDir, TRUE);
      }

      Ep0SubmitStatusIn (Dev);
      return;

    case USB_REQ_SET_ADDRESS:
      // Address register is programmed by UsbfnDwc3Dxe; complete status stage
      Ep0SubmitStatusIn (Dev);
      return;

    case USB_REQ_GET_DESCRIPTOR:
      HandleGetDescriptor (Dev, Req);
      return;

    case USB_REQ_GET_CONFIG:
      ByteVal = Dev->ConfigValue;
      Ep0SubmitDataIn (Dev, &ByteVal, 1, Req->Length);
      return;

    case USB_REQ_SET_CONFIG:
      if ((Req->Value & 0xFFU) == 1U) {
        Dev->ConfigValue   = 1;
        Dev->Configured    = TRUE;
        Dev->BulkRxPending = FALSE;
        Dev->BulkTxPending = FALSE;
        Ep0SubmitStatusIn (Dev);
        ArmBulkRx (Dev);
      } else if ((Req->Value & 0xFFU) == 0U) {
        Dev->ConfigValue   = 0;
        Dev->Configured    = FALSE;
        Dev->Dtr           = FALSE;
        Dev->Rts           = FALSE;
        Dev->HostReady     = FALSE;
        Dev->BulkRxPending = FALSE;
        Dev->BulkTxPending = FALSE;
        Ep0SubmitStatusIn (Dev);
      } else {
        Ep0Stall (Dev, Req);
      }

      return;

    case USB_REQ_GET_INTERFACE:
      ByteVal = 0;
      Ep0SubmitDataIn (Dev, &ByteVal, 1, Req->Length);
      return;

    case USB_REQ_SET_INTERFACE:
      if (Req->Value == 0) {
        Ep0SubmitStatusIn (Dev);
      } else {
        Ep0Stall (Dev, Req);
      }

      return;

    case NP05J_USB_REQ_SET_SEL:
      if (Req->Length > 0) {
        if (EFI_ERROR (Ep0SubmitDataOut (Dev, Req->Length))) {
          Ep0Stall (Dev, Req);
        }
      } else {
        Ep0SubmitStatusIn (Dev);
      }

      return;

    case NP05J_USB_REQ_SET_ISOCH_DELAY:
      Ep0SubmitStatusIn (Dev);
      return;

    default:
      Ep0Stall (Dev, Req);
      return;
  }
}

STATIC
VOID
HandleClassSetup (
  IN NP05J_USB_SERIAL_DEV          *Dev,
  IN CONST EFI_USB_DEVICE_REQUEST  *Req
  )
{
  BOOLEAN  NewDtr;

  switch (Req->Request) {
    case USB_CDC_REQ_SET_LINE_CODING:
      if ((Req->Length == sizeof (USB_CDC_LINE_CODING)) && ((Req->RequestType & 0x80U) == 0)) {
        if (EFI_ERROR (Ep0SubmitDataOut (Dev, Req->Length))) {
          Ep0Stall (Dev, Req);
        }
      } else {
        Ep0Stall (Dev, Req);
      }

      return;

    case USB_CDC_REQ_GET_LINE_CODING:
      if (((Req->RequestType & 0x80U) != 0) && (Req->Length >= sizeof (USB_CDC_LINE_CODING))) {
        Ep0SubmitDataIn (Dev, &Dev->LineCoding, sizeof (Dev->LineCoding), Req->Length);
      } else {
        Ep0Stall (Dev, Req);
      }

      return;

    case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
      NewDtr   = ((Req->Value & 0x0001U) != 0);
      Dev->Rts = ((Req->Value & 0x0002U) != 0);
      if (NewDtr && !Dev->Dtr) {
        Dev->Dtr       = TRUE;
        Dev->HostReady = TRUE;
        OnHostConnected (Dev, FALSE);
      } else if (!NewDtr) {
        Dev->Dtr       = FALSE;
        Dev->HostReady = FALSE;
      }

      Ep0SubmitStatusIn (Dev);
      return;

    case USB_CDC_REQ_SEND_BREAK:
      if (Req->Value != 0) {
        Dev->HostReady = TRUE;
        OnHostConnected (Dev, TRUE);
      }

      Ep0SubmitStatusIn (Dev);
      return;

    default:
      Ep0Stall (Dev, Req);
      return;
  }
}

STATIC
VOID
HandleSetupPacket (
  IN NP05J_USB_SERIAL_DEV          *Dev,
  IN CONST EFI_USB_DEVICE_REQUEST  *Req
  )
{
  UINT8  ReqType;

  CopyMem (&Dev->Ep0PendingReq, Req, sizeof (Dev->Ep0PendingReq));
  Dev->Ep0Stage = Ep0StageIdle;

  ReqType = Req->RequestType & 0x60U;
  if (ReqType == USB_REQ_TYPE_STANDARD) {
    HandleStandardSetup (Dev, Req);
  } else if (ReqType == USB_REQ_TYPE_CLASS) {
    HandleClassSetup (Dev, Req);
  } else {
    Ep0Stall (Dev, Req);
  }
}

STATIC
VOID
HandleEp0RxComplete (
  IN NP05J_USB_SERIAL_DEV             *Dev,
  IN CONST EFI_USBFN_TRANSFER_RESULT  *Result
  )
{
  if (Result->TransferStatus != UsbTransferStatusComplete) {
    Dev->Ep0Stage = Ep0StageIdle;
    return;
  }

  if (Dev->Ep0Stage == Ep0StageDataOut) {
    if (((Dev->Ep0PendingReq.RequestType & 0x60U) == USB_REQ_TYPE_CLASS) &&
        (Dev->Ep0PendingReq.Request == USB_CDC_REQ_SET_LINE_CODING) &&
        (Result->BytesTransferred >= sizeof (USB_CDC_LINE_CODING)))
    {
      CopyMem (&Dev->LineCoding, Dev->Ep0Buf, sizeof (Dev->LineCoding));
      if (Dev->LineCoding.DteRate != 0) {
        Dev->SerialMode.BaudRate = Dev->LineCoding.DteRate;
      }

      if (Dev->LineCoding.DataBits != 0) {
        Dev->SerialMode.DataBits = Dev->LineCoding.DataBits;
      }
    }

    Ep0SubmitStatusIn (Dev);
  } else {
    Dev->Ep0Stage = Ep0StageIdle;
  }
}

STATIC
VOID
HandleEp0TxComplete (
  IN NP05J_USB_SERIAL_DEV             *Dev,
  IN CONST EFI_USBFN_TRANSFER_RESULT  *Result
  )
{
  if (Result->TransferStatus != UsbTransferStatusComplete) {
    Dev->Ep0Stage = Ep0StageIdle;
    return;
  }

  if (Dev->Ep0Stage == Ep0StageDataIn) {
    Ep0SubmitStatusOut (Dev);
  } else {
    Dev->Ep0Stage = Ep0StageIdle;
  }
}

STATIC
VOID
ResetBusState (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  Dev->Configured    = FALSE;
  Dev->ConfigValue   = 0;
  Dev->Dtr           = FALSE;
  Dev->Rts           = FALSE;
  Dev->HostReady     = FALSE;
  Dev->Ep0Stage      = Ep0StageIdle;
  Dev->BulkRxPending = FALSE;
  Dev->BulkTxPending = FALSE;
}

//
// Main UsbfnIo EventHandler polling loop (always runs at TPL_CALLBACK)
//
STATIC
VOID
Np05jUsbSerialPoll (
  IN NP05J_USB_SERIAL_DEV  *Dev
  )
{
  EFI_TPL                    CurrentTpl;
  EFI_TPL                    OldTpl;
  EFI_USBFN_MESSAGE          Message;
  UINTN                      PayloadSize;
  EFI_USBFN_MESSAGE_PAYLOAD  Payload;
  EFI_STATUS                 Status;
  UINTN                      NoneCount;

  if (Dev->InPoll || Dev->ExitedBootServices || (Dev->UsbfnIo == NULL)) {
    return;
  }

  // Never invoke UsbfnDwc3Dxe above TPL_CALLBACK
  CurrentTpl = EfiGetCurrentTpl ();
  if (CurrentTpl > TPL_CALLBACK) {
    return;
  }

  OldTpl      = gBS->RaiseTPL (TPL_CALLBACK);
  Dev->InPoll = TRUE;
  NoneCount   = 0;

  for (UINTN Iter = 0; Iter < 32U; Iter++) {
    if (Dev->UsbfnIo == NULL) {
      break;
    }

    Message     = EfiUsbMsgNone;
    PayloadSize = sizeof (Payload);
    ZeroMem (&Payload, sizeof (Payload));

    Status = Dev->UsbfnIo->EventHandler (Dev->UsbfnIo, &Message, &PayloadSize, &Payload);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (Message == EfiUsbMsgNone) {
      // UsbfnDwc3Dxe polls hardware on empty queue and returns queued events on next call
      NoneCount++;
      if (NoneCount >= 2U) {
        break;
      }

      continue;
    }

    NoneCount = 0;

    switch (Message) {
      case EfiUsbMsgBusEventAttach:
        Dev->Attached = TRUE;
        break;

      case EfiUsbMsgBusEventDetach:
        Dev->Attached = FALSE;
        ResetBusState (Dev);
        break;

      case EfiUsbMsgBusEventReset:
        ResetBusState (Dev);
        break;

      case EfiUsbMsgBusEventSpeed:
        Dev->BusSpeed = Payload.ubs;
        if ((UINT32)Dev->BusSpeed >= (UINT32)UsbBusSpeedSuper) {
          Dev->BulkMaxPacket = 1024;
        } else if (Dev->BusSpeed == UsbBusSpeedFull) {
          Dev->BulkMaxPacket = 64;
        } else {
          Dev->BulkMaxPacket = 512;
        }

        break;

      case EfiUsbMsgSetupPacket:
        HandleSetupPacket (Dev, &Payload.udr);
        break;

      case EfiUsbMsgEndpointStatusChangedRx:
        if (Payload.utr.EndpointIndex == NP05J_EP_CONTROL) {
          HandleEp0RxComplete (Dev, &Payload.utr);
        } else if (Payload.utr.EndpointIndex == NP05J_EP_BULK_INDEX) {
          Dev->BulkRxPending = FALSE;
          if (Payload.utr.TransferStatus == UsbTransferStatusComplete) {
            if (Payload.utr.BytesTransferred > 0) {
              RxRingPushBytes (Dev, (CONST UINT8 *)Dev->BulkRxBuf, Payload.utr.BytesTransferred);
              if (!Dev->HostReady) {
                Dev->HostReady = TRUE;
                OnHostConnected (Dev, FALSE);
              }
            }

            ArmBulkRx (Dev);
          }
        }

        break;

      case EfiUsbMsgEndpointStatusChangedTx:
        if (Payload.utr.EndpointIndex == NP05J_EP_CONTROL) {
          HandleEp0TxComplete (Dev, &Payload.utr);
        } else if (Payload.utr.EndpointIndex == NP05J_EP_BULK_INDEX) {
          Dev->BulkTxPending = FALSE;
          if (Payload.utr.TransferStatus == UsbTransferStatusComplete) {
            PumpBulkTx (Dev);
          }
        }

        break;

      default:
        break;
    }
  }

  PumpBulkTx (Dev);
  ArmBulkRx (Dev);

  Dev->InPoll = FALSE;
  gBS->RestoreTPL (OldTpl);
}

STATIC
VOID
EFIAPI
Np05jUsbSerialTimerCb (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  Np05jUsbSerialPoll ((NP05J_USB_SERIAL_DEV *)Context);
}

//
// EFI_SERIAL_IO_PROTOCOL Implementation
//
STATIC
EFI_STATUS
EFIAPI
Np05jSerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jSerialSetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT64                  BaudRate,
  IN UINT32                  ReceiveFifoDepth,
  IN UINT32                  Timeout,
  IN EFI_PARITY_TYPE         Parity,
  IN UINT8                   DataBits,
  IN EFI_STOP_BITS_TYPE      StopBits
  )
{
  mDev.SerialMode.BaudRate         = (BaudRate == 0) ? 115200 : BaudRate;
  mDev.SerialMode.ReceiveFifoDepth = (ReceiveFifoDepth == 0) ? NP05J_RX_RING_SIZE : ReceiveFifoDepth;
  mDev.SerialMode.Timeout          = Timeout;
  mDev.SerialMode.Parity           = (Parity == DefaultParity) ? NoParity : Parity;
  mDev.SerialMode.DataBits         = (DataBits == 0) ? 8 : DataBits;
  mDev.SerialMode.StopBits         = (StopBits == DefaultStopBits) ? OneStopBit : StopBits;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jSerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                  Control
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jSerialGetControl (
  IN  EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                  *Control
  )
{
  EFI_TPL  OldTpl;
  UINT32   Bits;

  if (Control == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Np05jUsbSerialPoll (&mDev);

  Bits   = EFI_SERIAL_CLEAR_TO_SEND | EFI_SERIAL_DATA_SET_READY | EFI_SERIAL_CARRIER_DETECT |
           EFI_SERIAL_DATA_TERMINAL_READY | EFI_SERIAL_REQUEST_TO_SEND;
  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);
  if (mDev.RxCount == 0) {
    Bits |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
  }

  if (mDev.TxCount == 0) {
    Bits |= EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  }

  gBS->RestoreTPL (OldTpl);

  *Control = Bits;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jSerialWrite (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  IN     VOID                    *Buffer
  )
{
  if ((BufferSize == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (*BufferSize == 0) {
    return EFI_SUCCESS;
  }

  // Only queue console output when a PC terminal is attached; never block boot
  if (mDev.HostReady) {
    TxRingPushBytes (&mDev, (CONST UINT8 *)Buffer, *BufferSize);
    Np05jUsbSerialPoll (&mDev);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jSerialRead (
  IN     EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN                   *BufferSize,
  OUT    VOID                    *Buffer
  )
{
  UINTN  ReadCount;

  if ((BufferSize == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (*BufferSize == 0) {
    return EFI_SUCCESS;
  }

  Np05jUsbSerialPoll (&mDev);
  ReadCount   = RxRingPopBytes (&mDev, (UINT8 *)Buffer, *BufferSize);
  *BufferSize = ReadCount;

  return (ReadCount > 0) ? EFI_SUCCESS : EFI_TIMEOUT;
}

//
// Teardown helper shared by DriverBindingStop and ExitBootServices
//
STATIC
VOID
TeardownUsbfn (
  IN NP05J_USB_SERIAL_DEV  *Dev,
  IN BOOLEAN               CloseProtocol
  )
{
  EFI_USBFN_IO_PROTOCOL  *UsbfnIo;
  EFI_HANDLE             ControllerHandle;

  if (Dev->PollTimer != NULL) {
    gBS->SetTimer (Dev->PollTimer, TimerCancel, 0);
  }

  if (Dev->DeferredConnectTimer != NULL) {
    gBS->SetTimer (Dev->DeferredConnectTimer, TimerCancel, 0);
  }

  UsbfnIo          = Dev->UsbfnIo;
  ControllerHandle = Dev->UsbfnControllerHandle;

  Dev->UsbfnIo               = NULL;
  Dev->UsbfnControllerHandle = NULL;
  Dev->Attached              = FALSE;
  ResetBusState (Dev);

  if (UsbfnIo != NULL) {
    UsbfnIo->AbortTransfer (UsbfnIo, NP05J_EP_CONTROL, EfiUsbEndpointDirectionDeviceTx);
    UsbfnIo->AbortTransfer (UsbfnIo, NP05J_EP_CONTROL, EfiUsbEndpointDirectionDeviceRx);
    UsbfnIo->AbortTransfer (UsbfnIo, NP05J_EP_BULK_INDEX, EfiUsbEndpointDirectionDeviceRx);
    UsbfnIo->AbortTransfer (UsbfnIo, NP05J_EP_BULK_INDEX, EfiUsbEndpointDirectionDeviceTx);
    UsbfnIo->StopController (UsbfnIo);

    if (Dev->Ep0Buf != NULL) {
      UsbfnIo->FreeTransferBuffer (UsbfnIo, Dev->Ep0Buf);
      Dev->Ep0Buf = NULL;
    }

    if (Dev->BulkRxBuf != NULL) {
      UsbfnIo->FreeTransferBuffer (UsbfnIo, Dev->BulkRxBuf);
      Dev->BulkRxBuf = NULL;
    }

    if (Dev->BulkTxBuf != NULL) {
      UsbfnIo->FreeTransferBuffer (UsbfnIo, Dev->BulkTxBuf);
      Dev->BulkTxBuf = NULL;
    }

    if (CloseProtocol && (ControllerHandle != NULL)) {
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiUsbFunctionIoProtocolGuid,
             mDriverBinding.DriverBindingHandle,
             ControllerHandle
             );
    }
  }
}

//
// EFI_DRIVER_BINDING_PROTOCOL Implementation (binds to UsbfnDwc3Dxe)
//
STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS             Status;
  EFI_USBFN_IO_PROTOCOL  *UsbfnIo;

  if (mDev.UsbfnIo != NULL) {
    return (ControllerHandle == mDev.UsbfnControllerHandle) ? EFI_ALREADY_STARTED : EFI_UNSUPPORTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbFunctionIoProtocolGuid,
                  (VOID **)&UsbfnIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiUsbFunctionIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                  Status;
  EFI_USBFN_IO_PROTOCOL       *UsbfnIo;
  NP05J_USBFN_IO_PROTOCOL_EX  *UsbfnIoEx;

  if (mDev.ExitedBootServices) {
    return EFI_UNSUPPORTED;
  }

  if (mDev.UsbfnIo != NULL) {
    return (ControllerHandle == mDev.UsbfnControllerHandle) ? EFI_ALREADY_STARTED : EFI_UNSUPPORTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiUsbFunctionIoProtocolGuid,
                  (VOID **)&UsbfnIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mDev.UsbfnIo               = UsbfnIo;
  mDev.UsbfnControllerHandle = ControllerHandle;
  mDev.BusSpeed              = UsbBusSpeedHigh;
  mDev.BulkMaxPacket         = 512;
  ResetBusState (&mDev);

  UsbfnIo->StopController (UsbfnIo);
  Status = UsbfnIo->StartController (UsbfnIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Np05jUsbSerial: StartController failed: %r\n", Status));
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  Status = UsbfnIo->AllocateTransferBuffer (UsbfnIo, NP05J_EP0_BUF_SIZE, &mDev.Ep0Buf);
  if (EFI_ERROR (Status)) {
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  Status = UsbfnIo->AllocateTransferBuffer (UsbfnIo, NP05J_BULK_RX_BUF_SIZE, &mDev.BulkRxBuf);
  if (EFI_ERROR (Status)) {
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  Status = UsbfnIo->AllocateTransferBuffer (UsbfnIo, NP05J_BULK_TX_BUF_SIZE, &mDev.BulkTxBuf);
  if (EFI_ERROR (Status)) {
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  if (UsbfnIo->Revision >= NP05J_USBFN_IO_REV_SS) {
    UsbfnIoEx = (NP05J_USBFN_IO_PROTOCOL_EX *)UsbfnIo;
    Status    = UsbfnIoEx->ConfigureEnableEndpointsEx (UsbfnIo, &mHsDeviceInfo, &mSsDeviceInfo);
  } else {
    Status = UsbfnIo->ConfigureEnableEndpoints (UsbfnIo, &mHsDeviceInfo);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Np05jUsbSerial: ConfigureEnableEndpoints(Ex) failed: %r\n", Status));
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  Status = gBS->SetTimer (mDev.PollTimer, TimerPeriodic, NP05J_POLL_INTERVAL_100NS);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Np05jUsbSerial: SetTimer failed: %r\n", Status));
    TeardownUsbfn (&mDev, TRUE);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "Np05jUsbSerial: CDC-ACM ready (UsbfnIo rev 0x%08x)\n", UsbfnIo->Revision));
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Np05jUsbSerialBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  )
{
  if (ControllerHandle != mDev.UsbfnControllerHandle) {
    return EFI_SUCCESS;
  }

  TeardownUsbfn (&mDev, TRUE);
  return EFI_SUCCESS;
}

//
// Protocol & Event Notifications
//
STATIC
VOID
TryConnectExistingUsbfnHandles (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles;

  if (mDev.UsbfnIo != NULL) {
    return;
  }

  Handles     = NULL;
  HandleCount = 0;
  Status      = gBS->LocateHandleBuffer (
                       ByProtocol,
                       &gEfiUsbFunctionIoProtocolGuid,
                       NULL,
                       &HandleCount,
                       &Handles
                       );
  if (EFI_ERROR (Status) || (Handles == NULL)) {
    return;
  }

  for (UINTN Index = 0; Index < HandleCount; Index++) {
    gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
    if (mDev.UsbfnIo != NULL) {
      break;
    }
  }

  FreePool (Handles);
}

STATIC
VOID
EFIAPI
OnDeferredConnectTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (!mDev.ExitedBootServices) {
    TryConnectExistingUsbfnHandles ();
  }
}

STATIC
VOID
EFIAPI
OnUsbfnProtocolInstalled (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  //
  // Defer fallback ConnectController by 50 ms so we never re-enter
  // UsbfnDwc3Dxe::Start() while it is still inside InstallMultipleProtocolInterfaces().
  //
  if ((mDev.UsbfnIo == NULL) && !mDev.ExitedBootServices && (mDev.DeferredConnectTimer != NULL)) {
    gBS->SetTimer (mDev.DeferredConnectTimer, TimerRelative, 500000ULL);
  }
}

STATIC
VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  //
  // Register our VT-UTF8 USB UART terminal into ConIn / ConOut / ErrOut before
  // BdsDxe connects consoles so UEFI Boot Manager and UEFI Shell are interactive
  // over /dev/ttyACM0 alongside the tablet screen and buttons.
  //
  EfiBootManagerUpdateConsoleVariable (ConIn,  (EFI_DEVICE_PATH_PROTOCOL *)&mTerminalDevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ConOut, (EFI_DEVICE_PATH_PROTOCOL *)&mTerminalDevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ErrOut, (EFI_DEVICE_PATH_PROTOCOL *)&mTerminalDevicePath, NULL);

  TryConnectExistingUsbfnHandles ();
}

STATIC
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  mDev.ExitedBootServices = TRUE;
  TeardownUsbfn (&mDev, FALSE);
}

//
// Driver Entry Point
//
EFI_STATUS
EFIAPI
Np05jUsbSerialDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  ZeroMem (&mDev, sizeof (mDev));
  mDev.ImageHandle   = ImageHandle;
  mDev.BusSpeed      = UsbBusSpeedHigh;
  mDev.BulkMaxPacket = 512;

  // Default CDC-ACM line coding: 115200 8N1
  mDev.LineCoding.DteRate    = 115200;
  mDev.LineCoding.CharFormat = 0; // 1 stop bit
  mDev.LineCoding.ParityType = 0; // None
  mDev.LineCoding.DataBits   = 8;

  // Default EFI_SERIAL_IO_MODE
  mDev.SerialMode.ControlMask      = EFI_SERIAL_CLEAR_TO_SEND |
                                     EFI_SERIAL_DATA_SET_READY |
                                     EFI_SERIAL_CARRIER_DETECT |
                                     EFI_SERIAL_INPUT_BUFFER_EMPTY |
                                     EFI_SERIAL_OUTPUT_BUFFER_EMPTY |
                                     EFI_SERIAL_DATA_TERMINAL_READY |
                                     EFI_SERIAL_REQUEST_TO_SEND;
  mDev.SerialMode.Timeout          = 1000000;
  mDev.SerialMode.BaudRate         = 115200;
  mDev.SerialMode.ReceiveFifoDepth = NP05J_RX_RING_SIZE;
  mDev.SerialMode.DataBits         = 8;
  mDev.SerialMode.Parity           = NoParity;
  mDev.SerialMode.StopBits         = OneStopBit;

  mDev.SerialIo.Revision       = EFI_SERIAL_IO_PROTOCOL_REVISION;
  mDev.SerialIo.Reset          = Np05jSerialReset;
  mDev.SerialIo.SetAttributes  = Np05jSerialSetAttributes;
  mDev.SerialIo.SetControl     = Np05jSerialSetControl;
  mDev.SerialIo.GetControl     = Np05jSerialGetControl;
  mDev.SerialIo.Write          = Np05jSerialWrite;
  mDev.SerialIo.Read           = Np05jSerialRead;
  mDev.SerialIo.Mode           = &mDev.SerialMode;
  mDev.SerialIo.DeviceTypeGuid = NULL;

  LocateLogRing (&mDev);

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  Np05jUsbSerialTimerCb,
                  &mDev,
                  &mDev.PollTimer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnDeferredConnectTimer,
                  &mDev,
                  &mDev.DeferredConnectTimer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEndOfDxe,
                  &mDev,
                  &gEfiEndOfDxeEventGroupGuid,
                  &mDev.EndOfDxeEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Np05jUsbSerial: EndOfDxe event registration failed: %r\n", Status));
  }

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  OnExitBootServices,
                  &mDev,
                  &gEfiEventExitBootServicesGuid,
                  &mDev.ExitBootServicesEvent
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Np05jUsbSerial: ExitBootServices event registration failed: %r\n", Status));
  }

  // Install persistent SerialIo + DevicePath handle for TerminalDxe
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mDev.SerialHandle,
                  &gEfiSerialIoProtocolGuid,
                  &mDev.SerialIo,
                  &gEfiDevicePathProtocolGuid,
                  &mSerialDevicePath,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Install UEFI Driver Binding protocol to bind onto UsbfnDwc3Dxe
  mDriverBinding.ImageHandle         = ImageHandle;
  mDriverBinding.DriverBindingHandle = ImageHandle;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDriverBindingProtocolGuid,
                  &mDriverBinding,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Watch for UsbfnIo installations (in case ConnectController is non-recursive)
  mDev.UsbfnNotifyEvent = EfiCreateProtocolNotifyEvent (
                            &gEfiUsbFunctionIoProtocolGuid,
                            TPL_CALLBACK,
                            OnUsbfnProtocolInstalled,
                            &mDev,
                            &mDev.UsbfnNotifyReg
                            );

  TryConnectExistingUsbfnHandles ();

  DEBUG ((DEBUG_INFO, "Np05jUsbSerial: Driver initialized (SerialHandle=%p)\n", mDev.SerialHandle));
  return EFI_SUCCESS;
}
