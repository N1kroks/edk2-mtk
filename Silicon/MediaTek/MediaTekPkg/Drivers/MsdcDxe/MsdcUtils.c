#include "MsdcDxe.h"

VOID MsdcWrite (
  IN UINT32 Offset,
  IN UINT32 Value
  )
{
  MmioWrite32 (PlatformInfo.MsdcMmioReg + Offset, Value);
}

VOID MsdcRead (
  IN  UINT32  Offset,
  OUT UINT32 *Value
  )
{
  *Value = MmioRead32 (PlatformInfo.MsdcMmioReg + Offset);
}

VOID MsdcTopWrite (
  IN UINT32 Offset,
  IN UINT32 Value
  )
{
  MmioWrite32 (PlatformInfo.TopMmioReg + Offset, Value);
}

VOID MsdcTopRead (
  IN  UINT32  Offset,
  OUT UINT32 *Value
  )
{
  *Value = MmioRead32 (PlatformInfo.TopMmioReg + Offset);
}

VOID MsdcSetBits (
  IN UINT32 Offset,
  IN UINT32 BitMask
  )
{
  UINT32 Reg;
  MsdcRead (Offset, &Reg);
  Reg |= BitMask;
  MsdcWrite (Offset, Reg);
}

VOID MsdcClrSetBits (
  IN UINT32 Offset,
  IN UINT32 BitMask,
  IN UINT32 BitMaskSet
  )
{
  UINT32 Reg;
  MsdcRead (Offset, &Reg);
  Reg &= ~BitMask;
  Reg |= BitMaskSet;
  MsdcWrite (Offset, Reg);
}

VOID MsdcClrBits (
  IN UINT32 Offset,
  IN UINT32 BitMask
  )
{
  UINT32 Reg;
  MsdcRead (Offset, &Reg);
  Reg &= ~BitMask;
  MsdcWrite (Offset, Reg);
}

VOID MsdcTopSetBits (
  IN UINT32 Offset,
  IN UINT32 BitMask
  )
{
  UINT32 Reg;
  MsdcTopRead (Offset, &Reg);
  Reg |= BitMask;
  MsdcTopWrite (Offset, Reg);
}

VOID MsdcTopClrSetBits (
  IN UINT32 Offset,
  IN UINT32 BitMask,
  IN UINT32 BitMaskSet
  )
{
  UINT32 Reg;
  MsdcTopRead (Offset, &Reg);
  Reg &= ~BitMask;
  Reg |= BitMaskSet;
  MsdcTopWrite (Offset, Reg);
}

VOID MsdcTopClrBits (
  IN UINT32 Offset,
  IN UINT32 BitMask
  )
{
  UINT32 Reg;
  MsdcTopRead (Offset, &Reg);
  Reg &= ~BitMask;
  MsdcTopWrite (Offset, Reg);
}