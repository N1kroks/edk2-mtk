#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/MtkClk.h>

#define GENMASK(h, l)  (((1ULL << ((h) - (l) + 1)) - 1) << (l))
#define MHZ (1000ULL * 1000ULL)

#define XTAL_FREQ (26ULL * MHZ)
#define PLL_FMIN (1500ULL * MHZ)
#define PLL_FMAX (3800ULL * MHZ)

#define INTEGER_BITS    (8)
#define PCW_CHG_BIT     (31)

#define TOPCKGEN (0x10000000)
#define INFRACFG (0x10001000)
#define APMIXED  (0x1000c000)
#define MCUCFG   (0x0c530000)

#define APMIXED_PCW_CHG (APMIXED + 0x4)

struct MTK_PLL {
  CHAR8 *Name;
  UINT32 PllReg;
  UINT32 PwrReg;
  UINT32 PostDivReg;
  UINT32 PcwReg;
  UINT32 PcwChgReg;
  UINT8 PostDivShift;
  UINT8 PcwShift;
  UINT8 PcwBits;
  UINT8 DivTable;
};

struct MTK_FACTOR {
  UINT8  PllId;
  UINT32 Mult;
  UINT32 Div;
};

struct MTK_MUX_GATE {
  UINT32 StaReg;
  UINT32 SetReg;
  UINT32 ClrReg;
  UINT32 MuxShift;
  UINT32 MuxBits;
  UINT32 PdnShift;
};

struct MTK_MUX {
  UINT32 MuxReg;
  UINT32 MuxShift;
  UINT32 MuxBits;
};

struct MTK_GATE {
  UINT32 StaReg;
  UINT32 SetReg;
  UINT32 ClrReg;
  UINT32 PdnShift;
};

struct MTK_PLL_DIV_TABLE {
  UINT32 Div;
  UINT64 Freq;
};

struct MTK_PLL_DIV_TABLE ArmPllDivTable[] = {
  { .Div = 0, .Freq = PLL_FMAX },
  { .Div = 1, .Freq = 1600 * MHZ },
  { .Div = 2, .Freq = 800 * MHZ },
  { .Div = 3, .Freq = 400 * MHZ },
  { .Div = 4, .Freq = 200 * MHZ },
};

struct MTK_PLL_DIV_TABLE *GlobalDivTable[] = {
  ArmPllDivTable,
  ArmPllDivTable,
};

