#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#include <Protocol/MT6358Pmic.h>
#include <Protocol/MtkClk.h>

MT6358_PMIC        *MtkPmic;
MTK_CLK            *MtkClk;

VOID CpuSetMaxFrequency ()
{
  // set voltage
  // reparent to intermediate clk
  // set rate armpll to target frequency
  // reparent to original clk

  UINT32 ArmPllMaxFrequency = 1989 * 1000 * 1000; // 1989MHz
  UINT32 CciPllMaxFrequency = 1196 * 1000 * 1000; // 1196MHz
  // Regulators
  MtkPmic->BuckSetMicroVoltage (MT6358_BUCK_VPROC12, 1050000);
  MtkPmic->BuckSetMicroVoltage (MT6358_BUCK_VPROC11, 1050000);
  MtkPmic->BuckSetMicroVoltage (MT6358_BUCK_VSRAM_PROC12, 1050000);
  MtkPmic->BuckSetMicroVoltage (MT6358_BUCK_VSRAM_PROC11, 1050000);
  MicroSecondDelay (1000);
  // CPU0-CPU3
  MtkClk->McuSysSetParent (MT8183_MCUSYS_MP0, MT8183_MAINPLL);
  MicroSecondDelay (1000);
  MtkClk->PllSetRate (MT8183_ARMPLL_LL, ArmPllMaxFrequency);
  MicroSecondDelay (1000);
  MtkClk->McuSysSetParent (MT8183_MCUSYS_MP0, MT8183_ARMPLL_LL);
  MicroSecondDelay (1000);
  // CPU4-CPU7
  MtkClk->McuSysSetParent (MT8183_MCUSYS_MP2, MT8183_MAINPLL);
  MicroSecondDelay (1000);
  MtkClk->PllSetRate (MT8183_ARMPLL_L, ArmPllMaxFrequency);
  MicroSecondDelay (1000);
  MtkClk->McuSysSetParent (MT8183_MCUSYS_MP2, MT8183_ARMPLL_L);
  MicroSecondDelay (1000);
  // Cache Coherency
  MtkClk->McuSysSetParent (MT8183_MCUSYS_BUS, MT8183_MAINPLL);
  MicroSecondDelay (1000);
  MtkClk->PllSetRate (MT8183_CCIPLL, CciPllMaxFrequency);
  MicroSecondDelay (1000);
  MtkClk->McuSysSetParent (MT8183_MCUSYS_BUS, MT8183_CCIPLL);
}

EFI_STATUS EFIAPI CpuDxeInitialize (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
  ) {
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gMediaTekMT6358PmicProtocolGuid, NULL, (VOID **)&MtkPmic);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "CPUDxe: Failed to locate Pmic protocol: %r \n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gMediaTekClockProtocolGuid, NULL, (VOID **)&MtkClk);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "CPUDxe: Failed to locate Clock protocol: %r \n", Status));
    return Status;
  }

  CpuSetMaxFrequency ();
  DEBUG ((DEBUG_INFO, "CPUDxe: Successfully updated CPU frequency! \n"));

  return Status;
}