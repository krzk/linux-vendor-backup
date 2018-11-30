#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"

#include "cmucal-vclklut.h"

/*=================CMUCAL version: S5E9110================================*/

/*=================CLK in each VCLK================================*/


/* DVFS List */
enum clk_id cmucal_vclk_vdd_alive[] = {
	DIV_CLK_APM_BUS,
	MUX_CLKCMU_CHUB_BUS,
	DIV_CLK_CMGP_I2C0,
	DIV_CLK_CMGP_USI0,
	DIV_CLK_CMGP_USI2,
	DIV_CLK_CMGP_USI3,
	DIV_CLK_CMGP_I2C1,
	DIV_CLK_CMGP_I2C2,
	DIV_CLK_CMGP_I2C3,
	DIV_CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_vdd_int[] = {
	CLKCMU_DISPAUD_DISP,
	CLKCMU_FSYS_BUS,
	CLKCMU_FSYS_MMC_CARD,
	CLKCMU_FSYS_MMC_EMBD,
	CLKCMU_IS_BUS,
	CLKCMU_IS_VRA,
	CLKCMU_G3D_BUS,
	CLKCMU_MFCMSCL_MFC,
	CLKCMU_MFCMSCL_MSCL,
	CLKCMU_CORE_BUS,
	CLKCMU_CORE_SSS,
	MUX_CLKCMU_DISPAUD_DISP,
	MUX_CLKCMU_FSYS_BUS,
	MUX_CLKCMU_IS_BUS,
	MUX_CLKCMU_IS_VRA,
	MUX_CLKCMU_G3D_BUS,
	MUX_CLKCMU_CPUCL0_DBG,
	MUX_CLKCMU_MFCMSCL_MFC,
	MUX_CLKCMU_MFCMSCL_MSCL,
	MUX_CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_SSS,
	PLL_CPUCL0,
	DIV_CLK_AUD_AUDIF,
	PLL_AUD,
	PLL_MIF,
	DIV_CLK_PERI_SPI,
};

/* SPECIAL List */
enum clk_id cmucal_vclk_clkcmu_chub_bus[] = {
	CLKCMU_CHUB_BUS,
};
enum clk_id cmucal_vclk_div_clk_chub_i2c0[] = {
	DIV_CLK_CHUB_I2C0,
	MUX_CLK_CHUB_I2C0,
};
enum clk_id cmucal_vclk_clk_chub_timer_fclk[] = {
	CLK_CHUB_TIMER_FCLK,
};
enum clk_id cmucal_vclk_div_clk_chub_usi0[] = {
	DIV_CLK_CHUB_USI0,
	MUX_CLK_CHUB_USI0,
};
enum clk_id cmucal_vclk_div_clk_chub_i2c1[] = {
	DIV_CLK_CHUB_I2C1,
	MUX_CLK_CHUB_I2C1,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_usi3[] = {
	MUX_CLK_CMGP_USI3,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_adc[] = {
	MUX_CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_i2c0[] = {
	MUX_CLK_CMGP_I2C0,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_i2c2[] = {
	MUX_CLK_CMGP_I2C2,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_usi0[] = {
	MUX_CLK_CMGP_USI0,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_usi2[] = {
	MUX_CLK_CMGP_USI2,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_i2c1[] = {
	MUX_CLK_CMGP_I2C1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi1[] = {
	DIV_CLK_CMGP_USI1,
	MUX_CLK_CMGP_USI1,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_i2c3[] = {
	MUX_CLK_CMGP_I2C3,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk0[] = {
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_clkcmu_fsys_usb20drd[] = {
	CLKCMU_FSYS_USB20DRD,
	MUX_CLKCMU_FSYS_USB20DRD,
};
enum clk_id cmucal_vclk_clkcmu_peri_uart[] = {
	CLKCMU_PERI_UART,
	MUX_CLKCMU_PERI_UART,
};
enum clk_id cmucal_vclk_div_clk_cmu_cmuref[] = {
	MUX_CMU_CMUREF,
	DIV_CLK_CMU_CMUREF,
	MUX_CLK_CMU_CMUREF,
};
enum clk_id cmucal_vclk_clkcmu_peri_ip[] = {
	CLKCMU_PERI_IP,
	MUX_CLKCMU_PERI_IP,
};
enum clk_id cmucal_vclk_clkcmu_apm_bus[] = {
	CLKCMU_APM_BUS,
	MUX_CLKCMU_APM_BUS,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk1[] = {
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_clkcmu_mif_busp[] = {
	CLKCMU_MIF_BUSP,
	MUX_CLKCMU_MIF_BUSP,
};
enum clk_id cmucal_vclk_ap2cp_shared0_pll_clk[] = {
	AP2CP_SHARED0_PLL_CLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_cmuref[] = {
	DIV_CLK_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_cluster0_cntclk[] = {
	DIV_CLK_CLUSTER0_CNTCLK,
};
enum clk_id cmucal_vclk_div_clk_aud_fm[] = {
	DIV_CLK_AUD_FM,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif1[] = {
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF1,
};
enum clk_id cmucal_vclk_div_clk_aud_cpu_pclkdbg[] = {
	DIV_CLK_AUD_CPU_PCLKDBG,
};
enum clk_id cmucal_vclk_div_clk_aud_dmic[] = {
	DIV_CLK_AUD_DMIC,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif0[] = {
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF0,
};
enum clk_id cmucal_vclk_div_clk_aud_uaif2[] = {
	DIV_CLK_AUD_UAIF2,
};
enum clk_id cmucal_vclk_div_clk_aud_mclk[] = {
	DIV_CLK_AUD_MCLK,
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_i2c[] = {
	DIV_CLK_PERI_USI00_I2C,
};
enum clk_id cmucal_vclk_div_clk_peri_hsi2c[] = {
	DIV_CLK_PERI_HSI2C,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_usi[] = {
	DIV_CLK_PERI_USI00_USI,
	MUX_CLK_PERI_USI00_USI,
};
enum clk_id cmucal_vclk_mux_clk_peri_spi[] = {
	MUX_CLK_PERI_SPI,
};
enum clk_id cmucal_vclk_div_clk_vts_dmic[] = {
	DIV_CLK_VTS_DMIC,
};
enum clk_id cmucal_vclk_div_clk_vts_dmic_if_div2[] = {
	DIV_CLK_VTS_DMIC_IF_DIV2,
};

/* COMMON List */
enum clk_id cmucal_vclk_blk_apm[] = {
	MUX_CLK_APM_BUS,
	CLKCMU_VTS_BUS,
	MUX_CLKCMU_VTS_BUS,
};
enum clk_id cmucal_vclk_blk_chub[] = {
	DIV_CLK_CHUB_BUS,
	MUX_CLK_CHUB_BUS,
};
enum clk_id cmucal_vclk_blk_cmu[] = {
	CLKCMU_PERI_BUS,
	AP2CP_SHARED1_PLL_CLK,
	CLKCMU_CPUCL0_DBG,
	AP2CP_SHARED2_PLL_CLK,
	MUX_CLKCMU_FSYS_MMC_EMBD,
	MUX_CLKCMU_PERI_BUS,
	MUX_CLKCMU_FSYS_MMC_CARD,
	PLL_MMC,
	PLL_SHARED1_DIV4,
	PLL_SHARED1_DIV2,
	PLL_SHARED0_DIV4,
	PLL_SHARED0_DIV3,
	PLL_SHARED1_DIV3,
	PLL_SHARED1,
	PLL_SHARED0_DIV2,
	PLL_SHARED0,
};
enum clk_id cmucal_vclk_blk_core[] = {
	DIV_CLK_CORE_BUSP,
	MUX_CLK_CORE_GIC,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	DIV_CLK_CPUCL0_PCLK,
	DIV_CLK_CLUSTER0_ACLK,
	DIV_CLK_CPUCL0_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_dispaud[] = {
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_DISPAUD_BUSP,
	DIV_CLK_AUD_BUS,
	MUX_CLK_AUD_FM,
	DIV_CLK_AUD_FM_SPDY,
	MUX_CLK_AUD_CPU_HCH,
	DIV_CLK_AUD_CPU,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
};
enum clk_id cmucal_vclk_blk_is[] = {
	DIV_CLK_IS_BUSP,
};
enum clk_id cmucal_vclk_blk_mfcmscl[] = {
	DIV_CLK_MFCMSCL_BUSP,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_BUS,
	MUX_CLK_VTS_BUS,
	DIV_CLK_VTS_DMIC_IF,
};

/* GATING List */
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	GOUT_BLK_APM_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_cmgpalv[] = {
	GOUT_BLK_APM_UID_APBIF_GPIO_CMGPALV_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_APM_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_intr_gen[] = {
	GOUT_BLK_APM_UID_APBIF_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apm_cmu_apm[] = {
	CLK_BLK_APM_UID_APM_CMU_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dtzpc_apm[] = {
	GOUT_BLK_APM_UID_DTZPC_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_APM_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_chub[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_gnss[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_modem[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_vts[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_wlbt[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_apm[] = {
	GOUT_BLK_APM_UID_LHS_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_chub[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_vts[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_vts[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_abox[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pem[] = {
	GOUT_BLK_APM_UID_PEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_speedy_apm[] = {
	GOUT_BLK_APM_UID_SPEEDY_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_apm[] = {
	GOUT_BLK_APM_UID_SYSREG_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_apm[] = {
	GOUT_BLK_APM_UID_WDT_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_apm[] = {
	GOUT_BLK_APM_UID_XIU_DP_APM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix_chub[] = {
	GOUT_BLK_CHUB_UID_AHB_BUSMATRIX_CHUB_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_D_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_apm_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_P_APM_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_lp_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_chub_cmu_chub[] = {
	CLK_BLK_CHUB_UID_CHUB_CMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_chub_rtc_apbif[] = {
	GOUT_BLK_CHUB_UID_CHUB_RTC_APBIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cm4_chub[] = {
	GOUT_BLK_CHUB_UID_CM4_CHUB_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_chub[] = {
	GOUT_BLK_CHUB_UID_D_TZPC_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgpalv_chub_apbif[] = {
	GOUT_BLK_CHUB_UID_GPIO_CMGPALV_CHUB_APBIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub00[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB00_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub01[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB01_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB01_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_chub[] = {
	GOUT_BLK_CHUB_UID_LHM_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_chub[] = {
	GOUT_BLK_CHUB_UID_LHM_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_chub[] = {
	GOUT_BLK_CHUB_UID_LHS_AXI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_chub[] = {
	GOUT_BLK_CHUB_UID_LHS_AXI_D_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_chub[] = {
	GOUT_BLK_CHUB_UID_PDMA_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_pwm_chub[] = {
	GOUT_BLK_CHUB_UID_PWM_CHUB_IPCLKPORT_i_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sweeper_d_chub[] = {
	GOUT_BLK_CHUB_UID_SWEEPER_D_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sweeper_p_apm_chub[] = {
	GOUT_BLK_CHUB_UID_SWEEPER_P_APM_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_chub[] = {
	GOUT_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_chub[] = {
	GOUT_BLK_CHUB_UID_TIMER_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub00[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB00_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB00_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_chub[] = {
	GOUT_BLK_CHUB_UID_WDT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dtzpc_cmgp[] = {
	GOUT_BLK_CMGP_UID_DTZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp0[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp1[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp2[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp3[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp4[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp5[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP5_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp6[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2chub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_chub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2wlbt[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp2[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp3[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp[] = {
	CLK_BLK_CMU_UID_OTP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_adm_ahb_sss[] = {
	GOUT_BLK_CORE_UID_ADM_AHB_SSS_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma0[] = {
	GOUT_BLK_CORE_UID_AD_APB_PDMA0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	GOUT_BLK_CORE_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gic[] = {
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_sss[] = {
	GOUT_BLK_CORE_UID_AD_AXI_SSS_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_baaw_p_chub[] = {
	GOUT_BLK_CORE_UID_BAAW_P_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	GOUT_BLK_CORE_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_modem[] = {
	GOUT_BLK_CORE_UID_BAAW_P_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_vts[] = {
	GOUT_BLK_CORE_UID_BAAW_P_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_wlbt[] = {
	GOUT_BLK_CORE_UID_BAAW_P_WLBT_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_CORE_UID_DIT_IPCLKPORT_iClkL2A,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gic400_aihwacg[] = {
	GOUT_BLK_CORE_UID_GIC400_AIHWACG_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_abox[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_ABOX_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_chub[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dpu[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_fsys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_gnss[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_is[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mfcmscl[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vts[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_wlbt[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_chub[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dispaud[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DISPAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_fsys[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_gnss[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_is[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfcmscl[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_modem[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peri[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vts[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_wlbt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_core[] = {
	GOUT_BLK_CORE_UID_PDMA_CORE_IPCLKPORT_ACLK_PDMA0,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_i_ACLK,
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apbif_cmu_topc[] = {
	GOUT_BLK_CORE_UID_SFR_APBIF_CMU_TOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sirex[] = {
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_i_ACLK,
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_spdma_core[] = {
	GOUT_BLK_CORE_UID_SPDMA_CORE_IPCLKPORT_ACLK_PDMA1,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_i_ACLK,
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_i_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
	CLK_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_MCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_trex_d_nrt[] = {
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_ACLK,
	CLK_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_MCLK,
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_trex_p_core[] = {
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_ACLK_P_CORE,
	CLK_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_MCLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_pclk,
};
enum clk_id cmucal_vclk_ip_xiu_d_core[] = {
	GOUT_BLK_CORE_UID_XIU_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cssys_mif[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_CSSYS_MIF_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ads_ahb_g_cssys_sss[] = {
	GOUT_BLK_CPUCL0_UID_ADS_AHB_G_CSSYS_SSS_IPCLKPORT_HCLKS,
};
enum clk_id cmucal_vclk_ip_ads_apb_g_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_ADS_APB_G_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_AD_APB_P_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM,
	GOUT_BLK_CPUCL0_UID_AD_APB_P_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cssys_dbg[] = {
	GOUT_BLK_CPUCL0_UID_CSSYS_DBG_IPCLKPORT_PCLKDBG,
};
enum clk_id cmucal_vclk_ip_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_DUMP_PC_CPUCL0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_D_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_D_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_UID_SECJTAG_IPCLKPORT_i_clk,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_SPDY,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	CLK_BLK_DISPAUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_CA7,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_CCLK_DBG,
	GOUT_BLK_DISPAUD_UID_ABOX_IPCLKPORT_OSC_SPDY,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon0[] = {
	GOUT_BLK_DISPAUD_UID_AD_APB_DECON0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_abox[] = {
	GOUT_BLK_DISPAUD_UID_AD_APB_SMMU_ABOX_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_axi_us_32to128[] = {
	GOUT_BLK_DISPAUD_UID_AXI_US_32to128_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_dftmux_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_DFTMUX_DISPAUD_IPCLKPORT_AUD_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_dispaud_cmu_dispaud[] = {
	CLK_BLK_DISPAUD_UID_DISPAUD_CMU_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic[] = {
	GOUT_BLK_DISPAUD_UID_DMIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DECON,
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DISPAUD_UID_DPU_IPCLKPORT_ACLK_DPP,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_D_TZPC_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_GPIO_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_LHM_AXI_P_DISPAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_abox[] = {
	GOUT_BLK_DISPAUD_UID_LHS_AXI_D_ABOX_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dpu[] = {
	GOUT_BLK_DISPAUD_UID_LHS_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_DISPAUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
	GOUT_BLK_DISPAUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKS,
	GOUT_BLK_DISPAUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_abox[] = {
	GOUT_BLK_DISPAUD_UID_PPMU_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_PPMU_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpu[] = {
	GOUT_BLK_DISPAUD_UID_PPMU_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DISPAUD_UID_PPMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_abox[] = {
	GOUT_BLK_DISPAUD_UID_SMMU_ABOX_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_smmu_dpu[] = {
	GOUT_BLK_DISPAUD_UID_SMMU_DPU_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dispaud[] = {
	GOUT_BLK_DISPAUD_UID_SYSREG_DISPAUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_DISPAUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_fsys[] = {
	GOUT_BLK_FSYS_UID_D_TZPC_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_fsys_cmu_fsys[] = {
	CLK_BLK_FSYS_UID_FSYS_CMU_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_fsys[] = {
	GOUT_BLK_FSYS_UID_GPIO_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_fsys[] = {
	GOUT_BLK_FSYS_UID_LHM_AXI_P_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_fsys[] = {
	GOUT_BLK_FSYS_UID_LHS_AXI_D_FSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_FSYS_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_mmc_embd[] = {
	GOUT_BLK_FSYS_UID_MMC_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_FSYS_UID_MMC_EMBD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_ppmu_fsys[] = {
	GOUT_BLK_FSYS_UID_PPMU_FSYS_IPCLKPORT_ACLK,
	GOUT_BLK_FSYS_UID_PPMU_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_fsys[] = {
	GOUT_BLK_FSYS_UID_SYSREG_FSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb20drd_top[] = {
	GOUT_BLK_FSYS_UID_USB20DRD_TOP_IPCLKPORT_ACLK_PHYCTRL_20,
	CLK_BLK_FSYS_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_50,
	CLK_BLK_FSYS_UID_USB20DRD_TOP_IPCLKPORT_I_USB20_PHY_REFCLK_26,
	GOUT_BLK_FSYS_UID_USB20DRD_TOP_IPCLKPORT_bus_clk_early,
};
enum clk_id cmucal_vclk_ip_us_64to128_fsys[] = {
	GOUT_BLK_FSYS_UID_US_64to128_FSYS_IPCLKPORT_aclk,
};
enum clk_id cmucal_vclk_ip_xiu_d_fsys[] = {
	GOUT_BLK_FSYS_UID_XIU_D_FSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_async_g3d_p[] = {
	GOUT_BLK_G3D_UID_ASYNC_G3D_P_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d[] = {
	CLK_BLK_G3D_UID_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_is[] = {
	GOUT_BLK_IS_UID_D_TZPC_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_is_cmu_is[] = {
	CLK_BLK_IS_UID_IS_CMU_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_is[] = {
	GOUT_BLK_IS_UID_LHM_AXI_P_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_is[] = {
	GOUT_BLK_IS_UID_LHS_AXI_D_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_is[] = {
	GOUT_BLK_IS_UID_SYSREG_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_is3p21p0_is[] = {
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_AXI2APB_IS0,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_AXI2APB_IS1,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_CSIS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_CSIS_DMA,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_ISP,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_MCSC,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_PPMU_IS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_SMMU_IS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_VRA,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_XIU_ASYNCM_VRA,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_XIU_ASYNCS_VRA,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_XIU_D_IS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_ACLK_XIU_P_IS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_CSIS_DMA_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_CSIS_DMA_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_CSIS_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_CSIS_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_ISP_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_ISP_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_MCSC_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_MCSC_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_SMMU_IS_NS_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_SMMU_IS_NS_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_SMMU_IS_S_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_SMMU_IS_S_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_VRA_PCLKM,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_APB_ASYNC_VRA_PCLKS,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_CLK_CSIS0,
	GOUT_BLK_IS_UID_is3p21p0_IS_IPCLKPORT_PCLK_PPMU_IS,
};
enum clk_id cmucal_vclk_ip_as_apb_jpeg[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_JPEG_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_M2M_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_mcsc[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_MFC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_ns_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_SYSMMU_NS_MFCMSCL_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_s_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_SYSMMU_S_MFCMSCL_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_axi_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_AS_AXI_M2M_IPCLKPORT_ACLKM,
	GOUT_BLK_MFCMSCL_UID_AS_AXI_M2M_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_axi2apb_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AXI2APB_MFCMSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_D_TZPC_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_jpeg[] = {
	GOUT_BLK_MFCMSCL_UID_JPEG_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_LHM_AXI_P_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_LHS_AXI_D_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	GOUT_BLK_MFCMSCL_UID_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mfc[] = {
	GOUT_BLK_MFCMSCL_UID_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mfcmscl_cmu_mfcmscl[] = {
	CLK_BLK_MFCMSCL_UID_MFCMSCL_CMU_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_PPMU_MFCMSCL_IPCLKPORT_ACLK,
	GOUT_BLK_MFCMSCL_UID_PPMU_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_SYSMMU_MFCMSCL_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_SYSREG_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_XIU_D_MFCMSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	CLK_BLK_MIF_UID_DMC_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_dmc_cpu[] = {
	CLK_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_modem_cmu_modem[] = {
	CLK_BLK_MODEM_UID_MODEM_CMU_MODEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_tmu[] = {
	GOUT_BLK_PERI_UID_BUSIF_TMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peri[] = {
	GOUT_BLK_PERI_UID_D_TZPC_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peri[] = {
	GOUT_BLK_PERI_UID_GPIO_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_0[] = {
	GOUT_BLK_PERI_UID_I2C_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_1[] = {
	GOUT_BLK_PERI_UID_I2C_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_2[] = {
	GOUT_BLK_PERI_UID_I2C_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peri[] = {
	GOUT_BLK_PERI_UID_LHM_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERI_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	CLK_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
	GOUT_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peri_cmu_peri[] = {
	CLK_BLK_PERI_UID_PERI_CMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pwm_motor[] = {
	GOUT_BLK_PERI_UID_PWM_MOTOR_IPCLKPORT_i_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sysreg_peri[] = {
	GOUT_BLK_PERI_UID_SYSREG_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_i2c[] = {
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_usi[] = {
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_i2c_0[] = {
	GOUT_BLK_PERI_UID_USI_I2C_0_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI_I2C_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_spi[] = {
	GOUT_BLK_PERI_UID_USI_SPI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI_SPI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_uart[] = {
	GOUT_BLK_PERI_UID_USI_UART_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI_UART_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_cluster0[] = {
	GOUT_BLK_PERI_UID_WDT_CLUSTER0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix[] = {
	GOUT_BLK_VTS_UID_AHB_BUSMATRIX_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_baaw_c_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_C_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_D_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_lp_vts[] = {
	GOUT_BLK_VTS_UID_BPS_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_p_vts[] = {
	GOUT_BLK_VTS_UID_BPS_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cortexm4integration[] = {
	GOUT_BLK_VTS_UID_CORTEXM4INTEGRATION_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb1[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if[] = {
	GOUT_BLK_VTS_UID_DMIC_IF_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF_IPCLKPORT_DMIC_IF_CLK_DIV2,
	GOUT_BLK_VTS_UID_DMIC_IF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vts[] = {
	GOUT_BLK_VTS_UID_D_TZPC_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_vts[] = {
	GOUT_BLK_VTS_UID_GPIO_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic1[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_abox_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_ABOX_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_vts[] = {
	GOUT_BLK_VTS_UID_SWEEPER_C_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sweeper_d_vts[] = {
	GOUT_BLK_VTS_UID_SWEEPER_D_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vts[] = {
	GOUT_BLK_VTS_UID_SYSREG_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer[] = {
	GOUT_BLK_VTS_UID_TIMER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vts_cmu_vts[] = {
	CLK_BLK_VTS_UID_VTS_CMU_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_vts[] = {
	GOUT_BLK_VTS_UID_WDT_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xhb_lp_vts[] = {
	GOUT_BLK_VTS_UID_XHB_LP_VTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xhb_p_vts[] = {
	GOUT_BLK_VTS_UID_XHB_P_VTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_u_dmic_clk_mux[] = {
	CLK_BLK_VTS_UID_u_DMIC_CLK_MUX_IPCLKPORT_D0,
};

/* Switching LUT */
/* -1 is the Value of EMPTY_CAL_ID */
struct switch_lut tail_blk_cpucl0_lut[] = {
	{799500, 0, 0},
	{533000, 2, 0},
	{266500, 2, 1},
};
struct switch_lut tail_blk_dispaud_lut[] = {
	{799500, 1, 0},
	{666250, 2, 0},
	{333125, 2, 1},
};
struct switch_lut tail_blk_mif_lut[] = {
	{1599000, 0, -1},
	{1332500, 1, -1},
};

/* DVFS LUT */
struct vclk_lut vdd_alive_lut[] = {
	{200000, vdd_alive_nm_lut_params},
	{100000, vdd_alive_sud_lut_params},
};
struct vclk_lut vdd_int_lut[] = {
	{300000, vdd_int_nm_lut_params},
	{200000, vdd_int_sud_lut_params},
	{100000, vdd_int_ud_lut_params},
};

/* SPECIAL LUT */
struct vclk_lut clkcmu_chub_bus_lut[] = {
	{400000, spl_clk_chub_i2c0_blk_apm_nm_lut_params},
	{399750, spl_clk_chub_i2c0_blk_apm_sud_lut_params},
};
struct vclk_lut div_clk_chub_i2c0_lut[] = {
	{200000, spl_clk_chub_i2c0_blk_chub_nm_lut_params},
};
struct vclk_lut clk_chub_timer_fclk_lut[] = {
	{30000, mux_clk_chub_timer_fclk_blk_chub_nm_lut_params},
};
struct vclk_lut div_clk_chub_usi0_lut[] = {
	{50000, spl_clk_chub_usi0_blk_chub_nm_lut_params},
};
struct vclk_lut div_clk_chub_i2c1_lut[] = {
	{200000, spl_clk_chub_i2c1_blk_chub_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_usi3_lut[] = {
	{399750, spl_clk_cmgp_usi3_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_adc_lut[] = {
	{133250, mux_clk_cmgp_adc_blk_cmgp_sud_lut_params},
	{28553, mux_clk_cmgp_adc_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_i2c0_lut[] = {
	{399750, spl_clk_cmgp_i2c0_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_i2c2_lut[] = {
	{399750, spl_clk_cmgp_i2c2_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_usi0_lut[] = {
	{399750, spl_clk_cmgp_usi0_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_usi2_lut[] = {
	{399750, spl_clk_cmgp_usi2_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_i2c1_lut[] = {
	{399750, spl_clk_cmgp_i2c1_blk_cmgp_nm_lut_params},
};
struct vclk_lut div_clk_cmgp_usi1_lut[] = {
	{199875, spl_clk_cmgp_usi1_blk_cmgp_nm_lut_params},
};
struct vclk_lut mux_clk_cmgp_i2c3_lut[] = {
	{399750, spl_clk_cmgp_i2c3_blk_cmgp_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk0_lut[] = {
	{99937, clkcmu_cis_clk0_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_fsys_usb20drd_lut[] = {
	{49968, spl_clk_fsys_usb20drd_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_peri_uart_lut[] = {
	{199875, spl_clk_peri_uart_blk_cmu_nm_lut_params},
};
struct vclk_lut div_clk_cmu_cmuref_lut[] = {
	{199875, occ_cmu_cmuref_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_peri_ip_lut[] = {
	{399750, spl_clk_peri_usi00_i2c_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_apm_bus_lut[] = {
	{399750, spl_clk_chub_i2c0_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_cis_clk1_lut[] = {
	{99937, clkcmu_cis_clk1_blk_cmu_nm_lut_params},
};
struct vclk_lut clkcmu_mif_busp_lut[] = {
	{133250, occ_mif_cmuref_blk_cmu_nm_lut_params},
};
struct vclk_lut ap2cp_shared0_pll_clk_lut[] = {
	{799500, ckbufa_shared0_pll_clk_blk_cmu_nm_lut_params},
};
struct vclk_lut div_clk_cpucl0_cmuref_lut[] = {
	{550000, spl_clk_cpucl0_cmuref_blk_cpucl0_nm_lut_params},
	{300083, spl_clk_cpucl0_cmuref_blk_cpucl0_ud_lut_params},
	{150041, spl_clk_cpucl0_cmuref_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_cluster0_cntclk_lut[] = {
	{275000, div_clk_cluster0_cntclk_blk_cpucl0_nm_lut_params},
	{150041, div_clk_cluster0_cntclk_blk_cpucl0_ud_lut_params},
	{75020, div_clk_cluster0_cntclk_blk_cpucl0_sud_lut_params},
};
struct vclk_lut div_clk_aud_fm_lut[] = {
	{60000, dft_clk_aud_fm_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_uaif1_lut[] = {
	{26000, dft_clk_aud_uaif1_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_cpu_pclkdbg_lut[] = {
	{149999, spl_clk_aud_cpu_pclkdbg_blk_dispaud_nm_lut_params},
	{90000, spl_clk_aud_cpu_pclkdbg_blk_dispaud_ud_lut_params},
	{45000, spl_clk_aud_cpu_pclkdbg_blk_dispaud_sud_lut_params},
};
struct vclk_lut div_clk_aud_dmic_lut[] = {
	{51428, div_clk_aud_dmic_blk_dispaud_sud_lut_params},
	{49999, div_clk_aud_dmic_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_uaif0_lut[] = {
	{26000, dft_clk_aud_uaif0_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_uaif2_lut[] = {
	{25714, dft_clk_aud_uaif2_blk_dispaud_sud_lut_params},
	{24999, dft_clk_aud_uaif2_blk_dispaud_nm_lut_params},
};
struct vclk_lut div_clk_aud_mclk_lut[] = {
	{51428, div_clk_aud_mclk_blk_dispaud_sud_lut_params},
	{49999, div_clk_aud_mclk_blk_dispaud_nm_lut_params},
};
struct vclk_lut mux_mif_cmuref_lut[] = {
	{133250, occ_mif_cmuref_blk_mif_nm_lut_params},
};
struct vclk_lut div_clk_peri_usi00_i2c_lut[] = {
	{199875, spl_clk_peri_usi00_i2c_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_hsi2c_lut[] = {
	{199875, spl_clk_peri_hsi2c_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_peri_usi00_usi_lut[] = {
	{199875, spl_clk_peri_usi00_usi_blk_peri_nm_lut_params},
};
struct vclk_lut mux_clk_peri_spi_lut[] = {
	{399750, spl_clk_peri_spi_blk_peri_nm_lut_params},
};
struct vclk_lut div_clk_vts_dmic_lut[] = {
	{30000, div_clk_vts_dmic_blk_vts_nm_lut_params},
};
struct vclk_lut div_clk_vts_dmic_if_div2_lut[] = {
	{30000, spl_clk_vts_dmic_if_div2_blk_vts_nm_lut_params},
};

/* COMMON LUT */
struct vclk_lut blk_apm_lut[] = {
	{399750, blk_apm_lut_params},
};
struct vclk_lut blk_chub_lut[] = {
	{400000, blk_chub_lut_params},
};
struct vclk_lut blk_cmu_lut[] = {
	{799999, blk_cmu_lut_params},
};
struct vclk_lut blk_core_lut[] = {
	{266500, blk_core_lut_params},
};
struct vclk_lut blk_cpucl0_lut[] = {
	{1100000, blk_cpucl0_lut_params},
};
struct vclk_lut blk_dispaud_lut[] = {
	{1199999, blk_dispaud_lut_params},
};
struct vclk_lut blk_g3d_lut[] = {
	{166562, blk_g3d_lut_params},
};
struct vclk_lut blk_is_lut[] = {
	{166562, blk_is_lut_params},
};
struct vclk_lut blk_mfcmscl_lut[] = {
	{199875, blk_mfcmscl_lut_params},
};
struct vclk_lut blk_vts_lut[] = {
	{200000, blk_vts_lut_params},
};
/*=================VCLK Switch list================================*/

struct vclk_switch vclk_switch_blk_cpucl0[] = {
	{MUX_CLK_CPUCL0_PLL, MUX_CLKCMU_CPUCL0_SWITCH, CLKCMU_CPUCL0_SWITCH, GATE_CLKCMU_CPUCL0_SWITCH, MUX_CLKCMU_CPUCL0_SWITCH_USER, tail_blk_cpucl0_lut, 3},
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_DISPAUD_CPU, CLKCMU_DISPAUD_CPU, GATE_CLKCMU_DISPAUD_CPU, MUX_CLKCMU_DISPAUD_CPU_USER, tail_blk_dispaud_lut, 3},
	{MUX_CLK_MIF_DDRPHY_CLK2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, tail_blk_mif_lut, 2},
};

/*=================VCLK list================================*/

struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK */
	CMUCAL_VCLK(VCLK_VDD_ALIVE, vdd_alive_lut, cmucal_vclk_vdd_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_INT, vdd_int_lut, cmucal_vclk_vdd_int, NULL, vclk_switch_blk_cpucl0),

/* SPECIAL VCLK */
	CMUCAL_VCLK(VCLK_CLKCMU_CHUB_BUS, clkcmu_chub_bus_lut, cmucal_vclk_clkcmu_chub_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_I2C0, div_clk_chub_i2c0_lut, cmucal_vclk_div_clk_chub_i2c0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_CHUB_TIMER_FCLK, clk_chub_timer_fclk_lut, cmucal_vclk_clk_chub_timer_fclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI0, div_clk_chub_usi0_lut, cmucal_vclk_div_clk_chub_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_I2C1, div_clk_chub_i2c1_lut, cmucal_vclk_div_clk_chub_i2c1, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_USI3, mux_clk_cmgp_usi3_lut, cmucal_vclk_mux_clk_cmgp_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_ADC, mux_clk_cmgp_adc_lut, cmucal_vclk_mux_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_I2C0, mux_clk_cmgp_i2c0_lut, cmucal_vclk_mux_clk_cmgp_i2c0, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_I2C2, mux_clk_cmgp_i2c2_lut, cmucal_vclk_mux_clk_cmgp_i2c2, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_USI0, mux_clk_cmgp_usi0_lut, cmucal_vclk_mux_clk_cmgp_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_USI2, mux_clk_cmgp_usi2_lut, cmucal_vclk_mux_clk_cmgp_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_I2C1, mux_clk_cmgp_i2c1_lut, cmucal_vclk_mux_clk_cmgp_i2c1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI1, div_clk_cmgp_usi1_lut, cmucal_vclk_div_clk_cmgp_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_I2C3, mux_clk_cmgp_i2c3_lut, cmucal_vclk_mux_clk_cmgp_i2c3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_FSYS_USB20DRD, clkcmu_fsys_usb20drd_lut, cmucal_vclk_clkcmu_fsys_usb20drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_UART, clkcmu_peri_uart_lut, cmucal_vclk_clkcmu_peri_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMU_CMUREF, div_clk_cmu_cmuref_lut, cmucal_vclk_div_clk_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_IP, clkcmu_peri_ip_lut, cmucal_vclk_clkcmu_peri_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_APM_BUS, clkcmu_apm_bus_lut, cmucal_vclk_clkcmu_apm_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_MIF_BUSP, clkcmu_mif_busp_lut, cmucal_vclk_clkcmu_mif_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_AP2CP_SHARED0_PLL_CLK, ap2cp_shared0_pll_clk_lut, cmucal_vclk_ap2cp_shared0_pll_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_CMUREF, div_clk_cpucl0_cmuref_lut, cmucal_vclk_div_clk_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_CNTCLK, div_clk_cluster0_cntclk_lut, cmucal_vclk_div_clk_cluster0_cntclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_FM, div_clk_aud_fm_lut, cmucal_vclk_div_clk_aud_fm, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF1, div_clk_aud_uaif1_lut, cmucal_vclk_div_clk_aud_uaif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_CPU_PCLKDBG, div_clk_aud_cpu_pclkdbg_lut, cmucal_vclk_div_clk_aud_cpu_pclkdbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_DMIC, div_clk_aud_dmic_lut, cmucal_vclk_div_clk_aud_dmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF0, div_clk_aud_uaif0_lut, cmucal_vclk_div_clk_aud_uaif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_UAIF2, div_clk_aud_uaif2_lut, cmucal_vclk_div_clk_aud_uaif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_MCLK, div_clk_aud_mclk_lut, cmucal_vclk_div_clk_aud_mclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI00_I2C, div_clk_peri_usi00_i2c_lut, cmucal_vclk_div_clk_peri_usi00_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_HSI2C, div_clk_peri_hsi2c_lut, cmucal_vclk_div_clk_peri_hsi2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI00_USI, div_clk_peri_usi00_usi_lut, cmucal_vclk_div_clk_peri_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERI_SPI, mux_clk_peri_spi_lut, cmucal_vclk_mux_clk_peri_spi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_DMIC, div_clk_vts_dmic_lut, cmucal_vclk_div_clk_vts_dmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_DMIC_IF_DIV2, div_clk_vts_dmic_if_div2_lut, cmucal_vclk_div_clk_vts_dmic_if_div2, NULL, NULL),

/* COMMON VCLK */
	CMUCAL_VCLK(VCLK_BLK_APM, blk_apm_lut, cmucal_vclk_blk_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUB, blk_chub_lut, cmucal_vclk_blk_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMU, blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DISPAUD, blk_dispaud_lut, cmucal_vclk_blk_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_IS, blk_is_lut, cmucal_vclk_blk_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFCMSCL, blk_mfcmscl_lut, cmucal_vclk_blk_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),

/* GATING VCLK */
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CMGPALV, NULL, cmucal_vclk_ip_apbif_gpio_cmgpalv, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_INTR_GEN, NULL, cmucal_vclk_ip_apbif_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APM_CMU_APM, NULL, cmucal_vclk_ip_apm_cmu_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DTZPC_APM, NULL, cmucal_vclk_ip_dtzpc_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhm_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_GNSS, NULL, cmucal_vclk_ip_lhm_axi_c_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_MODEM, NULL, cmucal_vclk_ip_lhm_axi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_VTS, NULL, cmucal_vclk_ip_lhm_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_WLBT, NULL, cmucal_vclk_ip_lhm_axi_c_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM, NULL, cmucal_vclk_ip_lhm_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_APM, NULL, cmucal_vclk_ip_lhs_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhs_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhs_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CHUB, NULL, cmucal_vclk_ip_mailbox_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_GNSS, NULL, cmucal_vclk_ip_mailbox_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_VTS, NULL, cmucal_vclk_ip_mailbox_apm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_WLBT, NULL, cmucal_vclk_ip_mailbox_apm_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CHUB, NULL, cmucal_vclk_ip_mailbox_ap_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_GNSS, NULL, cmucal_vclk_ip_mailbox_ap_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_WLBT, NULL, cmucal_vclk_ip_mailbox_ap_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_CHUB, NULL, cmucal_vclk_ip_mailbox_cp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_GNSS, NULL, cmucal_vclk_ip_mailbox_cp_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_WLBT, NULL, cmucal_vclk_ip_mailbox_cp_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_CHUB, NULL, cmucal_vclk_ip_mailbox_gnss_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_WLBT, NULL, cmucal_vclk_ip_mailbox_gnss_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_ABOX, NULL, cmucal_vclk_ip_mailbox_wlbt_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_CHUB, NULL, cmucal_vclk_ip_mailbox_wlbt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PEM, NULL, cmucal_vclk_ip_pem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPEEDY_APM, NULL, cmucal_vclk_ip_speedy_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_APM, NULL, cmucal_vclk_ip_sysreg_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_APM, NULL, cmucal_vclk_ip_wdt_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_APM, NULL, cmucal_vclk_ip_xiu_dp_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX_CHUB, NULL, cmucal_vclk_ip_ahb_busmatrix_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_CHUB, NULL, cmucal_vclk_ip_baaw_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_APM_CHUB, NULL, cmucal_vclk_ip_baaw_p_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_bps_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_CHUB, NULL, cmucal_vclk_ip_bps_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_CMU_CHUB, NULL, cmucal_vclk_ip_chub_cmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_RTC_APBIF, NULL, cmucal_vclk_ip_chub_rtc_apbif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_CHUB, NULL, cmucal_vclk_ip_cm4_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CHUB, NULL, cmucal_vclk_ip_d_tzpc_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGPALV_CHUB_APBIF, NULL, cmucal_vclk_ip_gpio_cmgpalv_chub_apbif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB00, NULL, cmucal_vclk_ip_i2c_chub00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB01, NULL, cmucal_vclk_ip_i2c_chub01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhm_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhm_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhs_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhs_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_CHUB, NULL, cmucal_vclk_ip_pdma_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_CHUB, NULL, cmucal_vclk_ip_pwm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_CHUB, NULL, cmucal_vclk_ip_sweeper_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_P_APM_CHUB, NULL, cmucal_vclk_ip_sweeper_p_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUB, NULL, cmucal_vclk_ip_sysreg_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_CHUB, NULL, cmucal_vclk_ip_timer_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB00, NULL, cmucal_vclk_ip_usi_chub00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CHUB, NULL, cmucal_vclk_ip_wdt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADC_CMGP, NULL, cmucal_vclk_ip_adc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DTZPC_CMGP, NULL, cmucal_vclk_ip_dtzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP0, NULL, cmucal_vclk_ip_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP1, NULL, cmucal_vclk_ip_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP2, NULL, cmucal_vclk_ip_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP3, NULL, cmucal_vclk_ip_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP4, NULL, cmucal_vclk_ip_i2c_cmgp4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP5, NULL, cmucal_vclk_ip_i2c_cmgp5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP6, NULL, cmucal_vclk_ip_i2c_cmgp6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2WLBT, NULL, cmucal_vclk_ip_sysreg_cmgp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP2, NULL, cmucal_vclk_ip_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP3, NULL, cmucal_vclk_ip_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP, NULL, cmucal_vclk_ip_otp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_AHB_SSS, NULL, cmucal_vclk_ip_adm_ahb_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA0, NULL, cmucal_vclk_ip_ad_apb_pdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GIC, NULL, cmucal_vclk_ip_ad_axi_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_SSS, NULL, cmucal_vclk_ip_ad_axi_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_CHUB, NULL, cmucal_vclk_ip_baaw_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_MODEM, NULL, cmucal_vclk_ip_baaw_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VTS, NULL, cmucal_vclk_ip_baaw_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_WLBT, NULL, cmucal_vclk_ip_baaw_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC400_AIHWACG, NULL, cmucal_vclk_ip_gic400_aihwacg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_ABOX, NULL, cmucal_vclk_ip_lhm_axi_d_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhm_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_d_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DPU, NULL, cmucal_vclk_ip_lhm_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_FSYS, NULL, cmucal_vclk_ip_lhm_axi_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_G3D, NULL, cmucal_vclk_ip_lhm_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_GNSS, NULL, cmucal_vclk_ip_lhm_axi_d_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_IS, NULL, cmucal_vclk_ip_lhm_axi_d_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MFCMSCL, NULL, cmucal_vclk_ip_lhm_axi_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VTS, NULL, cmucal_vclk_ip_lhm_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_WLBT, NULL, cmucal_vclk_ip_lhm_axi_d_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhs_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DISPAUD, NULL, cmucal_vclk_ip_lhs_axi_p_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_FSYS, NULL, cmucal_vclk_ip_lhs_axi_p_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_GNSS, NULL, cmucal_vclk_ip_lhs_axi_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_IS, NULL, cmucal_vclk_ip_lhs_axi_p_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFCMSCL, NULL, cmucal_vclk_ip_lhs_axi_p_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MODEM, NULL, cmucal_vclk_ip_lhs_axi_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERI, NULL, cmucal_vclk_ip_lhs_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VTS, NULL, cmucal_vclk_ip_lhs_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_WLBT, NULL, cmucal_vclk_ip_lhs_axi_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_CORE, NULL, cmucal_vclk_ip_pdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APBIF_CMU_TOPC, NULL, cmucal_vclk_ip_sfr_apbif_cmu_topc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIREX, NULL, cmucal_vclk_ip_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA_CORE, NULL, cmucal_vclk_ip_spdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NRT, NULL, cmucal_vclk_ip_trex_d_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_CORE, NULL, cmucal_vclk_ip_trex_p_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CORE, NULL, cmucal_vclk_ip_xiu_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CSSYS_MIF, NULL, cmucal_vclk_ip_adm_apb_g_cssys_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_adm_apb_g_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_AHB_G_CSSYS_SSS, NULL, cmucal_vclk_ip_ads_ahb_g_cssys_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_G_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_ads_apb_g_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_P_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_ad_apb_p_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS_DBG, NULL, cmucal_vclk_ip_cssys_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_d_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON0, NULL, cmucal_vclk_ip_ad_apb_decon0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_ABOX, NULL, cmucal_vclk_ip_ad_apb_smmu_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_32to128, NULL, cmucal_vclk_ip_axi_us_32to128, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_DISPAUD, NULL, cmucal_vclk_ip_dftmux_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DISPAUD_CMU_DISPAUD, NULL, cmucal_vclk_ip_dispaud_cmu_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC, NULL, cmucal_vclk_ip_dmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DISPAUD, NULL, cmucal_vclk_ip_d_tzpc_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_DISPAUD, NULL, cmucal_vclk_ip_gpio_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DISPAUD, NULL, cmucal_vclk_ip_lhm_axi_p_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_ABOX, NULL, cmucal_vclk_ip_lhs_axi_d_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DPU, NULL, cmucal_vclk_ip_lhs_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ABOX, NULL, cmucal_vclk_ip_ppmu_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPU, NULL, cmucal_vclk_ip_ppmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_ABOX, NULL, cmucal_vclk_ip_smmu_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_DPU, NULL, cmucal_vclk_ip_smmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DISPAUD, NULL, cmucal_vclk_ip_sysreg_dispaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_FSYS, NULL, cmucal_vclk_ip_d_tzpc_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_FSYS_CMU_FSYS, NULL, cmucal_vclk_ip_fsys_cmu_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_FSYS, NULL, cmucal_vclk_ip_gpio_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_FSYS, NULL, cmucal_vclk_ip_lhm_axi_p_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_FSYS, NULL, cmucal_vclk_ip_lhs_axi_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_EMBD, NULL, cmucal_vclk_ip_mmc_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_FSYS, NULL, cmucal_vclk_ip_ppmu_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_FSYS, NULL, cmucal_vclk_ip_sysreg_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB20DRD_TOP, NULL, cmucal_vclk_ip_usb20drd_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_US_64to128_FSYS, NULL, cmucal_vclk_ip_us_64to128_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_FSYS, NULL, cmucal_vclk_ip_xiu_d_fsys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNC_G3D_P, NULL, cmucal_vclk_ip_async_g3d_p, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D, NULL, cmucal_vclk_ip_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_G3D, NULL, cmucal_vclk_ip_lhs_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_IS, NULL, cmucal_vclk_ip_d_tzpc_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IS_CMU_IS, NULL, cmucal_vclk_ip_is_cmu_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_IS, NULL, cmucal_vclk_ip_lhm_axi_p_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_IS, NULL, cmucal_vclk_ip_lhs_axi_d_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_IS, NULL, cmucal_vclk_ip_sysreg_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_is3p21p0_IS, NULL, cmucal_vclk_ip_is3p21p0_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_JPEG, NULL, cmucal_vclk_ip_as_apb_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_M2M, NULL, cmucal_vclk_ip_as_apb_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MCSC, NULL, cmucal_vclk_ip_as_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC, NULL, cmucal_vclk_ip_as_apb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_NS_MFCMSCL, NULL, cmucal_vclk_ip_as_apb_sysmmu_ns_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_S_MFCMSCL, NULL, cmucal_vclk_ip_as_apb_sysmmu_s_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_M2M, NULL, cmucal_vclk_ip_as_axi_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_MFCMSCL, NULL, cmucal_vclk_ip_axi2apb_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFCMSCL, NULL, cmucal_vclk_ip_d_tzpc_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG, NULL, cmucal_vclk_ip_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFCMSCL, NULL, cmucal_vclk_ip_lhm_axi_p_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_MFCMSCL, NULL, cmucal_vclk_ip_lhs_axi_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC, NULL, cmucal_vclk_ip_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFCMSCL_CMU_MFCMSCL, NULL, cmucal_vclk_ip_mfcmscl_cmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCMSCL, NULL, cmucal_vclk_ip_ppmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCMSCL, NULL, cmucal_vclk_ip_sysmmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFCMSCL, NULL, cmucal_vclk_ip_sysreg_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFCMSCL, NULL, cmucal_vclk_ip_xiu_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DMC_CPU, NULL, cmucal_vclk_ip_ppmu_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_DMC_CPU, NULL, cmucal_vclk_ip_qe_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MODEM_CMU_MODEM, NULL, cmucal_vclk_ip_modem_cmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_TMU, NULL, cmucal_vclk_ip_busif_tmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERI, NULL, cmucal_vclk_ip_d_tzpc_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERI, NULL, cmucal_vclk_ip_gpio_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_0, NULL, cmucal_vclk_ip_i2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_1, NULL, cmucal_vclk_ip_i2c_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_2, NULL, cmucal_vclk_ip_i2c_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERI, NULL, cmucal_vclk_ip_lhm_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_CMU_PERI, NULL, cmucal_vclk_ip_peri_cmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_MOTOR, NULL, cmucal_vclk_ip_pwm_motor, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERI, NULL, cmucal_vclk_ip_sysreg_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_I2C, NULL, cmucal_vclk_ip_usi00_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_USI, NULL, cmucal_vclk_ip_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_I2C_0, NULL, cmucal_vclk_ip_usi_i2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_SPI, NULL, cmucal_vclk_ip_usi_spi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_UART, NULL, cmucal_vclk_ip_usi_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CLUSTER0, NULL, cmucal_vclk_ip_wdt_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX, NULL, cmucal_vclk_ip_ahb_busmatrix, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_C_VTS, NULL, cmucal_vclk_ip_baaw_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_VTS, NULL, cmucal_vclk_ip_baaw_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_LP_VTS, NULL, cmucal_vclk_ip_bps_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_P_VTS, NULL, cmucal_vclk_ip_bps_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORTEXM4INTEGRATION, NULL, cmucal_vclk_ip_cortexm4integration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB1, NULL, cmucal_vclk_ip_dmic_ahb1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF, NULL, cmucal_vclk_ip_dmic_if, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VTS, NULL, cmucal_vclk_ip_d_tzpc_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_VTS, NULL, cmucal_vclk_ip_gpio_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC1, NULL, cmucal_vclk_ip_hwacg_sys_dmic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhm_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VTS, NULL, cmucal_vclk_ip_lhm_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_VTS, NULL, cmucal_vclk_ip_lhs_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VTS, NULL, cmucal_vclk_ip_lhs_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_ABOX_VTS, NULL, cmucal_vclk_ip_mailbox_abox_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_VTS, NULL, cmucal_vclk_ip_sweeper_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_VTS, NULL, cmucal_vclk_ip_sweeper_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VTS, NULL, cmucal_vclk_ip_sysreg_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER, NULL, cmucal_vclk_ip_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VTS_CMU_VTS, NULL, cmucal_vclk_ip_vts_cmu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_VTS, NULL, cmucal_vclk_ip_wdt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_LP_VTS, NULL, cmucal_vclk_ip_xhb_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_P_VTS, NULL, cmucal_vclk_ip_xhb_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_u_DMIC_CLK_MUX, NULL, cmucal_vclk_ip_u_dmic_clk_mux, NULL, NULL),
};
unsigned int cmucal_vclk_size = 327;