// APLL1 and APLL2 are skipped  
struct MTK_PLL Plls[] = {
  { .Name = "ARMPLL_LL", .PllReg = 0x200, .PwrReg = 0x20c, .PostDivReg = 0x204, .PcwReg = 0x204,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 1 },
  { .Name = "ARMPLL_L", .PllReg = 0x210, .PwrReg = 0x21c, .PostDivReg = 0x214, .PcwReg = 0x214,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 2 },
  { .Name = "CCIPLL", .PllReg = 0x290, .PwrReg = 0x29c, .PostDivReg = 0x294, .PcwReg = 0x294,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "MAINPLL", .PllReg = 0x220, .PwrReg = 0x22c, .PostDivReg = 0x224, .PcwReg = 0x224,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "UNIV2PLL", .PllReg = 0x230, .PwrReg = 0x23c, .PostDivReg = 0x234, .PcwReg = 0x234,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "MFGPLL", .PllReg = 0x240, .PwrReg = 0x24c, .PostDivReg = 0x244, .PcwReg = 0x244,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "MSDCPLL", .PllReg = 0x250, .PwrReg = 0x25c, .PostDivReg = 0x254, .PcwReg = 0x254,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "TVDPLL", .PllReg = 0x260, .PwrReg = 0x26c, .PostDivReg = 0x264, .PcwReg = 0x264,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
  { .Name = "MMPLL", .PllReg = 0x270, .PwrReg = 0x27c, .PostDivReg = 0x274, .PcwReg = 0x274,
    .PcwChgReg = 0, .PostDivShift = 24, .PcwShift = 0, .PcwBits = 22, .DivTable = 0 },
};

struct MTK_FACTOR TopFactors[] = {
};

struct MTK_MUX_GATE TopGates[] = {
};

struct MTK_GATE InfraGates[] = {
};

struct MTK_MUX McuMuxes[] = {
  { .MuxReg = 0x7a0, .MuxShift = 9, .MuxBits = 2}, // MP0
  { .MuxReg = 0x7a8, .MuxShift = 9, .MuxBits = 2}, // MP2
  { .MuxReg = 0x7c0, .MuxShift = 9, .MuxBits = 2}, // BUS
};

UINT32 Mcu_Mp0_Parents[] = {
  MT8183_CLK26M,
  MT8183_ARMPLL_LL,
  MT8183_MAINPLL,
  MT8183_UNIVPLL
};

UINT32 Mcu_Mp2_Parents[] = {
  MT8183_CLK26M,
  MT8183_ARMPLL_L,
  MT8183_MAINPLL,
  MT8183_UNIVPLL
};

UINT32 Mcu_Bus_Parents[] = {
  MT8183_CLK26M,
  MT8183_CCIPLL,
  MT8183_MAINPLL,
  MT8183_UNIVPLL
};

UINT32 *McuSysParents[] = {
  Mcu_Mp0_Parents,
  Mcu_Mp2_Parents,
  Mcu_Bus_Parents
};

UINT32 *TopClkParents[] = {
};

VOID PllCalcValues (
  IN  UINT32  PllId,
  IN  UINT64  Freq,
  OUT UINT32 *PcwR,
  OUT UINT32 *PostDivR
  )
{
  struct MTK_PLL Pll = Plls[PllId];

  UINT32 Val, PostDiv;
  UINT64 Pcw;

  PostDiv = 0;

  if (Pll.DivTable) {
    struct MTK_PLL_DIV_TABLE *DivTable = GlobalDivTable[Pll.DivTable - 1];

    if (Freq > DivTable[0].Freq)
      Freq = DivTable[0].Freq;

    for (Val = 0; DivTable[Val + 1].Freq != 0; Val++)
    {
      PostDiv = 1 << Val;
      if (Freq > DivTable[Val + 1].Freq)
        break;
    }
  } else {
    for (Val = 0; Val < 5; Val++)
    {
      PostDiv = 1 << Val;
      if (Freq * PostDiv >= PLL_FMIN)
        break;
    }
  }

  Pcw = (Freq << Val) << (Pll.PcwBits - INTEGER_BITS);
  Pcw /= XTAL_FREQ;
  *PcwR = Pcw;
  *PostDivR = PostDiv;
}

VOID PllSetRate (
  IN UINT32 PllId,
  IN UINT64 Freq
  )
{
  struct MTK_PLL Pll = Plls[PllId];
  UINT32 Val, Chg, PostDiv, Pcw;

  PllCalcValues (PllId, Freq, &Pcw, &PostDiv);

  Val = MmioRead32 (APMIXED + Pll.PostDivReg);
  Val &= ~(GENMASK (2, 0) << 24);
  PostDiv >>= 1;
  Val |= (PostDiv & GENMASK (2, 0)) << 24;

  if (Pll.PostDivReg != Pll.PcwReg) {
    MmioWrite32 (APMIXED + Pll.PostDivReg, Val);
    Val = MmioRead32 (APMIXED + Pll.PcwReg);
  }

  Val &= ~GENMASK (Pll.PcwShift + Pll.PcwBits - 1, Pll.PcwShift);
  Val |= Pcw << Pll.PcwShift;
  MmioWrite32 (APMIXED + Pll.PcwReg, Val);

  if (Pll.PcwChgReg) {
    Chg = MmioRead32 (Pll.PcwChgReg) | (1 << PCW_CHG_BIT);
    MmioWrite32 (Pll.PcwChgReg, Chg);
  } else {
    Chg = MmioRead32 (Pll.PllReg + 0x4) | (1 << PCW_CHG_BIT);
    MmioWrite32 (Pll.PllReg + 0x4, Chg);
  }
}

VOID PllGetRate (
  IN  UINT32  PllId,
  OUT UINT64 *Value
  )
{
  struct MTK_PLL Pll = Plls[PllId];

  UINT32 Pcw, PostDiv, Pcwfbits, Pll_L;
  UINT64 Vco, PllRate;

  if (PllId == MT8183_CLK26M) {
    *Value = XTAL_FREQ;
    return;
  }

  PostDiv = MmioRead32 (APMIXED + Pll.PostDivReg);
  Pcw = MmioRead32 (APMIXED + Pll.PcwReg);

  PostDiv = (PostDiv >> Pll.PostDivShift) & GENMASK(2, 0);
  PostDiv = 1 << PostDiv;

  Pcw = Pcw >> Pll.PcwShift;
  Pcw = Pcw & GENMASK (Pll.PcwBits - 1, 0);
  Pcwfbits = Pll.PcwBits - INTEGER_BITS;
  Vco = (XTAL_FREQ * (UINT64)Pcw);
  Vco = Vco >> Pcwfbits;

  PllRate = (Vco + PostDiv - 1) / PostDiv;

  // round pll rate, beacuse it has some jitter
  Pll_L = PllRate - ((PllRate / MHZ)*MHZ);
  Pll_L = MHZ - Pll_L;
  if (Pll_L <= 5000) {
    PllRate += Pll_L;
  }

  *Value = PllRate;
}

VOID TopClkStatus (
  IN  UINT32   ClkId,
  OUT BOOLEAN *Status
  )
{
  struct MTK_MUX_GATE Gate = TopGates[ClkId];
  UINT32 Reg;

  Reg = MmioRead32 (TOPCKGEN + Gate.StaReg);
  *Status = !(Reg & (1 << Gate.PdnShift));
}

VOID TopClkControl (
  IN UINT32  ClkId,
  IN BOOLEAN Ctrl
  )
{
  struct MTK_MUX_GATE Gate = TopGates[ClkId];
  BOOLEAN Status;

  TopClkStatus (ClkId, &Status);
  if (Status == Ctrl)
    return;
  if (Ctrl)
    MmioWrite32 (TOPCKGEN + Gate.ClrReg, (1 << Gate.PdnShift));
  else
  MmioWrite32 (TOPCKGEN + Gate.SetReg, (1 << Gate.PdnShift));
}

VOID TopClkGetRate (
  IN UINT32   ClkId,
  OUT UINT64 *Value
  )
{
  UINT64 Rate;
  UINT32 Reg, ParentId, FactorId;
  UINT32 *Parents;
  struct MTK_MUX_GATE Gate;
  struct MTK_FACTOR Factor;

  Gate = TopGates[ClkId];
  Parents = TopClkParents[ClkId];

  Reg = MmioRead32 (TOPCKGEN + Gate.StaReg);
  ParentId = (Reg >> Gate.MuxShift) & ((1 << Gate.MuxBits) - 1);
  
  FactorId = Parents[ParentId];
  Factor = TopFactors[FactorId];

  PllGetRate (Factor.PllId, &Rate);
  Rate *= Factor.Mult;
  Rate /= Factor.Div;
  
  *Value = Rate;
}

VOID InfraClkStatus (
  IN  UINT32   ClkId,
  OUT BOOLEAN *Status
  )
{
  struct MTK_GATE Gate = InfraGates[ClkId];
  UINT32 Reg;

  Reg = MmioRead32 (INFRACFG + Gate.StaReg);
  *Status = !(Reg & (1 << Gate.PdnShift));
}

VOID InfraClkControl (
  IN UINT32  ClkId,
  IN BOOLEAN Ctrl
  )
{
  struct MTK_GATE Gate = InfraGates[ClkId];
  BOOLEAN Status;

  InfraClkStatus (ClkId, &Status);

  if ( Status == Ctrl)
    return;
  if (Ctrl)
    MmioWrite32 (INFRACFG + Gate.ClrReg, (1 << Gate.PdnShift));
  else
  MmioWrite32 (INFRACFG + Gate.SetReg, (1 << Gate.PdnShift));
}

VOID McuSysSetParent (
  IN UINT32   ClkId,
  IN UINT32   ParentId
  )
{
  UINT32 Reg;
  UINT32 *Parents;
  UINT8 Pid;
  struct MTK_MUX Mux;

  Mux = McuMuxes[ClkId];
  Parents = McuSysParents[ClkId];

  for (Pid = 0; Pid >= ((1 << Mux.MuxBits) - 1); Pid++)
  {
    if (Parents[Pid] == ParentId)
      break;
  }

  Reg = MmioRead32 (MCUCFG + Mux.MuxReg);
  Reg &= ~((Reg >> Mux.MuxShift) & ((1 << Mux.MuxBits) - 1));
  Reg |= (Pid & ((1 << Mux.MuxBits) - 1)) << Mux.MuxShift;
  MmioWrite32 (MCUCFG + Mux.MuxReg, Reg);
}

MTK_CLK MtkClk = {
  PllGetRate,
  PllSetRate,
  TopClkStatus,
  TopClkControl,
  TopClkGetRate,
  InfraClkStatus,
  InfraClkControl,
  McuSysSetParent
};

EFI_STATUS
EFIAPI
ClockDxeInitialize (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
) {
  EFI_STATUS Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
        &ImageHandle, &gMediaTekClockProtocolGuid, &MtkClk, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MT8183ClockDxe: Failed to install protocol, Status = %r\n", Status));
    return Status;
  }

  return Status;
}