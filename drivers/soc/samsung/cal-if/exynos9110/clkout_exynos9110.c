#include "clkout_exynos9110.h"

#define CLKOUT_OFFSET		(0xA00)
#define CLKOUT_SEL_SHIFT	(8)
#define CLKOUT_SEL_WIDTH	(6)
#define CLKOUT_SEL_TCXO	(0x1)
#define CLKOUT_ENABLE_SHIFT	(0)
#define CLKOUT_ENABLE_WIDTH	(1)

struct cmucal_clkout cmucal_clkout_list[] = {
	CLKOUT(VCLK_CLKOUT, CLKOUT_OFFSET, CLKOUT_SEL_SHIFT, CLKOUT_SEL_WIDTH, CLKOUT_SEL_TCXO, CLKOUT_ENABLE_SHIFT, CLKOUT_ENABLE_WIDTH),
};

unsigned int cmucal_clkout_size = 1;
