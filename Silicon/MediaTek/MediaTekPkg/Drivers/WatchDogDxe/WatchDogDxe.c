#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>

#define WATCHDOG_BASE (0x10007000)
#define WATCHDOG_LENGTH (0x4)
#define WATCHDOG_RESTART (0x8)

#define WDT_LENGTH_KEY (0x8)
#define WDT_RST_KEY (0x1971)
#define WDT_MODE_KEY (0x22000000)

EFI_EVENT mEfiExitBootServicesEvent;

UINT32 WatchdogModeBackup;

VOID
WatchdogPing ()
{
  MmioWrite32 (WATCHDOG_BASE + WATCHDOG_RESTART, WDT_RST_KEY);
}

VOID
WatchdogDisable ()
{
  MmioWrite32 (WATCHDOG_BASE, WDT_MODE_KEY);
}

VOID
WatchdogEnable ()
{
  MmioWrite32 (WATCHDOG_BASE, WatchdogModeBackup | WDT_MODE_KEY);
}

VOID
SetTimeout (UINT32 TimeoutMicroSeconds)
{
  // Watchdog timer timeout period is multiple of 15.6ms
  UINT32 Reg = (TimeoutMicroSeconds / 15600) | WDT_LENGTH_KEY;
  MmioWrite32 (WATCHDOG_BASE + WATCHDOG_LENGTH, Reg);

  WatchdogPing ();
}

VOID
WatchdogExitBootServicesEvent (
  IN EFI_EVENT Event,
  IN VOID *Context
  )
{
  WatchdogEnable ();
}

EFI_STATUS
EFIAPI
WatchdogDxeEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  WatchdogModeBackup = MmioRead32 (WATCHDOG_BASE);

  WatchdogDisable ();

  // Set watchdog timeout to 30s
  SetTimeout (30*1000*1000);

   // Enable watchdog before os boot
  Status = gBS->CreateEvent (
    EVT_SIGNAL_EXIT_BOOT_SERVICES,
    TPL_NOTIFY,
    WatchdogExitBootServicesEvent,
    NULL,
    &mEfiExitBootServicesEvent
  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WatchDogDxe: Failed to create ExitBootServices event: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}