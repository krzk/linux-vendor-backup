/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_domains.h>
#include <plat/pm.h>

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef PM_DOMAIN_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_DOMAIN_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

/* exynos3 power domain */
EXYNOS_COMMON_GPD(exynos3_pd_mfc, EXYNOS3_MFC_CONFIGURATION, "pd-mfc");
EXYNOS_COMMON_GPD(exynos3_pd_g3d, EXYNOS3_G3D_CONFIGURATION, "pd-g3d");
EXYNOS_COMMON_GPD(exynos3_pd_lcd0, EXYNOS3_LCD0_CONFIGURATION, "pd-lcd0");
#ifdef CONFIG_S5P_DEV_FIMD0
EXYNOS_COMMON_GPD(exynos3_spd_fimd0, NULL, "pd-fimd0");
EXYNOS_COMMON_GPD(exynos3_spd_dsim0, NULL, "pd-dsim0");
#endif
EXYNOS_COMMON_GPD(exynos3_pd_cam, EXYNOS3_CAM_CONFIGURATION, "pd-cam");
#if defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)
EXYNOS_COMMON_GPD(exynos3_spd_gsc0, NULL, "pd-gsc0");
EXYNOS_COMMON_GPD(exynos3_spd_gsc1, NULL, "pd-gsc1");
#endif
#if defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
EXYNOS_COMMON_GPD(exynos3_spd_mscl, NULL, "pd-mscl");
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
EXYNOS_COMMON_GPD(exynos3_spd_csis0, NULL, "pd-csis0");
EXYNOS_COMMON_GPD(exynos3_spd_csis1, NULL, "pd-csis1");
#endif
#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
EXYNOS_COMMON_GPD(exynos3_spd_jpeg, NULL, "pd-jpeg");
#endif
EXYNOS_COMMON_GPD(exynos3_pd_isp, EXYNOS3_ISP_CONFIGURATION, "pd-isp");

static struct exynos_pm_domain *exynos3250_pm_domains[] = {
	&exynos3_pd_mfc,
	&exynos3_pd_g3d,
	&exynos3_pd_lcd0,
#ifdef CONFIG_S5P_DEV_FIMD0
	&exynos3_spd_fimd0,
	&exynos3_spd_dsim0,
#endif
	&exynos3_pd_cam,
#if defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)
	&exynos3_spd_gsc0,
	&exynos3_spd_gsc1,
#endif
#if defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
	&exynos3_spd_mscl,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
	&exynos3_spd_csis0,
	&exynos3_spd_csis1,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
	&exynos3_spd_jpeg,
#endif
	&exynos3_pd_isp,
	NULL
};

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS3_CLKGATE_BUS_G3D),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_G3D),
};

static struct sleep_save exynos_pd_mfc_clk_save[] = {
	SAVE_ITEM(EXYNOS3_CLKGATE_BUS_MFC),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_MFC),
};

static struct sleep_save exynos_pd_lcd0_clk_save[] = {
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_LCD),
	SAVE_ITEM(EXYNOS3_CLKGATE_BUS_LCD),
	SAVE_ITEM(EXYNOS3_CLKGATE_SCLK_LCD),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_LCD),
};

#if defined(CONFIG_EXYNOS3_DEV_FIMC_LITE) || defined(CONFIG_EXYNOS_DEV_FIMC_IS)	\
	|| defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)	\
	|| defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
static struct sleep_save exynos_pd_cam_clk_save[] = {
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS3_CLKGATE_BUS_CAM0),
	SAVE_ITEM(EXYNOS3_CLKGATE_BUS_CAM1),
	SAVE_ITEM(EXYNOS3_CLKGATE_SCLK_CAM),
	SAVE_ITEM(EXYNOS3_CLKGATE_IP_CAM),
};
#endif

#ifdef CONFIG_EXYNOS_DEV_FIMC_IS
static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS3_CLKSRC_MASK_ISP),
	SAVE_ITEM(EXYNOS3_CLKGATE_SCLK_TOP_ISP),
};
#endif

