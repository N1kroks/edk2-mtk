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
  UINT32 Index,
  BOOLEAN Enable
  );

typedef
VOID
(EFIAPI *MSDC_PLATFORM_GET_CLOCK_RATE)(
  UINT32 Index,
  UINTN *Hz
  );


struct _MSDC_PLATFORM {
  MSDC_PLATFORM_GET_CLOCK_RATE GetSourceClockRate;
  MSDC_PLATFORM_CALLBACK       SourceClockControl;
  MSDC_PLATFORM_CALLBACK       ClocksControl;
  MSDC_PLATFORM_CALLBACK       PowerControl;
};

extern EFI_GUID gMediaTekMsdcPlatformProtocolGuid;

#endif // __MSDC_PLATFORM_H__
