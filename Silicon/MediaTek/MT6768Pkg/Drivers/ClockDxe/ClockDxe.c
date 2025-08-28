#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/MtkClk.h>

#define GENMASK(h, l)  (((1ULL << ((h) - (l) + 1)) - 1) << (l))
#define MHZ (1000 * 1000)

#define XTAL_FREQ (26 * MHZ)
#define PLL_FMIN (1500 * MHZ)
#define PLL_FMAX (3800 * MHZ)

#define INTEGER_BITS    (8)
#define PCW_CHG_BIT     (31)

#define TOPCKGEN (0x10000000)
#define INFRACFG (0x10001000)
#define APMIXED  (0x1000c000)

#define APMIXED_PCW_CHG (APMIXED + 0x4)

struct MTK_PLL {
  UINT32 PostDivReg;
  UINT32 PcwReg;
  UINT8 PostDivShift;
  UINT8 PcwShift;
  UINT8 PcwBits;
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

struct MTK_GATE {
  UINT32 StaReg;
  UINT32 SetReg;
  UINT32 ClrReg;
  UINT32 PdnShift;
};

// FIXME: APLL1 have tuner reg
struct MTK_PLL Plls[] = {
  {0x21c, 0x21c, 24, 0, 22}, // ARMPLL_L
  {0x20c, 0x20c, 24, 0, 22}, // ARMPLL
  {0x22c, 0x22c, 24, 0, 22}, // CCIPLL
  {0x25c, 0x25c, 24, 0, 22}, // MAINPLL
  {0x24c, 0x24c, 24, 0, 22}, // MFGPLL
  {0x320, 0x320, 24, 0, 22}, // MMPLL
  {0x23c, 0x23c, 24, 0, 22}, // UNIV2PLL
  {0x320, 0x340, 24, 0, 22}, // MSDCPLL
  {0x30c, 0x310, 24, 0, 32}, // APLL1
  {0x330, 0x330, 24, 0, 22}, // MPLL
};

struct MTK_FACTOR TopFactors[] = {
  {MT6768_CLK26M,   1,   1}, // CLK26M_CK
  {MT6768_MSDCPLL,  1,   1}, // MSDCPLL_CK
  {MT6768_MSDCPLL,  1,   2}, // MSDCPLL_D2
  {MT6768_MAINPLL,  1,   4}, // SYSPLL1_D2
  {MT6768_MAINPLL,  1,   8}, // SYSPLL1_D4
  {MT6768_MAINPLL,  1,   6}, // SYSPLL2_D2
  {MT6768_MAINPLL,  1,  12}, // SYSPLL2_D4
  {MT6768_MAINPLL,  1,  14}, // SYSPLL4_D2
  {MT6768_UNIV2PLL, 1,  10}, // UNIVPLL_D5
  {MT6768_UNIV2PLL, 1,   8}, // UNIVPLL1_D2
  {MT6768_UNIV2PLL, 1,  16}, // UNIVPLL1_D4
  {MT6768_UNIV2PLL, 1,  12}, // UNIVPLL2_D2
  {MT6768_UNIV2PLL, 2, 104}, // USB20_192M_D4
};

struct MTK_MUX_GATE TopGates[] = {
  {0x70, 0x74, 0x78,  8, 3, 15}, // MSDC50_0
  {0x70, 0x74, 0x78, 16, 3, 23}  // MSDC30_1
};

struct MTK_GATE InfraGates[] = {
  {0x94, 0x88, 0x8c, 2},  // MSDC0
  {0x94, 0x88, 0x8c, 4},  // MSDC1
  {0xac, 0xa4, 0xa8, 29}, // FAES_FDE
  {0xc8, 0xc0, 0xc4, 9},  // MSDC0_SRC
  {0xc8, 0xc0, 0xc4, 10}, // MSDC1_SRC
};

UINT32 Msdc50_0_parents[] = {
  MT6768_CLK26M_CK,
  MT6768_MSDCPLL_CK,
  MT6768_SYSPLL2_D2,
  MT6768_SYSPLL4_D2,
  MT6768_UNIVPLL1_D2,
  MT6768_SYSPLL1_D2,
  MT6768_UNIVPLL_D5,
  MT6768_UNIVPLL1_D4
};

UINT32 Msdc30_1_parents[] = {
  MT6768_CLK26M_CK,
  MT6768_MSDCPLL_D2,
  MT6768_UNIVPLL2_D2,
  MT6768_SYSPLL2_D2,
  MT6768_SYSPLL1_D4,
  MT6768_UNIVPLL1_D4,
  MT6768_USB20_192M_D4,
  MT6768_SYSPLL2_D4
};

UINT32 *TopClkParents[] = {
    Msdc50_0_parents,
    Msdc30_1_parents,
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

  for (Val = 0; Val < 5; Val++)
  {
    PostDiv = 1 << Val;
    if (Freq * PostDiv >= PLL_FMIN)
      break;
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

  Chg = MmioRead32 (APMIXED_PCW_CHG) | (1 << PCW_CHG_BIT);
  MmioWrite32 (APMIXED_PCW_CHG, Chg);
}

VOID PllGetRate (
  IN  UINT32  PllId,
  OUT UINT64 *Value
  )
{
  struct MTK_PLL Pll = Plls[PllId];

  UINT32 Pcw, PostDiv, Pcwfbits, Pll_L;
  UINT64 Vco, PllRate;

  if (PllId == MT6768_CLK26M) {
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

MTK_CLK MtkClk = {
  PllGetRate,
  PllSetRate,
  TopClkStatus,
  TopClkControl,
  TopClkGetRate,
  InfraClkStatus,
  InfraClkControl
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
    DEBUG ((DEBUG_ERROR, "MT6768ClockDxe: Failed to install protocol, Status = %r\n", Status));
    return Status;
  }

  return Status;
}