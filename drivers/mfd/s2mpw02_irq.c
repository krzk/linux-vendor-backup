
/*
 * s2mpw02-irq.c - Interrupt controller support for S2MPW02
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mpw02.h>
#include <linux/mfd/samsung/s2mpw02-regulator.h>

#include <sound/soc.h>
//#include <sound/cod3035x.h>

#if defined(CONFIG_CHARGER_S2MPW02)
#include <linux/power/s2mpw02_charger.h>
#endif
#if defined(CONFIG_FUELGAUGE_S2MPW02)
#include <linux/power/s2mpw02_fuelgauge.h>
#endif
#ifdef CONFIG_IRQ_HISTORY
#include <linux/power/irq_history.h>
#endif

static const u8 s2mpw02_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[PMIC_INT1] = 	S2MPW02_PMIC_REG_INT1M,
	[PMIC_INT2] =	S2MPW02_PMIC_REG_INT2M,
	[PMIC_INT3] = 	S2MPW02_PMIC_REG_INT3M,
#if defined(CONFIG_CHARGER_S2MPW02)
	[CHG_INT1] = 	S2MPW02_CHG_REG_INT1M,
	[CHG_INT2] = 	S2MPW02_CHG_REG_INT2M,
	[CHG_INT3] = 	S2MPW02_CHG_REG_INT3M,
	[CHG_INT4] = 	S2MPW02_CHG_REG_INT4M,
#endif
#if defined(CONFIG_FUELGAUGE_S2MPW02)
	[FG_INT] = S2MPW02_FG_REG_INT,
#endif
};

static struct i2c_client *get_i2c(struct s2mpw02_dev *s2mpw02,
				enum s2mpw02_irq_source src)
{
	switch (src) {
	case PMIC_INT1 ... PMIC_INT3:
		return s2mpw02->pmic;
#if defined(CONFIG_CHARGER_S2MPW02)
	case CHG_INT1 ... CHG_INT4:
		return s2mpw02->charger;
#endif
#if defined(CONFIG_FUELGAUGE_S2MPW02)
	case FG_INT:
		return s2mpw02->fuelgauge;
#endif
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpw02_irq_data {
	int mask;
	enum s2mpw02_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mpw02_irq_data s2mpw02_irqs[] = {
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_PWRONR_INT1,	PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_PWRONF_INT1,	PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_JIGONBF_INT1,	PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_JIGONBR_INT1,	PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_ACOKBF_INT1,	PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_ACOKBR_INT1,	PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_PWRON1S_INT1,	PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_MRB_INT1,		PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPW02_PMIC_IRQ_RTC60S_INT2,	PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_RTCA1_INT2,	PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_RTCA0_INT2,	PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_SMPL_INT2,		PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_RTC1S_INT2,	PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_WTSR_INT2,		PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_WRSTB_INT2,	PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPW02_PMIC_IRQ_120C_INT3,		PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_140C_INT3,		PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPW02_PMIC_IRQ_TSD_INT3,		PMIC_INT3, 1 << 2),

#if defined(CONFIG_CHARGER_S2MPW02)
	DECLARE_IRQ(S2MPW02_CHG_IRQ_RECHG_INT1,		CHG_INT1, 1 << 0),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CHGDONE_INT1,	CHG_INT1, 1 << 1),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_TOPOFF_INT1,	CHG_INT1, 1 << 2),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_PRECHG_INT1,	CHG_INT1, 1 << 3),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CHGSTS_INT1,	CHG_INT1, 1 << 4),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CIN2BAT_INT1,	CHG_INT1, 1 << 5),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CHGVINOVP_INT1,	CHG_INT1, 1 << 6),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CHGVIN_INT1,	CHG_INT1, 1 << 7),

	DECLARE_IRQ(S2MPW02_CHG_IRQ_BAT_LVL_ON_INT2,	CHG_INT2, 1 << 1),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_TMROUT_INT2,		CHG_INT2, 1 << 2),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_CHGR_TSD_INT2,		CHG_INT2, 1 << 3),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_ADPATH_INT2,		CHG_INT2, 1 << 4),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_FCHG_INT2,			CHG_INT2, 1 << 5),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_A2D_CHGINOK_INT2,	CHG_INT2, 1 << 6),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_BATDET_INT2,		CHG_INT2, 1 << 7),

	DECLARE_IRQ(S2MPW02_CHG_IRQ_WDT_INT3,		CHG_INT3, 1 << 7),

	DECLARE_IRQ(S2MPW02_CHG_IRQ_JIGON_INT4,			CHG_INT4, 1 << 0),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_RID_ATTACH_INT4,	CHG_INT4, 1 << 1),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_FACT_LEAK_INT4,		CHG_INT4, 1 << 2),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_UART_BOOT_ON_INT4,	CHG_INT4, 1 << 3),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_UART_BOOT_OFF_INT4,	CHG_INT4, 1 << 4),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_USB_BOOT_ON_INT4,	CHG_INT4, 1 << 5),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_USB_BOOT_OFF_INT4,	CHG_INT4, 1 << 6),
	DECLARE_IRQ(S2MPW02_CHG_IRQ_UART_CABLE_INT4,	CHG_INT4, 1 << 7),
#endif

#if defined(CONFIG_FUELGAUGE_S2MPW02)
	DECLARE_IRQ(S2MPW02_FG_IRQ_VBAT_L_INT,		FG_INT, 1 << 0),
	DECLARE_IRQ(S2MPW02_FG_IRQ_SOC_L_INT,		FG_INT, 1 << 1),
	DECLARE_IRQ(S2MPW02_FG_IRQ_IDLE_ST_INT,		FG_INT, 1 << 2),
	DECLARE_IRQ(S2MPW02_FG_IRQ_INIT_ST_INT,		FG_INT, 1 << 3),
#endif
};

static void s2mpw02_irq_lock(struct irq_data *data)
{
	struct s2mpw02_dev *s2mpw02 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpw02->irqlock);
}

static void s2mpw02_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpw02_dev *s2mpw02 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPW02_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpw02_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpw02, i);

		if (mask_reg == S2MPW02_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mpw02->irq_masks_cache[i] = s2mpw02->irq_masks_cur[i];

		s2mpw02_write_reg(i2c, s2mpw02_mask_reg[i],
				s2mpw02->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpw02->irqlock);
}

static const inline struct s2mpw02_irq_data *
irq_to_s2mpw02_irq(struct s2mpw02_dev *s2mpw02, int irq)
{
	return &s2mpw02_irqs[irq - s2mpw02->irq_base];
}

static void s2mpw02_irq_mask(struct irq_data *data)
{
	struct s2mpw02_dev *s2mpw02 = irq_get_chip_data(data->irq);
	const struct s2mpw02_irq_data *irq_data =
	    irq_to_s2mpw02_irq(s2mpw02, data->irq);

	if (irq_data->group >= S2MPW02_IRQ_GROUP_NR)
		return;

	s2mpw02->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpw02_irq_unmask(struct irq_data *data)
{
	struct s2mpw02_dev *s2mpw02 = irq_get_chip_data(data->irq);
	const struct s2mpw02_irq_data *irq_data =
	    irq_to_s2mpw02_irq(s2mpw02, data->irq);

	if (irq_data->group >= S2MPW02_IRQ_GROUP_NR)
		return;

	s2mpw02->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpw02_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpw02_irq_lock,
	.irq_bus_sync_unlock	= s2mpw02_irq_sync_unlock,
	.irq_mask		= s2mpw02_irq_mask,
	.irq_unmask		= s2mpw02_irq_unmask,
};

int codec_notifier_flag = 0;
/*
void set_codec_notifier_flag()
{
	codec_notifier_flag = 1;
}
*/

