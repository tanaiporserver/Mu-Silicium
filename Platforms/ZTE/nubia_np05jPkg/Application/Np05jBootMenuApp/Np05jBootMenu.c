/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  Main Entry & Touch-First 3x3 Tile UEFI Boot Menu for nubia NP05J.
**/

#include "Np05jBootMenuApp.h"

#define TILE_COUNT 9U

typedef struct {
  CONST CHAR8                  *Title;
  CONST CHAR8                  *Subtitle;
  CONST CHAR8                  *Tag;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Accent;
} NP05J_BOOT_TILE;

STATIC CONST NP05J_BOOT_TILE mBootTiles[TILE_COUNT] = {
  { "Windows 11 ARM",   "Scan FS* for bootmgfw.efi (WoA ACPI)", "WOA",    { 0xF8, 0xBD, 0x38, 0 } },
  { "Linux (DeviceTree)","Boot GRUB / systemd-boot / UKI (DTB)", "LINUX",  { 0x81, 0xB9, 0x10, 0 } },
  { "Linux (ACPI)",     "Boot Linux ARM64 via ACPI tables",      "ACPI",   { 0xF7, 0x55, 0xA8, 0 } },
  { "Android Boot",     "Cold reset back to active Android slot", "HLOS",   { 0x0B, 0x9E, 0xF5, 0 } },
  { "BIOS Setup",       "Unlocked All-Options Firmware Utility", "UNLOCK", { 0xF8, 0xBD, 0x38, 0 } },
  { "Boot From File",   "Browse FAT/ESP & launch any .EFI image", "FILE",   { 0xB8, 0x94, 0x64, 0 } },
  { "UEFI Shell",       "Launch built-in EDK2 Interactive Shell", "SHELL",  { 0x81, 0xB9, 0x10, 0 } },
  { "Touch Test",       "Synaptics S3930 canvas & 4-corner cal", "TOUCH",  { 0xF8, 0xBD, 0x38, 0 } },
  { "Power & Reboot",   "Normal / Recovery / Fastboot / EDL 9008", "PWR",   { 0x44, 0x44, 0xEF, 0 } }
};

