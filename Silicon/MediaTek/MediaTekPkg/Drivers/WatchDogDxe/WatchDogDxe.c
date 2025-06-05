#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/MemoryAllocationLib.h>

#define WATCHDOG_BASE (0x10007000)
#define WATCHDOG_LENGTH (0x4)
#define WATCHDOG_RESTART (0x8)

#define WDT_LENGTH_KEY (0x8)
#define WDT_RST_KEY (0x1971)

typedef struct {
  EFI_EVENT WatchdogTimerEvent;
  EFI_EVENT mEfiExitBootServicesEvent;
  UINT64 TimerPeriod;
} WATCHDOG_TIMER_CONTEXT;

WATCHDOG_TIMER_CONTEXT *mTimerContext;

VOID
WatchdogPing ()
{
  MmioWrite32 (WATCHDOG_BASE + WATCHDOG_RESTART, WDT_RST_KEY);
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
EFIAPI
PingWatchdogEvent (
  IN EFI_EVENT Event,
  IN VOID *Context
  )
{
  DEBUG ((DEBUG_ERROR, "WatchDogDxe: WOOF WOOF\n"));
  WatchdogPing ();
}

EFI_STATUS
EFIAPI
WatchdogDxeEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  // Set watchdog timeout to 30s
  SetTimeout (30*1000*1000);

  mTimerContext = AllocateZeroPool (sizeof (WATCHDOG_TIMER_CONTEXT));
  if (mTimerContext == NULL) {
    DEBUG ((DEBUG_ERROR, "WatchDogDxe: Failed to allocate timer context\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Ping watchdog every 5s
  mTimerContext->TimerPeriod = 5*1000*1000;

  Status = gBS->CreateEvent (
    EVT_TIMER | EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    PingWatchdogEvent,
    mTimerContext,
    &mTimerContext->WatchdogTimerEvent
  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WatchDogDxe: Failed to create timer event: %r\n", Status));
    return Status;
  }

  Status = gBS->SetTimer (
    mTimerContext->WatchdogTimerEvent,
    TimerPeriodic, // Periodic timer
    mTimerContext->TimerPeriod
  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WatchDogDxe: Failed to set periodic timer: %r\n", Status));
    return Status;
  }

  // Ping watchdog before os boot
  /*Status = gBS->CreateEvent (
    EVT_SIGNAL_EXIT_BOOT_SERVICES,
    TPL_NOTIFY,
    PingWatchdogEvent,
    mTimerContext,
    &mTimerContext->mEfiExitBootServicesEvent
  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WatchDogDxe: Failed to create ExitBootServices event: %r\n", Status));
    return Status;
  }*/

  DEBUG ((DEBUG_INFO, "WatchDogDxe: Watchdog kicker initialized.\n"));
  return EFI_SUCCESS;
}