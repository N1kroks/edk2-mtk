#ifndef __MSDC_PLATFORM_H__
#define __MSDC_PLATFORM_H__
//
// Protocol interface structure
//

typedef struct _MSDC_PLATFORM MSDC_PLATFORM;

//
// Function Prototypes
//

typedef
VOID
(EFIAPI *MSDC_PLATFORM_CALLBACK)(
  VOID
  );

typedef
VOID
(EFIAPI *MSDC_PLATFORM_GET_CLOCK_RATE)(
  UINTN *Hz
  );

typedef
VOID
(EFIAPI *MSDC_PLATFORM_POWER_CALLBACK)(
  BOOLEAN Enable
  );


struct _MSDC_PLATFORM {
  MSDC_PLATFORM_GET_CLOCK_RATE GetSourceClockRate;
  MSDC_PLATFORM_CALLBACK       DisableSourceClock;
  MSDC_PLATFORM_CALLBACK       EnableSourceClock;
  MSDC_PLATFORM_CALLBACK       EnableClocks;
  MSDC_PLATFORM_POWER_CALLBACK PowerControl;
};

extern EFI_GUID gMediaTekMsdcPlatformProtocolGuid;

#endif // __MSDC_PLATFORM_H__
