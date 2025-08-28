#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#include <Protocol/MT6358Pmic.h>
#include <Protocol/MtkClk.h>
#include <Protocol/MtkGpio.h>
#include <Protocol/MsdcPlatform.h>

MT6358_PMIC *MtkPmic;
MTK_CLK *MtkClk;
MTK_GPIO *MtkGpio;

VOID
GetSourceClockRate (
  UINT32 Index,
  UINTN *Hz
  )
{
  MtkClk->TopClkGetRate (Index == 0 ? MT6768_MSDC50_0 : MT6768_MSDC30_1, Hz);
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Source clock rate: %llu, Index: %d \n", *Hz, Index));
}

VOID
SourceClockControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Control source clock index: %d, Enable %d!\n", Index, Enable));
  MtkClk->InfraClkControl (Index == 0 ? MT6768_MSDC0_SRC : MT6768_MSDC1_SRC, Enable);
}

VOID
ClocksControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Control clocks index: %d, Enable %d!\n", Index, Enable));
  if (Index == 0) {
    MtkClk->TopClkControl (MT6768_MSDC50_0, Enable);
    MtkClk->InfraClkControl (MT6768_MSDC0, Enable);
    MtkClk->InfraClkControl (MT6768_MSDC0_SRC, Enable);
    MtkClk->InfraClkControl (MT6768_FAES_FDE, Enable);
  } else {
    MtkClk->TopClkControl (MT6768_MSDC30_1, Enable);
    MtkClk->InfraClkControl (MT6768_MSDC1, Enable);
    MtkClk->InfraClkControl (MT6768_MSDC1_SRC, Enable);
  }
}

VOID
ConfigureGpio ()
{
  /* MT6768 MSDC1 Gpios:
   * GPIO171:     CLK
   * GPIO170:     CMD
   * GPIO164-161: DAT0-3
   */
  UINT8 Pin;
  for (Pin = 161; Pin <= 171; Pin++)
  {
    if (Pin >= 165 && Pin <= 169) {
      continue;
    }

    MtkGpio->SetMode (Pin, GPIO_MODE_SPECIAL_FUNCTION_1);

    /*if (Pin != 171) {
      MtkGpio->SetDir (Pin, MTK_GPIO_DIR_INPUT);
    }*/

    // MtkGpio->SetDrv (Pin, 4);
    // MtkGpio->SetR0 (Pin);

    // if (Pin == 171) {
    //   MtkGpio->SetPupd (Pin, MTK_GPIO_PULL_DOWN);
    // } else {
    //   MtkGpio->SetPupd (Pin, MTK_GPIO_PULL_UP);
    // }
  }
}

VOID
PowerControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Power control index: %d, Enable %d\n", Index, Enable));
  if (Index == 1)
  {
    MtkPmic->LdoControl (MT6358_LDO_VMCH, Enable);
    MtkPmic->LdoControl (MT6358_LDO_VMC,  Enable);
  }
}

VOID
PlatformInitialization ()
{
  // Set voltage
  MtkPmic->LdoSetVoltage (MT6358_LDO_VMCH, 3300);
  MtkPmic->LdoSetVoltage (MT6358_LDO_VMC,  3300);
  // Configure GPIO
  ConfigureGpio ();
}

MSDC_PLATFORM MsdcPlatform = {
  GetSourceClockRate,
  SourceClockControl,
  ClocksControl,
  PowerControl
};

EFI_STATUS
EFIAPI
MsdcPlatformDxeEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gMtkGpioProtocolGuid, NULL, (VOID **)&MtkGpio);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Failed to locate gpio protocol, Status = %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gMediaTekMT6358PmicProtocolGuid, NULL, (VOID **)&MtkPmic);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Failed to locate pmic protocol, Status = %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gMediaTekClockProtocolGuid, NULL, (VOID **)&MtkClk);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Failed to locate clock protocol, Status = %r\n", Status));
    return Status;
  }

  PlatformInitialization ();

  Status = gBS->InstallMultipleProtocolInterfaces (
    &ImageHandle,
    &gMediaTekMsdcPlatformProtocolGuid,
    &MsdcPlatform,
    NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Failed to install protocol, Status = %r\n", Status));
    return Status;
  }

  return Status;
}