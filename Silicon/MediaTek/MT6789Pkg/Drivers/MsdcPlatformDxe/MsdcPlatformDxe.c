#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#include <Protocol/MT6358Pmic.h>
#include <Protocol/MtkClk.h>
#include <Protocol/MtkGpio.h>
#include <Protocol/MsdcPlatform.h>

MT6358_PMIC        *MtkPmic;
MTK_CLK            *MtkClk;
MTK_GPIO           *MtkGpio;

VOID
GetSourceClockRate (
  UINT32 Index,
  UINTN *Hz
)
{
  MtkClk->TopClkGetRate (MT6789_MSDC30_1, Hz);
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Source clock rate: %llu, index: %d\n", *Hz, Index));
}

VOID
SourceClockControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Control source clock index: %d, enable %d!\n", Index, Enable));
  MtkClk->InfraClkControl (MT6789_MSDC1_SRC, Enable);
}

VOID
ClocksControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Control clocks index: %d, enable %d!\n", Index, Enable));
  MtkClk->TopClkControl (MT6789_MSDC30_1, TRUE);
  MtkClk->InfraClkControl (MT6789_MSDC1, TRUE);
  MtkClk->InfraClkControl (MT6789_MSDC1_SRC, TRUE);
}

VOID
ConfigureGpio ()
{
  /* MT6789 MSDC1 Gpios:
   * GPIO71:    CLK
   * GPIO72:    CMD
   * GPIO73-76: DAT0-3
   */
  UINT8 Pin;
  for (Pin = 71; Pin <= 76; Pin++)
  {
    MtkGpio->SetMode (Pin, GPIO_MODE_SPECIAL_FUNCTION_1);

    /*if (Pin != 71) {
      MtkGpio->SetDir (Pin, MTK_GPIO_DIR_INPUT);
    }*/

    MtkGpio->SetDrv (Pin, 3);
    MtkGpio->SetR0 (Pin);

    if (Pin == 71) {
      MtkGpio->SetPupd (Pin, MTK_GPIO_PULL_DOWN);
    } else {
      MtkGpio->SetPupd (Pin, MTK_GPIO_PULL_UP);
    }
  }
}

VOID
PowerControl (
  UINT32 Index,
  BOOLEAN Enable
  )
{
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Power control index: %d, enable %d\n", Index, Enable));
  MtkPmic->LdoControl (MT6358_LDO_VMCH, Enable);
  MtkPmic->LdoControl (MT6358_LDO_VMC,  Enable);
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
  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Hi\n"));

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

  DEBUG ((DEBUG_ERROR, "MsdcPlatformDxe: Okeeey \n"));
  return Status;
}