static int exynos3_pd_g3d_clk_enable_on(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power on\n", "G3D");

	s3c_pm_do_restore_core(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_G3D);
	tmp |= ((0x1 << 18)|(0x1 << 16)|(0x7 << 3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_G3D);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_G3D);
	tmp |= 0xF;
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_G3D);

	return 0;
}

static int exynos3_pd_g3d_clk_enable_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power off\n", "G3D");

	s3c_pm_do_save(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_G3D);
	tmp |= ((0x1 << 18)|(0x1 << 16)|(0x7 << 3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_G3D);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_G3D);
	tmp |= 0xF;
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_G3D);

	return 0;
}

static int exynos3_pd_mfc_clk_enable_on(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power on\n", "MFC");

	s3c_pm_do_restore_core(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_MFC);
	tmp |= ((0x1 << 19)|(0x7 << 15)|(0x1 << 6)|(0x1 << 4)|(0x3 << 1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_MFC);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_MFC);
	tmp |= ((0x1 << 5)|(0x1 << 3)|(0x3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_MFC);

	return 0;
}

static int exynos3_pd_mfc_clk_enable_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power off\n", "MFC");

	s3c_pm_do_save(exynos_pd_mfc_clk_save,
			ARRAY_SIZE(exynos_pd_mfc_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_MFC);
	tmp |= ((0x1 << 19)|(0x7 << 15)|(0x1 << 6)|(0x1 << 4)|(0x3 << 1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_MFC);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_MFC);
	tmp |= ((0x1 << 5)|(0x1 << 3)|(0x3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_MFC);

	return 0;
}

static int exynos3_pd_lcd0_clk_enable_on(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power on\n", "LCD");

	s3c_pm_do_restore_core(exynos_pd_lcd0_clk_save,
			ARRAY_SIZE(exynos_pd_lcd0_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_LCD);
	tmp |= ((0x1 << 12)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_LCD);
	tmp |= ((0x7F << 14)|(0x3 << 9)|(0x1F << 3)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_LCD);
	tmp |= ((0x3 << 3)|(0x3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_LCD);
	tmp |= ((0x3F << 2)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_LCD);

	return 0;
}

static int exynos3_pd_lcd0_clk_enable_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power off\n", "LCD");

	s3c_pm_do_save(exynos_pd_lcd0_clk_save,
			ARRAY_SIZE(exynos_pd_lcd0_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_LCD);
	tmp |= ((0x1 << 12)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_LCD);
	tmp |= ((0x7F << 14)|(0x3 << 9)|(0x1F << 3)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_LCD);
	tmp |= ((0x3 << 3)|(0x3));
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_LCD);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_LCD);
	tmp |= ((0x3F << 2)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_LCD);

	return 0;
}

#if defined(CONFIG_EXYNOS3_DEV_FIMC_LITE) || defined(CONFIG_EXYNOS_DEV_FIMC_IS)	\
	|| defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)	\
	|| defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
static int exynos3_pd_cam_clk_enable_on(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power on\n", "CAM");

	s3c_pm_do_restore_core(exynos_pd_cam_clk_save,
			ARRAY_SIZE(exynos_pd_cam_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_CAM);
	tmp |= ((0x1 << 20)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_CAM);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_CAM0);
	tmp |= ((0x3 << 29)|(0x1 << 27)|(0xF << 22)|(0x7 << 18)|(0x3 << 14)|(0x7 << 10)|(0xF << 5)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_CAM0);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_CAM1);
	tmp |= ((0xF << 10)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_CAM1);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_CAM);
	tmp |= (0x1 << 8);
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_CAM);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_CAM);
	tmp |= ((0xF << 16)|(0xF << 11)|(0xF << 6)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_CAM);

	return 0;
}

