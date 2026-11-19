/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  High-DPI vector UEFI splash renderer with progress bar and touch pills.
  Adapted from Project SunUEFI (https://github.com/slwyts/Project-SunUEFI)
  ProductSplash.c for nubia NP05J (2400x1504 GOP).
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/Np05jSplashLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BootLogo2.h>

#include "Np05jSplashAssets.h"

STATIC NP05J_SPLASH_HITBOX mHitboxes[4];
STATIC UINTN               mHitboxCount = 0;
STATIC UINT32              mScreenW     = 0;
STATIC UINT32              mScreenH     = 0;
STATIC BOOLEAN             mDarkTheme   = FALSE;
STATIC UINT32              mLogoCropX   = 0;
STATIC UINT32              mLogoCropY   = 0;
STATIC UINT32              mLogoCropW   = 0;
STATIC UINT32              mLogoCropH   = 0;

STATIC
EFI_GRAPHICS_OUTPUT_BLT_PIXEL
MakeColor (
  IN UINT8 R,
  IN UINT8 G,
  IN UINT8 B
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel = { B, G, R, 0 };
  return Pixel;
}

STATIC
EFI_STATUS
FillRect (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN UINT32                         X,
  IN UINT32                         Y,
  IN UINT32                         W,
  IN UINT32                         H
  )
{
  if ((Gop == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }
  if ((W == 0U) || (H == 0U)) {
    return EFI_SUCCESS;
  }
  if ((X >= Gop->Mode->Info->HorizontalResolution) ||
      (Y >= Gop->Mode->Info->VerticalResolution) ||
      (W > Gop->Mode->Info->HorizontalResolution - X) ||
      (H > Gop->Mode->Info->VerticalResolution - Y))
  {
    return EFI_INVALID_PARAMETER;
  }
  return Gop->Blt (Gop, &Color, EfiBltVideoFill, 0, 0, X, Y, W, H, 0);
}

STATIC
UINT32
IntSqrt (
  IN UINT32 N
  )
{
  UINT32 Root = 0;
  UINT32 Bit  = 1U << 30;

  while (Bit > N) {
    Bit >>= 2;
  }
  while (Bit != 0U) {
    if (N >= Root + Bit) {
      N    -= Root + Bit;
      Root  = (Root >> 1) + Bit;
    } else {
      Root >>= 1;
    }
    Bit >>= 2;
  }
  return Root;
}

STATIC
EFI_STATUS
DrawDisc (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN UINT32                         X,
  IN UINT32                         Y,
  IN UINT32                         Radius,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  for (INT32 Dy = -(INT32)Radius; Dy <= (INT32)Radius; Dy++) {
    UINT32 Dx = IntSqrt (Radius * Radius - (UINT32)(Dy * Dy));
    if ((INT32)Y + Dy >= 0 && (INT32)X >= (INT32)Dx) {
      FillRect (Gop, Color, X - Dx, (UINT32)((INT32)Y + Dy), 2 * Dx + 1, 1);
    }
  }
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DrawPolygon4 (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN CONST NP05J_SPLASH_POINT       V[4],
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  INT32 Top    = V[0].Y;
  INT32 Bottom = Top;

  for (UINTN I = 1; I < 4; I++) {
    if (V[I].Y < Top) {
      Top = V[I].Y;
    }
    if (V[I].Y > Bottom) {
      Bottom = V[I].Y;
    }
  }

  for (INT32 Y = Top; Y < Bottom; Y++) {
    INT32 Left  = MAX_INT32;
    INT32 Right = MIN_INT32;

    for (UINTN I = 0; I < 4; I++) {
      NP05J_SPLASH_POINT A = V[I];
      NP05J_SPLASH_POINT B = V[(I + 1) % 4];
      INT32              X;

      if (A.Y > B.Y) {
        NP05J_SPLASH_POINT Tmp = A;
        A = B;
        B = Tmp;
      }
      if ((A.Y == B.Y) || (Y < A.Y) || (Y >= B.Y)) {
        continue;
      }
      X = A.X + (INT32)(((INT64)(B.X - A.X) * (Y - A.Y)) / (B.Y - A.Y));
      if (X < Left) {
        Left = X;
      }
      if (X > Right) {
        Right = X;
      }
    }

    if ((Left <= Right) && (Left >= 0) && (Y >= 0)) {
      FillRect (Gop, Color, (UINT32)Left, (UINT32)Y, (UINT32)(Right - Left + 1), 1);
    }
  }
  return EFI_SUCCESS;
}

STATIC
VOID
DrawStroke (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN NP05J_SPLASH_POINT             A,
  IN NP05J_SPLASH_POINT             B,
  IN UINT32                         Radius,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  INT32  Dx = B.X - A.X;
  INT32  Dy = B.Y - A.Y;
  UINT32 L  = IntSqrt ((UINT32)(Dx * Dx + Dy * Dy));

  if (L == 0U) {
    DrawDisc (Gop, (UINT32)A.X, (UINT32)A.Y, Radius, Color);
    return;
  }

  {
    INT32              Nx   = (INT32)(((INT64)Dy * Radius) / L);
    INT32              Ny   = (INT32)(-((INT64)Dx * Radius) / L);
    NP05J_SPLASH_POINT V[4] = {
      { A.X + Nx, A.Y + Ny },
      { B.X + Nx, B.Y + Ny },
      { B.X - Nx, B.Y - Ny },
      { A.X - Nx, A.Y - Ny }
    };
    DrawPolygon4 (Gop, V, Color);
    DrawDisc (Gop, (UINT32)A.X, (UINT32)A.Y, Radius, Color);
    DrawDisc (Gop, (UINT32)B.X, (UINT32)B.Y, Radius, Color);
  }
}

STATIC
UINT32
BrandWidth (
  IN UINT32 Height
  )
{
  UINT32 Width = Height * 2U / 3U;
  UINT32 Total = (Height / 25U) * 8U;

  for (UINTN I = 0; I < 9; I++) {
    UINT32 Factor = (mBrand[I] == 'i') ? 38U : ((mBrand[I] == 't') ? 76U : ((mBrand[I] == 'r') ? 82U : 100U));
    Total += Width * Factor / 100U;
  }
  return Total;
}

STATIC
VOID
DrawBrandWordmark (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN UINT32                         X,
  IN UINT32                         Y,
  IN UINT32                         Height,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  UINT32 BaseWidth = Height * 2U / 3U;
  UINT32 Gap       = Height / 25U;
  UINT32 Radius    = Height / 32U;

  if (Radius < 1U) {
    Radius = 1U;
  }

  for (UINTN I = 0; I < 9; I++) {
    NP05J_SPLASH_POINT Path[40];
    UINT32             Count  = 0;
    CHAR8              Ch     = mBrand[I];
    UINT32             Factor = (Ch == 'i') ? 38U : ((Ch == 't') ? 76U : ((Ch == 'r') ? 82U : 100U));
    UINT32             Width  = BaseWidth * Factor / 100U;

    if ((Ch == 'o') || (Ch == 'a') || (Ch == 'c') || (Ch == 'e')) {
      UINT32 First = (Ch == 'c') ? 4U : ((Ch == 'e') ? 2U : 0U);
      UINT32 Last  = (Ch == 'c') ? 28U : ((Ch == 'e') ? 30U : 32U);
      for (UINT32 J = First; J <= Last; J++) {
        Path[Count++] = (NP05J_SPLASH_POINT){
          500 + mCircle[J % 32U].X * 390 / 1000,
          750 + mCircle[J % 32U].Y * 450 / 1000
        };
      }
    } else if (Ch == 'n') {
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 1200 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 300 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 700 };
      for (UINT32 J = 16; J <= 32; J++) {
        Path[Count++] = (NP05J_SPLASH_POINT){
          500 + mCircle[J % 32U].X * 390 / 1000,
          700 + mCircle[J % 32U].Y * 400 / 1000
        };
      }
      Path[Count++] = (NP05J_SPLASH_POINT){ 890, 1200 };
    } else if (Ch == 't') {
      Path[Count++] = (NP05J_SPLASH_POINT){ 430, 0 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 430, 1050 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 500, 1150 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 640, 1200 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 840, 1200 };
    } else if (Ch == 'i') {
      Path[Count++] = (NP05J_SPLASH_POINT){ 500, 350 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 500, 1200 };
    } else if (Ch == 'r') {
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 1200 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 300 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 110, 700 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 270, 430 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 520, 300 };
      Path[Count++] = (NP05J_SPLASH_POINT){ 760, 320 };
    }

    for (UINTN J = 1; J < Count; J++) {
      NP05J_SPLASH_POINT A = Path[J - 1];
      NP05J_SPLASH_POINT B = Path[J];
      A.X = (INT32)X + A.X * (INT32)Width / 1000;
      B.X = (INT32)X + B.X * (INT32)Width / 1000;
      A.Y = (INT32)Y + A.Y * (INT32)Height / 1200;
      B.Y = (INT32)Y + B.Y * (INT32)Height / 1200;
      DrawStroke (Gop, A, B, Radius, Color);
    }

    if ((Ch == 't') || (Ch == 'a') || (Ch == 'e')) {
      NP05J_SPLASH_POINT A = (Ch == 't') ? (NP05J_SPLASH_POINT){ 100, 350 } :
                             (Ch == 'a') ? (NP05J_SPLASH_POINT){ 890, 300 } :
                                           (NP05J_SPLASH_POINT){ 200, 1050 };
      NP05J_SPLASH_POINT B = (Ch == 't') ? (NP05J_SPLASH_POINT){ 850, 350 } :
                             (Ch == 'a') ? (NP05J_SPLASH_POINT){ 890, 1200 } :
                                           (NP05J_SPLASH_POINT){ 855, 500 };
      A.X = (INT32)X + A.X * (INT32)Width / 1000;
      B.X = (INT32)X + B.X * (INT32)Width / 1000;
      A.Y = (INT32)Y + A.Y * (INT32)Height / 1200;
      B.Y = (INT32)Y + B.Y * (INT32)Height / 1200;
      DrawStroke (Gop, A, B, Radius, Color);
    }

    if (Ch == 'i') {
      DrawDisc (Gop, X + Width / 2U, Y + Height / 12U, Radius * 2U, Color);
    }

    X += Width + Gap;
  }
}

STATIC
CONST UINT8 *
LookupGlyph5x7 (
  IN CHAR8 Ch
  )
{
  STATIC CONST UINT8 Space[5]  = { 0, 0, 0, 0, 0 };
  STATIC CONST UINT8 Plus[5]   = { 0x08, 0x08, 0x3e, 0x08, 0x08 };
  STATIC CONST UINT8 Minus[5]  = { 0x08, 0x08, 0x08, 0x08, 0x08 };
  STATIC CONST UINT8 Slash[5]  = { 0x20, 0x10, 0x08, 0x04, 0x02 };
  STATIC CONST UINT8 Colon[5]  = { 0x00, 0x36, 0x36, 0x00, 0x00 };
  STATIC CONST UINT8 LParen[5] = { 0x00, 0x1c, 0x22, 0x41, 0x00 };
  STATIC CONST UINT8 RParen[5] = { 0x00, 0x41, 0x22, 0x1c, 0x00 };
  STATIC CONST UINT8 Pipe[5]   = { 0x00, 0x00, 0x7f, 0x00, 0x00 };
  STATIC CONST UINT8 Dot[5]    = { 0x00, 0x60, 0x60, 0x00, 0x00 };

  if ((Ch >= 'a') && (Ch <= 'z')) {
    Ch = (CHAR8)(Ch - 'a' + 'A');
  }
  if ((Ch >= 'A') && (Ch <= 'Z')) {
    return mLetters[Ch - 'A'];
  }
  if ((Ch >= '0') && (Ch <= '9')) {
    return mDigits[Ch - '0'];
  }
  switch (Ch) {
    case '+': return Plus;
    case '-': return Minus;
    case '/': return Slash;
    case ':': return Colon;
    case '(': return LParen;
    case ')': return RParen;
    case '|': return Pipe;
    case '.': return Dot;
    default:  return Space;
  }
}

STATIC
VOID
DrawText5x7 (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN CONST CHAR8                   *Text,
  IN UINT32                         X,
  IN UINT32                         Y,
  IN UINT32                         Scale,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  UINT32 Len = (UINT32)AsciiStrLen (Text);
  UINT32 W   = (Len > 0U) ? ((Len * 6U - 1U) * Scale) : 0U;

  while ((W > Gop->Mode->Info->HorizontalResolution - X - 16U) && (Scale > 1U)) {
    Scale--;
    W = (Len * 6U - 1U) * Scale;
  }

  for (UINT32 I = 0; I < Len; I++) {
    CONST UINT8 *G = LookupGlyph5x7 (Text[I]);
    for (UINT32 Col = 0; Col < 5U; Col++) {
      UINT32 Row = 0;
      while (Row < 7U) {
        UINT32 First;
        if ((G[Col] & (1U << Row)) == 0U) {
          Row++;
          continue;
        }
        First = Row;
        while ((Row < 7U) && ((G[Col] & (1U << Row)) != 0U)) {
          Row++;
        }
        FillRect (
          Gop,
          Color,
          X + (I * 6U + Col) * Scale,
          Y + First * Scale,
          Scale,
          (Row - First) * Scale
          );
      }
    }
  }
}

STATIC
VOID
DrawTouchPills (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop
  )
{
  STATIC CONST struct {
    CONST CHAR8        *Label;
    NP05J_SPLASH_ACTION Action;
  } Pills[4] = {
    { "UEFI MENU",     Np05jSplashActionBootMenu },
    { "BIOS UNLOCKED", Np05jSplashActionBiosSetup },
    { "WINDOWS ARM",   Np05jSplashActionWindows },
    { "LINUX",         Np05jSplashActionLinux }
  };

  UINT32 MarginX   = mScreenW * 6U / 100U;
  UINT32 Gap       = 28U;
  UINT32 TotalW    = mScreenW - 2U * MarginX;
  UINT32 PillW     = (TotalW - 3U * Gap) / 4U;
  UINT32 PillH     = 96U;
  UINT32 PillY     = mScreenH * 77U / 100U;
  UINT32 TextScale = 4U;

  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Border = mDarkTheme ? MakeColor (70, 92, 130) : MakeColor (210, 218, 232);
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Fill   = mDarkTheme ? MakeColor (28, 36, 52) : MakeColor (245, 247, 251);
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Accent = MakeColor (255, 102, 0);
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL TextC  = mDarkTheme ? MakeColor (240, 244, 250) : MakeColor (44, 52, 66);

  mHitboxCount = 0;
  for (UINTN I = 0; I < 4; I++) {
    UINT32 X     = MarginX + (UINT32)I * (PillW + Gap);
    UINT32 LLen  = (UINT32)AsciiStrLen (Pills[I].Label);
    UINT32 TextW = (LLen * 6U - 1U) * TextScale;
    UINT32 TextX = X + (PillW > TextW ? (PillW - TextW) / 2U : 12U);
    UINT32 TextY = PillY + (PillH - 7U * TextScale) / 2U;

    FillRect (Gop, (I == 1) ? Accent : Border, X, PillY, PillW, PillH);
    FillRect (Gop, Fill, X + 3U, PillY + 3U, PillW - 6U, PillH - 6U);
    DrawText5x7 (Gop, Pills[I].Label, TextX, TextY, TextScale, (I == 1) ? Accent : TextC);

    mHitboxes[mHitboxCount].X      = X;
    mHitboxes[mHitboxCount].Y      = PillY;
    mHitboxes[mHitboxCount].Width  = PillW;
    mHitboxes[mHitboxCount].Height = PillH;
    mHitboxes[mHitboxCount].Action = Pills[I].Action;
    mHitboxCount++;
  }
}

EFI_STATUS
EFIAPI
Np05jDrawSplash (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,
  IN UINT8                         Style
  )
{
  UINT32 Short;
  UINT32 LogoSize;
  UINT32 WordmarkScale;
  UINT32 SubScale;
  UINT32 HintScale;
  UINT32 TitleHeight;
  UINT32 GroupWidth;
  UINT32 CenterY;
  UINT32 LogoX;
  UINT32 LogoY;
  UINT32 WordmarkX;
  UINT32 WordmarkY;

  if ((Gop == NULL) || (Gop->Blt == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  mScreenW   = Gop->Mode->Info->HorizontalResolution;
  mScreenH   = Gop->Mode->Info->VerticalResolution;
  mDarkTheme = (Style != 0U);

  if ((mScreenW < 480U) || (mScreenH < 320U)) {
    return EFI_UNSUPPORTED;
  }

  Short         = (mScreenW < mScreenH) ? mScreenW : mScreenH;
  LogoSize      = Short * 20U / 100U;
  WordmarkScale = Short / 52U;
  SubScale      = Short / 340U;
  HintScale     = Short / 380U;
  if (WordmarkScale == 0U) { WordmarkScale = 1U; }
  if (SubScale == 0U)      { SubScale = 1U; }
  if (HintScale == 0U)     { HintScale = 1U; }

  TitleHeight = WordmarkScale * 7U;
  GroupWidth  = LogoSize + Short / 20U + BrandWidth (TitleHeight);
  while ((GroupWidth > mScreenW - 48U) && (WordmarkScale > 1U)) {
    WordmarkScale--;
    TitleHeight = WordmarkScale * 7U;
    GroupWidth  = LogoSize + Short / 20U + BrandWidth (TitleHeight);
  }

  CenterY   = mScreenH * 36U / 100U;
  LogoX     = (mScreenW - GroupWidth) / 2U;
  LogoY     = CenterY - LogoSize / 2U;
  WordmarkX = LogoX + LogoSize + Short / 20U;
  WordmarkY = CenterY - TitleHeight / 2U;

  mLogoCropX = (LogoX > 24U) ? (LogoX - 24U) : 0U;
  mLogoCropY = (LogoY > 24U) ? (LogoY - 24U) : 0U;
  mLogoCropW = GroupWidth + 48U;
  mLogoCropH = LogoSize + 48U;
  if (mLogoCropX + mLogoCropW > mScreenW) { mLogoCropW = mScreenW - mLogoCropX; }
  if (mLogoCropY + mLogoCropH > mScreenH) { mLogoCropH = mScreenH - mLogoCropY; }

  {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Bg    = mDarkTheme ? MakeColor (15, 20, 30) : MakeColor (255, 255, 255);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Gold  = MakeColor (255, 102, 0);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Shade = MakeColor (214, 82, 0);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL WordC = mDarkTheme ? MakeColor (235, 240, 248) : MakeColor (54, 60, 68);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Muted = mDarkTheme ? MakeColor (150, 162, 180) : MakeColor (110, 118, 128);

    FillRect (Gop, Bg, 0, 0, mScreenW, mScreenH);

    for (UINTN I = 0; I < 6; I++) {
      NP05J_SPLASH_POINT D      = mSpokes[I];
      NP05J_SPLASH_POINT N      = { -D.Y, D.X };
      NP05J_SPLASH_POINT Center = { (INT32)LogoX + (INT32)LogoSize / 2, (INT32)LogoY + (INT32)LogoSize / 2 };
      NP05J_SPLASH_POINT End    = {
        Center.X + D.X * (INT32)LogoSize * 43 / 100000,
        Center.Y + D.Y * (INT32)LogoSize * 43 / 100000
      };
      INT32              Inner  = (INT32)LogoSize * 20 / 1000;
      INT32              Outer  = (INT32)LogoSize * 48 / 1000;
      NP05J_SPLASH_POINT P[4]   = {
        { Center.X + N.X * Inner / 1000, Center.Y + N.Y * Inner / 1000 },
        { End.X + N.X * Outer / 1000,    End.Y + N.Y * Outer / 1000 },
        { End.X - N.X * Outer / 1000,    End.Y - N.Y * Outer / 1000 },
        { Center.X - N.X * Inner / 1000, Center.Y - N.Y * Inner / 1000 }
      };
      NP05J_SPLASH_POINT Face[4] = { Center, End, P[2], P[3] };

      DrawDisc (Gop, (UINT32)End.X, (UINT32)End.Y, (UINT32)Outer, Gold);
      DrawPolygon4 (Gop, P, Gold);
      DrawPolygon4 (Gop, Face, Shade);
    }

    DrawBrandWordmark (Gop, WordmarkX, WordmarkY, TitleHeight, WordC);
    DrawText5x7 (Gop, mSubtitle, WordmarkX, WordmarkY + TitleHeight + Short / 60U, SubScale, Muted);
    DrawTouchPills (Gop);
    DrawText5x7 (Gop, mSetupHint, mScreenW * 6U / 100U, mScreenH * 90U / 100U, HintScale, Muted);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
Np05jSplashSetProgress (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,
  IN UINT8                         Percent,
  IN CONST CHAR8                  *StatusText OPTIONAL
  )
{
  UINT32 BarX;
  UINT32 BarY;
  UINT32 BarW;
  UINT32 BarH;
  UINT32 FillW;

  if ((Gop == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  mScreenW = Gop->Mode->Info->HorizontalResolution;
  mScreenH = Gop->Mode->Info->VerticalResolution;
  if (Percent > 100U) {
    Percent = 100U;
  }

  BarX  = mScreenW * 20U / 100U;
  BarW  = mScreenW * 60U / 100U;
  BarY  = mScreenH * 63U / 100U;
  BarH  = 12U;
  FillW = BarW * Percent / 100U;

  {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Bg     = mDarkTheme ? MakeColor (15, 20, 30) : MakeColor (255, 255, 255);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Track  = mDarkTheme ? MakeColor (42, 52, 70) : MakeColor (230, 234, 240);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Accent = MakeColor (255, 102, 0);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL TextC  = mDarkTheme ? MakeColor (200, 210, 224) : MakeColor (90, 98, 108);

    FillRect (Gop, Track, BarX, BarY, BarW, BarH);
    if (FillW > 0U) {
      FillRect (Gop, Accent, BarX, BarY, FillW, BarH);
    }

    if (StatusText != NULL) {
      UINT32 TextY = BarY + 26U;
      FillRect (Gop, Bg, BarX, TextY, BarW, 42U);
      DrawText5x7 (Gop, StatusText, BarX, TextY, 4U, TextC);
    }
  }

  return EFI_SUCCESS;
}

NP05J_SPLASH_ACTION
EFIAPI
Np05jSplashHitTest (
  IN UINT32 DisplayX,
  IN UINT32 DisplayY
  )
{
  for (UINTN I = 0; I < mHitboxCount; I++) {
    if ((DisplayX >= mHitboxes[I].X) &&
        (DisplayX < mHitboxes[I].X + mHitboxes[I].Width) &&
        (DisplayY >= mHitboxes[I].Y) &&
        (DisplayY < mHitboxes[I].Y + mHitboxes[I].Height))
    {
      return mHitboxes[I].Action;
    }
  }
  return Np05jSplashActionNone;
}

VOID
EFIAPI
Np05jSplashRegisterBootLogo2 (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop
  )
{
  EDKII_BOOT_LOGO2_PROTOCOL     *BootLogo2 = NULL;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt       = NULL;
  EFI_STATUS                     Status;

  if ((Gop == NULL) || (mLogoCropW == 0U) || (mLogoCropH == 0U)) {
    return;
  }

  Status = gBS->LocateProtocol (&gEdkiiBootLogo2ProtocolGuid, NULL, (VOID **)&BootLogo2);
  if (EFI_ERROR (Status) || (BootLogo2 == NULL) || (BootLogo2->SetBootLogo == NULL)) {
    return;
  }

  Blt = AllocatePool ((UINTN)mLogoCropW * (UINTN)mLogoCropH * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (Blt == NULL) {
    return;
  }

  Status = Gop->Blt (
                  Gop,
                  Blt,
                  EfiBltVideoToBltBuffer,
                  mLogoCropX,
                  mLogoCropY,
                  0,
                  0,
                  mLogoCropW,
                  mLogoCropH,
                  0
                  );
  if (!EFI_ERROR (Status)) {
    BootLogo2->SetBootLogo (BootLogo2, Blt, mLogoCropX, mLogoCropY, mLogoCropW, mLogoCropH);
  }

  FreePool (Blt);
}
