//
// MT6789 MSDC Configuration
//

#include <Library/MsdcPlatformImplLib.h>

MSDC_PLATFORM_INFO PlatformInfo = {
  .HostInfo = {
    {
      .MsdcMmioReg = 0x11240000,
      .TopMmioReg  = 0x11ef0000,
    }
  },
  .MsdcPadTuneReg = 0xf0,
  .UseTop         = TRUE
};