static irqreturn_t s2mpw02_irq_thread(int irq, void *data)
{
	struct s2mpw02_dev *s2mpw02 = data;
	u8 irq_reg[S2MPW02_IRQ_GROUP_NR] = {0};
	u8 irq_src;
//	u8 irq1_codec, irq2_codec, irq3_codec, irq4_codec, status1;
//	u8 param1, param2, param3, param4, param5;
	int i, ret;

	ret = s2mpw02_read_reg(s2mpw02->i2c,
					S2MPW02_PMIC_REG_INTSRC, &irq_src);
	pr_info("%s: interrupt source(0x%02x) irq gpio pre-state(0x%02x)\n",
		__func__, irq_src, gpio_get_value(s2mpw02->irq_gpio));
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	if (irq_src & S2MPW02_IRQSRC_PMIC) {
		/* PMIC_INT */
		ret = s2mpw02_bulk_read(s2mpw02->pmic, S2MPW02_PMIC_REG_INT1,
				S2MPW02_NUM_IRQ_PMIC_REGS, &irq_reg[PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s: pmic interrupt(0x%02x, 0x%02x, 0x%02x)\n",
			 __func__, irq_reg[PMIC_INT1], irq_reg[PMIC_INT2], irq_reg[PMIC_INT3]);
	}

#if defined(CONFIG_CHARGER_S2MPW02)
	if (irq_src & S2MPW02_IRQSRC_CHG) {
		/* CHG_INT */
		ret = s2mpw02_bulk_read(s2mpw02->charger, S2MPW02_CHG_REG_INT1,
				S2MPW02_NUM_IRQ_CHG_REGS, &irq_reg[CHG_INT1]);
		if (ret) {
			pr_err("%s() Failed to read charger interrupt: %d\n",
				__func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s() CHARGER intrrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
				__func__, irq_reg[CHG_INT1], irq_reg[CHG_INT2],
				irq_reg[CHG_INT3], irq_reg[CHG_INT4]);
	}

#endif
#if defined(CONFIG_FUELGAUGE_S2MPW02)
	if (irq_src & S2MPW02_IRQSRC_FG) {
		/* FG_INT */
		ret = s2mpw02_read_reg(s2mpw02->fuelgauge, S2MPW02_FG_REG_INT, &irq_reg[FG_INT]);
		if (ret) {
			pr_err("%s() Failed to read fuelgauge interrupt: %d\n",
				__func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s() FUELGAUGE intrrupt(0x%02x)\n", __func__, irq_reg[FG_INT]);
	}
#endif

/*
	if(irq_src & S2MPW02_IRQSRC_CODEC) {
		if(s2mpw02->codec){
			pr_err("%s codec interrupt occur\n", __func__);
			ret = s2mpw02_read_reg(s2mpw02->codec, 0x1, &irq1_codec);
			ret = s2mpw02_read_reg(s2mpw02->codec, 0x2, &irq2_codec);
			ret = s2mpw02_read_reg(s2mpw02->codec, 0x3, &irq3_codec);
			ret = s2mpw02_read_reg(s2mpw02->codec, 0x4, &irq4_codec);
			ret = s2mpw02_read_reg(s2mpw02->codec, 0x9, &status1);
			ret = s2mpw02_read_reg(s2mpw02->close, 0x7B, &param1);
			ret = s2mpw02_read_reg(s2mpw02->close, 0x7C, &param2);
			ret = s2mpw02_read_reg(s2mpw02->close, 0x7E, &param3);
			ret = s2mpw02_read_reg(s2mpw02->close, 0x7F, &param4);
			ret = s2mpw02_read_reg(s2mpw02->close, 0x80, &param5);

			if(codec_notifier_flag)
				cod3035x_call_notifier(irq1_codec, irq2_codec, irq3_codec,
						irq4_codec, status1, param1, param2, param3, param4, param5);
		}
	}
*/

	/* Apply masking */
	for (i = 0; i < S2MPW02_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpw02->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPW02_IRQ_NR; i++) {
		if (irq_reg[s2mpw02_irqs[i].group] & s2mpw02_irqs[i].mask) {
			handle_nested_irq(s2mpw02->irq_base + i);
#ifdef CONFIG_IRQ_HISTORY
			if (get_irq_history_flag() == IRQ_HISTORY_S2MPW02_IRQ) {
				add_irq_history(s2mpw02->irq_base + i, NULL);
				clear_irq_history_flag();
			}
#endif
		}
	}

	return IRQ_HANDLED;
}

int s2mpw02_irq_init(struct s2mpw02_dev *s2mpw02)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!s2mpw02->irq_base) {
		dev_err(s2mpw02->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mpw02->irqlock);

	s2mpw02->irq = gpio_to_irq(s2mpw02->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			s2mpw02->irq, s2mpw02->irq_gpio);

	ret = gpio_request(s2mpw02->irq_gpio, "pmic_irq");
	if (ret) {
		dev_err(s2mpw02->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mpw02->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mpw02->irq_gpio);
	gpio_free(s2mpw02->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPW02_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpw02->irq_masks_cur[i] = 0xff;
		s2mpw02->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpw02, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpw02_mask_reg[i] == S2MPW02_REG_INVALID)
			continue;

		s2mpw02_write_reg(i2c, s2mpw02_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPW02_IRQ_NR; i++) {
		cur_irq = i + s2mpw02->irq_base;
		irq_set_chip_data(cur_irq, s2mpw02);
		irq_set_chip_and_handler(cur_irq, &s2mpw02_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpw02_write_reg(s2mpw02->i2c, S2MPW02_PMIC_REG_INTSRC_MASK, 0xff);

	/* Unmask s2mpw02 interrupt */
	ret = s2mpw02_read_reg(s2mpw02->i2c, S2MPW02_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	//if(s2mpw02->codec)
	i2c_data &= ~(S2MPW02_IRQSRC_CODEC);

	i2c_data &= ~(S2MPW02_IRQSRC_PMIC);	/* Unmask pmic interrupt */
	s2mpw02_write_reg(s2mpw02->i2c, S2MPW02_PMIC_REG_INTSRC_MASK,
			   i2c_data);

#if defined(CONFIG_CHARGER_S2MPW02)
	i2c_data &= ~(S2MPW02_IRQSRC_CHG);	/* Unmask charger interrupt */
#endif

	s2mpw02_write_reg(s2mpw02->i2c, S2MPW02_PMIC_REG_INTSRC_MASK, i2c_data);

	pr_info("%s:%s s2mpw02_PMIC_REG_INTSRC_MASK=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	ret = request_threaded_irq(s2mpw02->irq, NULL, s2mpw02_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "s2mpw02-irq", s2mpw02);

	if (ret) {
		dev_err(s2mpw02->dev, "Failed to request IRQ %d: %d\n",
			s2mpw02->irq, ret);
		return ret;
	}

	return 0;
}

void s2mpw02_irq_exit(struct s2mpw02_dev *s2mpw02)
{
	if (s2mpw02->irq)
		free_irq(s2mpw02->irq, s2mpw02);
}
