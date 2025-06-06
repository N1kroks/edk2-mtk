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

// FIXME: APLL1 and APLL2 have tuner reg
struct MTK_PLL Plls[] = {
  {0x20c, 0x20c, 24, 0, 22}, // ARMPLL_L
  {0x21c, 0x21c, 24, 0, 22}, // ARMPLL_B
  {0x25c, 0x25c, 24, 0, 22}, // CCIPLL
  {0x394, 0x394, 24, 0, 22}, // MPLL
  {0x344, 0x344, 24, 0, 22}, // MAINPLL
  {0x30c, 0x30c, 24, 0, 22}, // UNIVPLL
  {0x354, 0x354, 24, 0, 22}, // MSDCPLL
  {0x364, 0x364, 24, 0, 22}, // MMPLL
  {0x3b8, 0x3b8, 24, 0, 22}, // NPUPLL
  {0x26c, 0x26c, 24, 0, 22}, // MFGPLL
  {0x384, 0x384, 24, 0, 22}, // TVDPLL
  {0x31c, 0x320, 24, 0, 32}, // APLL1
  {0x330, 0x334, 24, 0, 32}, // APLL2
  {0x3c4, 0x3c4, 24, 0, 22}, // USBPLL
};

struct MTK_FACTOR TopFactors[] = {
  /* MSDC30_1 parents */
  {MT6789_MSDCPLL, 1, 2},  // MSDCPLL_D2
  {MT6789_CLK26M,  1, 1},  // TCK_26M_MX9
  {MT6789_UNIVPLL, 1, 12}, // UNIVPLL_D6_D2
  {MT6789_MAINPLL, 1, 12}, // MAINPLL_D6_D2
  {MT6789_MAINPLL, 1, 14}, // MAINPLL_D7_D2
  {MT6789_MAINPLL, 1, 16}, // MAINPLL_D4_D4
  {MT6789_MAINPLL, 1, 32}, // MAINPLL_D4_D8
  {MT6789_MAINPLL, 1, 10}, // MAINPLL_D5_D2
  {MT6789_UNIVPLL, 1, 16}, // UNIVPLL_D4_D4
};

struct MTK_MUX_GATE TopGates[] = {
  {0x80, 0x84, 0x88,  8, 3, 15}, // MSDC30_1
  {0xd0, 0xd4, 0xd8, 24, 3, 31}  // UFS_SEL
};

struct MTK_GATE InfraGates[] = {
  {0x94, 0x88, 0x8c, 4},  // MSDC1
  {0x94, 0x88, 0x8c, 16}, // MSDC1_SRC
  {0xac, 0xa4, 0xa8, 11}, // UNIPRO_SYSCLK
  {0xac, 0xa4, 0xa8, 13}, // UFS_SAP_BCLK
  {0xac, 0xa4, 0xa8, 28}, // UFS
};

UINT32 Msdc30_1_parents[] = {
  MT6789_TCK_26M_MX9,
  MT6789_UNIVPLL_D6_D2,
  MT6789_MAINPLL_D6_D2,
  MT6789_MAINPLL_D7_D2,
  MT6789_MSDCPLL_D2,
};

UINT32 Ufs_sel_parents[] = {
  MT6789_TCK_26M_MX9,
  MT6789_MAINPLL_D4_D4,
  MT6789_MAINPLL_D4_D8,
  MT6789_UNIVPLL_D4_D4,
  MT6789_MAINPLL_D6_D2,
  MT6789_MAINPLL_D5_D2,
  MT6789_MSDCPLL_D2
};

UINT32 *TopClkParents[] = {
    Msdc30_1_parents,
    Ufs_sel_parents,
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

  if (PllId == MT6789_CLK26M) {
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
    DEBUG ((DEBUG_ERROR, "MT6789ClockDxe: Failed to install protocol, Status = %r\n", Status));
    return Status;
  }

  return Status;
}