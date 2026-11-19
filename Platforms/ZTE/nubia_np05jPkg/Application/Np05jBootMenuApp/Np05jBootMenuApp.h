/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  Shared declarations for NP05J touch-first UEFI Boot Menu & Unlocked BIOS Setup.
**/

#ifndef _NP05J_BOOT_MENU_APP_H_
#define _NP05J_BOOT_MENU_APP_H_

#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/Np05jBiosConfigLib.h>
#include <Library/Np05jResetLib.h>
#include <Library/Np05jSplashLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/AbsolutePointer.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EFIChargerEx.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/Np05jBiosConfig.h>
#include <Protocol/Np05jTouchInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimplePointer.h>

#include <Np05jBiosConfig.h>
#include <Np05jBootHandoff.h>

#define NP05J_MAX_ABS_HANDLES    8U
#define NP05J_MAX_MOUSE_HANDLES  8U
#define NP05J_UEFI_SHELL_GUID    { 0x7C04A583, 0x9E3E, 0x4F1C, { 0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1 } }

typedef struct {
  INT32 X;
  INT32 Y;
  INT32 W;
  INT32 H;
} NP05J_RECT;

typedef enum {
  Np05jInputNone = 0,
  Np05jInputTap,
  Np05jInputUp,
  Np05jInputDown,
  Np05jInputLeft,
  Np05jInputRight,
  Np05jInputEnter,
  Np05jInputBack,
  Np05jInputTouchMove
} NP05J_INPUT_KIND;

typedef struct {
  NP05J_INPUT_KIND Kind;
  INT32            X;
  INT32            Y;
  BOOLEAN          PointerDown;
} NP05J_INPUT_EVENT;

typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BackBuf;
  UINT32                         ScreenW;
  UINT32                         ScreenH;
  BOOLEAN                        Dirty;

  NP05J_BIOS_CONFIG              Config;
  BOOLEAN                        ConfigModified;
  BOOLEAN                        StatusValid;
  CHAR8                          StatusBanner[128];
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  StatusColor;

  /* Input tracking across touch + mouse + keypad */
  EFI_ABSOLUTE_POINTER_PROTOCOL *AbsPtrs[NP05J_MAX_ABS_HANDLES];
  UINTN                          AbsCount;
  BOOLEAN                        AbsWasDown[NP05J_MAX_ABS_HANDLES];
  INT32                          AbsStartX[NP05J_MAX_ABS_HANDLES];
  INT32                          AbsStartY[NP05J_MAX_ABS_HANDLES];
  INT32                          AbsCurX[NP05J_MAX_ABS_HANDLES];
  INT32                          AbsCurY[NP05J_MAX_ABS_HANDLES];

  EFI_SIMPLE_POINTER_PROTOCOL   *MousePtrs[NP05J_MAX_MOUSE_HANDLES];
  UINTN                          MouseCount;
  BOOLEAN                        MouseWasLeftDown;
  BOOLEAN                        MouseVisible;
  INT32                          CursorX;
  INT32                          CursorY;

  BOOLEAN                        AnyTouchDown;
  INT32                          ActiveTouchX;
  INT32                          ActiveTouchY;
} NP05J_UI_CTX;

/* Standard SunUEFI high-contrast palette */
static inline
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
Np05jRgb (
  IN UINT8 R,
  IN UINT8 G,
  IN UINT8 B
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL P = { B, G, R, 0 };
  return P;
}

#define COL_BG         Np05jRgb (10, 15, 26)
#define COL_PANEL      Np05jRgb (18, 27, 46)
#define COL_CARD       Np05jRgb (25, 38, 64)
#define COL_CARD_HI    Np05jRgb (35, 55, 92)
#define COL_CARD_PRESS Np05jRgb (20, 74, 118)
#define COL_BORDER     Np05jRgb (48, 69, 108)
#define COL_ACCENT     Np05jRgb (56, 189, 248)
#define COL_GOLD       Np05jRgb (245, 158, 11)
#define COL_GREEN      Np05jRgb (16, 185, 129)
#define COL_RED        Np05jRgb (239, 68, 68)
#define COL_PURPLE     Np05jRgb (168, 85, 247)
#define COL_TEXT       Np05jRgb (241, 245, 249)
#define COL_SUBTEXT    Np05jRgb (148, 163, 184)
#define COL_BLACK      Np05jRgb (0, 0, 0)
#define COL_WHITE      Np05jRgb (255, 255, 255)