static int exynos3_pd_cam_clk_enable_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power off\n", "CAM");

	s3c_pm_do_save(exynos_pd_cam_clk_save,
			ARRAY_SIZE(exynos_pd_cam_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_CAM);
	tmp |= ((0x1 << 20)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_CAM);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_CAM0);
	tmp |= ((0x3 << 29)|(0x1 << 27)|(0xF << 22)|(0x7 << 18)|(0x3 << 14)|(0x7 << 10)|(0xF << 5)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_CAM0);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_CAM1);
	tmp |= ((0xF << 10)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_CAM1);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_CAM);
	tmp |= (0x1 << 8);
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_CAM);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_CAM);
	tmp |= ((0xF << 16)|(0xF << 11)|(0xF << 6)|(0x7));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_CAM);

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_DEV_FIMC_IS
static int exynos3_pd_isp_clk_enable_on(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power on : restore clock and masking\n", "ISP");

	s3c_pm_do_restore_core(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_ISP);
	tmp |= ((0x1 << 12)|(0x1 << 8)|(0x1 << 4));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_ISP);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_TOP_ISP);
	tmp |= (0xF << 1);
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_TOP_ISP);

	return 0;
}

static int exynos3_pd_isp_clk_enable_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;

	DEBUG_PRINT_INFO("%s pre power off : save clocks and masking\n", "ISP");

	s3c_pm_do_save(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_ISP0);
	tmp |= ((0x7 << 27)|(0xF << 21)|(0xFFF << 8)|(0x7F));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_ISP0);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_ISP1);
	tmp |= ((0x7F << 15)|(0x1 << 4));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_ISP1);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_ISP2);
	tmp |= ((0x3 << 30)|(0x1 << 28)|(0xF << 23)|(0x3 << 20)|(0x1F << 14)|(0xFFF << 1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_ISP2);

	tmp = __raw_readl(EXYNOS3_CLKGATE_BUS_ISP3);
	tmp |= ((0x7F << 15)|(0x3 << 12)|(0x3 << 3)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_BUS_ISP3);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_ISP0);
	tmp |= ((0x3 << 30)|(0x1 << 28)|(0xF << 23)|(0x3 << 20)|(0x3FFFF));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_ISP0);

	tmp = __raw_readl(EXYNOS3_CLKGATE_IP_ISP1);
	tmp |= ((0x7F << 15)|(0x3 << 12)|(0x1 << 4)|(0x1));
	__raw_writel(tmp, EXYNOS3_CLKGATE_IP_ISP1);

	tmp = __raw_readl(EXYNOS3_CLKSRC_MASK_ISP);
	tmp |= ((0x1 << 12)|(0x1 << 8)|(0x3 << 4));
	__raw_writel(tmp, EXYNOS3_CLKSRC_MASK_ISP);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_TOP_ISP);
	tmp |= (0xF << 1);
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_TOP_ISP);

	tmp = __raw_readl(EXYNOS3_CLKGATE_SCLK_ISP);
	tmp |= (0x1);
	__raw_writel(tmp, EXYNOS3_CLKGATE_SCLK_ISP);

	return 0;
}

static int exynos3_pd_isp_arm_control_off(struct exynos_pm_domain *domain)
{
	unsigned int tmp;
	unsigned long timeout;

	DEBUG_PRINT_INFO("%s post power off : ISP ARM control\n", "ISP");

	/* Wait max 70ms */
	timeout = 700;

	do {
		tmp = (__raw_readl(EXYNOS3_ISP_STATUS) & 0x7);
		if (timeout == 0) {
			pr_err("PM DOMAIN : %s can't turn off, timeout\n", "ISP_ARM");
			break;
		}
		--timeout;
		cpu_relax();
		usleep_range(80, 100);
	} while (tmp != 0);

	if(tmp != 0){
		/* Wait max 5ms */
		timeout = 50;
		do {
			tmp = (__raw_readl(EXYNOS3_ISP_STATUS) & 0x7);
			if (timeout == 0) {
				pr_err("PM DOMAIN : %s can't turn off, timeout\n", "ISP_ARM");
				break;
			}
			--timeout;
			cpu_relax();
			usleep_range(80, 100);
		} while (tmp != 0);
	}

	if(tmp != 0)
	    __raw_writel(0x00000000, EXYNOS3_ISP_ARM_OPTION);

	return 0;
}
#endif

