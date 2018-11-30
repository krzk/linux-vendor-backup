#include "../cmucal.h"

#include "cmucal-vclklut.h"

/*=================CMUCAL version: S5E9110================================*/

/*=================LUT in each VCLK================================*/
unsigned int vdd_alive_nm_lut_params[] = {
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 13,
};
unsigned int vdd_alive_sud_lut_params[] = {
	 4, 0, 0, 0, 0, 0, 0, 0, 0, 2,
};
unsigned int vdd_int_nm_lut_params[] = {
	 1, 2, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 0, 1, 0, 1100000, 23, 1199999, 3201250, 0,
};
unsigned int vdd_int_sud_lut_params[] = {
	 3, 7, 1, 1, 3, 1, 3, 2, 1, 3, 3, 2, 0, 3, 3, 0, 1, 3, 3, 0, 3, 300083, 6, 360000, 1333090, 1,
};
unsigned int vdd_int_ud_lut_params[] = {
	 1, 3, 0, 0, 1, 0, 0, 1, 0, 1, 1, 3, 1, 3, 3, 2, 0, 1, 3, 0, 3, 600166, 13, 720000, 2666444, 1,
};
unsigned int spl_clk_chub_i2c0_blk_apm_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_chub_i2c0_blk_apm_sud_lut_params[] = {
	 0,
};
unsigned int spl_clk_chub_i2c0_blk_chub_nm_lut_params[] = {
	 1, 1,
};
unsigned int mux_clk_chub_timer_fclk_blk_chub_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_chub_usi0_blk_chub_nm_lut_params[] = {
	 7, 1,
};
unsigned int spl_clk_chub_i2c1_blk_chub_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_usi3_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int mux_clk_cmgp_adc_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int mux_clk_cmgp_adc_blk_cmgp_sud_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_i2c0_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_i2c2_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_usi0_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_usi2_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_i2c1_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cmgp_usi1_blk_cmgp_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_cmgp_i2c3_blk_cmgp_nm_lut_params[] = {
	 1,
};
unsigned int clkcmu_cis_clk0_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int spl_clk_fsys_usb20drd_blk_cmu_nm_lut_params[] = {
	 7, 1,
};
unsigned int spl_clk_peri_uart_blk_cmu_nm_lut_params[] = {
	 1, 1,
};
unsigned int occ_cmu_cmuref_blk_cmu_nm_lut_params[] = {
	 1, 1, 0,
};
unsigned int spl_clk_peri_usi00_i2c_blk_cmu_nm_lut_params[] = {
	 0, 1,
};
unsigned int spl_clk_chub_i2c0_blk_cmu_nm_lut_params[] = {
	 0, 0,
};
unsigned int clkcmu_cis_clk1_blk_cmu_nm_lut_params[] = {
	 3, 1,
};
unsigned int occ_mif_cmuref_blk_cmu_nm_lut_params[] = {
	 2, 0,
};
unsigned int ckbufa_shared0_pll_clk_blk_cmu_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_sud_lut_params[] = {
	 1,
};
unsigned int spl_clk_cpucl0_cmuref_blk_cpucl0_ud_lut_params[] = {
	 1,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_nm_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_sud_lut_params[] = {
	 3,
};
unsigned int div_clk_cluster0_cntclk_blk_cpucl0_ud_lut_params[] = {
	 3,
};
unsigned int dft_clk_aud_fm_blk_dispaud_nm_lut_params[] = {
	 0,
};
unsigned int dft_clk_aud_uaif1_blk_dispaud_nm_lut_params[] = {
	 0, 1,
};
unsigned int spl_clk_aud_cpu_pclkdbg_blk_dispaud_nm_lut_params[] = {
	 7,
};
unsigned int spl_clk_aud_cpu_pclkdbg_blk_dispaud_sud_lut_params[] = {
	 7,
};
unsigned int spl_clk_aud_cpu_pclkdbg_blk_dispaud_ud_lut_params[] = {
	 7,
};
unsigned int div_clk_aud_dmic_blk_dispaud_nm_lut_params[] = {
	 0,
};
unsigned int div_clk_aud_dmic_blk_dispaud_sud_lut_params[] = {
	 0,
};
unsigned int dft_clk_aud_uaif0_blk_dispaud_nm_lut_params[] = {
	 0, 1,
};
unsigned int dft_clk_aud_uaif2_blk_dispaud_nm_lut_params[] = {
	 1,
};
unsigned int dft_clk_aud_uaif2_blk_dispaud_sud_lut_params[] = {
	 1,
};
unsigned int div_clk_aud_mclk_blk_dispaud_nm_lut_params[] = {
	 0,
};
unsigned int div_clk_aud_mclk_blk_dispaud_sud_lut_params[] = {
	 0,
};
unsigned int occ_mif_cmuref_blk_mif_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_peri_usi00_i2c_blk_peri_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_peri_hsi2c_blk_peri_nm_lut_params[] = {
	 1,
};
unsigned int spl_clk_peri_usi00_usi_blk_peri_nm_lut_params[] = {
	 1, 1,
};
unsigned int spl_clk_peri_spi_blk_peri_nm_lut_params[] = {
	 1,
};
unsigned int div_clk_vts_dmic_blk_vts_nm_lut_params[] = {
	 0,
};
unsigned int spl_clk_vts_dmic_if_div2_blk_vts_nm_lut_params[] = {
	 0,
};
unsigned int blk_apm_lut_params[] = {
	 1, 1, 1,
};
unsigned int blk_chub_lut_params[] = {
	 0, 1,
};
unsigned int blk_cmu_lut_params[] = {
	 5, 0, 1, 0, 5, 0, 5, 799999, 1, 1, 1, 2, 2, 1332500, 1, 1599000,
};
unsigned int blk_core_lut_params[] = {
	 1, 0,
};
unsigned int blk_cpucl0_lut_params[] = {
	 3, 0, 3,
};
unsigned int blk_dispaud_lut_params[] = {
	 1, 1, 1, 1, 0, 0, 0,
};
unsigned int blk_g3d_lut_params[] = {
	 3,
};
unsigned int blk_is_lut_params[] = {
	 1,
};
unsigned int blk_mfcmscl_lut_params[] = {
	 1,
};
unsigned int blk_vts_lut_params[] = {
	 0, 1, 1,
};
