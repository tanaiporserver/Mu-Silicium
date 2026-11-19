/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/** @file
  Freestanding memcpy/memset/memcmp symbols for bare-metal BootSelect.
**/

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *Dst, const void *Src, size_t Len) {
  uint8_t       *D = (uint8_t *)Dst;
  const uint8_t *S = (const uint8_t *)Src;
  for (size_t I = 0; I < Len; I++) {
    D[I] = S[I];
  }
  return Dst;
}

void *memset(void *Dst, int Val, size_t Len) {
  uint8_t *D = (uint8_t *)Dst;
  for (size_t I = 0; I < Len; I++) {
    D[I] = (uint8_t)Val;
  }
  return Dst;
}

int memcmp(const void *A, const void *B, size_t Len) {
  const uint8_t *Pa = (const uint8_t *)A;
  const uint8_t *Pb = (const uint8_t *)B;
  for (size_t I = 0; I < Len; I++) {
    if (Pa[I] != Pb[I]) {
      return (Pa[I] < Pb[I]) ? -1 : 1;
    }
  }
  return 0;
}