static void __iomem *exynos3_mfc_pwr_reg[] = {
	EXYNOS3_CMU_CLKSTOP_MFC_SYS_PWR_REG,
	EXYNOS3_CMU_RESET_MFC_SYS_PWR_REG,
};

static void __iomem *exynos3_g3d_pwr_reg[] = {
	EXYNOS3_CMU_CLKSTOP_G3D_SYS_PWR_REG,
	EXYNOS3_CMU_RESET_G3D_SYS_PWR_REG,
};

static void __iomem *exynos3_lcd0_pwr_reg[] = {
	EXYNOS3_CMU_CLKSTOP_LCD0_SYS_PWR_REG,
	EXYNOS3_CMU_RESET_LCD0_SYS_PWR_REG,
};

static void __iomem *exynos3_cam_pwr_reg[] = {
	EXYNOS3_CMU_CLKSTOP_CAM_SYS_PWR_REG,
	EXYNOS3_CMU_RESET_CAM_SYS_PWR_REG,
};

static void __iomem *exynos3_isp_pwr_reg[] = {
	EXYNOS3_CMU_CLKSTOP_ISP_SYS_PWR_REG,
	EXYNOS3_CMU_SYSCLK_ISP_SYS_PWR_REG,
	EXYNOS3_CMU_RESET_ISP_SYS_PWR_REG,
	EXYNOS3_ISP_ARM_SYS_PWR_REG,
};

static void __iomem *exynos3_g3d_pwr_seq_reg[] = {
	EXYNOS3_CLKGATE_BUS_G3D,
	EXYNOS3_CLKGATE_IP_G3D,
};

static const char *exynos3_g3d_pwr_seq_name[] = {
	"clk_gate_bus_g3d",
	"clk_gate_ip_g3d",
};

static unsigned int exynos3_g3d_pwr_seq_value[] = {
	0x00050038,
	0x0000000f,
};

static void __iomem *exynos3_mfc_pwr_seq_reg[] = {
	EXYNOS3_CLKGATE_BUS_MFC,
	EXYNOS3_CLKGATE_IP_MFC,
};

static const char *exynos3_mfc_pwr_seq_name[] = {
	"clk_gate_bus_mfc",
	"clk_gate_ip_mfc",
};

static unsigned int exynos3_mfc_pwr_seq_value[] = {
	0x000b8036,
	0x0000002b,
};

static void __iomem *exynos3_lcd0_pwr_seq_reg[] = {
	EXYNOS3_CLKSRC_MASK_LCD,
	EXYNOS3_CLKGATE_BUS_LCD,
	EXYNOS3_CLKGATE_SCLK_LCD,
	EXYNOS3_CLKGATE_IP_LCD,
};

static const char *exynos3_lcd0_pwr_seq_name[] = {
	"clk_src_mask_lcd",
	"clk_gate_bus_lcd",
	"clk_gate_sclk_lcd",
	"clk_gate_ip_lcd",
};

static unsigned int exynos3_lcd0_pwr_seq_value[] = {
	0x00001001,
	0x000fc6f9,
	0x0000001b,
	0x000000fd,
};

static void __iomem *exynos3_cam_pwr_seq_reg[] = {
	EXYNOS3_CLKSRC_MASK_CAM,
	EXYNOS3_CLKGATE_BUS_CAM0,
	EXYNOS3_CLKGATE_BUS_CAM1,
	EXYNOS3_CLKGATE_SCLK_CAM,
	EXYNOS3_CLKGATE_IP_CAM,
};

static const char *exynos3_cam_pwr_seq_name[] = {
	"clk_src_mask_cam",
	"clk_gate_bus_cam0",
	"clk_gate_bus_isp",
	"clk_gate_sclk_cam",
	"clk_gate_ip_cam",
};

