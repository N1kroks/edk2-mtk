#ifndef __PROTOCOL_MTK_CLK_H__
#define __PROTOCOL_MTK_CLK_H__
//
// Protocol interface structure
//

typedef struct _MTK_CLK MTK_CLK;

enum MT8183_PLL {
  MT8183_ARMPLL_LL,
  MT8183_ARMPLL_L,
  MT8183_CCIPLL,
  MT8183_MAINPLL,
  MT8183_UNIV2PLL,
  MT8183_MFGPLL,
  MT8183_MSDCPLL,
  MT8183_TVDPLL,
  MT8183_MMPLL,
  MT8183_UNIVPLL,
  // FAKE PLL
  // Should be always last enum
  MT8183_CLK26M,
};

enum MT8183_MCUSYS_CLKS {
  MT8183_MCUSYS_MP0,
  MT8183_MCUSYS_MP2,
  MT8183_MCUSYS_BUS
};

//
// Function Prototypes
//

typedef
VOID
(EFIAPI *MTK_CLK_GET_RATE)(
  IN  UINT32 ClkId,
  OUT UINT64 *Value
  );

typedef
VOID
(EFIAPI *MTK_CLK_SET_RATE)(
  IN UINT32 ClkId,
  IN UINT64 Freq
  );

typedef
VOID
(EFIAPI *MTK_CLK_STA)(
  IN  UINT32   ClkId,
  OUT BOOLEAN *Status
  );

typedef
VOID
(EFIAPI *MTK_CLK_CTRL)(
  IN UINT32  ClkId,
  IN BOOLEAN Ctrl
  );

typedef
VOID
(EFIAPI *MTK_CLK_SET_PARENT)(
  IN UINT32 ClkId,
  IN UINT32 ParentId
  );

struct _MTK_CLK {
  MTK_CLK_GET_RATE    PllGetRate;
  MTK_CLK_SET_RATE    PllSetRate;
  MTK_CLK_STA         TopClkStatus;
  MTK_CLK_CTRL        TopClkControl;
  MTK_CLK_GET_RATE    TopClkGetRate;
  MTK_CLK_STA         InfraClkStatus;
  MTK_CLK_CTRL        InfraClkControl;
  MTK_CLK_SET_PARENT  McuSysSetParent;
};

extern EFI_GUID gMediaTekClockProtocolGuid;

#endif // __PROTOCOL_MTK_CLK_H__