STATIC
VOID
RunPowerModal (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  STATIC CONST CHAR8 *mPwrLabels[6] = {
    "Reboot Normal (Android / BootSelect)",
    "Reboot to Recovery",
    "Reboot to Bootloader / Fastboot",
    "Reboot to EDL (Qualcomm 9008)",
    "Shutdown / Power Off",
    "Cancel"
  };
  NP05J_RECT                    Box   = { 560, 260, 1280, 960 };
  NP05J_RECT                    Btns[6];
  INTN                          Focus = 0;
  BOOLEAN                       Redraw = TRUE;

  Np05jUiDrainInput (Ctx);

  while (TRUE) {
    NP05J_INPUT_EVENT Ev;

    if (Redraw) {
      Np05jUiDrawCard (Ctx, Box, COL_PANEL, COL_RED, 4);
      Np05jUiDrawTextCentered (Ctx, (NP05J_RECT){ Box.X, Box.Y + 28, Box.W, 60 }, 3, COL_RED, "POWER & HARDWARE RESET MODE");
      Np05jUiDrawTextCentered (Ctx, (NP05J_RECT){ Box.X, Box.Y + 86, Box.W, 40 }, 2, COL_SUBTEXT, "PMK8550 SDAM0x71 / TCSR SCM 9008");

      for (UINTN I = 0; I < 6U; I++) {
        Btns[I] = (NP05J_RECT){ Box.X + 80, Box.Y + 150 + (INT32)I * 124, Box.W - 160, 102 };
        {
          BOOLEAN Sel = ((INTN)I == Focus);
          Np05jUiDrawCard (
            Ctx,
            Btns[I],
            Sel ? COL_CARD_HI : COL_CARD,
            (I == 3U) ? COL_GOLD : (Sel ? COL_ACCENT : COL_BORDER),
            Sel ? 3 : 2
            );
          Np05jUiDrawTextCentered (Ctx, Btns[I], 2, (I == 5U) ? COL_SUBTEXT : COL_TEXT, mPwrLabels[I]);
        }
      }
      Np05jUiPresent (Ctx);
      Redraw = FALSE;
    }

    if (EFI_ERROR (Np05jUiPollInput (Ctx, 100, &Ev))) {
      continue;
    }
    if (Ev.Kind == Np05jInputBack) {
      return;
    }
    if (Ev.Kind == Np05jInputUp) {
      Focus  = (Focus > 0) ? (Focus - 1) : 5;
      Redraw = TRUE;
      continue;
    }
    if (Ev.Kind == Np05jInputDown) {
      Focus  = (Focus + 1) % 6;
      Redraw = TRUE;
      continue;
    }

    INTN Pick = -1;
    if (Ev.Kind == Np05jInputEnter) {
      Pick = Focus;
    } else if (Ev.Kind == Np05jInputTap) {
      if (!Np05jRectContains (Box, Ev.X, Ev.Y)) {
        return;
      }
      for (UINTN I = 0; I < 6U; I++) {
        if (Np05jRectContains (Btns[I], Ev.X, Ev.Y)) {
          Pick = (INTN)I;
          break;
        }
      }
    }

    if (Pick >= 0) {
      switch (Pick) {
        case 0: Np05jExecuteReset (Np05jResetNormal); break;
        case 1: Np05jExecuteReset (Np05jResetRecovery); break;
        case 2: Np05jExecuteReset (Np05jResetBootloader); break;
        case 3:
        {
          EFI_STATUS St = Np05jExecuteReset (Np05jResetEdl9008);
          Np05jUiSetBanner (Ctx, COL_RED, "EDL 9008 entry failed: %r", St);
          return;
        }
        case 4: Np05jExecuteReset (Np05jResetShutdown); break;
        case 5: return;
      }
    }
  }
}

STATIC
VOID
ActivateTile (
  IN OUT NP05J_UI_CTX *Ctx,
  IN     UINTN         Index
  )
{
  BOOLEAN RebootReq = FALSE;

  switch (Index) {
    case 0:
      Np05jLaunchWindowsArm (Ctx);
      break;
    case 1:
      Np05jLaunchLinux (Ctx, FALSE);
      break;
    case 2:
      Np05jLaunchLinux (Ctx, TRUE);
      break;
    case 3:
      Np05jExecuteReset (Np05jResetNormal);
      break;
    case 4:
      Np05jRunBiosSetup (Ctx, &RebootReq);
      break;
    case 5:
      Np05jRunFileBrowser (Ctx);
      break;
    case 6:
      Np05jLaunchEfiShell (Ctx);
      break;
    case 7:
      Np05jRunTouchTest (Ctx);
      break;
    case 8:
      RunPowerModal (Ctx);
      break;
    default:
      break;
  }
  Np05jUiDrainInput (Ctx);
}