static unsigned int exynos3_cam_pwr_seq_value[] = {
	0x00100001,
	0x6bdcdd77,
	0x00003c07,
	0x00000100,
	0x000f7bc7,
};

static void __iomem *exynos3_isp_pwr_seq_reg[] = {
	EXYNOS3_CLKSRC_MASK_ISP,
	EXYNOS3_CLKGATE_SCLK_TOP_ISP,
};

static const char *exynos3_isp_pwr_seq_name[] = {
	"clk_src_mask_isp",
	"clk_gate_sclk_top_isp",
};

static unsigned int exynos3_isp_pwr_seq_value[] = {
	0x00001110,
	0x0000001e,
};

static int __init exynos3_pm_domain_init(void)
{
	int i;

	exynos_pm_powerdomain_init(&exynos3_pd_mfc);
	exynos_pm_powerdomain_init(&exynos3_pd_g3d);
	exynos_pm_powerdomain_init(&exynos3_pd_lcd0);
	exynos_pm_powerdomain_init(&exynos3_pd_cam);
	exynos_pm_powerdomain_init(&exynos3_pd_isp);

	exynos_pm_add_reg(&exynos3_pd_mfc, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS3_MFC_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos3_mfc_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos3_pd_mfc, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_mfc_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos3_pd_g3d, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS3_G3D_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos3_g3d_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos3_pd_g3d, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_g3d_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos3_pd_lcd0, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS3_LCD0_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos3_lcd0_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos3_pd_lcd0, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_lcd0_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos3_pd_cam, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS3_CAM_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos3_cam_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos3_pd_cam, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_cam_pwr_reg[i],
				0);

	exynos_pm_add_reg(&exynos3_pd_isp, EXYNOS_PROCESS_BEFORE,
			EXYNOS_PROCESS_ONOFF, EXYNOS3_ISP_OPTION, 2);
	for (i = 0; i < ARRAY_SIZE(exynos3_isp_pwr_reg); ++i)
		exynos_pm_add_reg(&exynos3_pd_isp, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_isp_pwr_reg[i],
				0);

	for (i = 0; i < ARRAY_SIZE(exynos3_g3d_pwr_seq_reg); ++i)
		exynos_pm_add_pwr_seq_reg(&exynos3_pd_g3d, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_g3d_pwr_seq_reg[i],
				exynos3_g3d_pwr_seq_name[i],
				exynos3_g3d_pwr_seq_value[i]);

	for (i = 0; i < ARRAY_SIZE(exynos3_mfc_pwr_seq_reg); ++i)
		exynos_pm_add_pwr_seq_reg(&exynos3_pd_mfc, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF,exynos3_mfc_pwr_seq_reg[i],
				exynos3_mfc_pwr_seq_name[i],
				exynos3_mfc_pwr_seq_value[i]);

	for (i = 0; i < ARRAY_SIZE(exynos3_lcd0_pwr_seq_reg); ++i)
		exynos_pm_add_pwr_seq_reg(&exynos3_pd_lcd0, EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_lcd0_pwr_seq_reg[i],
				exynos3_lcd0_pwr_seq_name[i],
				exynos3_lcd0_pwr_seq_value[i]);

	for (i = 0; i < ARRAY_SIZE(exynos3_cam_pwr_seq_reg); ++i)
		exynos_pm_add_pwr_seq_reg(&exynos3_pd_cam,  EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_cam_pwr_seq_reg[i],
				exynos3_cam_pwr_seq_name[i],
				exynos3_cam_pwr_seq_value[i]);

	for (i = 0; i < ARRAY_SIZE(exynos3_isp_pwr_seq_reg); ++i)
		exynos_pm_add_pwr_seq_reg(&exynos3_pd_isp,  EXYNOS_PROCESS_BEFORE,
				EXYNOS_PROCESS_ONOFF, exynos3_isp_pwr_seq_reg[i],
				exynos3_isp_pwr_seq_name[i],
				exynos3_isp_pwr_seq_value[i]);

#ifdef CONFIG_S5P_DEV_MFC
	exynos_pm_add_platdev(&exynos3_pd_mfc, &s5p_device_mfc);
	exynos_pm_add_clk(&exynos3_pd_mfc, &s5p_device_mfc.dev, "mfc");
	exynos_pm_add_platdev(&exynos3_pd_mfc, &SYSMMU_PLATDEV(mfc_lr));
	exynos_pm_add_clk(&exynos3_pd_mfc, &SYSMMU_PLATDEV(mfc_lr).dev, "sysmmu");

	exynos_pm_add_callback(&exynos3_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos3_pd_mfc_clk_enable_on);

	exynos_pm_add_callback(&exynos3_pd_mfc, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_mfc_clk_enable_off);
#endif
	exynos_pm_add_platdev(&exynos3_pd_g3d, &exynos4_device_g3d);

	exynos_pm_add_callback(&exynos3_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos3_pd_g3d_clk_enable_on);

	exynos_pm_add_callback(&exynos3_pd_g3d, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_g3d_clk_enable_off);
#ifdef CONFIG_S5P_DEV_FIMD0
	exynos_pm_powerdomain_init(&exynos3_spd_fimd0);
	exynos_pm_add_platdev(&exynos3_spd_fimd0, &s5p_device_fimd0);
	exynos_pm_add_clk(&exynos3_spd_fimd0, &s5p_device_fimd0.dev, "fimd");
	exynos_pm_add_platdev(&exynos3_spd_fimd0, &SYSMMU_PLATDEV(fimd0));
	exynos_pm_add_clk(&exynos3_spd_fimd0, &SYSMMU_PLATDEV(fimd0).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos3_pd_lcd0, &exynos3_spd_fimd0, true);

	exynos_pm_add_callback(&exynos3_pd_lcd0, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos3_pd_lcd0_clk_enable_on);

	exynos_pm_add_callback(&exynos3_pd_lcd0, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_lcd0_clk_enable_off);

	exynos_pm_powerdomain_init(&exynos3_spd_dsim0);
#ifdef CONFIG_FB_MIPI_DSIM
	exynos_pm_add_platdev(&exynos3_spd_dsim0, &s5p_device_mipi_dsim0);
#endif
	exynos_pm_add_clk(&exynos3_spd_dsim0, NULL, "dsim0");
	exynos_pm_add_subdomain(&exynos3_pd_lcd0, &exynos3_spd_dsim0, true);
#endif
#if defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)
	exynos_pm_powerdomain_init(&exynos3_spd_gsc0);
	exynos_pm_add_platdev(&exynos3_spd_gsc0, &exynos3_device_gsc0);
	exynos_pm_add_clk(&exynos3_spd_gsc0, &exynos3_device_gsc0.dev, "gscl");
	exynos_pm_add_platdev(&exynos3_spd_gsc0, &SYSMMU_PLATDEV(gsc0));
	exynos_pm_add_clk(&exynos3_spd_gsc0, &SYSMMU_PLATDEV(gsc0).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_gsc0, true);

	exynos_pm_powerdomain_init(&exynos3_spd_gsc1);
	exynos_pm_add_platdev(&exynos3_spd_gsc1, &exynos3_device_gsc1);
	exynos_pm_add_clk(&exynos3_spd_gsc1, &exynos3_device_gsc1.dev, "gscl");
	exynos_pm_add_platdev(&exynos3_spd_gsc1, &SYSMMU_PLATDEV(gsc1));
	exynos_pm_add_clk(&exynos3_spd_gsc1, &SYSMMU_PLATDEV(gsc1).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_gsc1, true);
#endif

#if defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
	exynos_pm_powerdomain_init(&exynos3_spd_mscl);
	exynos_pm_add_platdev(&exynos3_spd_mscl, &exynos5_device_scaler0);
	exynos_pm_add_platdev(&exynos3_spd_mscl, &SYSMMU_PLATDEV(scaler));
	exynos_pm_add_clk(&exynos3_spd_mscl,
			&exynos5_device_scaler0.dev, "mscl");
	exynos_pm_add_clk(&exynos3_spd_mscl,
			&SYSMMU_PLATDEV(scaler).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_mscl, true);

#endif

#ifdef CONFIG_VIDEO_EXYNOS_MIPI_CSIS
	exynos_pm_powerdomain_init(&exynos3_spd_csis0);
	exynos_pm_add_platdev(&exynos3_spd_csis0, &s5p_device_mipi_csis0);
	exynos_pm_add_clk(&exynos3_spd_csis0, &s5p_device_mipi_csis0.dev, "csis");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_csis0, true);
	exynos_pm_powerdomain_init(&exynos3_spd_csis1);
	exynos_pm_add_platdev(&exynos3_spd_csis1, &s5p_device_mipi_csis1);
	exynos_pm_add_clk(&exynos3_spd_csis1, &s5p_device_mipi_csis1.dev, "csis");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_csis1, true);
