/*
 * MT6768 GPIO Configuration
 */

#include <Protocol/MtkGpio.h>

MTK_GPIO_PLATFORM_INFO PlatformInfo = {
  .BaseAddr      = 0x10005000,
  .SetOffset     = 0x4,
  .ResetOffset   = 0x8,
  .DirOffset     = 0x0,
  .DataOutOffset = 0x100,
  .DataInOffset  = 0x200,
  .ModeOffset    = 0x300,
  .MaxPin        = 179
};

struct MTK_GPIO_DRV_TABLE DrvTable[] = {};
struct MTK_GPIO_R_TABLE RTable[] = {};
struct MTK_GPIO_PUPD_TABLE PupdTable[] = {};