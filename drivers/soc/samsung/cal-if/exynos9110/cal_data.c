#include "../pmucal_common.h"
#include "../pmucal_cpu.h"
#include "../pmucal_local.h"
#include "../pmucal_rae.h"
#include "../pmucal_system.h"
#include "../pmucal_powermode.h"
#include "../pmucal_cp.h"
#include "../pmucal_gnss.h"

#include "flexpmu_cal_cpu_exynos9110.h"
#include "flexpmu_cal_local_exynos9110.h"
#include "flexpmu_cal_p2vmap_exynos9110.h"
#include "flexpmu_cal_system_exynos9110.h"
#include "flexpmu_cal_powermode_exynos9110.h"
#include "flexpmu_cal_define_exynos9110.h"

#include "pmucal_cp_exynos9110.h"
#include "pmucal_gnss_exynos9110.h"

#include "cmucal-node.c"
#include "cmucal-qch.c"
#include "cmucal-sfr.c"
#include "cmucal-vclk.c"
#include "cmucal-vclklut.c"

#include "clkout_exynos9110.c"
#include "acpm_dvfs_exynos9110.h"
#include "asv_exynos9110.h"

#include "../ra.h"
void exynos9110_cal_data_init(void)
{
	pr_info("%s: cal data init\n", __func__);

	/* cpu inform sfr initialize */
	pmucal_sys_powermode[SYS_SICD] = CPU_INFORM_SICD;
	pmucal_sys_powermode[SYS_SLEEP] = CPU_INFORM_SLEEP;

	cpu_inform_c2 = CPU_INFORM_C2;
	cpu_inform_cpd = CPU_INFORM_CPD;
}

void (*cal_data_init)(void) = exynos9110_cal_data_init;
