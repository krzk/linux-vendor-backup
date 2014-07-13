#ifndef _DT_BINDINGS_CLOCK_EXYNOS_5410_H
#define _DT_BINDINGS_CLOCK_EXYNOS_5410_H

/* core clocks */
#define CLK_FIN_PLL 1
#define CLK_FOUT_APLL 2
#define CLK_FOUT_BPLL 3
#define CLK_FOUT_CPLL 4
#define CLK_FOUT_DPLL 5
#define CLK_FOUT_EPLL 6
#define CLK_FOUT_IPLL 7
#define CLK_FOUT_KPLL 8
#define CLK_FOUT_MPLL 9
#define CLK_FOUT_VPLL 10

/* some fixed rate clocks */
#define CLK_SCLK_HDMIPHY 20


/* gate clocks */
#define CLK_SCLK_MMC0 50
#define CLK_SCLK_MMC1 51
#define CLK_SCLK_MMC2 52

#define CLK_SCLK_FIMD1 56
#define CLK_SCLK_HDMI 57
#define CLK_SCLK_PIXEL 58
#define CLK_SCLK_DP1 59
#define CLK_PDMA0 60
#define CLK_PDMA1 61
#define CLK_MDMA0 62
#define CLK_MDMA1 63
#define CLK_SATA  64
#define CLK_USBOTG 65
#define CLK_MIPI_HSI 66

#define CLK_MMC0 70
#define CLK_MMC1 71
#define CLK_MMC2 72
#define CLK_SROMC 73
#define CLK_USB2 74
#define CLK_USBPHY300 75
#define CLK_USBPHY301 76

#define CLK_UART0 80
#define CLK_UART1 81
#define CLK_UART2 82
#define CLK_UART3 83

#define CLK_I2C0 90
#define CLK_I2C1 91
#define CLK_I2C2 92
#define CLK_I2C3 93
#define CLK_I2C4 94
#define CLK_I2C5 95
#define CLK_I2C6 96
#define CLK_I2C7 97
#define CLK_I2C_HDMI 98
#define CLK_ADC 99
#define CLK_SPI0 100
#define CLK_SPI1 101
#define CLK_SPI2 102
#define CLK_I2S0 103
#define CLK_I2S1 104
#define CLK_I2S2 105
#define CLK_PCM1 106
#define CLK_PCM2 107
#define CLK_PWM  108
#define CLK_PCM0 109
#define CLK_SPDIF 110
#define CLK_AC97 111
#define CLK_KEYIF 112

#define CLK_CHIPID 120
#define CLK_SYSREG 121
#define CLK_PMU_APBIF 122
#define CLK_CMU_TOPPART 123
#define CLK_CMU_CORE 124
#define CLK_CMU_MEM 125

#define CLK_TZPC0 130
#define CLK_TZPC1 131
#define CLK_TZPC2 132
#define CLK_TZPC3 133
#define CLK_TZPC4 134
#define CLK_TZPC5 135
#define CLK_TZPC6 136
#define CLK_TZPC7 137
#define CLK_TZPC8 138
#define CLK_TZPC9 139
#define CLK_HDMI_CEC 140
#define CLK_SECKEY_APBIF 141
#define CLK_MCT 142
#define CLK_WDT 143
#define CLK_RTC 144
#define CLK_TMU_APBIF 145

#define CLK_SCLK_PWM 150
#define CLK_SCLK_MAU_AUDIO0 151
#define CLK_SCLK_MAU_PCM0 152
#define CLK_SCLK_AUDIO0 153
#define CLK_SCLK_AUDIO1 154
#define CLK_SCLK_AUDIO2 155
#define CLK_SCLK_SPDIF 156

#define CLK_SCLK_UART0 160
#define CLK_SCLK_UART1 161
#define CLK_SCLK_UART2 162
#define CLK_SCLK_UART3 163

#define CLK_FIMD1 170
#define CLK_MIE1  171
#define CLK_DSIM1 172
#define CLK_DP    173
#define CLK_MIXER 174
#define CLK_HDMI  175

#define CLK_GSCL0 180
#define CLK_GSCL1 181
#define CLK_GSCL2 182
#define CLK_GSCL3 183
#define CLK_GSCL4 184

#define CLK_MFC 190
#define CLK_SMMU_MFCL 191
#define CLK_SMMU_MFCR 192


#define CLK_SCLK_I2S1 63
#define CLK_SCLK_I2S2 64
#define CLK_SCLK_PCM1 142
#define CLK_SCLK_PCM2 143


/* Div clocks */
#define CLK_DIV_HDMI_PIXEL 300
#define CLK_DIV_USBPHY300 152
#define CLK_DIV_USBPHY301 153

/* mux clocks */
#define CLK_MOUT_EPLL 400
#define CLK_MOUT_MAU_AUDIO0 401
#define CLK_MOUT_AUDIO0 402
#define CLK_MOUT_HDMI 403


#define CLK_NR_CLKS 512

#endif /* _DT_BINDINGS_CLOCK_EXYNOS_5410_H */