#endif

#ifdef CONFIG_VIDEO_EXYNOS_JPEG_HX
	exynos_pm_powerdomain_init(&exynos3_spd_jpeg);
	exynos_pm_add_platdev(&exynos3_spd_jpeg, &exynos5_device_jpeg_hx);
	exynos_pm_add_clk(&exynos3_spd_jpeg, &exynos5_device_jpeg_hx.dev, "jpeg-hx");
	exynos_pm_add_platdev(&exynos3_spd_jpeg, &SYSMMU_PLATDEV(mjpeg));
	exynos_pm_add_clk(&exynos3_spd_jpeg, &SYSMMU_PLATDEV(mjpeg).dev, "sysmmu");
	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_spd_jpeg, true);
#endif

#ifdef CONFIG_EXYNOS_DEV_FIMC_IS
	exynos_pm_add_platdev(&exynos3_pd_isp,
			&exynos3_device_fimc_is);

	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(isp0));
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(isp1));

	exynos_pm_add_subdomain(&exynos3_pd_cam, &exynos3_pd_isp,
			false);
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(camif0));
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(camif1));

	exynos_pm_add_callback(&exynos3_pd_isp, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos3_pd_isp_clk_enable_on);

	exynos_pm_add_callback(&exynos3_pd_isp, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_isp_clk_enable_off);

	exynos_pm_add_callback(&exynos3_pd_isp, EXYNOS_PROCESS_AFTER, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_isp_arm_control_off);
