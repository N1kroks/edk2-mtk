//
// MT6768 MSDC Configuration
//

#include <Library/MsdcPlatformImplLib.h>

MSDC_PLATFORM_INFO PlatformInfo = {
  .HostInfo = {
    {
      .MsdcMmioReg = 0x11230000,
      .TopMmioReg  = 0x11cd0000,
    }
  },
  .MsdcPadTuneReg = 0xf0,
  .UseTop         = TRUE
};