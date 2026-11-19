/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  Double-buffered 2D vector/bitmap canvas and unified touch+mouse+keypad
  input engine for NP05J UEFI Boot Menu & Unlocked BIOS Setup.
**/

#include "Np05jBootMenuApp.h"

/* Crisp 8x8 monochrome bitmap font covering ASCII 0x20..0x7E */
STATIC CONST UINT8 mFont8x8[95][8] = {
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
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 }, /* '_' */
  { 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '`' */
  { 0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00 }, /* 'a' */
  { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00 }, /* 'b' */
  { 0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00 }, /* 'c' */
  { 0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00 }, /* 'd' */
  { 0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00 }, /* 'e' */
  { 0x1C, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x00 }, /* 'f' */
  { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C }, /* 'g' */
  { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, /* 'h' */
  { 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00 }, /* 'i' */
  { 0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0x6C, 0x38, 0x00 }, /* 'j' */
  { 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00 }, /* 'k' */
  { 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, /* 'l' */
  { 0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xC6, 0x00 }, /* 'm' */
  { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, /* 'n' */
  { 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 }, /* 'o' */
  { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60 }, /* 'p' */
  { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06 }, /* 'q' */
  { 0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00 }, /* 'r' */
  { 0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00 }, /* 's' */
  { 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00 }, /* 't' */
  { 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00 }, /* 'u' */
  { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, /* 'v' */
  { 0x00, 0x00, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00 }, /* 'w' */
  { 0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00 }, /* 'x' */
  { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C }, /* 'y' */
  { 0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00 }, /* 'z' */
  { 0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00 }, /* '{' */
  { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 }, /* '|' */
  { 0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00 }, /* '}' */
  { 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }  /* '~' */
};

STATIC INT32 mPrevCursorX = -1;
STATIC INT32 mPrevCursorY = -1;

BOOLEAN
Np05jRectContains (
  IN NP05J_RECT R,
  IN INT32      X,
  IN INT32      Y
  )
{
  return (X >= R.X) && (X < (R.X + R.W)) && (Y >= R.Y) && (Y < (R.Y + R.H));
}

STATIC
VOID
PutPixel (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X,
  IN     INT32                          Y,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  if ((X >= 0) && (Y >= 0) && ((UINT32)X < Ctx->ScreenW) && ((UINT32)Y < Ctx->ScreenH)) {
    Ctx->BackBuf[(UINTN)Y * Ctx->ScreenW + (UINTN)X] = Color;
  }
}

VOID
Np05jUiClear (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  UINTN Total = (UINTN)Ctx->ScreenW * Ctx->ScreenH;
  for (UINTN I = 0; I < Total; I++) {
    Ctx->BackBuf[I] = Color;
  }
}

VOID
Np05jUiFillRect (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  INT32 X0 = (Rect.X < 0) ? 0 : Rect.X;
  INT32 Y0 = (Rect.Y < 0) ? 0 : Rect.Y;
  INT32 X1 = Rect.X + Rect.W;
  INT32 Y1 = Rect.Y + Rect.H;

  if (X1 > (INT32)Ctx->ScreenW) {
    X1 = (INT32)Ctx->ScreenW;
  }
  if (Y1 > (INT32)Ctx->ScreenH) {
    Y1 = (INT32)Ctx->ScreenH;
  }
  if ((X0 >= X1) || (Y0 >= Y1)) {
    return;
  }

  for (INT32 Y = Y0; Y < Y1; Y++) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Row = Ctx->BackBuf + (UINTN)Y * Ctx->ScreenW;
    for (INT32 X = X0; X < X1; X++) {
      Row[X] = Color;
    }
  }
}

VOID
Np05jUiDrawBorder (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     INT32                          Thickness,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  if (Thickness <= 0) {
    return;
  }
  Np05jUiFillRect (Ctx, (NP05J_RECT){ Rect.X, Rect.Y, Rect.W, Thickness }, Color);
  Np05jUiFillRect (Ctx, (NP05J_RECT){ Rect.X, Rect.Y + Rect.H - Thickness, Rect.W, Thickness }, Color);
  Np05jUiFillRect (Ctx, (NP05J_RECT){ Rect.X, Rect.Y, Thickness, Rect.H }, Color);
  Np05jUiFillRect (Ctx, (NP05J_RECT){ Rect.X + Rect.W - Thickness, Rect.Y, Thickness, Rect.H }, Color);
}

VOID
Np05jUiDrawCard (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Fill,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Border,
  IN     INT32                          BorderThickness
  )
{
  Np05jUiFillRect (Ctx, Rect, Fill);
  Np05jUiDrawBorder (Ctx, Rect, BorderThickness, Border);
}

VOID
Np05jUiDrawCircle (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          Cx,
  IN     INT32                          Cy,
  IN     INT32                          Radius,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  INT32 R2 = Radius * Radius;
  for (INT32 Dy = -Radius; Dy <= Radius; Dy++) {
    for (INT32 Dx = -Radius; Dx <= Radius; Dx++) {
      if ((Dx * Dx + Dy * Dy) <= R2) {
        PutPixel (Ctx, Cx + Dx, Cy + Dy, Color);
      }
    }
  }
}

VOID
Np05jUiDrawLine (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X0,
  IN     INT32                          Y0,
  IN     INT32                          X1,
  IN     INT32                          Y1,
  IN     INT32                          Thickness,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color
  )
{
  INT32 Dx  = (X1 >= X0) ? (X1 - X0) : (X0 - X1);
  INT32 Sx  = (X0 < X1) ? 1 : -1;
  INT32 Dy  = -((Y1 >= Y0) ? (Y1 - Y0) : (Y0 - Y1));
  INT32 Sy  = (Y0 < Y1) ? 1 : -1;
  INT32 Err = Dx + Dy;
  INT32 Rad = (Thickness > 1) ? (Thickness / 2) : 0;

  while (TRUE) {
    if (Rad > 0) {
      Np05jUiFillRect (Ctx, (NP05J_RECT){ X0 - Rad, Y0 - Rad, Thickness, Thickness }, Color);
    } else {
      PutPixel (Ctx, X0, Y0, Color);
    }
    if ((X0 == X1) && (Y0 == Y1)) {
      break;
    }
    {
      INT32 E2 = 2 * Err;
      if (E2 >= Dy) {
        Err += Dy;
        X0  += Sx;
      }
      if (E2 <= Dx) {
        Err += Dx;
        Y0  += Sy;
      }
    }
  }
}

STATIC
VOID
DrawChar (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X,
  IN     INT32                          Y,
  IN     UINT32                         Scale,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CHAR8                          Ch
  )
{
  UINT8        Idx = ((Ch >= 0x20) && (Ch <= 0x7E)) ? (UINT8)(Ch - 0x20) : 0U;
  CONST UINT8 *G   = mFont8x8[Idx];

  for (UINT32 Row = 0; Row < 8U; Row++) {
    UINT8 Bits = G[Row];
    if (Bits == 0U) {
      continue;
    }
    for (UINT32 Col = 0; Col < 8U; Col++) {
      if ((Bits & (0x80U >> Col)) != 0U) {
        Np05jUiFillRect (
          Ctx,
          (NP05J_RECT){
            X + (INT32)(Col * Scale),
            Y + (INT32)(Row * Scale),
            (INT32)Scale,
            (INT32)Scale
          },
          Color
          );
      }
    }
  }
}

VOID
Np05jUiDrawText (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     INT32                          X,
  IN     INT32                          Y,
  IN     UINT32                         Scale,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Text
  )
{
  INT32 CurX = X;

  if ((Text == NULL) || (Scale == 0U)) {
    return;
  }

  while (*Text != '\0') {
    if (*Text == '\n') {
      CurX = X;
      Y   += (INT32)(10U * Scale);
    } else {
      DrawChar (Ctx, CurX, Y, Scale, Color, *Text);
      CurX += (INT32)(8U * Scale);
    }
    Text++;
  }
}

VOID
Np05jUiDrawTextCentered (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     NP05J_RECT                     Rect,
  IN     UINT32                         Scale,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Text
  )
{
  UINTN Len;
  INT32 TextW;
  INT32 TextH;

  if (Text == NULL) {
    return;
  }

  Len   = AsciiStrLen (Text);
  TextW = (INT32)(Len * 8U * Scale);
  TextH = (INT32)(8U * Scale);

  Np05jUiDrawText (
    Ctx,
    Rect.X + (Rect.W - TextW) / 2,
    Rect.Y + (Rect.H - TextH) / 2,
    Scale,
    Color,
    Text
    );
}

VOID
Np05jQueryPowerStatus (
  OUT BOOLEAN *BatteryPresent,
  OUT BOOLEAN *ChargerPresent,
  OUT UINT32  *VoltageMv
  )
{
  EFI_CHARGER_EX_PROTOCOL *Charger = NULL;
  BOOLEAN                  Bat     = FALSE;
  BOOLEAN                  Chg     = FALSE;
  UINT32                   Mv      = 0;

  if (EFI_SUCCESS == gBS->LocateProtocol (&gChargerExProtocolGuid, NULL, (VOID **)&Charger) &&
      (Charger != NULL))
  {
    if (Charger->GetBatteryPresence != NULL) {
      Charger->GetBatteryPresence (&Bat);
    }
    if (Charger->GetChargerPresence != NULL) {
      Charger->GetChargerPresence (&Chg);
    }
    if (Charger->GetBatteryVoltage != NULL) {
      Charger->GetBatteryVoltage (&Mv);
    }
  }

  if (BatteryPresent != NULL) {
    *BatteryPresent = Bat;
  }
  if (ChargerPresent != NULL) {
    *ChargerPresent = Chg;
  }
  if (VoltageMv != NULL) {
    *VoltageMv = Mv;
  }
}

VOID
Np05jUiDrawHeader (
  IN OUT NP05J_UI_CTX *Ctx,
  IN     CONST CHAR8  *Title,
  IN     CONST CHAR8  *Subtitle
  )
{
  EFI_TIME Now;
  BOOLEAN  Bat = FALSE;
  BOOLEAN  Chg = FALSE;
  UINT32   Mv  = 0;
  CHAR8    RightBuf[96];

  Np05jUiDrawCard (
    Ctx,
    (NP05J_RECT){ 0, 0, (INT32)Ctx->ScreenW, 118 },
    COL_PANEL,
    COL_BORDER,
    2
    );

  Np05jUiFillRect (Ctx, (NP05J_RECT){ 0, 115, (INT32)Ctx->ScreenW, 3 }, COL_ACCENT);
  Np05jUiDrawText (Ctx, 42, 22, 4, COL_TEXT, Title);
  if (Subtitle != NULL) {
    Np05jUiDrawText (Ctx, 44, 72, 2, COL_SUBTEXT, Subtitle);
  }

  ZeroMem (&Now, sizeof (Now));
  if (gRT != NULL) {
    gRT->GetTime (&Now, NULL);
  }
  Np05jQueryPowerStatus (&Bat, &Chg, &Mv);

  AsciiSPrint (
    RightBuf,
    sizeof (RightBuf),
    "SM8750P 24GB | %04u-%02u-%02u %02u:%02u | %umV%a | TOUCH:%u",
    (UINT32)Now.Year,
    (UINT32)Now.Month,
    (UINT32)Now.Day,
    (UINT32)Now.Hour,
    (UINT32)Now.Minute,
    Mv,
    Chg ? "+" : "",
    (UINT32)Ctx->AbsCount
    );
  {
    INT32 Tw = (INT32)(AsciiStrLen (RightBuf) * 8U * 2U);
    Np05jUiDrawText (Ctx, (INT32)Ctx->ScreenW - Tw - 42, 44, 2, COL_ACCENT, RightBuf);
  }
}

VOID
Np05jUiSetBanner (
  IN OUT NP05J_UI_CTX                  *Ctx,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL  Color,
  IN     CONST CHAR8                   *Format,
  ...
  )
{
  VA_LIST Marker;

  VA_START (Marker, Format);
  AsciiVSPrint (Ctx->StatusBanner, sizeof (Ctx->StatusBanner), Format, Marker);
  VA_END (Marker);
  Ctx->StatusValid = TRUE;
  Ctx->StatusColor = Color;
}

VOID
Np05jUiDrawBannerIfAny (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  if (!Ctx->StatusValid || (Ctx->StatusBanner[0] == '\0')) {
    return;
  }
  Np05jUiDrawCard (
    Ctx,
    (NP05J_RECT){ 42, (INT32)Ctx->ScreenH - 128, (INT32)Ctx->ScreenW - 84, 54 },
    COL_PANEL,
    Ctx->StatusColor,
    3
    );
  Np05jUiDrawTextCentered (
    Ctx,
    (NP05J_RECT){ 42, (INT32)Ctx->ScreenH - 128, (INT32)Ctx->ScreenW - 84, 54 },
    2,
    Ctx->StatusColor,
    Ctx->StatusBanner
    );
}

STATIC
VOID
RestoreMouseBackground (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  if ((mPrevCursorX >= 0) && (mPrevCursorY >= 0)) {
    UINTN W = 20;
    UINTN H = 20;
    if ((UINT32)mPrevCursorX + W > Ctx->ScreenW) {
      W = Ctx->ScreenW - (UINT32)mPrevCursorX;
    }
    if ((UINT32)mPrevCursorY + H > Ctx->ScreenH) {
      H = Ctx->ScreenH - (UINT32)mPrevCursorY;
    }
    if ((W > 0) && (H > 0)) {
      Ctx->Gop->Blt (
                  Ctx->Gop,
                  Ctx->BackBuf,
                  EfiBltBufferToVideo,
                  (UINTN)mPrevCursorX,
                  (UINTN)mPrevCursorY,
                  (UINTN)mPrevCursorX,
                  (UINTN)mPrevCursorY,
                  W,
                  H,
                  Ctx->ScreenW * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                  );
    }
  }
}

STATIC
VOID
DrawCursorOverlay (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL White  = COL_WHITE;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Accent = COL_ACCENT;

  if (!Ctx->MouseVisible) {
    mPrevCursorX = -1;
    mPrevCursorY = -1;
    return;
  }

  RestoreMouseBackground (Ctx);

  for (INT32 Y = 0; Y < 18; Y++) {
    INT32 Width = (Y < 12) ? (Y + 1) : (18 - Y);
    if (Width < 1) {
      Width = 1;
    }
    if ((Ctx->CursorX >= 0) && ((UINT32)Ctx->CursorX < Ctx->ScreenW) &&
        (Ctx->CursorY + Y >= 0) && ((UINT32)(Ctx->CursorY + Y) < Ctx->ScreenH))
    {
      INT32 W = Width;
      if ((UINT32)(Ctx->CursorX + W) > Ctx->ScreenW) {
        W = (INT32)Ctx->ScreenW - Ctx->CursorX;
      }
      if (W > 0) {
        Ctx->Gop->Blt (Ctx->Gop, &White, EfiBltVideoFill, 0, 0, (UINTN)Ctx->CursorX, (UINTN)(Ctx->CursorY + Y), (UINTN)W, 1, 0);
        if (W > 3) {
          Ctx->Gop->Blt (Ctx->Gop, &Accent, EfiBltVideoFill, 0, 0, (UINTN)(Ctx->CursorX + 1), (UINTN)(Ctx->CursorY + Y), (UINTN)(W - 2), 1, 0);
        }
      }
    }
  }

  mPrevCursorX = Ctx->CursorX;
  mPrevCursorY = Ctx->CursorY;
}

VOID
Np05jUiPresent (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  mPrevCursorX = -1;
  mPrevCursorY = -1;
  Ctx->Gop->Blt (
              Ctx->Gop,
              Ctx->BackBuf,
              EfiBltBufferToVideo,
              0,
              0,
              0,
              0,
              Ctx->ScreenW,
              Ctx->ScreenH,
              Ctx->ScreenW * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
              );
  DrawCursorOverlay (Ctx);
  Ctx->Dirty = FALSE;
}

VOID
Np05jUiRefreshHandles (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  EFI_HANDLE *Handles = NULL;
  UINTN       Count   = 0;
  EFI_STATUS  Status;

  Ctx->AbsCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiAbsolutePointerProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status) && (Handles != NULL)) {
    for (UINTN I = 0; (I < Count) && (Ctx->AbsCount < NP05J_MAX_ABS_HANDLES); I++) {
      EFI_ABSOLUTE_POINTER_PROTOCOL *Ptr = NULL;
      if (!EFI_ERROR (gBS->HandleProtocol (Handles[I], &gEfiAbsolutePointerProtocolGuid, (VOID **)&Ptr)) && (Ptr != NULL)) {
        Ctx->AbsPtrs[Ctx->AbsCount++] = Ptr;
      }
    }
    FreePool (Handles);
  }

  Handles         = NULL;
  Count           = 0;
  Ctx->MouseCount = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimplePointerProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status) && (Handles != NULL)) {
    for (UINTN I = 0; (I < Count) && (Ctx->MouseCount < NP05J_MAX_MOUSE_HANDLES); I++) {
      EFI_SIMPLE_POINTER_PROTOCOL *Ptr = NULL;
      if (!EFI_ERROR (gBS->HandleProtocol (Handles[I], &gEfiSimplePointerProtocolGuid, (VOID **)&Ptr)) && (Ptr != NULL)) {
        Ctx->MousePtrs[Ctx->MouseCount++] = Ptr;
      }
    }
    FreePool (Handles);
  }
}

