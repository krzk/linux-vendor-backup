/*
 * linux/arch/arm/mach-omap2/usb-ehci.c
 *
 * This file will contain the board specific details for the
 * Synopsys EHCI host controller on OMAP3430
 *
 * Copyright (C) 2007 Texas Instruments
 * Author: Vikram Pandita <vikram.pandita@ti.com>
 *
 * Generalization by:
 * Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <plat/mux.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <plat/usb.h>
#include "mux.h"

#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE)

static struct resource ehci_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
	},
	{
		.flags	= IORESOURCE_MEM,
	},
	{
		.flags	= IORESOURCE_MEM,
	},
	{         /* general IRQ */
		.flags   = IORESOURCE_IRQ,
	}
};

static u64 ehci_dmamask = ~(u32)0;
static struct platform_device ehci_device = {
	.name           = "ehci-omap",
	.id             = 0,
	.dev = {
		.dma_mask               = &ehci_dmamask,
		.coherent_dma_mask      = 0xffffffff,
		.platform_data          = NULL,
	},
	.num_resources  = ARRAY_SIZE(ehci_resources),
	.resource       = ehci_resources,
};

/* MUX settings for EHCI pins */
/*
 * setup_ehci_io_mux - initialize IO pad mux for USBHOST
 */
static void setup_ehci_io_mux(const enum ehci_hcd_omap_mode *port_mode)
{
	switch (port_mode[0]) {
	case EHCI_HCD_OMAP_MODE_PHY:
		omap_mux_init_signal("hsusb1_stp", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb1_clk", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb1_dir", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_nxt", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data0", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data1", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data2", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data3", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data4", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data5", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data6", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data7", OMAP_PIN_INPUT_PULLDOWN);
		break;
	case EHCI_HCD_OMAP_MODE_TLL:
		omap_mux_init_signal("hsusb1_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb1_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case EHCI_HCD_OMAP_MODE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case EHCI_HCD_OMAP_MODE_PHY:
		omap_mux_init_signal("hsusb2_stp", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb2_clk", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb2_dir", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_nxt", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case EHCI_HCD_OMAP_MODE_TLL:
		omap_mux_init_signal("hsusb2_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb2_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case EHCI_HCD_OMAP_MODE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[2]) {
	case EHCI_HCD_OMAP_MODE_PHY:
		printk(KERN_WARNING "Port3 can't be used in PHY mode\n");
		break;
	case EHCI_HCD_OMAP_MODE_TLL:
		omap_mux_init_signal("hsusb3_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb3_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case EHCI_HCD_OMAP_MODE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}


static void setup_4430ehci_io_mux(const enum ehci_hcd_omap_mode *port_mode)
{
	/*
	 * FIXME: This funtion should use mux framework functions;
	 * For now, we are hardcoding this.
	 */

	switch (port_mode[0]) {
	case EHCI_HCD_OMAP_MODE_PHY:

		/* HUSB1_PHY CLK , INPUT ENABLED, PULLDOWN  */
		omap_writew(0x010C, 0x4A1000C2);

		/* HUSB1 STP */
		omap_writew(0x0004, 0x4A1000C4);

		/* HUSB1_DIR */
		omap_writew(0x010C, 0x4A1000C6);

		/* HUSB1_NXT */
		omap_writew(0x010C, 0x4A1000C8);

		/* HUSB1_DATA0 */
		omap_writew(0x010C, 0x4A1000CA);

		/* HUSB1_DATA1 */
		omap_writew(0x010C, 0x4A1000CC);

		/* HUSB1_DATA2 */
		omap_writew(0x010C, 0x4A1000CE);

		/* HUSB1_DATA3 */
		omap_writew(0x010C, 0x4A1000D0);

		/* HUSB1_DATA4 */
		omap_writew(0x010C, 0x4A1000D2);

		/* HUSB1_DATA5 */
		omap_writew(0x010C, 0x4A1000D4);

		/* HUSB1_DATA6 */
		omap_writew(0x010C, 0x4A1000D6);

		/* HUSB1_DATA7 */
		omap_writew(0x010C, 0x4A1000D8);

		break;


	case EHCI_HCD_OMAP_MODE_TLL:
		/* TODO */


		break;
	case EHCI_HCD_OMAP_MODE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case EHCI_HCD_OMAP_MODE_PHY:
		/* HUSB2_PHY CLK , INPUT PULLDOWN ENABLED  */
		omap_writew(0x010C, 0x4A100160);

		/* HUSB2 STP */
		omap_writew(0x0002, 0x4A100162);

		/* HUSB2_DIR */
		omap_writew(0x010A, 0x4A100164);

		/* HUSB2_NXT */
		omap_writew(0x010A, 0x4A100166);

		/* HUSB2_DATA0 */
		omap_writew(0x010A, 0x4A100168);

		/* HUSB2_DATA1 */
		omap_writew(0x010A, 0x4A10016A);

		/* HUSB2_DATA2 */
		omap_writew(0x010A, 0x4A10016C);

		/* HUSB2_DATA3 */
		omap_writew(0x010A, 0x4A10016E);

		/* HUSB2_DATA4 */
		omap_writew(0x010A, 0x4A100170);

		/* HUSB2_DATA5 */
		omap_writew(0x010A, 0x4A100172);

		/* HUSB2_DATA6 */
		omap_writew(0x010A, 0x4A100174);

		/* HUSB2_DATA7 */
		omap_writew(0x010A, 0x4A100176);

		break;

	case EHCI_HCD_OMAP_MODE_TLL:
		/* TODO */

		break;
	case EHCI_HCD_OMAP_MODE_UNKNOWN:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}
void __init usb_ehci_init(const struct ehci_hcd_omap_platform_data *pdata)
{
	platform_device_add_data(&ehci_device, pdata, sizeof(*pdata));

	/* Setup Pin IO MUX for EHCI */
	if (cpu_is_omap34xx()) {
		ehci_resources[0].start	= OMAP34XX_EHCI_BASE;
		ehci_resources[0].end	= OMAP34XX_EHCI_BASE + SZ_1K - 1;
		ehci_resources[1].start	= OMAP34XX_UHH_CONFIG_BASE;
		ehci_resources[1].end	= OMAP34XX_UHH_CONFIG_BASE + SZ_1K - 1;
		ehci_resources[2].start	= OMAP34XX_USBTLL_BASE;
		ehci_resources[2].end	= OMAP34XX_USBTLL_BASE + SZ_4K - 1;
		ehci_resources[3].start = INT_34XX_EHCI_IRQ;
		setup_ehci_io_mux(pdata->port_mode);
	} else if (cpu_is_omap44xx()) {
		ehci_resources[0].start	= OMAP44XX_HSUSB_EHCI_BASE;
		ehci_resources[0].end	= OMAP44XX_HSUSB_EHCI_BASE + SZ_1K - 1;
		ehci_resources[1].start	= OMAP44XX_UHH_CONFIG_BASE;
		ehci_resources[1].end	= OMAP44XX_UHH_CONFIG_BASE + SZ_2K - 1;
		ehci_resources[2].start	= OMAP44XX_USBTLL_BASE;
		ehci_resources[2].end	= OMAP44XX_USBTLL_BASE + SZ_4K - 1;
#define OMAP44XX_IRQ_EHCI INT_44XX_USB_IRQ_ISO
		ehci_resources[3].start = OMAP44XX_IRQ_EHCI;
		setup_4430ehci_io_mux(pdata->port_mode);
	}

	if (platform_device_register(&ehci_device) < 0) {
		printk(KERN_ERR "Unable to register HS-USB (EHCI) device\n");
		return;
	}
}

#else

void __init usb_ehci_init(const struct ehci_hcd_omap_platform_data *pdata)

{
}

#endif /* CONFIG_USB_EHCI_HCD */

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)

static struct resource ohci_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
	},
	{
		.flags	= IORESOURCE_MEM,
	},
	{
		.flags	= IORESOURCE_MEM,
	},
	{	/* general IRQ */
		.flags	= IORESOURCE_IRQ,
	}
};

static u64 ohci_dmamask = DMA_BIT_MASK(32);;

static struct platform_device ohci_device = {
	.name		= "ohci-omap3",
	.id		= 0,
	.dev = {
		.dma_mask		= &ohci_dmamask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ohci_resources),
	.resource	= ohci_resources,
};

static void setup_ohci_io_mux(const enum ohci_omap3_port_mode *port_mode)
{
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm1_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm1_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm1_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm1_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm1_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm1_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_OHCI_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm2_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm2_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm2_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm2_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm2_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm2_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_OHCI_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
	switch (port_mode[2]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm3_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm3_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm3_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm3_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm3_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm3_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_OHCI_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
}

static void setup_4430ohci_io_mux(const enum ohci_omap3_port_mode *port_mode)
{
	/* FIXME: This funtion should use Mux frame work functions;
	 * for now, we are hardcodeing it
	 * This function will be later replaced by MUX framework API.
	 */
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb1_mm_rxdp */
		omap_writew(0x001D, 0x4A1000C4);

		/* usbb1_mm_rxdm */
		omap_writew(0x001D, 0x4A1000C8);

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb1_mm_rxrcv */
		omap_writew(0x001D, 0x4A1000CA);

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb1_mm_txen */
		omap_writew(0x001D, 0x4A1000D0);

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb1_mm_txdat */
		omap_writew(0x001D, 0x4A1000CE);

		/* usbb1_mm_txse0 */
		omap_writew(0x001D, 0x4A1000CC);
		break;

	case OMAP_OHCI_PORT_MODE_UNUSED:
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb2_mm_rxdp */
		omap_writew(0x010C, 0x4A1000F8);

		/* usbb2_mm_rxdm */
		omap_writew(0x010C, 0x4A1000F6);

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb2_mm_rxrcv */
		omap_writew(0x010C, 0x4A1000FA);

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb2_mm_txen */
		omap_writew(0x080C, 0x4A1000FC);

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb2_mm_txdat */
		omap_writew(0x000C, 0x4A100112);

		/* usbb2_mm_txse0 */
		omap_writew(0x000C, 0x4A100110);
		break;

	case OMAP_OHCI_PORT_MODE_UNUSED:
	default:
		break;
	}
}

void __init usb_ohci_init(const struct ohci_hcd_omap_platform_data *pdata)
{
	platform_device_add_data(&ohci_device, pdata, sizeof(*pdata));
	if (cpu_is_omap34xx()) {
		ohci_resources[0].start	= OMAP34XX_OHCI_BASE;
		ohci_resources[0].end	= OMAP34XX_OHCI_BASE + SZ_1K - 1;
		ohci_resources[1].start	= OMAP34XX_UHH_CONFIG_BASE;
		ohci_resources[1].end	= OMAP34XX_UHH_CONFIG_BASE + SZ_1K - 1;
		ohci_resources[2].start	= OMAP34XX_USBTLL_BASE;
		ohci_resources[2].end	= OMAP34XX_USBTLL_BASE + SZ_4K - 1;
		ohci_resources[3].start = INT_34XX_OHCI_IRQ;
		setup_ohci_io_mux(pdata->port_mode);
	} else if (cpu_is_omap44xx()) {
		ohci_resources[0].start	= OMAP44XX_HSUSB_OHCI_BASE;
		ohci_resources[0].end	= OMAP44XX_HSUSB_OHCI_BASE + SZ_1K - 1;
		ohci_resources[1].start	= OMAP44XX_UHH_CONFIG_BASE;
		ohci_resources[1].end	= OMAP44XX_UHH_CONFIG_BASE + SZ_1K - 1;
		ohci_resources[2].start	= OMAP44XX_USBTLL_BASE;
		ohci_resources[2].end	= OMAP44XX_USBTLL_BASE + SZ_4K - 1;
#define OMAP44XX_IRQ_OHCI INT_44XX_USB_IRQ_NISO
		ohci_resources[3].start = OMAP44XX_IRQ_OHCI;
		setup_4430ohci_io_mux(pdata->port_mode);
	}

	if (platform_device_register(&ohci_device) < 0) {
		pr_err("Unable to register FS-USB (OHCI) device\n");
		return;
	}
}

#else

void __init usb_ohci_init(const struct ohci_hcd_omap_platform_data *pdata)
{
}

#endif /* CONFIG_USB_OHCI_HCD */