STATIC
VOID
CheckAutoLaunchHandoff (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  CONST volatile NP05J_EMBED_RECORD *Rec = (CONST volatile NP05J_EMBED_RECORD *)(UINTN)NP05J_EMBED_RECORD_ADDR;
  UINT8                              OpenSetup = 0;
  UINTN                              VarSize   = sizeof (OpenSetup);
  BOOLEAN                            RebootReq = FALSE;

  /* Consume splash-pill OpenSetup trigger */
  if ((gRT != NULL) &&
      !EFI_ERROR (gRT->GetVariable (L"Np05jOpenSetup", &gNp05jBiosConfigGuid, NULL, &VarSize, &OpenSetup)))
  {
    gRT->SetVariable (L"Np05jOpenSetup", &gNp05jBiosConfigGuid, 0, 0, NULL);
    if (OpenSetup != 0U) {
      Np05jRunBiosSetup (Ctx, &RebootReq);
      return;
    }
  }

  /* Honor pre-UEFI BootSelect SUNUEFI-EMBEDv1 handoff if valid */
  if ((AsciiStrnCmp ((CONST CHAR8 *)Rec->Magic, NP05J_EMBED_RECORD_MAGIC, 15) == 0) &&
      (Rec->Version == NP05J_EMBED_RECORD_VERSION) &&
      (Rec->Size == NP05J_EMBED_RECORD_SIZE))
  {
    if (Rec->Target == NP05J_BOOT_TARGET_BIOS_SETUP) {
      Np05jRunBiosSetup (Ctx, &RebootReq);
    } else if (Rec->Target == NP05J_BOOT_TARGET_WINDOWS) {
      Np05jLaunchWindowsArm (Ctx);
    } else if (Rec->Target == NP05J_BOOT_TARGET_LINUX) {
      Np05jLaunchLinux (Ctx, FALSE);
    }
  }
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  NP05J_UI_CTX Ctx;
  EFI_STATUS   Status;
  INTN         Focus      = 0;
  BOOLEAN      NeedRedraw = TRUE;

  EfiBootManagerConnectAll ();

  Status = Np05jUiInit (&Ctx);
  if (EFI_ERROR (Status)) {
    Print (L"NP05J BootMenu: GOP init failed (%r)\n", Status);
    return Status;
  }

  CheckAutoLaunchHandoff (&Ctx);
  Np05jUiDrainInput (&Ctx);

  while (TRUE) {
    NP05J_RECT        TileRect[TILE_COUNT];
    NP05J_RECT        QuickStrip[4];
    NP05J_INPUT_EVENT Ev;
    INT32             GridX = 48;
    INT32             GridY = 152;
    INT32             GapX  = 36;
    INT32             GapY  = 32;
    INT32             TileW = ((INT32)Ctx.ScreenW - (GridX * 2) - (GapX * 2)) / 3;
    INT32             TileH = ((INT32)Ctx.ScreenH - 370 - (GapY * 2)) / 3;

    for (UINTN I = 0; I < TILE_COUNT; I++) {
      INT32 Row = (INT32)(I / 3U);
      INT32 Col = (INT32)(I % 3U);
      TileRect[I] = (NP05J_RECT){
        GridX + Col * (TileW + GapX),
        GridY + Row * (TileH + GapY),
        TileW,
        TileH
      };
    }

    for (UINTN Q = 0; Q < 4U; Q++) {
      QuickStrip[Q] = (NP05J_RECT){ 48 + (INT32)Q * 580, (INT32)Ctx.ScreenH - 186, 560, 74 };
    }

    if (NeedRedraw) {
      CHAR8 Sub[120];
      Np05jUiClear (&Ctx, COL_BG);
      AsciiSPrint (
        Sub,
        sizeof (Sub),
        "Touch tiles directly or use Vol+/Vol-/Power | %u ESP/FAT volume(s) | Adapted from Project SunUEFI",
        (UINT32)Np05jCountFileSystems ()
        );
      Np05jUiDrawHeader (&Ctx, "SUNUEFI TOUCH BOOT MANAGER - NUBIA NP05J", Sub);

      for (UINTN I = 0; I < TILE_COUNT; I++) {
        BOOLEAN                       Focused = ((INTN)I == Focus);
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL Acc     = mBootTiles[I].Accent;
        NP05J_RECT                    R       = TileRect[I];

        Np05jUiDrawCard (&Ctx, R, Focused ? COL_CARD_HI : COL_CARD, Focused ? COL_ACCENT : COL_BORDER, Focused ? 4 : 2);
        Np05jUiFillRect (&Ctx, (NP05J_RECT){ R.X, R.Y, 14, R.H }, Acc);

        /* Badge */
        Np05jUiDrawCard (&Ctx, (NP05J_RECT){ R.X + 34, R.Y + 26, 136, 44 }, COL_PANEL, Acc, 2);
        Np05jUiDrawTextCentered (&Ctx, (NP05J_RECT){ R.X + 34, R.Y + 26, 136, 44 }, 2, Acc, mBootTiles[I].Tag);

        /* Title & Description */
        Np05jUiDrawText (&Ctx, R.X + 34, R.Y + 96, 3, COL_TEXT, mBootTiles[I].Title);
        Np05jUiDrawText (&Ctx, R.X + 34, R.Y + 148, 2, COL_SUBTEXT, mBootTiles[I].Subtitle);
      }

      /* Quick action footer pills */
      {
        STATIC CONST CHAR8 *mQuick[4] = {
          "Quick: BIOS Setup (Unlocked)",
          "Quick: Touch Calibration",
          "Quick: Built-in UEFI Shell",
          "Quick: Refresh Drives & Touch"
        };
        for (UINTN Q = 0; Q < 4U; Q++) {
          Np05jUiDrawCard (&Ctx, QuickStrip[Q], COL_PANEL, COL_ACCENT, 2);
          Np05jUiDrawTextCentered (&Ctx, QuickStrip[Q], 2, COL_ACCENT, mQuick[Q]);
        }
      }

      Np05jUiDrawBannerIfAny (&Ctx);
      Np05jUiPresent (&Ctx);
      NeedRedraw = FALSE;
    }

    if (EFI_ERROR (Np05jUiPollInput (&Ctx, 150, &Ev))) {
      continue;
    }

    if (Ev.Kind == Np05jInputLeft) {
      Focus      = (Focus > 0) ? (Focus - 1) : ((INTN)TILE_COUNT - 1);
      NeedRedraw = TRUE;
    } else if (Ev.Kind == Np05jInputRight) {
      Focus      = (Focus + 1) % (INTN)TILE_COUNT;
      NeedRedraw = TRUE;
    } else if (Ev.Kind == Np05jInputUp) {
      Focus      = (Focus >= 3) ? (Focus - 3) : ((Focus + 6) % (INTN)TILE_COUNT);
      NeedRedraw = TRUE;
    } else if (Ev.Kind == Np05jInputDown) {
      Focus      = (Focus + 3) % (INTN)TILE_COUNT;
      NeedRedraw = TRUE;
    } else if (Ev.Kind == Np05jInputEnter) {
      ActivateTile (&Ctx, (UINTN)Focus);
      NeedRedraw = TRUE;
    } else if (Ev.Kind == Np05jInputTap) {
      BOOLEAN Hit = FALSE;
      for (UINTN I = 0; I < TILE_COUNT; I++) {
        if (Np05jRectContains (TileRect[I], Ev.X, Ev.Y)) {
          Focus = (INTN)I;
          ActivateTile (&Ctx, I);
          Hit        = TRUE;
          NeedRedraw = TRUE;
          break;
        }
      }
      if (!Hit) {
        if (Np05jRectContains (QuickStrip[0], Ev.X, Ev.Y)) {
          ActivateTile (&Ctx, 4);
        } else if (Np05jRectContains (QuickStrip[1], Ev.X, Ev.Y)) {
          ActivateTile (&Ctx, 7);
        } else if (Np05jRectContains (QuickStrip[2], Ev.X, Ev.Y)) {
          ActivateTile (&Ctx, 6);
        } else if (Np05jRectContains (QuickStrip[3], Ev.X, Ev.Y)) {
          EfiBootManagerConnectAll ();
          Np05jUiRefreshHandles (&Ctx);
          Np05jUiSetBanner (&Ctx, COL_GREEN, "Refreshed: %u FAT volumes, %u touch handle(s)",
            (UINT32)Np05jCountFileSystems (), (UINT32)Ctx.AbsCount);
        }
        NeedRedraw = TRUE;
      }
    }
  }

  Np05jUiFree (&Ctx);
  return EFI_SUCCESS;
}
