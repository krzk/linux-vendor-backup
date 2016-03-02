#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <plat/gpio-cfg.h>

#include <mach/gpio.h>
#include "board-universal3250.h"

#define ENABLE_PD_GPIO 1
#define ENABLE_PD_GPIO_ALIVE 1

 unsigned init_gpio_tables[][3] =
{
	{ EXYNOS3_GPA1(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPA1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPA1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPA1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPA1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPA1(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPB(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPB(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPB(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPB(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPC0(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC0(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC0(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC0(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC0(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPC1(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPC1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPD0(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE},			/* MUIC_SDA */
	{ EXYNOS3_GPD0(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE },			/* MUIC_SCL*/

	{ EXYNOS3_GPD1(0), S3C_GPIO_SFN(0x2), S3C_GPIO_PULL_NONE },			/* AP_PMIC_SDA */
	{ EXYNOS3_GPD1(1), S3C_GPIO_SFN(0x2), S3C_GPIO_PULL_NONE },			/* AP_PMIC_SCL */
	{ EXYNOS3_GPD1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE },			/* FUEL_SDA */
	{ EXYNOS3_GPD1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_NONE },			/* FUEL_SCL */

	{ EXYNOS3_GPE0(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE0(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE0(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE0(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE0(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE0(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPE1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE1(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPE1(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	#if 0 /* it's occured to kernel panic */
	{ EXYNOS3_GPK0(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* TP*/
	#endif

	{ EXYNOS3_GPK1(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPK1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPK1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK1(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK1(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPK2(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPK2(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPM0(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPM0(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPM0(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM0(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM0(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM0(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM0(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM0(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPM1(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPM1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPM1(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM1(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM1(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPM2(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPM2(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPM2(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM2(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM2(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPM3(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPM3(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPM3(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM3(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM3(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM3(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM3(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM3(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

	{ EXYNOS3_GPM4(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },   		/* NC */
	{ EXYNOS3_GPM4(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },     		/* NC */
	{ EXYNOS3_GPM4(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM4(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM4(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */
	{ EXYNOS3_GPM4(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN },			/* NC */

};

unsigned int  init_gpio_alive_table[][3]=
{
	{EXYNOS3_GPX0(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX1(5), S3C_GPIO_SFN(0xf), S3C_GPIO_PULL_NONE},	/*CHG_INTB*/
	{EXYNOS3_GPX1(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX2(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX2(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX2(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX2(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX2(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX2(5), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX3(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	#if 0 /* it's occured to loc-up issue */
	{EXYNOS3_GPX3(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	#endif
	{EXYNOS3_GPX3(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
};

unsigned int sleep_gpio_alive_table[][3] =
{
	{EXYNOS3_GPX0(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX1(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX1(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX1(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX1(5), S3C_GPIO_SFN(0xf), S3C_GPIO_PULL_NONE},	/*CHG_INTB*/
	{EXYNOS3_GPX1(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX2(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
        {EXYNOS3_GPX2(1), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
        {EXYNOS3_GPX2(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
        {EXYNOS3_GPX2(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/

	{EXYNOS3_GPX3(0), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(2), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(3), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(4), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(6), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
	{EXYNOS3_GPX3(7), S3C_GPIO_INPUT, S3C_GPIO_PULL_DOWN},	/*NC*/
};

 unsigned int sleep_gpio_pd_table[][3] =
{
	{EXYNOS3_GPA0(0),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* BT_UART_RXD */
	{EXYNOS3_GPA0(1),  S5P_GPIO_PD_OUTPUT1, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* BT_UART_TXD */
	{EXYNOS3_GPA0(2),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* BT_UART_CTS */
	{EXYNOS3_GPA0(3),  S5P_GPIO_PD_OUTPUT1, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* BT_UART_RTS */
	{EXYNOS3_GPA0(4),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE},	 /* AP_RXD */
	{EXYNOS3_GPA0(5),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE},	 /* AP_TXD */
	{EXYNOS3_GPA0(6),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* TSP_SDA */
	{EXYNOS3_GPA0(7),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},	 /* TSP_SCL */

	{ EXYNOS3_GPA1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* NC */
	{ EXYNOS3_GPA1(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* NC */
	{ EXYNOS3_GPA1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },	/* FPGA_SPI_SI*/
	{ EXYNOS3_GPA1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },	/* FPGA_SPI_CLK  */
	{ EXYNOS3_GPA1(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },	/* NC */
	{ EXYNOS3_GPA1(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },	/* NC */

	{ EXYNOS3_GPB(0),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* SHUB_SPI_SCK */
	{ EXYNOS3_GPB(1),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* SHUB_SPI_SSN */
	{ EXYNOS3_GPB(2),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* SHUB_SPI_MISO */
	{ EXYNOS3_GPB(3),  S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* SHUB_SPI_MOSI */
	{ EXYNOS3_GPB(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },	/* NC */
	{ EXYNOS3_GPB(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* NC */
	{ EXYNOS3_GPB(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },		/* NC */
	{ EXYNOS3_GPB(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },	/* NC */

	{ EXYNOS3_GPC0(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },	 			 /* NC */
	{ EXYNOS3_GPC0(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				 /* NC */
	{ EXYNOS3_GPC0(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				 /* NC */
	{ EXYNOS3_GPC0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				 /* NC */
	{ EXYNOS3_GPC0(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				 /* NC */

	{ EXYNOS3_GPC1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPC1(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPC1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPC1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPC1(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */

	{ EXYNOS3_GPD0(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPD0(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },				/* NC */
	{ EXYNOS3_GPD0(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },				/* MUIC_SDA */
	{ EXYNOS3_GPD0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* MUIC_SCL */

	{ EXYNOS3_GPD1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* AP_PMIC_SDA */
	{ EXYNOS3_GPD1(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* AP_PMIC_SCL */
	{ EXYNOS3_GPD1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* FUEL_SCL */
	{ EXYNOS3_GPD1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },    			/* FUEL_SDA */

	{ EXYNOS3_GPE0(0), S5P_GPIO_PD_PREV_STATE, S5P_GPIO_PD_UPDOWN_DISABLE },        /* BT_EN */
	{ EXYNOS3_GPE0(1), S5P_GPIO_PD_PREV_STATE, S5P_GPIO_PD_DOWN_ENABLE },        			/* DISP_RST */
	{ EXYNOS3_GPE0(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPE0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPE0(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },      		/* NC */
	{ EXYNOS3_GPE0(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPE0(6), S5P_GPIO_PD_OUTPUT0, S5P_GPIO_PD_UPDOWN_DISABLE },        			/* FPGA_SPI_EN */
	{ EXYNOS3_GPE0(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */


	{ EXYNOS3_GPE1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPE1(1), S5P_GPIO_PD_PREV_STATE, S5P_GPIO_PD_UPDOWN_DISABLE },        	/* WRSTBI */
	{ EXYNOS3_GPE1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPE1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     			/* NC */
	{ EXYNOS3_GPE1(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     			/* NC */
	{ EXYNOS3_GPE1(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPE1(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPE1(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* HW_REV0 */

	{ EXYNOS3_GPE2(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* HW_REV1 */
	{ EXYNOS3_GPE2(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },    			 /* HW_REV2 */
	{ EXYNOS3_GPE2(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },     		/* HW_REV3 */

	{ EXYNOS3_GPK0(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* TP */
	{ EXYNOS3_GPK0(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_CMD */
	{ EXYNOS3_GPK0(2), S5P_GPIO_PD_OUTPUT0, S5P_GPIO_PD_UPDOWN_DISABLE},        	/* eMMC_EN */
	{ EXYNOS3_GPK0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE }, 				/* NAND_D0 */
	{ EXYNOS3_GPK0(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_D1 */
	{ EXYNOS3_GPK0(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE }, 				/* NAND_D2 */
	{ EXYNOS3_GPK0(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE }, 				/* NAND_D3 */
	{ EXYNOS3_GPK0(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE }, 				/* 10K PD */

	{ EXYNOS3_GPK1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },   		/* NC */
	{ EXYNOS3_GPK1(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     		/* NC */
	{ EXYNOS3_GPK1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        		/* NC */
	{ EXYNOS3_GPK1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     		/* NC */
	{ EXYNOS3_GPK1(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     		/* NC */
	{ EXYNOS3_GPK1(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     		/* NC */
	{ EXYNOS3_GPK1(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     		/* NC */

	{ EXYNOS3_GPK2(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPK2(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPK2(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPK2(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPK2(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPK2(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPK2(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */

	{ EXYNOS3_GPL0(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_D4 */
	{ EXYNOS3_GPL0(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_D5 */
	{ EXYNOS3_GPL0(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_D6 */
	{ EXYNOS3_GPL0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },  			/* NAND_D7 */

	{ EXYNOS3_GPM0(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM0(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM0(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM0(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM0(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM0(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM0(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM0(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */

	{ EXYNOS3_GPM1(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM1(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */

	{ EXYNOS3_GPM2(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM2(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM2(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM2(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM2(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */

	{ EXYNOS3_GPM3(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },      			/* NC */
	{ EXYNOS3_GPM3(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM3(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */

	{ EXYNOS3_GPM4(0), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },     			/* NC */
	{ EXYNOS3_GPM4(1), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },    			/* NC */
	{ EXYNOS3_GPM4(2), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },       			/* AF_SCL  */
	{ EXYNOS3_GPM4(3), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE },       			/* AF_SDA */
	{ EXYNOS3_GPM4(4), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },      			/* NC */
	{ EXYNOS3_GPM4(5), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },        			/* NC */
	{ EXYNOS3_GPM4(6), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
	{ EXYNOS3_GPM4(7), S5P_GPIO_PD_INPUT, S5P_GPIO_PD_DOWN_ENABLE },       			/* NC */
};

#ifdef ENABLE_PD_GPIO
static void config_init_gpio_table_alive(void)
{
	int i=0, gpio;

	for (i = 0; i < ARRAY_SIZE(init_gpio_alive_table); i++)
	{
		gpio = init_gpio_alive_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SPECIAL(init_gpio_alive_table[i][1]));
		s3c_gpio_setpull(gpio, init_gpio_alive_table[i][2]);
	}
}

static void config_init_gpio_table(void)
{
	int i=0, gpio;
	for (i = 0; i <ARRAY_SIZE(init_gpio_tables);  i++)
	{
		gpio = init_gpio_tables[i][0];
		s3c_gpio_cfgpin(gpio, init_gpio_tables[i][1]);
		s3c_gpio_setpull(gpio, init_gpio_tables[i][2]);

 	}
}

static void config_sleep_gpio_table_alive(void)
{
	int i=0, gpio;

	for (i = 0; i < ARRAY_SIZE(sleep_gpio_alive_table); i++)
	{
		gpio = sleep_gpio_alive_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SPECIAL( sleep_gpio_alive_table[i][1]));
		s3c_gpio_setpull(gpio,  sleep_gpio_alive_table[i][2]);
	}
}

static void config_sleep_gpio_table_pd(void)
{
	int i=0, gpio;

	for (i = 0; i < ARRAY_SIZE(sleep_gpio_pd_table); i++)
	{
		gpio =  sleep_gpio_pd_table[i][0];
		s5p_gpio_set_pd_cfg(gpio, sleep_gpio_pd_table[i][1]);
		s5p_gpio_set_pd_pull(gpio, sleep_gpio_pd_table[i][2]);
	}
}
#endif


void config_init_gpio_tables(void)
{
	config_init_gpio_table();
	config_init_gpio_table_alive();
}


static void config_sleep_gpio_tables(void)
{
#ifdef ENABLE_PD_GPIO_ALIVE
	config_sleep_gpio_table_alive();
#endif
#ifdef ENABLE_PD_GPIO
	config_sleep_gpio_table_pd();
#endif
}

#ifdef CONFIG_DEBUG_FS
struct pd_gpio_debugfs_data {
	char *buffer;
	size_t buffer_size;
	size_t data_size;
};

struct value_name_map {
	unsigned int value;
	char name[16];
};

static const struct value_name_map gpio_conf_name_table[] = {
	{S3C_GPIO_INPUT, "INPUT"},
	{S3C_GPIO_OUTPUT, "OUTPUT"},
	{S3C_GPIO_SFN(0x2), "SFN(2)"},
	{S3C_GPIO_SFN(0x3), "SFN(3)"},
	{S3C_GPIO_SFN(0x4), "SFN(4)"},
	{S3C_GPIO_SFN(0x5), "SFN(5)"},
	{S3C_GPIO_SFN(0x6), "SFN(6)"},
	{S3C_GPIO_SFN(0x7), "SFN(7)"},
	{S3C_GPIO_SFN(0x8), "SFN(8)"},
	{S3C_GPIO_SFN(0x9), "SFN(9)"},
	{S3C_GPIO_SFN(0xa), "SFN(a)"},
	{S3C_GPIO_SFN(0xb), "SFN(b)"},
	{S3C_GPIO_SFN(0xc), "SFN(c)"},
	{S3C_GPIO_SFN(0xd), "SFN(d)"},
	{S3C_GPIO_SFN(0xe), "SFN(e)"},
	{S3C_GPIO_SFN(0xf), "SFN(f)"}
};

static const struct value_name_map gpio_pull_name_table[] = {
	{S3C_GPIO_PULL_NONE, "PULL_NONE"},
	{S3C_GPIO_PULL_DOWN, "PULL_DOWN"},
	{S3C_GPIO_PULL_UP, "PULL_UP"}
};

static const struct value_name_map gpio_pd_conf_name_table[] = {
	{S5P_GPIO_PD_OUTPUT0, "OUT0"},
	{S5P_GPIO_PD_OUTPUT1, "OUT1"},
	{S5P_GPIO_PD_INPUT, "INPUT"},
	{S5P_GPIO_PD_PREV_STATE, "PREV"}
};

static const struct value_name_map gpio_pd_pull_name_table[] = {
	{S5P_GPIO_PD_UPDOWN_DISABLE, "PULL_NONE"},
	{S5P_GPIO_PD_DOWN_ENABLE, "PULL_DOWN"},
	{S5P_GPIO_PD_UP_ENABLE, "PULL_UP"}
};

static const struct value_name_map gpio_name_table[] = {
	{EXYNOS3_GPIO_A0_START, "EXYNOS3_GPA0"},
	{EXYNOS3_GPIO_A1_START, "EXYNOS3_GPA1"},
	{EXYNOS3_GPIO_B_START, "EXYNOS3_GPB"},
	{EXYNOS3_GPIO_C0_START, "EXYNOS3_GPC0"},
	{EXYNOS3_GPIO_C1_START, "EXYNOS3_GPC1"},
	{EXYNOS3_GPIO_D0_START, "EXYNOS3_GPD0"},
	{EXYNOS3_GPIO_D1_START, "EXYNOS3_GPD1"},
	{EXYNOS3_GPIO_E0_START, "EXYNOS3_GPE0"},
	{EXYNOS3_GPIO_E1_START, "EXYNOS3_GPE1"},
	{EXYNOS3_GPIO_E2_START, "EXYNOS3_GPE2"},
	{EXYNOS3_GPIO_K0_START, "EXYNOS3_GPK0"},
	{EXYNOS3_GPIO_K1_START, "EXYNOS3_GPK1"},
	{EXYNOS3_GPIO_K2_START, "EXYNOS3_GPK2"},
	{EXYNOS3_GPIO_L0_START, "EXYNOS3_GPL0"},
	{EXYNOS3_GPIO_X0_START, "EXYNOS3_GPX0"},
	{EXYNOS3_GPIO_X1_START, "EXYNOS3_GPX1"},
	{EXYNOS3_GPIO_X2_START, "EXYNOS3_GPX2"},
	{EXYNOS3_GPIO_X3_START, "EXYNOS3_GPX3"},
	{EXYNOS3_GPIO_M0_START, "EXYNOS3_GPM0"},
	{EXYNOS3_GPIO_M1_START, "EXYNOS3_GPM1"},
	{EXYNOS3_GPIO_M2_START, "EXYNOS3_GPM2"},
	{EXYNOS3_GPIO_M3_START, "EXYNOS3_GPM3"},
	{EXYNOS3_GPIO_M4_START, "EXYNOS3_GPM4"},
};

static unsigned int name_to_value(const struct value_name_map *first, const struct value_name_map *last, const char *name)
{
	pr_debug("%s: name=%s\n", __func__, name);
	do {
		if (strncasecmp(first->name, name, sizeof(first->name)) == 0) {
			pr_debug("%s: value=%u\n", __func__, first->value);
			return (int)first->value;
		}
	} while (++first <= last);

	pr_debug("%s: No matching name: name=%s, value=%u\n", __func__, name, first->value);
	return 0;
}

static const char *value_to_name(const struct value_name_map *first, const struct value_name_map *last, unsigned int value)
{
	pr_debug("%s: value=%u\n", __func__, value);
	do {
		if (first->value == value) {
			pr_debug("%s: name=%s\n", __func__, first->name);
			return first->name;
		}
	} while (++first <= last);

	pr_debug("%s: No matching value: name=%s, value=%u\n", __func__, first->name, value);
	return "";
}

static unsigned int name_to_gpio_conf(const char *name) {
	return name_to_value(&gpio_conf_name_table[0],
			&gpio_conf_name_table[ARRAY_SIZE(gpio_conf_name_table) - 1],
			name);
}

static const char *gpio_conf_to_name(unsigned int gpio_conf) {
	return value_to_name(&gpio_conf_name_table[0],
			&gpio_conf_name_table[ARRAY_SIZE(gpio_conf_name_table) - 1],
			gpio_conf);
}

static unsigned int name_to_gpio_pull(const char *name) {
	return name_to_value(&gpio_pull_name_table[0],
			&gpio_pull_name_table[ARRAY_SIZE(gpio_pull_name_table) - 1],
			name);
}

static const char *gpio_pull_to_name(unsigned int gpio_pull) {
	return value_to_name(&gpio_pull_name_table[0],
			&gpio_pull_name_table[ARRAY_SIZE(gpio_pull_name_table) - 1],
			gpio_pull);;
}

static unsigned int name_to_gpio_pd_conf(const char *name) {
	return name_to_value(&gpio_pd_conf_name_table[0],
			&gpio_pd_conf_name_table[ARRAY_SIZE(gpio_pd_conf_name_table) - 1],
			name);
}

static const char *gpio_pd_conf_to_name(unsigned int gpio_pd_conf) {
	return value_to_name(&gpio_pd_conf_name_table[0],
			&gpio_pd_conf_name_table[ARRAY_SIZE(gpio_pd_conf_name_table) - 1],
			gpio_pd_conf);
}

static unsigned int name_to_gpio_pd_pull(const char *name) {
	return name_to_value(&gpio_pd_pull_name_table[0],
			&gpio_pd_pull_name_table[ARRAY_SIZE(gpio_pd_pull_name_table) - 1],
			name);
}

static const char *gpio_pd_pull_to_name(unsigned int gpio_pd_pull) {
	return value_to_name(&gpio_pd_pull_name_table[0],
			&gpio_pd_pull_name_table[ARRAY_SIZE(gpio_pd_pull_name_table) - 1],
			gpio_pd_pull);
}

static int name_to_gpio(const char *name)
{
	const struct value_name_map *map = &gpio_name_table[ARRAY_SIZE(gpio_name_table) - 1];
	size_t len;
	unsigned long offset;

	do {
		len = strlen(map->name);
		if (strncasecmp(name, map->name, len) == 0) {
			offset = simple_strtoul(name + len + 1, NULL, 10);
			return map->value + offset;
		}
	} while (--map >= &gpio_name_table[0]);

	return -EINVAL;
}

static ssize_t gpio_to_name(unsigned int gpio, char **pname, size_t length)
{
	const struct value_name_map *map = &gpio_name_table[ARRAY_SIZE(gpio_name_table) - 1];

	do {
		if (map->value <= gpio) {
			pr_debug("%s, map->name=%s, gpio - map->value=%d\n", __func__, map->name, gpio - map->value);
			return snprintf(*pname, length, "%s(%u)", map->name, gpio - map->value);
		}
	} while (--map >= &gpio_name_table[0]);

	return -EINVAL;
}

static ssize_t pd_gpio_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	ssize_t size;
	unsigned int (*item)[3] = (unsigned int(*) [3])sleep_gpio_alive_table;
	char *p, *buf = vmalloc(PAGE_SIZE * 3);

	pr_debug("%s: count=%zu, ppos=%lld\n", __func__, count, *ppos);

	if (IS_ERR(buf)) {
		return PTR_ERR(buf);
	}

	p = buf;
	do {
		size = gpio_to_name((*item)[0], &p, 20);
		if (size > 0) {
			p += size;
		} else {
			pr_err("A bad data is in the table. (%08x, %08x, %08x)\n", (*item)[0], (*item)[1], (*item)[2]);
			continue;
		}
		size = snprintf(p, 80, "\t%s\t%s\n",
			gpio_conf_to_name((*item)[1]),
			gpio_pull_to_name((*item)[2]));
		if (size > 0) {
			p += size;
		} else {
			pr_err("A bad data is in the table. (%08x, %08x, %08x)\n", (*item)[0], (*item)[1], (*item)[2]);
			continue;
		}
	} while (++item <= (unsigned int (*)[3])&sleep_gpio_alive_table[ARRAY_SIZE(sleep_gpio_alive_table) - 1]);

	item = (unsigned int (*)[3])sleep_gpio_pd_table;

	do {
		size = gpio_to_name((*item)[0], &p, 20);
		if (size > 0) {
			p += size;
		} else {
			pr_err("A bad data is in the table. (%08x, %08x, %08x)\n", (*item)[0], (*item)[1], (*item)[2]);
			continue;
		}
		size = snprintf(p, 80, "\t%s\t%s\n",
			gpio_pd_conf_to_name((*item)[1]),
			gpio_pd_pull_to_name((*item)[2]));
		if (size > 0) {
			p += size;
		} else {
			pr_err("A bad data is in the table. (%08x, %08x, %08x)\n", (*item)[0], (*item)[1], (*item)[2]);
			continue;
		}
	} while (++item <=(unsigned int (*)[3]) &sleep_gpio_pd_table[ARRAY_SIZE(sleep_gpio_pd_table) - 1]);

	size = simple_read_from_buffer(data, count, ppos, buf, p - buf + 1);

	pr_debug("%s: size=%zd, *ppos=%lld\n", __func__, size, *ppos);
	vfree(buf);
	return size;
}

static ssize_t pd_gpio_write(struct file *file, const char __user *data, size_t count, loff_t *ppos)
{
	struct pd_gpio_debugfs_data *private_data = (struct pd_gpio_debugfs_data *)file->private_data;
	ssize_t size = simple_write_to_buffer(private_data->buffer, private_data->buffer_size, ppos, data, count);

	if (size > 0) {
		private_data->data_size += size;
	}
	pr_debug("%s: size=%zd\n", __func__, size);
	return size;
}

static int pd_gpio_open(struct inode *inode, struct file *file)
{
	struct pd_gpio_debugfs_data *data;

	if (file->f_flags | O_WRONLY) {
		file->private_data = NULL;

		data = (struct pd_gpio_debugfs_data *)kmalloc(sizeof(struct pd_gpio_debugfs_data), GFP_KERNEL);
		if (IS_ERR(data)) {
			return PTR_ERR(data);
		}

		data->buffer = (char *)vmalloc(4 * PAGE_SIZE);
		if (IS_ERR(data->buffer)) {
			kfree(data);
			return PTR_ERR(data->buffer);
		}

		data->buffer_size = 4 * PAGE_SIZE;
		data->data_size = 0;

		file->private_data = data;
	} else {
		file->private_data = inode->i_private;
	}

	return 0;
}

static int pd_gpio_release(struct inode *inode, struct file *file)
{
	struct pd_gpio_debugfs_data *data;

	if (file->f_flags | O_WRONLY) {
		char *p;
		char *gpio_name, *gpio_setting_name, *gpio_pull_name;
		unsigned int gpio;
		unsigned int (*item)[3];

		data = (struct pd_gpio_debugfs_data *)file->private_data;
		if (data == NULL) {
			return 0;
		}

		p = data->buffer;

		while (p - data->buffer < data->data_size) {
			p = skip_spaces(p);
			gpio_name = strsep(&p, " \t");
			if (p == NULL) {
				pr_err("Invalid data: %s\n", data->buffer);
				break;
			}
			p = skip_spaces(p);
			gpio_setting_name = strsep(&p, " \t");
			if (p == NULL) {
				pr_err("Invalid data: %s\n", data->buffer);
				break;
			}
			p = skip_spaces(p);
			gpio_pull_name = strsep(&p, " \n");
			if (p == NULL) {
				pr_err("Invalid data: %s\n", data->buffer);
				break;
			}

			pr_debug("%s: token1=%s, token2=%s, token3=%s\n", __func__, gpio_name, gpio_setting_name, gpio_pull_name);

			gpio = name_to_gpio(gpio_name);

			item = (unsigned int (*)[3])sleep_gpio_alive_table;
			do {
				if (gpio == (*item)[0]) {
					(*item)[1] = name_to_gpio_conf(gpio_setting_name);
					(*item)[2] = name_to_gpio_pull(gpio_pull_name);
					break;
				}
			} while (++item <= (unsigned int (*)[3])&sleep_gpio_alive_table[ARRAY_SIZE(sleep_gpio_alive_table) - 1]);
			if (item <= (unsigned int(*)[3])&sleep_gpio_alive_table[ARRAY_SIZE(sleep_gpio_alive_table) - 1]) {
				pr_info("Data is modified: %s, %s, %s\n", gpio_name, gpio_setting_name, gpio_pull_name);
				continue;
			}

			item = (unsigned int (*)[3])sleep_gpio_pd_table[0];
			do {
				if (gpio == (*item)[0]) {
					(*item)[1] = name_to_gpio_pd_conf(gpio_setting_name);
					(*item)[2] = name_to_gpio_pd_pull(gpio_pull_name);
					break;
				}
			} while (++item <= (unsigned int (*)[3])&sleep_gpio_pd_table[ARRAY_SIZE(sleep_gpio_pd_table) - 1]);
			if (item <= (unsigned int (*)[3])&sleep_gpio_pd_table[ARRAY_SIZE(sleep_gpio_pd_table) - 1]) {
				pr_info("Data is modified: %s, %s, %s\n", gpio_name, gpio_setting_name, gpio_pull_name);
				continue;
			}

			pr_err("Invalid data: %s, %s, %s\n", gpio_name, gpio_setting_name, gpio_pull_name);
		}

		vfree(data->buffer);
		kfree(data);
	}
	return 0;
}

static const struct file_operations pd_gpio_operations = {
	.open		= pd_gpio_open,
	.read		= pd_gpio_read,
	.write		= pd_gpio_write,
	.llseek		= no_llseek,
	.release	= pd_gpio_release,
};

static __init int pd_gpio_debugfs_init(void)
{
	struct dentry *root;

	pr_debug("%s is called.\n", __func__);

	root = debugfs_create_file("pd_gpio",S_IFREG|S_IRUGO|S_IWUSR, NULL, NULL, &pd_gpio_operations);
	if (IS_ERR(root)) {
		pr_err("Debug fs for GPIO power down setting is failed. (%ld)\n", PTR_ERR(root));
		return PTR_ERR(root);
	}

	return 0;
}

late_initcall(pd_gpio_debugfs_init);
#endif

void __init exynos3_universal3250_gpio_init(void)
{
	exynos_config_sleep_gpio = config_sleep_gpio_tables;
}
