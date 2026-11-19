/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  Direct 2400x1504 BGRA32 framebuffer renderer & hitboxes for NP05J BootSelect.
  Adapted from Project SunUEFI (https://github.com/slwyts/Project-SunUEFI).
  Note: Uses 2D static arrays and integer-only math for PIC freestanding safety.
**/

#include "BootSelect.h"

/* Minimal 8x8 ASCII font for BootSelect cards (ASCII 0x20..0x5F + lowercase mapped) */
static const uint8_t mMiniFont[64][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
  { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 }, /* '!' */
  { 0x6C, 0x6C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '"' */
  { 0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00 }, /* '#' */
  { 0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00 }, /* '$' */
  { 0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00 }, /* '%' */
  { 0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00 }, /* '&' */
  { 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '\'' */
  { 0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00 }, /* '(' */
  { 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00 }, /* ')' */
  { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 }, /* '*' */
  { 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00 }, /* '+' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30 }, /* ',' */
  { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* '-' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 }, /* '.' */
  { 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00 }, /* '/' */
  { 0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00 }, /* '0' */
  { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00 }, /* '1' */
  { 0x3C, 0x66, 0x06, 0x1C, 0x30, 0x60, 0x7E, 0x00 }, /* '2' */
  { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00 }, /* '3' */
  { 0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00 }, /* '4' */
  { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00 }, /* '5' */
  { 0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00 }, /* '6' */
  { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00 }, /* '7' */
  { 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00 }, /* '8' */
  { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00 }, /* '9' */
  { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00 }, /* ':' */
  { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00 }, /* ';' */
  { 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00 }, /* '<' */
  { 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 }, /* '=' */
  { 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00 }, /* '>' */
  { 0x3C, 0x66, 0x06, 0x1C, 0x18, 0x00, 0x18, 0x00 }, /* '?' */
  { 0x3C, 0x66, 0x6E, 0x6A, 0x6E, 0x60, 0x3C, 0x00 }, /* '@' */
  { 0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00 }, /* 'A' */
  { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 }, /* 'B' */
  { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 }, /* 'C' */
  { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 }, /* 'D' */
  { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00 }, /* 'E' */
  { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00 }, /* 'F' */
  { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00 }, /* 'G' */
  { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 }, /* 'H' */
  { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, /* 'I' */
  { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00 }, /* 'J' */
  { 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00 }, /* 'K' */
  { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 }, /* 'L' */
  { 0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0x00 }, /* 'M' */
  { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 }, /* 'N' */
  { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, /* 'O' */
  { 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00 }, /* 'P' */
  { 0x3C, 0x66, 0x66, 0x66, 0x6A, 0x6C, 0x36, 0x00 }, /* 'Q' */
  { 0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00 }, /* 'R' */
  { 0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00 }, /* 'S' */
  { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, /* 'T' */
  { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, /* 'U' */
  { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, /* 'V' */
  { 0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00 }, /* 'W' */
  { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 }, /* 'X' */
  { 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00 }, /* 'Y' */
  { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00 }, /* 'Z' */
  { 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00 }, /* '[' */
  { 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00 }, /* '\\' */
  { 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00 }, /* ']' */
  { 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '^' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 }  /* '_' */
};

static uint32_t RgbBgra(uint8_t R, uint8_t G, uint8_t B) {
  return ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
}

static void FillRect(
  uint32_t *Fb,
  uint32_t  W,
  uint32_t  H,
  uint32_t  Stride,
  int32_t   X,
  int32_t   Y,
  int32_t   Rw,
  int32_t   Rh,
  uint32_t  Color
  )
{
  int32_t X0 = (X < 0) ? 0 : X;
  int32_t Y0 = (Y < 0) ? 0 : Y;
  int32_t X1 = X + Rw;
  int32_t Y1 = Y + Rh;

  if (X1 > (int32_t)W) {
    X1 = (int32_t)W;
  }
  if (Y1 > (int32_t)H) {
    Y1 = (int32_t)H;
  }
  if ((X0 >= X1) || (Y0 >= Y1)) {
    return;
  }

  for (int32_t Py = Y0; Py < Y1; Py++) {
    uint32_t *Row = Fb + (uint32_t)Py * Stride;
    for (int32_t Px = X0; Px < X1; Px++) {
      Row[Px] = Color;
    }
  }
}

static void DrawBorder(
  uint32_t *Fb,
  uint32_t  W,
  uint32_t  H,
  uint32_t  Stride,
  int32_t   X,
  int32_t   Y,
  int32_t   Rw,
  int32_t   Rh,
  int32_t   Thick,
  uint32_t  Color
  )
{
  FillRect(Fb, W, H, Stride, X, Y, Rw, Thick, Color);
  FillRect(Fb, W, H, Stride, X, Y + Rh - Thick, Rw, Thick, Color);
  FillRect(Fb, W, H, Stride, X, Y, Thick, Rh, Color);
  FillRect(Fb, W, H, Stride, X + Rw - Thick, Y, Thick, Rh, Color);
}

static void DrawText(
  uint32_t   *Fb,
  uint32_t    W,
  uint32_t    H,
  uint32_t    Stride,
  int32_t     X,
  int32_t     Y,
  uint32_t    Scale,
  uint32_t    Color,
  const char *Text
  )
{
  int32_t Cx = X;

  while (*Text != '\0') {
    char Ch = *Text++;
    if ((Ch >= 'a') && (Ch <= 'z')) {
      Ch = (char)(Ch - 'a' + 'A');
    }
    uint32_t Idx = ((Ch >= ' ') && (Ch <= '_')) ? (uint32_t)(Ch - ' ') : 0U;
    for (uint32_t R = 0; R < 8U; R++) {
      uint8_t Bits = mMiniFont[Idx][R];
      if (Bits == 0U) {
        continue;
      }
      for (uint32_t C = 0; C < 8U; C++) {
        if ((Bits & (0x80U >> C)) != 0U) {
          FillRect(
            Fb,
            W,
            H,
            Stride,
            Cx + (int32_t)(C * Scale),
            Y + (int32_t)(R * Scale),
            (int32_t)Scale,
            (int32_t)Scale,
            Color
            );
        }
      }
    }
    Cx += (int32_t)(8U * Scale);
  }
}

static uint32_t StrLenAscii(const char *S) {
  uint32_t L = 0U;
  while (S[L] != '\0') {
    L++;
  }
  return L;
}

static void DrawCentered(
  uint32_t   *Fb,
  uint32_t    W,
  uint32_t    H,
  uint32_t    Stride,
  int32_t     X,
  int32_t     Y,
  int32_t     Rw,
  int32_t     Rh,
  uint32_t    Scale,
  uint32_t    Color,
  const char *Text
  )
{
  uint32_t Len = StrLenAscii(Text);
  int32_t  Tw  = (int32_t)(Len * 8U * Scale);
  int32_t  Th  = (int32_t)(8U * Scale);
  DrawText(Fb, W, H, Stride, X + (Rw - Tw) / 2, Y + (Rh - Th) / 2, Scale, Color, Text);
}

void BootSelectGetCardRect(uint32_t Index, NP05J_BOOT_CARD_RECT *Out) {
  static const uint32_t mTargets[NP05J_CARD_COUNT] = {
    NP05J_BOOT_TARGET_ANDROID,
    NP05J_BOOT_TARGET_UEFI_MENU,
    NP05J_BOOT_TARGET_BIOS_SETUP
  };
  int32_t StartX = 120;
  int32_t CardW  = 680;
  int32_t GapX   = 60;

  if (!Out || (Index >= NP05J_CARD_COUNT)) {
    return;
  }
  Out->X      = StartX + (int32_t)Index * (CardW + GapX);
  Out->Y      = 290;
  Out->W      = CardW;
  Out->H      = 720;
  Out->Target = mTargets[Index];
}

int32_t BootSelectHitTest(uint32_t X, uint32_t Y) {
  for (uint32_t I = 0U; I < NP05J_CARD_COUNT; I++) {
    NP05J_BOOT_CARD_RECT R;
    BootSelectGetCardRect(I, &R);
    if (((int32_t)X >= R.X) && ((int32_t)X < (R.X + R.W)) &&
        ((int32_t)Y >= R.Y) && ((int32_t)Y < (R.Y + R.H)))
    {
      return (int32_t)I;
    }
  }
  return -1;
}

void BootSelectRenderFrame(
  uint32_t *FbBgra,
  uint32_t  Width,
  uint32_t  Height,
  uint32_t  StridePx,
  uint32_t  SelectedCard,
  uint32_t  ProgressPct,
  uint32_t  RemainingSec,
  int       TouchOnline
  )
{
  static const char mTitles[3][20] = {
    "ANDROID",
    "UEFI BOOT MENU",
    "BIOS SETUP"
  };
  static const char mTags[3][20] = {
    "HLOS / STOCK",
    "WIN11 / LINUX",
    "UNLOCKED ALL"
  };
  static const char mSub1[3][32] = {
    "STOCK ANDROID + KERNEL",
    "TOUCH BOOT MANAGER",
    "9 UNLOCKED CATEGORIES"
  };
  static const char mSub2[3][32] = {
    "TAP TO BOOT IMMEDIATELY",
    "WIN11 ARM / LINUX / SHELL",
    "TOUCH / ACPI / WOA / USB"
  };

  uint32_t Bg        = RgbBgra(10, 15, 26);
  uint32_t Panel     = RgbBgra(18, 27, 46);
  uint32_t Card      = RgbBgra(25, 38, 64);
  uint32_t CardHi    = RgbBgra(35, 55, 92);
  uint32_t Border    = RgbBgra(48, 69, 108);
  uint32_t White     = RgbBgra(241, 245, 249);
  uint32_t SubText   = RgbBgra(148, 163, 184);
  uint32_t Accents[3] = {
    RgbBgra(16, 185, 129),
    RgbBgra(56, 189, 248),
    RgbBgra(245, 158, 11)
  };

  if (!FbBgra) {
    return;
  }

  FillRect(FbBgra, Width, Height, StridePx, 0, 0, (int32_t)Width, (int32_t)Height, Bg);
  FillRect(FbBgra, Width, Height, StridePx, 0, 0, (int32_t)Width, 160, Panel);
  FillRect(FbBgra, Width, Height, StridePx, 0, 156, (int32_t)Width, 4, Accents[1]);

  DrawCentered(FbBgra, Width, Height, StridePx, 0, 28, (int32_t)Width, 56, 4U, White,
    "SUNUEFI PRE-UEFI BOOT SELECTION - NUBIA NP05J");
  DrawCentered(FbBgra, Width, Height, StridePx, 0, 96, (int32_t)Width, 40, 2U,
    TouchOnline ? Accents[0] : Accents[2],
    TouchOnline ? "TOUCHSCREEN: SYNAPTICS S3930 ONLINE | VOL+/VOL-/POWER READY"
                : "BUTTONS READY: VOL+/VOL- SELECT, POWER CONFIRM");

  for (uint32_t I = 0U; I < NP05J_CARD_COUNT; I++) {
    NP05J_BOOT_CARD_RECT R;
    int                  Sel = (I == SelectedCard);
    uint32_t             Acc = Accents[I];

    BootSelectGetCardRect(I, &R);
    FillRect(FbBgra, Width, Height, StridePx, R.X, R.Y, R.W, R.H, Sel ? CardHi : Card);
    DrawBorder(FbBgra, Width, Height, StridePx, R.X, R.Y, R.W, R.H, Sel ? 6 : 2, Sel ? Acc : Border);
    FillRect(FbBgra, Width, Height, StridePx, R.X, R.Y, R.W, 18, Acc);

    /* Badge */
    FillRect(FbBgra, Width, Height, StridePx, R.X + 140, R.Y + 86, R.W - 280, 64, Panel);
    DrawBorder(FbBgra, Width, Height, StridePx, R.X + 140, R.Y + 86, R.W - 280, 64, 2, Acc);
    DrawCentered(FbBgra, Width, Height, StridePx, R.X + 140, R.Y + 86, R.W - 280, 64, 2U, Acc, mTags[I]);

    /* Main card title */
    DrawCentered(FbBgra, Width, Height, StridePx, R.X + 20, R.Y + 250, R.W - 40, 96, 4U, White, mTitles[I]);
    DrawCentered(FbBgra, Width, Height, StridePx, R.X + 20, R.Y + 410, R.W - 40, 48, 2U, White, mSub1[I]);
    DrawCentered(FbBgra, Width, Height, StridePx, R.X + 20, R.Y + 474, R.W - 40, 48, 2U, SubText, mSub2[I]);

    if (Sel) {
      FillRect(FbBgra, Width, Height, StridePx, R.X + 120, R.Y + R.H - 110, R.W - 240, 68, Acc);
      DrawCentered(FbBgra, Width, Height, StridePx, R.X + 120, R.Y + R.H - 110, R.W - 240, 68, 2U, Bg, "SELECTED");
    }
  }

  /* Countdown bar */
  {
    int32_t  BarX = 120;
    int32_t  BarY = 1140;
    int32_t  BarW = (int32_t)Width - 240;
    int32_t  BarH = 44;
    uint32_t Pct  = (ProgressPct > 100U) ? 100U : ProgressPct;
    int32_t  Fill = (int32_t)(((uint64_t)BarW * Pct) / 100U);

    FillRect(FbBgra, Width, Height, StridePx, BarX, BarY, BarW, BarH, Panel);
    DrawBorder(FbBgra, Width, Height, StridePx, BarX, BarY, BarW, BarH, 2, Border);
    if (Fill > 8) {
      FillRect(FbBgra, Width, Height, StridePx, BarX + 4, BarY + 4, Fill - 8, BarH - 8,
        Accents[(SelectedCard < 3U) ? SelectedCard : 0U]);
    }
  }

  {
    char Foot[48] = "AUTO-BOOT IN 0S | TAP A CARD OR PRESS POWER";
    uint32_t Sec = (RemainingSec > 9U) ? 9U : RemainingSec;
    Foot[13] = (char)('0' + Sec);
    DrawCentered(FbBgra, Width, Height, StridePx, 0, 1230, (int32_t)Width, 48, 2U, SubText, Foot);
    DrawCentered(FbBgra, Width, Height, StridePx, 0, 1295, (int32_t)Width, 48, 2U, Border,
      "ADAPTED FROM PROJECT SUNUEFI (GITHUB.COM/SLWYTS/PROJECT-SUNUEFI)");
  }
}