#endif
#ifdef CONFIG_EXYNOS3_DEV_FIMC_LITE
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(camif0));
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(camif1));
	exynos_pm_add_platdev(&exynos3_pd_isp, &SYSMMU_PLATDEV(camif2));
#endif

#if defined(CONFIG_EXYNOS3_DEV_FIMC_LITE) || defined(CONFIG_EXYNOS_DEV_FIMC_IS)	\
	|| defined(CONFIG_VIDEO_EXYNOS_GSCALER) ||defined(CONFIG_DRM_EXYNOS_GSC)	\
	|| defined(CONFIG_EXYNOS5_DEV_SCALER) ||defined(CONFIG_DRM_EXYNOS_SC)
	exynos_pm_add_callback(&exynos3_pd_cam, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_ON,
				true, exynos3_pd_cam_clk_enable_on);

	exynos_pm_add_callback(&exynos3_pd_cam, EXYNOS_PROCESS_BEFORE, EXYNOS_PROCESS_OFF,
				true, exynos3_pd_cam_clk_enable_off);
#endif
	return 0;
}

struct exynos_pm_domain ** exynos_pm_get_power_domain(void)
{
	return exynos3250_pm_domains;
}

static int __init exynos_pm_init_power_domain(void)
{
	return exynos3_pm_domain_init();
}
arch_initcall(exynos_pm_init_power_domain);
