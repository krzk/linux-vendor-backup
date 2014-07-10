/*
 * sec-irq.c
 *
 * Copyright (c) 2011-2014 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/irqdomain.h>

#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/irq.h>
#include <linux/mfd/samsung/s2mps11.h>
#include <linux/mfd/samsung/s2mps14.h>
#include <linux/mfd/samsung/s5m8763.h>
#include <linux/mfd/samsung/s5m8767.h>

static const struct regmap_irq s2mps11_irqs[] = {
	[S2MPS11_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONF_MASK,
	},
	[S2MPS11_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONR_MASK,
	},
	[S2MPS11_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBF_MASK,
	},
	[S2MPS11_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBR_MASK,
	},
	[S2MPS11_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBF_MASK,
	},
	[S2MPS11_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBR_MASK,
	},
	[S2MPS11_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRON1S_MASK,
	},
	[S2MPS11_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_MRB_MASK,
	},
	[S2MPS11_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC60S_MASK,
	},
	[S2MPS11_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA0_MASK,
	},
	[S2MPS11_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA1_MASK,
	},
	[S2MPS11_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_SMPL_MASK,
	},
	[S2MPS11_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC1S_MASK,
	},
	[S2MPS11_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_WTSR_MASK,
	},
	[S2MPS11_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT120C_MASK,
	},
	[S2MPS11_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT140C_MASK,
	},
};

static const struct regmap_irq s2mps14_irqs[] = {
	[S2MPS14_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONF_MASK,
	},
	[S2MPS14_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRONR_MASK,
	},
	[S2MPS14_IRQ_JIGONBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBF_MASK,
	},
	[S2MPS14_IRQ_JIGONBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_JIGONBR_MASK,
	},
	[S2MPS14_IRQ_ACOKBF] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBF_MASK,
	},
	[S2MPS14_IRQ_ACOKBR] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_ACOKBR_MASK,
	},
	[S2MPS14_IRQ_PWRON1S] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_PWRON1S_MASK,
	},
	[S2MPS14_IRQ_MRB] = {
		.reg_offset = 0,
		.mask = S2MPS11_IRQ_MRB_MASK,
	},
	[S2MPS14_IRQ_RTC60S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC60S_MASK,
	},
	[S2MPS14_IRQ_RTCA1] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA1_MASK,
	},
	[S2MPS14_IRQ_RTCA0] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTCA0_MASK,
	},
	[S2MPS14_IRQ_SMPL] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_SMPL_MASK,
	},
	[S2MPS14_IRQ_RTC1S] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_RTC1S_MASK,
	},
	[S2MPS14_IRQ_WTSR] = {
		.reg_offset = 1,
		.mask = S2MPS11_IRQ_WTSR_MASK,
	},
	[S2MPS14_IRQ_INT120C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT120C_MASK,
	},
	[S2MPS14_IRQ_INT140C] = {
		.reg_offset = 2,
		.mask = S2MPS11_IRQ_INT140C_MASK,
	},
	[S2MPS14_IRQ_TSD] = {
		.reg_offset = 2,
		.mask = S2MPS14_IRQ_TSD_MASK,
	},
};

static const struct regmap_irq s5m8767_irqs[] = {
	[S5M8767_IRQ_PWRR] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWRR_MASK,
	},
	[S5M8767_IRQ_PWRF] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWRF_MASK,
	},
	[S5M8767_IRQ_PWR1S] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_PWR1S_MASK,
	},
	[S5M8767_IRQ_JIGR] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_JIGR_MASK,
	},
	[S5M8767_IRQ_JIGF] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_JIGF_MASK,
	},
	[S5M8767_IRQ_LOWBAT2] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_LOWBAT2_MASK,
	},
	[S5M8767_IRQ_LOWBAT1] = {
		.reg_offset = 0,
		.mask = S5M8767_IRQ_LOWBAT1_MASK,
	},
	[S5M8767_IRQ_MRB] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_MRB_MASK,
	},
	[S5M8767_IRQ_DVSOK2] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK2_MASK,
	},
	[S5M8767_IRQ_DVSOK3] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK3_MASK,
	},
	[S5M8767_IRQ_DVSOK4] = {
		.reg_offset = 1,
		.mask = S5M8767_IRQ_DVSOK4_MASK,
	},
	[S5M8767_IRQ_RTC60S] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTC60S_MASK,
	},
	[S5M8767_IRQ_RTCA1] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTCA1_MASK,
	},
	[S5M8767_IRQ_RTCA2] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTCA2_MASK,
	},
	[S5M8767_IRQ_SMPL] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_SMPL_MASK,
	},
	[S5M8767_IRQ_RTC1S] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_RTC1S_MASK,
	},
	[S5M8767_IRQ_WTSR] = {
		.reg_offset = 2,
		.mask = S5M8767_IRQ_WTSR_MASK,
	},
};

static const struct regmap_irq s5m8763_irqs[] = {
	[S5M8763_IRQ_DCINF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_DCINF_MASK,
	},
	[S5M8763_IRQ_DCINR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_DCINR_MASK,
	},
	[S5M8763_IRQ_JIGF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_JIGF_MASK,
	},
	[S5M8763_IRQ_JIGR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_JIGR_MASK,
	},
	[S5M8763_IRQ_PWRONF] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_PWRONF_MASK,
	},
	[S5M8763_IRQ_PWRONR] = {
		.reg_offset = 0,
		.mask = S5M8763_IRQ_PWRONR_MASK,
	},
	[S5M8763_IRQ_WTSREVNT] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_WTSREVNT_MASK,
	},
	[S5M8763_IRQ_SMPLEVNT] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_SMPLEVNT_MASK,
	},
	[S5M8763_IRQ_ALARM1] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_ALARM1_MASK,
	},
	[S5M8763_IRQ_ALARM0] = {
		.reg_offset = 1,
		.mask = S5M8763_IRQ_ALARM0_MASK,
	},
	[S5M8763_IRQ_ONKEY1S] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_ONKEY1S_MASK,
	},
	[S5M8763_IRQ_TOPOFFR] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_TOPOFFR_MASK,
	},
	[S5M8763_IRQ_DCINOVPR] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_DCINOVPR_MASK,
	},
	[S5M8763_IRQ_CHGRSTF] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_CHGRSTF_MASK,
	},
	[S5M8763_IRQ_DONER] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_DONER_MASK,
	},
	[S5M8763_IRQ_CHGFAULT] = {
		.reg_offset = 2,
		.mask = S5M8763_IRQ_CHGFAULT_MASK,
	},
	[S5M8763_IRQ_LOBAT1] = {
		.reg_offset = 3,
		.mask = S5M8763_IRQ_LOBAT1_MASK,
	},
	[S5M8763_IRQ_LOBAT2] = {
		.reg_offset = 3,
		.mask = S5M8763_IRQ_LOBAT2_MASK,
	},
};

static const struct regmap_irq_chip s2mps11_irq_chip = {
	.name = "s2mps11",
	.irqs = s2mps11_irqs,
	.num_irqs = ARRAY_SIZE(s2mps11_irqs),
	.num_regs = 3,
	.status_base = S2MPS11_REG_INT1,
	.mask_base = S2MPS11_REG_INT1M,
	.ack_base = S2MPS11_REG_INT1,
};

static const struct regmap_irq_chip s2mps14_irq_chip = {
	.name = "s2mps14",
	.irqs = s2mps14_irqs,
	.num_irqs = ARRAY_SIZE(s2mps14_irqs),
	.num_regs = 3,
	.status_base = S2MPS14_REG_INT1,
	.mask_base = S2MPS14_REG_INT1M,
	.ack_base = S2MPS14_REG_INT1,
};

static const struct regmap_irq_chip s5m8767_irq_chip = {
	.name = "s5m8767",
	.irqs = s5m8767_irqs,
	.num_irqs = ARRAY_SIZE(s5m8767_irqs),
	.num_regs = 3,
	.status_base = S5M8767_REG_INT1,
	.mask_base = S5M8767_REG_INT1M,
	.ack_base = S5M8767_REG_INT1,
};

static const struct regmap_irq_chip s5m8763_irq_chip = {
	.name = "s5m8763",
	.irqs = s5m8763_irqs,
	.num_irqs = ARRAY_SIZE(s5m8763_irqs),
	.num_regs = 4,
	.status_base = S5M8763_REG_IRQ1,
	.mask_base = S5M8763_REG_IRQM1,
	.ack_base = S5M8763_REG_IRQ1,
};

static irqreturn_t sec_pmic_irq_thread(int irq, void *data)
{
	struct sec_pmic_dev *sec_pmic = data;

	handle_nested_irq(irq_create_mapping(sec_pmic->domain, 0));

	return IRQ_HANDLED;
}

static void sec_pmic_irq_enable(struct irq_data *data)
{
}

static void sec_pmic_irq_disable(struct irq_data *data)
{
}

static int sec_pmic_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct sec_pmic_dev *sec_pmic = irq_data_get_irq_chip_data(data);
	int ret;

	/*
	 * PMIC has only one hardware interrupt, so do not even bother looking
	 * at data->irq/hwirq.
	 */
	ret = irq_set_irq_wake(sec_pmic->irq, on);

	return ret;
}