VOID
Np05jUiDrainInput (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  EFI_INPUT_KEY Key;

  for (UINTN I = 0; I < Ctx->AbsCount; I++) {
    EFI_ABSOLUTE_POINTER_STATE S;
    while (!EFI_ERROR (Ctx->AbsPtrs[I]->GetState (Ctx->AbsPtrs[I], &S))) {
    }
    Ctx->AbsWasDown[I] = FALSE;
  }

  for (UINTN I = 0; I < Ctx->MouseCount; I++) {
    EFI_SIMPLE_POINTER_STATE S;
    while (!EFI_ERROR (Ctx->MousePtrs[I]->GetState (Ctx->MousePtrs[I], &S))) {
    }
  }
  Ctx->MouseWasLeftDown = FALSE;
  Ctx->AnyTouchDown     = FALSE;

  if (gST->ConIn != NULL) {
    while (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
    }
  }
}

EFI_STATUS
Np05jUiInit (
  OUT NP05J_UI_CTX *Ctx
  )
{
  EFI_STATUS Status;

  ZeroMem (Ctx, sizeof (*Ctx));

  Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **)&Ctx->Gop);
  if (EFI_ERROR (Status) || (Ctx->Gop == NULL)) {
    Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Ctx->Gop);
  }
  if (EFI_ERROR (Status) || (Ctx->Gop == NULL) || (Ctx->Gop->Mode == NULL) || (Ctx->Gop->Mode->Info == NULL)) {
    return EFI_NOT_FOUND;
  }

  Ctx->ScreenW = Ctx->Gop->Mode->Info->HorizontalResolution;
  Ctx->ScreenH = Ctx->Gop->Mode->Info->VerticalResolution;
  Ctx->BackBuf = AllocateZeroPool ((UINTN)Ctx->ScreenW * Ctx->ScreenH * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (Ctx->BackBuf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ctx->CursorX = (INT32)(Ctx->ScreenW / 2U);
  Ctx->CursorY = (INT32)(Ctx->ScreenH / 2U);

  Np05jBiosConfigLoad (&Ctx->Config);
  Np05jUiRefreshHandles (Ctx);
  Np05jUiDrainInput (Ctx);

  if (gST->ConOut != NULL) {
    gST->ConOut->EnableCursor (gST->ConOut, FALSE);
  }

  return EFI_SUCCESS;
}

VOID
Np05jUiFree (
  IN OUT NP05J_UI_CTX *Ctx
  )
{
  if (Ctx->BackBuf != NULL) {
    FreePool (Ctx->BackBuf);
    Ctx->BackBuf = NULL;
  }
}

STATIC
INT32
MapAbsCoord (
  IN UINT64 Val,
  IN UINT64 Min,
  IN UINT64 Max,
  IN UINT32 DisplaySize
  )
{
  UINT64 Span;

  if ((Max <= Min) || (DisplaySize == 0U)) {
    return 0;
  }
  if (Val <= Min) {
    return 0;
  }
  if (Val >= Max) {
    return (INT32)(DisplaySize - 1U);
  }
  Span = Max - Min;
  return (INT32)(((Val - Min) * (UINT64)(DisplaySize - 1U)) / Span);
}

EFI_STATUS
Np05jUiPollInput (
  IN OUT NP05J_UI_CTX      *Ctx,
  IN     UINTN              TimeoutMs,
  OUT    NP05J_INPUT_EVENT *Event
  )
{
  UINTN ElapsedMs = 0;

  ZeroMem (Event, sizeof (*Event));

  while (TRUE) {
    /* 1. Poll touchscreen & absolute pointer handles */
    for (UINTN I = 0; I < Ctx->AbsCount; I++) {
      EFI_ABSOLUTE_POINTER_PROTOCOL *Ptr = Ctx->AbsPtrs[I];
      EFI_ABSOLUTE_POINTER_STATE     St;
      EFI_ABSOLUTE_POINTER_STATE     Latest;
      BOOLEAN                        GotAny = FALSE;

      while (!EFI_ERROR (Ptr->GetState (Ptr, &St))) {
        Latest = St;
        GotAny = TRUE;
      }

      if (GotAny) {
        INT32   X    = MapAbsCoord (Latest.CurrentX, Ptr->Mode->AbsoluteMinX, Ptr->Mode->AbsoluteMaxX, Ctx->ScreenW);
        INT32   Y    = MapAbsCoord (Latest.CurrentY, Ptr->Mode->AbsoluteMinY, Ptr->Mode->AbsoluteMaxY, Ctx->ScreenH);
        BOOLEAN Down = ((Latest.ActiveButtons & EFI_ABSP_TouchActive) != 0U);

        Ctx->AbsCurX[I] = X;
        Ctx->AbsCurY[I] = Y;

        if (Down && !Ctx->AbsWasDown[I]) {
          Ctx->AbsWasDown[I] = TRUE;
          Ctx->AbsStartX[I]  = X;
          Ctx->AbsStartY[I]  = Y;
          Ctx->AnyTouchDown  = TRUE;
          Ctx->ActiveTouchX  = X;
          Ctx->ActiveTouchY  = Y;
          Event->Kind        = Np05jInputTouchMove;
          Event->X           = X;
          Event->Y           = Y;
          Event->PointerDown = TRUE;
          return EFI_SUCCESS;
        } else if (Down) {
          Ctx->AnyTouchDown  = TRUE;
          Ctx->ActiveTouchX  = X;
          Ctx->ActiveTouchY  = Y;
          Event->Kind        = Np05jInputTouchMove;
          Event->X           = X;
          Event->Y           = Y;
          Event->PointerDown = TRUE;
          return EFI_SUCCESS;
        } else if (!Down && Ctx->AbsWasDown[I]) {
          Ctx->AbsWasDown[I] = FALSE;
          Ctx->AnyTouchDown  = FALSE;
          Event->Kind        = Np05jInputTap;
          Event->X           = X;
          Event->Y           = Y;
          Event->PointerDown = FALSE;
          return EFI_SUCCESS;
        }
      }
    }

    /* 2. Poll USB mice / SimplePointer handles */
    for (UINTN I = 0; I < Ctx->MouseCount; I++) {
      EFI_SIMPLE_POINTER_PROTOCOL *Ptr = Ctx->MousePtrs[I];
      EFI_SIMPLE_POINTER_STATE     St;

      if (!EFI_ERROR (Ptr->GetState (Ptr, &St))) {
        INT64 ResX = (Ptr->Mode->ResolutionX > 0) ? (INT64)Ptr->Mode->ResolutionX : 1LL;
        INT64 ResY = (Ptr->Mode->ResolutionY > 0) ? (INT64)Ptr->Mode->ResolutionY : 1LL;
        INT32 Dx   = (INT32)(((INT64)St.RelativeMovementX * 4LL) / ResX);
        INT32 Dy   = (INT32)(((INT64)St.RelativeMovementY * 4LL) / ResY);

        if ((Dx == 0) && (St.RelativeMovementX != 0)) {
          Dx = (St.RelativeMovementX > 0) ? 2 : -2;
        }
        if ((Dy == 0) && (St.RelativeMovementY != 0)) {
          Dy = (St.RelativeMovementY > 0) ? 2 : -2;
        }

        Ctx->MouseVisible = TRUE;
        Ctx->CursorX     += Dx;
        Ctx->CursorY     += Dy;
        if (Ctx->CursorX < 0) {
          Ctx->CursorX = 0;
        }
        if (Ctx->CursorX >= (INT32)Ctx->ScreenW) {
          Ctx->CursorX = (INT32)Ctx->ScreenW - 1;
        }
        if (Ctx->CursorY < 0) {
          Ctx->CursorY = 0;
        }
        if (Ctx->CursorY >= (INT32)Ctx->ScreenH) {
          Ctx->CursorY = (INT32)Ctx->ScreenH - 1;
        }

        if (St.RightButton) {
          Event->Kind = Np05jInputBack;
          return EFI_SUCCESS;
        }

        if (Ctx->MouseWasLeftDown && !St.LeftButton) {
          Ctx->MouseWasLeftDown = FALSE;
          Event->Kind           = Np05jInputTap;
          Event->X              = Ctx->CursorX;
          Event->Y              = Ctx->CursorY;
          return EFI_SUCCESS;
        }
        Ctx->MouseWasLeftDown = St.LeftButton;
        DrawCursorOverlay (Ctx);
      }
    }

    /* 3. Poll keypad / keyboard / serial ConIn */
    if (gST->ConIn != NULL) {
      EFI_INPUT_KEY Key;
      if (!EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) {
        Event->X = 0;
        Event->Y = 0;
        if ((Key.ScanCode == SCAN_UP) || (Key.UnicodeChar == L'k') || (Key.UnicodeChar == L'K')) {
          Event->Kind = Np05jInputUp;
          return EFI_SUCCESS;
        }
        if ((Key.ScanCode == SCAN_DOWN) || (Key.UnicodeChar == L'j') || (Key.UnicodeChar == L'J')) {
          Event->Kind = Np05jInputDown;
          return EFI_SUCCESS;
        }
        if ((Key.ScanCode == SCAN_LEFT) || (Key.UnicodeChar == L'h') || (Key.UnicodeChar == L'-')) {
          Event->Kind = Np05jInputLeft;
          return EFI_SUCCESS;
        }
        if ((Key.ScanCode == SCAN_RIGHT) || (Key.UnicodeChar == L'l') || (Key.UnicodeChar == L'+') || (Key.UnicodeChar == L'=')) {
          Event->Kind = Np05jInputRight;
          return EFI_SUCCESS;
        }
        if ((Key.UnicodeChar == CHAR_CARRIAGE_RETURN) || (Key.UnicodeChar == CHAR_LINEFEED) || (Key.UnicodeChar == L' ')) {
          Event->Kind = Np05jInputEnter;
          return EFI_SUCCESS;
        }
        if ((Key.ScanCode == SCAN_ESC) || (Key.UnicodeChar == CHAR_BACKSPACE) || (Key.UnicodeChar == L'q') || (Key.UnicodeChar == L'Q')) {
          Event->Kind = Np05jInputBack;
          return EFI_SUCCESS;
        }
      }
    }

    if ((TimeoutMs != MAX_UINTN) && (ElapsedMs >= TimeoutMs)) {
      return EFI_TIMEOUT;
    }

    gBS->Stall (10000); /* 10 ms slice */
    ElapsedMs += 10;
  }
}
