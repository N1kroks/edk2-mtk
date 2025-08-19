//
// MT6768 PWRAP Configuration
//

#include <Protocol/MtkPmicWrapper.h>

MTK_PWRAP_REG PwrapRegs[] = {
  0x94,  // INIT_DONE2
  0xc20, // WACS2_CMD
  0xc28, // WACS2_VLDCLR
  0xc24, // WACS2_RDATA
};

MTK_PWRAP_PLATFORM_INFO PlatformInfo = {
  0x1000d000, // Base
  PwrapRegs,
  FALSE       // Have ARB Capability
};