/* Canvas & Input (Np05jUiCanvas.c) */
EFI_STATUS
Np05jUiInit (
  OUT NP05J_UI_CTX *Ctx
  );

VOID
Np05jUiFree (
  IN OUT NP05J_UI_CTX *Ctx
  );

VOID
Np05jUiRefreshHandles (
  IN OUT NP05J_UI_CTX *Ctx
  );

VOID
Np05jUiDrainInput (
  IN OUT NP05J_UI_CTX *Ctx
  );

BOOLEAN
Np05jRectContains (
  IN NP05J_RECT R,
  IN INT32      X,
  IN INT32      Y
  );

VOID
Np05jUiClear (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  );

VOID
Np05jUiFillRect (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  );

VOID
Np05jUiDrawBorder (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     INT32                          Thickness,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  );

VOID
Np05jUiDrawCard (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Fill,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Border,
  IN     INT32                          BorderThickness
  );

VOID
Np05jUiDrawCircle (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          Cx,
  IN     INT32                          Cy,
  IN     INT32                          Radius,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  );

VOID
Np05jUiDrawLine (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X0,
  IN     INT32                          Y0,
  IN     INT32                          X1,
  IN     INT32                          Y1,
  IN     INT32                          Thickness,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  );

VOID
Np05jUiDrawText (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X,
  IN     INT32                          Y,
  IN     UINT32                         Scale,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Text
  );

VOID
Np05jUiDrawTextCentered (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     UINT32                         Scale,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Text
  );

VOID
Np05jUiDrawHeader (
  IN OUT NP05J_UI_CTX *Ctx,
  IN     CONST CHAR8  *Title,
  IN     CONST CHAR8  *Subtitle
  );

VOID
Np05jUiSetBanner (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Format,
  ...
  );

VOID
Np05jUiDrawBannerIfAny (
  IN OUT NP05J_UI_CTX *Ctx
  );

VOID
Np05jUiPresent (
  IN OUT NP05J_UI_CTX *Ctx
  );

EFI_STATUS
Np05jUiPollInput (
  IN OUT NP05J_UI_CTX      *Ctx,
  IN     UINTN              TimeoutMs,
  OUT    NP05J_INPUT_EVENT *Event
  );

VOID
Np05jQueryPowerStatus (
  OUT BOOLEAN *BatteryPresent,
  OUT BOOLEAN *ChargerPresent,
  OUT UINT32  *VoltageMv
  );

/* Screens & OS Launchers */
VOID
Np05jRunBiosSetup (
  IN OUT NP05J_UI_CTX *Ctx,
  OUT    BOOLEAN      *RequestReboot
  );

VOID
Np05jRunFileBrowser (
  IN OUT NP05J_UI_CTX *Ctx
  );

VOID
Np05jRunTouchTest (
  IN OUT NP05J_UI_CTX *Ctx
  );

EFI_STATUS
Np05jLaunchWindowsArm (
  IN OUT NP05J_UI_CTX *Ctx
  );

EFI_STATUS
Np05jLaunchLinux (
  IN OUT NP05J_UI_CTX *Ctx,
  IN     BOOLEAN       UseAcpi
  );

EFI_STATUS
Np05jLaunchEfiShell (
  IN OUT NP05J_UI_CTX *Ctx
  );

EFI_STATUS
Np05jLaunchEfiPathOnVolume (
  IN EFI_HANDLE    FsHandle,
  IN CONST CHAR16 *FilePath,
  IN BOOLEAN       ConnectDrivers
  );

UINTN
Np05jCountFileSystems (
  VOID
  );

#endif /* _NP05J_BOOT_MENU_APP_H_ */