static struct irq_chip sec_pmic_irq_chip = {
	.name			= "sec_pmic_irq",
	.irq_disable		= sec_pmic_irq_disable,
	.irq_enable		= sec_pmic_irq_enable,
	.irq_set_wake		= sec_pmic_irq_set_wake,
};

static int sec_pmic_irq_domain_map(struct irq_domain *d, unsigned int irq,
				irq_hw_number_t hw)
{
	struct sec_pmic_dev *sec_pmic = d->host_data;

	irq_set_chip_data(irq, sec_pmic);
	irq_set_chip_and_handler(irq, &sec_pmic_irq_chip, handle_edge_irq);
	irq_set_nested_thread(irq, 1);
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	irq_set_noprobe(irq);
#endif
	return 0;
}

static struct irq_domain_ops sec_pmic_irq_domain_ops = {
	.map = sec_pmic_irq_domain_map,
};

int sec_irq_init(struct sec_pmic_dev *sec_pmic)
{
	struct sec_platform_data *pdata = sec_pmic->pdata;
	const struct regmap_irq_chip *irq_chip;
	unsigned int irq_nr;
	unsigned int irqflags;
	char *irq_name;
	int ret = 0;
	int type = sec_pmic->device_type;

	if (!sec_pmic->irq) {
		dev_warn(sec_pmic->dev,
			 "No interrupt specified, no interrupts\n");
		sec_pmic->irq_base = 0;
		return 0;
	}

	switch (type) {
	case S5M8763X:
		irq_chip = &s5m8763_irq_chip;
		irq_nr = S5M8763_IRQ_NR;
		irq_name = "s5m8763-irq";
		break;
	case S5M8767X:
		irq_chip = &s5m8767_irq_chip,
		irq_nr = S5M8767_IRQ_NR;
		irq_name = "s5m8767-irq";
		break;
	case S2MPS11X:
		irq_chip = &s2mps11_irq_chip,
		irq_nr = S2MPS11_IRQ_NR;
		irq_name = "s2mps11-irq";
		break;
	case S2MPS14X:
		irq_chip = &s2mps14_irq_chip,
		irq_nr = S2MPS14_IRQ_NR;
		irq_name = "s2mps14-irq";
		break;
	default:
		dev_err(sec_pmic->dev, "Unknown device type %d\n",
			sec_pmic->device_type);
		return -EINVAL;
	}

	/* default irq flags */
	irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	if (pdata->irqflags)
		irqflags = pdata->irqflags;
	else
		pdata->irqflags = irqflags;

	if (irqflags & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		sec_pmic->domain = irq_domain_add_linear(NULL, irq_nr,
					&sec_pmic_irq_domain_ops, sec_pmic);
		if (!sec_pmic->domain) {
			dev_err(sec_pmic->dev, "Failed to create irq domain\n");
			return -ENODEV;
		}

		ret = regmap_add_irq_chip(sec_pmic->regmap_pmic,
				  irq_create_mapping(sec_pmic->domain, 0),
				  irqflags, sec_pmic->irq_base, irq_chip,
				  &sec_pmic->irq_data);
		if (ret != 0) {
			dev_err(sec_pmic->dev,
				"Failed to register IRQ chip: %d\n", ret);
			return ret;
		}

		ret = request_threaded_irq(sec_pmic->irq,
				   NULL, sec_pmic_irq_thread,
				   irqflags, irq_name, sec_pmic);
		if (ret != 0) {
			dev_err(sec_pmic->dev,
				"Failed to request interrupt: %d\n", ret);
			goto err;
		}
	} else {
		ret = regmap_add_irq_chip(sec_pmic->regmap_pmic,
				  sec_pmic->irq,
				  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				  sec_pmic->irq_base, irq_chip,
				  &sec_pmic->irq_data);
		if (ret != 0) {
			dev_err(sec_pmic->dev,
				"Failed to register IRQ chip: %d\n", ret);
			return ret;
		}
	}

	return 0;
err:
	regmap_del_irq_chip(irq_create_mapping(sec_pmic->domain, 0),
			    sec_pmic->irq_data);

	return ret;
}

void sec_irq_exit(struct sec_pmic_dev *sec_pmic)
{
	/* regmap_del_irq_chip(sec_pmic->irq, sec_pmic->irq_data); */
}
