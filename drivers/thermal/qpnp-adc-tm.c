/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <uapi/linux/thermal.h>

/* QPNP VADC TM register definition */
#define QPNP_REVISION3					0x2
#define QPNP_PERPH_SUBTYPE				0x5
#define QPNP_PERPH_TYPE2				0x2
#define QPNP_REVISION_EIGHT_CHANNEL_SUPPORT		2
#define QPNP_STATUS1					0x8
#define QPNP_STATUS1_OP_MODE				4
#define QPNP_STATUS1_MEAS_INTERVAL_EN_STS		BIT(2)
#define QPNP_STATUS1_REQ_STS				BIT(1)
#define QPNP_STATUS1_EOC				BIT(0)
#define QPNP_STATUS2					0x9
#define QPNP_STATUS2_CONV_SEQ_STATE			6
#define QPNP_STATUS2_FIFO_NOT_EMPTY_FLAG		BIT(1)
#define QPNP_STATUS2_CONV_SEQ_TIMEOUT_STS		BIT(0)
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_MODE_CTL					0x40
#define QPNP_OP_MODE_SHIFT				3
#define QPNP_VREF_XO_THM_FORCE				BIT(2)
#define QPNP_AMUX_TRIM_EN				BIT(1)
#define QPNP_ADC_TRIM_EN				BIT(0)
#define QPNP_EN_CTL1					0x46
#define QPNP_ADC_TM_EN					BIT(7)
#define QPNP_ADC_CH_SEL_CTL				0x48
#define QPNP_ADC_DIG_PARAM				0x50
#define QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT		3
#define QPNP_HW_SETTLE_DELAY				0x51
#define QPNP_CONV_REQ					0x52
#define QPNP_CONV_REQ_SET				BIT(7)
#define QPNP_CONV_SEQ_CTL				0x54
#define QPNP_CONV_SEQ_HOLDOFF_SHIFT			4
#define QPNP_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL			0x57
#define QPNP_ADC_TM_MEAS_INTERVAL_TIME_SHIFT		0x3
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2			0x58
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_SHIFT		0x4
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL2_MASK		0xf0
#define QPNP_ADC_TM_MEAS_INTERVAL_CTL3_MASK		0xf

#define QPNP_ADC_MEAS_INTERVAL_OP_CTL			0x59
#define QPNP_ADC_MEAS_INTERVAL_OP			BIT(7)

#define QPNP_FAST_AVG_CTL				0x5a
#define QPNP_FAST_AVG_EN				0x5b

#define QPNP_M0_LOW_THR_LSB				0x5c
#define QPNP_M0_LOW_THR_MSB				0x5d
#define QPNP_M0_HIGH_THR_LSB				0x5e
#define QPNP_M0_HIGH_THR_MSB				0x5f
#define QPNP_M1_ADC_CH_SEL_CTL				0x68
#define QPNP_M1_LOW_THR_LSB				0x69
#define QPNP_M1_LOW_THR_MSB				0x6a
#define QPNP_M1_HIGH_THR_LSB				0x6b
#define QPNP_M1_HIGH_THR_MSB				0x6c
#define QPNP_M2_ADC_CH_SEL_CTL				0x70
#define QPNP_M2_LOW_THR_LSB				0x71
#define QPNP_M2_LOW_THR_MSB				0x72
#define QPNP_M2_HIGH_THR_LSB				0x73
#define QPNP_M2_HIGH_THR_MSB				0x74
#define QPNP_M3_ADC_CH_SEL_CTL				0x78
#define QPNP_M3_LOW_THR_LSB				0x79
#define QPNP_M3_LOW_THR_MSB				0x7a
#define QPNP_M3_HIGH_THR_LSB				0x7b
#define QPNP_M3_HIGH_THR_MSB				0x7c
#define QPNP_M4_ADC_CH_SEL_CTL				0x80
#define QPNP_M4_LOW_THR_LSB				0x81
#define QPNP_M4_LOW_THR_MSB				0x82
#define QPNP_M4_HIGH_THR_LSB				0x83
#define QPNP_M4_HIGH_THR_MSB				0x84
#define QPNP_M5_ADC_CH_SEL_CTL				0x88
#define QPNP_M5_LOW_THR_LSB				0x89
#define QPNP_M5_LOW_THR_MSB				0x8a
#define QPNP_M5_HIGH_THR_LSB				0x8b
#define QPNP_M5_HIGH_THR_MSB				0x8c
#define QPNP_M6_ADC_CH_SEL_CTL				0x90
#define QPNP_M6_LOW_THR_LSB				0x91
#define QPNP_M6_LOW_THR_MSB				0x92
#define QPNP_M6_HIGH_THR_LSB				0x93
#define QPNP_M6_HIGH_THR_MSB				0x94
#define QPNP_M7_ADC_CH_SEL_CTL				0x98
#define QPNP_M7_LOW_THR_LSB				0x99
#define QPNP_M7_LOW_THR_MSB				0x9a
#define QPNP_M7_HIGH_THR_LSB				0x9b
#define QPNP_M7_HIGH_THR_MSB				0x9c

#define QPNP_ADC_TM_MULTI_MEAS_EN			0x41
#define QPNP_ADC_TM_MULTI_MEAS_EN_M0			BIT(0)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M1			BIT(1)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M2			BIT(2)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M3			BIT(3)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M4			BIT(4)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M5			BIT(5)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M6			BIT(6)
#define QPNP_ADC_TM_MULTI_MEAS_EN_M7			BIT(7)
#define QPNP_ADC_TM_LOW_THR_INT_EN			0x42
#define QPNP_ADC_TM_LOW_THR_INT_EN_M0			BIT(0)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M1			BIT(1)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M2			BIT(2)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M3			BIT(3)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M4			BIT(4)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M5			BIT(5)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M6			BIT(6)
#define QPNP_ADC_TM_LOW_THR_INT_EN_M7			BIT(7)
#define QPNP_ADC_TM_HIGH_THR_INT_EN			0x43
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M0			BIT(0)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M1			BIT(1)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M2			BIT(2)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M3			BIT(3)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M4			BIT(4)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M5			BIT(5)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M6			BIT(6)
#define QPNP_ADC_TM_HIGH_THR_INT_EN_M7			BIT(7)

#define QPNP_ADC_TM_M0_MEAS_INTERVAL_CTL			0x59
#define QPNP_ADC_TM_M1_MEAS_INTERVAL_CTL			0x6d
#define QPNP_ADC_TM_M2_MEAS_INTERVAL_CTL			0x75
#define QPNP_ADC_TM_M3_MEAS_INTERVAL_CTL			0x7d
#define QPNP_ADC_TM_M4_MEAS_INTERVAL_CTL			0x85
#define QPNP_ADC_TM_M5_MEAS_INTERVAL_CTL			0x8d
#define QPNP_ADC_TM_M6_MEAS_INTERVAL_CTL			0x95
#define QPNP_ADC_TM_M7_MEAS_INTERVAL_CTL			0x9d
#define QPNP_ADC_TM_STATUS1				0x8
#define QPNP_ADC_TM_STATUS_LOW				0xa
#define QPNP_ADC_TM_STATUS_HIGH				0xb

#define QPNP_ADC_TM_M0_LOW_THR				0x5d5c
#define QPNP_ADC_TM_M0_HIGH_THR				0x5f5e
#define QPNP_ADC_TM_MEAS_INTERVAL			0x0

#define QPNP_ADC_TM_THR_LSB_MASK(val)			(val & 0xff)
#define QPNP_ADC_TM_THR_MSB_MASK(val)			((val & 0xff00) >> 8)

#define QPNP_MIN_TIME			2000
#define QPNP_MAX_TIME			2100

struct qpnp_adc_tm_sensor {
	struct thermal_zone_device	*tz_dev;
	enum thermal_device_mode	mode;
	uint32_t			sensor_num;
	enum qpnp_adc_meas_timer_select	timer_select;
	uint32_t			meas_interval;
	uint64_t			low_thr;
	uint64_t			high_thr;
	uint32_t			btm_channel_num;
	uint32_t			vadc_channel_num;
	struct work_struct		work;
	struct qpnp_adc_tm_btm_param	*btm_param;
	bool				thermal_node;
	bool				low_thr_notify;
	bool				high_thr_notify;
	uint32_t			scale_type;
};

struct qpnp_adc_tm_drv {
	struct qpnp_adc_drv		*adc;
	bool				adc_tm_initialized;
	int				max_channels_available;
	struct qpnp_adc_tm_sensor	sensor[0];
	bool				usb_id_ext_pull_up;
};

struct qpnp_adc_tm_drv	*qpnp_adc_tm;

struct qpnp_adc_tm_trip_reg_type {
	uint16_t low_thr_lsb_addr;
	uint16_t low_thr_msb_addr;
	uint16_t high_thr_lsb_addr;
	uint16_t high_thr_msb_addr;
	u8 multi_meas_en;
	u8 low_thr_int_chan_en;
	u8 high_thr_int_chan_en;
	u8 meas_interval_ctl;
};

static struct qpnp_adc_tm_trip_reg_type adc_tm_data[] = {
	[QPNP_ADC_TM_M0_ADC_CH_SEL_CTL] = {QPNP_M0_LOW_THR_LSB,
		QPNP_M0_LOW_THR_MSB, QPNP_M0_HIGH_THR_LSB,
		QPNP_M0_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M0,
		QPNP_ADC_TM_LOW_THR_INT_EN_M0, QPNP_ADC_TM_HIGH_THR_INT_EN_M0,
		QPNP_ADC_TM_M0_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M1_ADC_CH_SEL_CTL] = {QPNP_M1_LOW_THR_LSB,
		QPNP_M1_LOW_THR_MSB, QPNP_M1_HIGH_THR_LSB,
		QPNP_M1_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M1,
		QPNP_ADC_TM_LOW_THR_INT_EN_M1, QPNP_ADC_TM_HIGH_THR_INT_EN_M1,
		QPNP_ADC_TM_M1_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M2_ADC_CH_SEL_CTL] = {QPNP_M2_LOW_THR_LSB,
		QPNP_M2_LOW_THR_MSB, QPNP_M2_HIGH_THR_LSB,
		QPNP_M2_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M2,
		QPNP_ADC_TM_LOW_THR_INT_EN_M2, QPNP_ADC_TM_HIGH_THR_INT_EN_M2,
		QPNP_ADC_TM_M2_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M3_ADC_CH_SEL_CTL] = {QPNP_M3_LOW_THR_LSB,
		QPNP_M3_LOW_THR_MSB, QPNP_M3_HIGH_THR_LSB,
		QPNP_M3_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M3,
		QPNP_ADC_TM_LOW_THR_INT_EN_M3, QPNP_ADC_TM_HIGH_THR_INT_EN_M3,
		QPNP_ADC_TM_M3_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M4_ADC_CH_SEL_CTL] = {QPNP_M4_LOW_THR_LSB,
		QPNP_M4_LOW_THR_MSB, QPNP_M4_HIGH_THR_LSB,
		QPNP_M4_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M4,
		QPNP_ADC_TM_LOW_THR_INT_EN_M4, QPNP_ADC_TM_HIGH_THR_INT_EN_M4,
		QPNP_ADC_TM_M4_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M5_ADC_CH_SEL_CTL] = {QPNP_M5_LOW_THR_LSB,
		QPNP_M5_LOW_THR_MSB, QPNP_M5_HIGH_THR_LSB,
		QPNP_M5_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M5,
		QPNP_ADC_TM_LOW_THR_INT_EN_M5, QPNP_ADC_TM_HIGH_THR_INT_EN_M5,
		QPNP_ADC_TM_M5_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M6_ADC_CH_SEL_CTL] = {QPNP_M6_LOW_THR_LSB,
		QPNP_M6_LOW_THR_MSB, QPNP_M6_HIGH_THR_LSB,
		QPNP_M6_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M6,
		QPNP_ADC_TM_LOW_THR_INT_EN_M6, QPNP_ADC_TM_HIGH_THR_INT_EN_M6,
		QPNP_ADC_TM_M6_MEAS_INTERVAL_CTL},
	[QPNP_ADC_TM_M7_ADC_CH_SEL_CTL] = {QPNP_M7_LOW_THR_LSB,
		QPNP_M7_LOW_THR_MSB, QPNP_M7_HIGH_THR_LSB,
		QPNP_M7_HIGH_THR_MSB, QPNP_ADC_TM_MULTI_MEAS_EN_M7,
		QPNP_ADC_TM_LOW_THR_INT_EN_M7, QPNP_ADC_TM_HIGH_THR_INT_EN_M7,
		QPNP_ADC_TM_M7_MEAS_INTERVAL_CTL},
};

static struct qpnp_adc_tm_reverse_scale_fn adc_tm_rscale_fn[] = {
	[SCALE_R_VBATT] = {qpnp_adc_vbatt_rscaler},
	[SCALE_RBATT_THERM] = {qpnp_adc_btm_scaler},
	[SCALE_R_USB_ID] = {qpnp_adc_usb_scaler},
	[SCALE_RPMIC_THERM] = {qpnp_adc_scale_millidegc_pmic_voltage_thr},
};

static int32_t qpnp_adc_tm_read_reg(int16_t reg, u8 *data)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	int rc = 0;

	rc = spmi_ext_register_readl_legacy(adc_tm->adc->spmi->ctrl,
		adc_tm->adc->slave, (adc_tm->adc->offset + reg), data, 1);
	if (rc < 0)
		pr_err("adc-tm read reg %d failed with %d\n", reg, rc);

	return rc;
}

static int32_t qpnp_adc_tm_write_reg(int16_t reg, u8 data)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	int rc = 0;
	u8 *buf;

	buf = &data;

	rc = spmi_ext_register_writel_legacy(adc_tm->adc->spmi->ctrl,
		adc_tm->adc->slave, (adc_tm->adc->offset + reg), buf, 1);
	if (rc < 0)
		pr_err("adc-tm write reg %d failed with %d\n", reg, rc);

	return rc;
}

static int32_t qpnp_adc_tm_enable(void)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_ADC_TM_EN;
	rc = qpnp_adc_tm_write_reg(QPNP_EN_CTL1, data);
	if (rc < 0)
		pr_err("adc-tm enable failed\n");

	return rc;
}

static int32_t qpnp_adc_tm_disable(void)
{
	u8 data = 0;
	int rc = 0;

	rc = qpnp_adc_tm_write_reg(QPNP_EN_CTL1, data);
	if (rc < 0)
		pr_err("adc-tm disable failed\n");

	return rc;
}

static int32_t qpnp_adc_tm_enable_if_channel_meas(void)
{
	u8 adc_tm_meas_en = 0;
	int rc = 0;

	/* Check if a measurement request is still required */
	rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_MULTI_MEAS_EN,
							&adc_tm_meas_en);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		return rc;
	}

	/* Enable only if there are pending measurement requests */
	if (adc_tm_meas_en) {
		qpnp_adc_tm_enable();

		/* Request conversion */
		rc = qpnp_adc_tm_write_reg(QPNP_CONV_REQ, QPNP_CONV_REQ_SET);
		if (rc < 0) {
			pr_err("adc-tm request conversion failed\n");
			return rc;
		}
	}

	return rc;
}

static int32_t qpnp_adc_tm_req_sts_check(void)
{
	u8 status1;
	int rc, count = 0;

	/* The VADC_TM bank needs to be disabled for new conversion request */
	rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_STATUS1, &status1);
	if (rc) {
		pr_err("adc-tm read status1 failed\n");
		return rc;
	}

	/* Disable the bank if a conversion is occuring */
	while ((status1 & QPNP_STATUS1_REQ_STS) && (count < 5)) {
		rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_STATUS1, &status1);
		if (rc < 0)
			pr_err("adc-tm disable failed\n");
		/* Wait time is based on the optimum sampling rate
		 * and adding enough time buffer to account for ADC conversions
		 * occuring on different peripheral banks */
		usleep_range(QPNP_MIN_TIME, QPNP_MAX_TIME);
		count++;
	}

	return rc;
}

static int32_t qpnp_adc_tm_check_revision(uint32_t btm_chan_num)
{
	u8 rev, perph_subtype;
	int rc = 0;

	rc = qpnp_adc_tm_read_reg(QPNP_REVISION3, &rev);
	if (rc) {
		pr_err("adc-tm revision read failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_read_reg(QPNP_PERPH_SUBTYPE, &perph_subtype);
	if (rc) {
		pr_err("adc-tm perph_subtype read failed\n");
		return rc;
	}

	if (perph_subtype == QPNP_PERPH_TYPE2) {
		if ((rev < QPNP_REVISION_EIGHT_CHANNEL_SUPPORT) &&
			(btm_chan_num > QPNP_ADC_TM_M4_ADC_CH_SEL_CTL)) {
			pr_debug("Version does not support more than 5 channels\n");
			return -EINVAL;
		}
	}

	return rc;
}
static int32_t qpnp_adc_tm_mode_select(u8 mode_ctl)
{
	int rc;

	mode_ctl |= (QPNP_ADC_TRIM_EN | QPNP_AMUX_TRIM_EN);

	/* VADC_BTM current sets mode to recurring measurements */
	rc = qpnp_adc_tm_write_reg(QPNP_MODE_CTL, mode_ctl);
	if (rc < 0)
		pr_err("adc-tm write mode selection err\n");

	return rc;
}

static int32_t qpnp_adc_tm_timer_interval_select(uint32_t btm_chan,
		struct qpnp_vadc_chan_properties *chan_prop)
{
	int rc;
	u8 meas_interval_timer2 = 0;

	/* Configure kernel clients to timer1 */
	switch (chan_prop->timer_select) {
	case ADC_MEAS_TIMER_SELECT1:
		rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_MEAS_INTERVAL_CTL,
				chan_prop->meas_interval1);
		if (rc < 0) {
			pr_err("timer1 configure failed\n");
			return rc;
		}
	break;
	case ADC_MEAS_TIMER_SELECT2:
		/* Thermal channels uses timer2, default to 1 second */
		rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				&meas_interval_timer2);
		if (rc < 0) {
			pr_err("timer2 configure read failed\n");
			return rc;
		}
		meas_interval_timer2 |=
			(chan_prop->meas_interval2 <<
			QPNP_ADC_TM_MEAS_INTERVAL_CTL2_SHIFT);
		rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
			meas_interval_timer2);
		if (rc < 0) {
			pr_err("timer2 configure failed\n");
			return rc;
		}
	break;
	case ADC_MEAS_TIMER_SELECT3:
		rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
				&meas_interval_timer2);
		if (rc < 0) {
			pr_err("timer3 read failed\n");
			return rc;
		}
		chan_prop->meas_interval2 = ADC_MEAS3_INTERVAL_1S;
		meas_interval_timer2 |= chan_prop->meas_interval2;
		rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_MEAS_INTERVAL_CTL2,
			meas_interval_timer2);
		if (rc < 0) {
			pr_err("timer3 configure failed\n");
			return rc;
		}
	break;
	default:
		pr_err("Invalid timer selection\n");
		return -EINVAL;
	}

	/* Select the timer to use for the corresponding channel */
	adc_tm_data[btm_chan].meas_interval_ctl = chan_prop->timer_select;

	return rc;
}

static int32_t qpnp_adc_tm_reg_update(uint16_t addr,
		u8 mask, bool state)
{
	u8 reg_value = 0;
	int rc = 0;

	rc = qpnp_adc_tm_read_reg(addr, &reg_value);
	if (rc < 0) {
		pr_err("read failed for addr:0x%x\n", addr);
		return rc;
	}

	reg_value = reg_value & ~mask;
	if (state)
		reg_value |= mask;

	pr_debug("state:%d, reg:0x%x with bits:0x%x and mask:0x%x\n",
					state, addr, reg_value, ~mask);
	rc = qpnp_adc_tm_write_reg(addr, reg_value);
	if (rc < 0) {
		pr_err("write failed for addr:%x\n", addr);
		return rc;
	}

	return rc;
}

static int32_t qpnp_adc_tm_thr_update(uint32_t btm_chan,
			struct qpnp_vadc_chan_properties *chan_prop)
{
	int rc = 0;

	rc = qpnp_adc_tm_write_reg(
			adc_tm_data[btm_chan].low_thr_lsb_addr,
			QPNP_ADC_TM_THR_LSB_MASK(chan_prop->low_thr));
	if (rc < 0) {
		pr_err("low threshold lsb setting failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(
		adc_tm_data[btm_chan].low_thr_msb_addr,
		QPNP_ADC_TM_THR_MSB_MASK(chan_prop->low_thr));
	if (rc < 0) {
		pr_err("low threshold msb setting failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(
		adc_tm_data[btm_chan].high_thr_lsb_addr,
		QPNP_ADC_TM_THR_LSB_MASK(chan_prop->high_thr));
	if (rc < 0) {
		pr_err("high threshold lsb setting failed\n");
		return rc;
	}

	rc = qpnp_adc_tm_write_reg(
		adc_tm_data[btm_chan].high_thr_msb_addr,
		QPNP_ADC_TM_THR_MSB_MASK(chan_prop->high_thr));
	if (rc < 0)
		pr_err("high threshold msb setting failed\n");

	pr_debug("client requested low:%d and high:%d\n",
		chan_prop->low_thr, chan_prop->high_thr);

	return rc;
}

static int32_t qpnp_adc_tm_channel_configure(uint32_t btm_chan,
			struct qpnp_vadc_chan_properties *chan_prop,
			uint32_t amux_channel)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	int rc = 0, i = 0, chan_idx = 0;
	bool chan_found = false;
	u8 sensor_mask = 0;
	bool high_thr_int_en = false;
	bool low_thr_int_en = false;
	bool measure_en = true;

	while (i < adc_tm->max_channels_available) {
		if (adc_tm->sensor[i].btm_channel_num == btm_chan) {
			chan_idx = i;
			chan_found = true;
			i++;
		} else
			i++;
	}

	if (!chan_found) {
		pr_err("Channel not found\n");
		return -EINVAL;
	}

	sensor_mask = 1 << chan_idx;
	if (!adc_tm->sensor[chan_idx].thermal_node) {
		/* Update low and high notification thresholds */
		rc = qpnp_adc_tm_thr_update(btm_chan,
				chan_prop);
		if (rc < 0) {
			pr_err("setting chan:%d threshold failed\n", btm_chan);
			return rc;
		}

		switch (chan_prop->state_request) {
		case ADC_TM_HIGH_THR_ENABLE:
			high_thr_int_en = true;
			measure_en = true;
			break;
		case ADC_TM_LOW_THR_ENABLE:
			low_thr_int_en = true;
			measure_en = true;
			break;
		case ADC_TM_HIGH_LOW_THR_ENABLE:
			high_thr_int_en = true;
			low_thr_int_en = true;
			measure_en = true;
			break;
		case ADC_TM_HIGH_THR_DISABLE:
			high_thr_int_en = false;
			measure_en = false;
			break;
		case ADC_TM_LOW_THR_DISABLE:
			low_thr_int_en = false;
			measure_en = false;
			break;
		case ADC_TM_HIGH_LOW_THR_DISABLE:
			high_thr_int_en = false;
			low_thr_int_en = false;
			measure_en = false;
			break;
		default:
			break;
		}

		pr_debug("low sensor mask:%x with state:%d\n",
				sensor_mask, chan_prop->state_request);
		rc = qpnp_adc_tm_reg_update(
			QPNP_ADC_TM_LOW_THR_INT_EN, sensor_mask, low_thr_int_en);
		if (rc < 0) {
			pr_err("low thr %s err:%d\n", low_thr_int_en ?
				"enable" : "disable", btm_chan);
			return rc;
		}

		pr_debug("high sensor mask:%x with state:%d\n",
				sensor_mask, chan_prop->state_request);
		rc = qpnp_adc_tm_reg_update(
			QPNP_ADC_TM_HIGH_THR_INT_EN, sensor_mask, high_thr_int_en);
		if (rc < 0) {
			pr_err("high thr %s err:%d\n", high_thr_int_en ?
					"enable" : "disable", btm_chan);
			return rc;
		}

	}

	/* Enable corresponding BTM channel measurement */
	rc = qpnp_adc_tm_reg_update(
		QPNP_ADC_TM_MULTI_MEAS_EN, sensor_mask, measure_en);
	if (rc < 0) {
		pr_err("multi measurement %s failed\n",
				measure_en ? "enable" : "disable");
		return rc;
	}

	return rc;
}

static int32_t qpnp_adc_tm_configure(
			struct qpnp_adc_amux_properties *chan_prop)
{
	u8 decimation = 0, op_cntrl = 0;
	int rc = 0;
	uint32_t btm_chan = 0;

	/* Disable bank */
	rc = qpnp_adc_tm_disable();
	if (rc)
		return rc;

	/* Check if a conversion is in progress */
	rc = qpnp_adc_tm_req_sts_check();
	if (rc < 0) {
		pr_err("adc-tm req_sts check failed\n");
		return rc;
	}

	/* Set measurement in recurring mode */
	rc = qpnp_adc_tm_mode_select(chan_prop->mode_sel);
	if (rc < 0) {
		pr_err("adc-tm mode select failed\n");
		return rc;
	}

	/* Configure AMUX channel select for the corresponding BTM channel*/
	btm_chan = chan_prop->chan_prop->tm_channel_select;
	rc = qpnp_adc_tm_write_reg(btm_chan, chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("adc-tm channel selection err\n");
		return rc;
	}

	/* Digital paramater setup */
	decimation |= chan_prop->decimation <<
				QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT;
	rc = qpnp_adc_tm_write_reg(QPNP_ADC_DIG_PARAM, decimation);
	if (rc < 0) {
		pr_err("adc-tm digital parameter setup err\n");
		return rc;
	}

	/* Hardware setting time */
	rc = qpnp_adc_tm_write_reg(QPNP_HW_SETTLE_DELAY,
					chan_prop->hw_settle_time);
	if (rc < 0) {
		pr_err("adc-tm hw settling time setup err\n");
		return rc;
	}

	/* Fast averaging setup */
	rc = qpnp_adc_tm_write_reg(QPNP_FAST_AVG_CTL,
					chan_prop->fast_avg_setup);
	if (rc < 0) {
		pr_err("adc-tm fast-avg setup err\n");
		return rc;
	}

	/* Measurement interval setup */
	rc = qpnp_adc_tm_timer_interval_select(btm_chan,
						chan_prop->chan_prop);
	if (rc < 0) {
		pr_err("adc-tm timer select failed\n");
		return rc;
	}

	/* Channel configuration setup */
	rc = qpnp_adc_tm_channel_configure(btm_chan, chan_prop->chan_prop,
					chan_prop->amux_channel);
	if (rc < 0) {
		pr_err("adc-tm channel configure failed\n");
		return rc;
	}

	/* Recurring interval measurement enable */
	rc = qpnp_adc_tm_read_reg(QPNP_ADC_MEAS_INTERVAL_OP_CTL, &op_cntrl);
	op_cntrl |= QPNP_ADC_MEAS_INTERVAL_OP;
	rc = qpnp_adc_tm_reg_update(QPNP_ADC_MEAS_INTERVAL_OP_CTL,
			op_cntrl, true);
	if (rc < 0) {
		pr_err("adc-tm meas interval op configure failed\n");
		return rc;
	}

	/* Enable bank */
	rc = qpnp_adc_tm_enable();
	if (rc)
		return rc;

	/* Request conversion */
	rc = qpnp_adc_tm_write_reg(QPNP_CONV_REQ, QPNP_CONV_REQ_SET);
	if (rc < 0) {
		pr_err("adc-tm request conversion failed\n");
		return rc;
	}

	return 0;
}

static int qpnp_adc_tm_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct qpnp_adc_tm_sensor *adc_tm_sensor = thermal->devdata;

	if (!adc_tm_sensor || qpnp_adc_tm_check_revision(
			adc_tm_sensor->btm_channel_num) || !mode)
		return -EINVAL;

	*mode = adc_tm_sensor->mode;

	return 0;
}

static int qpnp_adc_tm_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct qpnp_adc_tm_sensor *adc_tm = thermal->devdata;
	struct qpnp_adc_tm_drv *adc_drv = qpnp_adc_tm;
	int rc = 0, channel;
	u8 sensor_mask = 0;

	if (!adc_tm || qpnp_adc_tm_check_revision(adc_tm->btm_channel_num))
		return -EINVAL;

	if (mode == THERMAL_DEVICE_ENABLED) {
		adc_drv->adc->amux_prop->amux_channel =
					adc_tm->vadc_channel_num;
		channel = adc_tm->sensor_num;
		adc_drv->adc->amux_prop->decimation =
			adc_drv->adc->adc_channels[channel].adc_decimation;
		adc_drv->adc->amux_prop->hw_settle_time =
			adc_drv->adc->adc_channels[channel].hw_settle_time;
		adc_drv->adc->amux_prop->fast_avg_setup =
			adc_drv->adc->adc_channels[channel].fast_avg_setup;
		adc_drv->adc->amux_prop->mode_sel =
			ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
		adc_drv->adc->amux_prop->chan_prop->timer_select =
					ADC_MEAS_TIMER_SELECT1;
		adc_drv->adc->amux_prop->chan_prop->meas_interval1 =
						ADC_MEAS1_INTERVAL_1S;
		adc_drv->adc->amux_prop->chan_prop->low_thr = adc_tm->low_thr;
		adc_drv->adc->amux_prop->chan_prop->high_thr = adc_tm->high_thr;
		adc_drv->adc->amux_prop->chan_prop->tm_channel_select =
			adc_tm->btm_channel_num;

		rc = qpnp_adc_tm_configure(adc_drv->adc->amux_prop);
		if (rc) {
			pr_err("adc-tm tm configure failed with %d\n", rc);
			return -EINVAL;
		}
	} else if (mode == THERMAL_DEVICE_DISABLED) {
		sensor_mask = 1 << adc_tm->sensor_num;
		/* Disable bank */
		rc = qpnp_adc_tm_disable();
		if (rc < 0) {
			pr_err("adc-tm disable failed\n");
			return rc;
		}

		/* Check if a conversion is in progress */
		rc = qpnp_adc_tm_req_sts_check();
		if (rc < 0) {
			pr_err("adc-tm req_sts check failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_MULTI_MEAS_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("multi measurement update failed\n");
			return rc;
		}

		rc = qpnp_adc_tm_enable_if_channel_meas();
		if (rc < 0) {
			pr_err("re-enabling measurement failed\n");
			return rc;
		}
	}

	adc_tm->mode = mode;

	return 0;
}

static int qpnp_adc_tm_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct qpnp_adc_tm_sensor *adc_tm = thermal->devdata;

	if (!adc_tm || qpnp_adc_tm_check_revision(adc_tm->btm_channel_num)
						|| !type || type < 0)
		return -EINVAL;

	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		*type = THERMAL_TRIP_ACTIVE;
	break;
	case ADC_TM_TRIP_LOW_COOL:
		*type = THERMAL_TRIP_PASSIVE;
	break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_adc_tm_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int *temp)
{
	struct qpnp_adc_tm_sensor *adc_tm_sensor = thermal->devdata;
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	int64_t result = 0;
	u8 trip_cool_thr0, trip_cool_thr1, trip_warm_thr0, trip_warm_thr1;
	unsigned int reg, rc = 0, btm_channel_num;
	uint16_t reg_low_thr_lsb, reg_low_thr_msb;
	uint16_t reg_high_thr_lsb, reg_high_thr_msb;

	if (!adc_tm || qpnp_adc_tm_check_revision(
					adc_tm_sensor->btm_channel_num))
		return -EINVAL;

	btm_channel_num = adc_tm_sensor->btm_channel_num;
	reg_low_thr_lsb = adc_tm_data[btm_channel_num].low_thr_lsb_addr;
	reg_low_thr_msb = adc_tm_data[btm_channel_num].low_thr_msb_addr;
	reg_high_thr_lsb = adc_tm_data[btm_channel_num].high_thr_lsb_addr;
	reg_high_thr_msb = adc_tm_data[btm_channel_num].high_thr_msb_addr;

	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		rc = qpnp_adc_tm_read_reg(reg_low_thr_lsb, &trip_warm_thr0);
		if (rc) {
			pr_err("adc-tm low_thr_lsb err\n");
			return rc;
		}

		rc = qpnp_adc_tm_read_reg(reg_low_thr_msb, &trip_warm_thr1);
		if (rc) {
			pr_err("adc-tm low_thr_msb err\n");
			return rc;
		}
	reg = (trip_warm_thr1 << 8) | trip_warm_thr0;
	break;
	case ADC_TM_TRIP_LOW_COOL:
		rc = qpnp_adc_tm_read_reg(reg_high_thr_lsb, &trip_cool_thr0);
		if (rc) {
			pr_err("adc-tm_tm high_thr_lsb err\n");
			return rc;
		}

		rc = qpnp_adc_tm_read_reg(reg_high_thr_msb, &trip_cool_thr1);
		if (rc) {
			pr_err("adc-tm_tm high_thr_lsb err\n");
			return rc;
		}
	reg = (trip_cool_thr1 << 8) | trip_cool_thr0;
	break;
	default:
		return -EINVAL;
	}

	rc = qpnp_adc_tm_scale_voltage_therm_pu2(reg, &result);
	if (rc < 0) {
		pr_err("Failed to lookup the therm thresholds\n");
		return rc;
	}

	*temp = result;

	return 0;
}

static int qpnp_adc_tm_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, int temp)
{
	struct qpnp_adc_tm_sensor *adc_tm_sensor = thermal->devdata;
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	struct qpnp_adc_tm_config tm_config;
	u8 trip_cool_thr0, trip_cool_thr1, trip_warm_thr0, trip_warm_thr1;
	uint16_t reg_low_thr_lsb, reg_low_thr_msb;
	uint16_t reg_high_thr_lsb, reg_high_thr_msb;
	int rc = 0, btm_channel_num;

	if (!adc_tm || qpnp_adc_tm_check_revision(
				adc_tm_sensor->btm_channel_num))
		return -EINVAL;

	tm_config.channel = adc_tm_sensor->vadc_channel_num;
	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		tm_config.high_thr_temp = temp;
		break;
	case ADC_TM_TRIP_LOW_COOL:
		tm_config.low_thr_temp = temp;
		break;
	default:
		return -EINVAL;
	}

	pr_debug("requested a high - %d and low - %d with trip - %d\n",
			tm_config.high_thr_temp, tm_config.low_thr_temp, trip);
	rc = qpnp_adc_tm_scale_therm_voltage_pu2(&tm_config);
	if (rc < 0) {
		pr_err("Failed to lookup the adc-tm thresholds\n");
		return rc;
	}

	trip_warm_thr0 = ((tm_config.low_thr_voltage << 24) >> 24);
	trip_warm_thr1 = ((tm_config.low_thr_voltage << 16) >> 24);
	trip_cool_thr0 = ((tm_config.high_thr_voltage << 24) >> 24);
	trip_cool_thr1 = ((tm_config.high_thr_voltage << 16) >> 24);

	btm_channel_num = adc_tm_sensor->btm_channel_num;
	reg_low_thr_lsb = adc_tm_data[btm_channel_num].low_thr_lsb_addr;
	reg_low_thr_msb = adc_tm_data[btm_channel_num].low_thr_msb_addr;
	reg_high_thr_lsb = adc_tm_data[btm_channel_num].high_thr_lsb_addr;
	reg_high_thr_msb = adc_tm_data[btm_channel_num].high_thr_msb_addr;

	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		rc = qpnp_adc_tm_write_reg(reg_low_thr_lsb, trip_cool_thr0);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(reg_low_thr_msb, trip_cool_thr1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}
	adc_tm_sensor->low_thr = tm_config.high_thr_voltage;
	break;
	case ADC_TM_TRIP_LOW_COOL:
		rc = qpnp_adc_tm_write_reg(reg_high_thr_lsb, trip_warm_thr0);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}

		rc = qpnp_adc_tm_write_reg(reg_high_thr_msb, trip_warm_thr1);
		if (rc) {
			pr_err("adc-tm_tm read threshold err\n");
			return rc;
		}
	adc_tm_sensor->high_thr = tm_config.low_thr_voltage;
	break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void notify_battery_therm(struct qpnp_adc_tm_sensor *adc_tm)
{
	/* Battery therm's warm temperature translates to low voltage */
	if (adc_tm->low_thr_notify) {
		/* HIGH_STATE = WARM_TEMP for battery client */
		adc_tm->btm_param->threshold_notification(
		ADC_TM_WARM_STATE, adc_tm->btm_param->btm_ctx);
		adc_tm->low_thr_notify = false;
	}

	/* Battery therm's cool temperature translates to high voltage */
	if (adc_tm->high_thr_notify) {
		/* LOW_STATE = COOL_TEMP for battery client */
		adc_tm->btm_param->threshold_notification(
		ADC_TM_COOL_STATE, adc_tm->btm_param->btm_ctx);
		adc_tm->high_thr_notify = false;
	}

	return;
}

static void notify_clients(struct qpnp_adc_tm_sensor *adc_tm)
{
	/* For non batt therm clients */
	if (adc_tm->low_thr_notify) {
		pr_debug("notify kernel with low state\n");
		adc_tm->btm_param->threshold_notification(
		ADC_TM_LOW_STATE, adc_tm->btm_param->btm_ctx);
		adc_tm->low_thr_notify = false;
	}

	if (adc_tm->high_thr_notify) {
		pr_debug("notify kernel with high state\n");
		adc_tm->btm_param->threshold_notification(
		ADC_TM_HIGH_STATE, adc_tm->btm_param->btm_ctx);
		adc_tm->high_thr_notify = false;
	}

	return;
}

static void notify_adc_tm_fn(struct work_struct *work)
{
	struct qpnp_adc_tm_sensor *adc_tm = container_of(work,
		struct qpnp_adc_tm_sensor, work);

	if (adc_tm->thermal_node) {
		sysfs_notify(&adc_tm->tz_dev->device.kobj,
					NULL, "btm");
		pr_debug("notifying uspace client\n");
	} else {
		if (adc_tm->btm_param->threshold_notification != NULL) {
			if (adc_tm->scale_type == SCALE_RBATT_THERM)
				notify_battery_therm(adc_tm);
			else
				notify_clients(adc_tm);
		}
	}

	return;
}

static int qpnp_adc_tm_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct qpnp_adc_tm_sensor *adc_tm = thermal->devdata;
	int rc = 0, sensor_mask = 0;
	u8 thr_int_en = 0;
	bool state = false;

	if (!adc_tm || qpnp_adc_tm_check_revision(adc_tm->btm_channel_num))
		return -EINVAL;

	if (mode == THERMAL_TRIP_ACTIVATION_ENABLED)
		state = true;

	sensor_mask = 1 << adc_tm->sensor_num;

	pr_debug("Sensor number:%x with state:%d\n", adc_tm->sensor_num, state);

	switch (trip) {
	case ADC_TM_TRIP_HIGH_WARM:
		/* low_thr (lower voltage) for higher temp */
		thr_int_en = adc_tm_data[adc_tm->btm_channel_num].
							low_thr_int_chan_en;
		rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_LOW_THR_INT_EN,
				sensor_mask, state);
		if (rc)
			pr_err("channel:%x failed\n", adc_tm->btm_channel_num);
	break;
	case ADC_TM_TRIP_LOW_COOL:
		/* high_thr (higher voltage) for cooler temp */
		thr_int_en = adc_tm_data[adc_tm->btm_channel_num].
							high_thr_int_chan_en;
		rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_HIGH_THR_INT_EN,
				sensor_mask, state);
		if (rc)
			pr_err("channel:%x failed\n", adc_tm->btm_channel_num);
	break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int qpnp_adc_tm_read_status(void)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	u8 status_low = 0, status_high = 0, qpnp_adc_tm_meas_en = 0;
	u8 adc_tm_low_enable = 0, adc_tm_high_enable = 0;
	u8 sensor_mask = 0;
	int rc = 0, sensor_notify_num = 0, i = 0, sensor_num = 0, btm_chan_num;

	if (!adc_tm || !adc_tm->adc_tm_initialized)
		return -ENODEV;

	mutex_lock(&adc_tm->adc->adc_lock);

	rc = qpnp_adc_tm_req_sts_check();
	if (rc) {
		pr_err("adc-tm-tm req sts check failed with %d\n", rc);
		goto fail;
	}

	rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_STATUS_LOW, &status_low);
	if (rc) {
		pr_err("adc-tm-tm read status low failed with %d\n", rc);
		goto fail;
	}

	rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_STATUS_HIGH, &status_high);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		goto fail;
	}

	/* Check which interrupt threshold is lower and measure against the
	 * enabled channel */
	rc = qpnp_adc_tm_read_reg(QPNP_ADC_TM_MULTI_MEAS_EN,
							&qpnp_adc_tm_meas_en);
	if (rc) {
		pr_err("adc-tm-tm read status high failed with %d\n", rc);
		goto fail;
	}

	adc_tm_low_enable = qpnp_adc_tm_meas_en & status_low;
	adc_tm_high_enable = qpnp_adc_tm_meas_en & status_high;

	if (adc_tm_high_enable) {
		sensor_notify_num = adc_tm_high_enable;
		while (i < adc_tm->max_channels_available) {
			if ((sensor_notify_num & 0x1) == 1)
				sensor_num = i;
			sensor_notify_num >>= 1;
			i++;
		}

		btm_chan_num = adc_tm->sensor[sensor_num].btm_channel_num;
		pr_debug("high:sen:%d, hs:0x%x, ls:0x%x, meas_en:0x%x\n",
			sensor_num, adc_tm_high_enable, adc_tm_low_enable,
			qpnp_adc_tm_meas_en);
		if (!adc_tm->sensor[sensor_num].thermal_node) {
			/* For non thermal registered clients
				such as usb_id, vbatt, pmic_therm */
			sensor_mask = 1 << sensor_num;
			pr_debug("non thermal node - mask:%x\n", sensor_mask);
			rc = qpnp_adc_tm_reg_update(
				QPNP_ADC_TM_HIGH_THR_INT_EN,
				sensor_mask, false);
			if (rc < 0) {
				pr_err("high threshold int read failed\n");
				goto fail;
			}
			adc_tm->sensor[sensor_num].high_thr_notify = true;
		} else {
			/* Uses the thermal sysfs registered device to disable
				the corresponding high voltage threshold which
				 is triggered by low temp */
			pr_debug("thermal node with mask:%x\n", sensor_mask);
			rc = qpnp_adc_tm_activate_trip_type(
				adc_tm->sensor[sensor_num].tz_dev,
				ADC_TM_TRIP_LOW_COOL,
				THERMAL_TRIP_ACTIVATION_DISABLED);
			if (rc < 0) {
				pr_err("notify error:%d\n", sensor_num);
				goto fail;
			}
		}
	}

	if (adc_tm_low_enable) {
		sensor_notify_num = adc_tm_low_enable;
		i = 0;
		while (i < adc_tm->max_channels_available) {
			if ((sensor_notify_num & 0x1) == 1)
				sensor_num = i;
			sensor_notify_num >>= 1;
			i++;
		}

		btm_chan_num = adc_tm->sensor[sensor_num].btm_channel_num;
		pr_debug("low:sen:%d, hs:0x%x, ls:0x%x, meas_en:0x%x\n",
			sensor_num, adc_tm_high_enable, adc_tm_low_enable,
			qpnp_adc_tm_meas_en);
		if (!adc_tm->sensor[sensor_num].thermal_node) {
			/* For non thermal registered clients
				such as usb_id, vbatt, pmic_therm */
			pr_debug("non thermal node - mask:%x\n", sensor_mask);
			sensor_mask = 1 << sensor_num;
			rc = qpnp_adc_tm_reg_update(
				QPNP_ADC_TM_LOW_THR_INT_EN,
				sensor_mask, false);
			if (rc < 0) {
				pr_err("low threshold int read failed\n");
				goto fail;
			}
			adc_tm->sensor[sensor_num].low_thr_notify = true;
		} else {
			/* Uses the thermal sysfs registered device to disable
				the corresponding low voltage threshold which
				 is triggered by high temp */
			pr_debug("thermal node with mask:%x\n", sensor_mask);
			rc = qpnp_adc_tm_activate_trip_type(
				adc_tm->sensor[sensor_num].tz_dev,
				ADC_TM_TRIP_HIGH_WARM,
				THERMAL_TRIP_ACTIVATION_DISABLED);
			if (rc < 0) {
				pr_err("notify error:%d\n", sensor_num);
				goto fail;
			}
		}
	}

	if (adc_tm_high_enable || adc_tm_low_enable) {
		rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_MULTI_MEAS_EN,
			sensor_mask, false);
		if (rc < 0) {
			pr_err("multi meas disable for channel failed\n");
			goto fail;
		}

		rc = qpnp_adc_tm_enable_if_channel_meas();
		if (rc < 0) {
			pr_err("re-enabling measurement failed\n");
			return rc;
		}
	} else
		pr_debug("No threshold status enable %d for high/low??\n",
								sensor_mask);

fail:
	mutex_unlock(&adc_tm->adc->adc_lock);

	if (adc_tm_high_enable || adc_tm_low_enable)
		schedule_work(&adc_tm->sensor[sensor_num].work);

	return rc;
}

static void qpnp_adc_tm_high_thr_work(struct work_struct *work)
{
	int rc;

	rc = qpnp_adc_tm_read_status();
	if (rc < 0)
		pr_err("adc-tm high thr work failed\n");

	return;
}
DECLARE_WORK(trigger_completion_adc_tm_high_thr_work,
					qpnp_adc_tm_high_thr_work);

static irqreturn_t qpnp_adc_tm_high_thr_isr(int irq, void *data)
{
	qpnp_adc_tm_disable();

	schedule_work(&trigger_completion_adc_tm_high_thr_work);

	return IRQ_HANDLED;
}

static void qpnp_adc_tm_low_thr_work(struct work_struct *work)
{
	int rc;

	rc = qpnp_adc_tm_read_status();
	if (rc < 0)
		pr_err("adc-tm low thr work failed\n");

	return;
}
DECLARE_WORK(trigger_completion_adc_tm_low_thr_work, qpnp_adc_tm_low_thr_work);

static irqreturn_t qpnp_adc_tm_low_thr_isr(int irq, void *data)
{
	qpnp_adc_tm_disable();

	schedule_work(&trigger_completion_adc_tm_low_thr_work);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_adc_tm_isr(int irq, void *dev_id)
{
	struct qpnp_adc_tm_drv *adc_tm = dev_id;

	complete(&adc_tm->adc->adc_rslt_completion);

	return IRQ_HANDLED;
}

static int qpnp_adc_read_temp(struct thermal_zone_device *thermal,
			     int *temp)
{
	struct qpnp_adc_tm_sensor *adc_tm_sensor = thermal->devdata;
	struct qpnp_vadc_result result;
	int rc = 0;

	rc = qpnp_vadc_read(adc_tm_sensor->vadc_channel_num, &result);
	if (rc)
		return rc;

	*temp = result.physical;

	return rc;
}

static struct thermal_zone_device_ops qpnp_adc_tm_thermal_ops = {
	.get_temp = qpnp_adc_read_temp,
	//.get_mode = qpnp_adc_tm_get_mode,
	//.set_mode = qpnp_adc_tm_set_mode,
	.get_trip_type = qpnp_adc_tm_get_trip_type,
	//.activate_trip_type = qpnp_adc_tm_activate_trip_type,
	.get_trip_temp = qpnp_adc_tm_get_trip_temp,
	.set_trip_temp = qpnp_adc_tm_set_trip_temp,
};

int32_t qpnp_adc_tm_channel_measure(struct qpnp_adc_tm_btm_param *param)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	uint32_t channel, dt_index = 0, scale_type = 0;
	int rc = 0, i = 0;
	bool chan_found = false;

	if (!adc_tm || !adc_tm->adc_tm_initialized)
		return -ENODEV;

	if (param->threshold_notification == NULL) {
		pr_err("No notification for high/low temp??\n");
		return -EINVAL;
	}

	mutex_lock(&adc_tm->adc->adc_lock);

	channel = param->channel;
	while (i < adc_tm->max_channels_available) {
		if (adc_tm->adc->adc_channels[i].channel_num ==
							channel) {
			dt_index = i;
			chan_found = true;
			i++;
		} else
			i++;
	}

	if (!chan_found)  {
		pr_err("not a valid ADC_TM channel\n");
		rc = -EINVAL;
		goto fail_unlock;
	}

	rc = qpnp_adc_tm_check_revision(
			adc_tm->sensor[dt_index].btm_channel_num);
	if (rc < 0)
		goto fail_unlock;

	scale_type = adc_tm->adc->adc_channels[dt_index].adc_scale_fn;
	if (scale_type >= SCALE_RSCALE_NONE) {
		rc = -EBADF;
		goto fail_unlock;
	}

	pr_debug("channel:%d, scale_type:%d, dt_idx:%d",
					channel, scale_type, dt_index);
	adc_tm->adc->amux_prop->amux_channel = channel;
	adc_tm->adc->amux_prop->decimation =
			adc_tm->adc->adc_channels[dt_index].adc_decimation;
	adc_tm->adc->amux_prop->hw_settle_time =
			adc_tm->adc->adc_channels[dt_index].hw_settle_time;
	adc_tm->adc->amux_prop->fast_avg_setup =
			adc_tm->adc->adc_channels[dt_index].fast_avg_setup;
	adc_tm->adc->amux_prop->mode_sel =
		ADC_OP_MEASUREMENT_INTERVAL << QPNP_OP_MODE_SHIFT;
	adc_tm->adc->amux_prop->chan_prop->meas_interval1 =
						ADC_MEAS1_INTERVAL_1S;
	adc_tm_rscale_fn[scale_type].chan(param,
			&adc_tm->adc->amux_prop->chan_prop->low_thr,
			&adc_tm->adc->amux_prop->chan_prop->high_thr);
	adc_tm->adc->amux_prop->chan_prop->tm_channel_select =
				adc_tm->sensor[dt_index].btm_channel_num;
	adc_tm->adc->amux_prop->chan_prop->timer_select =
					ADC_MEAS_TIMER_SELECT1;
	adc_tm->adc->amux_prop->chan_prop->state_request =
					param->state_request;
	rc = qpnp_adc_tm_configure(adc_tm->adc->amux_prop);
	if (rc) {
		pr_err("adc-tm configure failed with %d\n", rc);
		goto fail_unlock;
	}

	adc_tm->sensor[dt_index].btm_param = param;
	adc_tm->sensor[dt_index].scale_type = scale_type;

fail_unlock:
	mutex_unlock(&adc_tm->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_adc_tm_channel_measure);

int32_t qpnp_adc_tm_disable_chan_meas(struct qpnp_adc_tm_btm_param *param)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;
	uint32_t channel, dt_index = 0, btm_chan_num;
	u8 sensor_mask = 0;
	int rc = 0;

	if (!adc_tm || !adc_tm->adc_tm_initialized)
		return -ENODEV;

	mutex_lock(&adc_tm->adc->adc_lock);

	/* Disable bank */
	rc = qpnp_adc_tm_disable();
	if (rc < 0) {
		pr_err("adc-tm disable failed\n");
		goto fail;
	}

	/* Check if a conversion is in progress */
	rc = qpnp_adc_tm_req_sts_check();
	if (rc < 0) {
		pr_err("adc-tm req_sts check failed\n");
		goto fail;
	}

	channel = param->channel;
	while ((adc_tm->adc->adc_channels[dt_index].channel_num
		!= channel) && (dt_index < adc_tm->max_channels_available))
		dt_index++;

	if (dt_index >= adc_tm->max_channels_available) {
		pr_err("not a valid ADC_TMN channel\n");
		rc = -EINVAL;
		goto fail;
	}

	btm_chan_num = adc_tm->sensor[dt_index].btm_channel_num;
	sensor_mask = 1 << adc_tm->sensor[dt_index].sensor_num;

	rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_LOW_THR_INT_EN,
		sensor_mask, false);
	if (rc < 0) {
		pr_err("low threshold int write failed\n");
		goto fail;
	}

	rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_HIGH_THR_INT_EN,
		sensor_mask, false);
	if (rc < 0) {
		pr_err("high threshold int enable failed\n");
		goto fail;
	}

	rc = qpnp_adc_tm_reg_update(QPNP_ADC_TM_MULTI_MEAS_EN,
		sensor_mask, false);
	if (rc < 0) {
		pr_err("multi measurement en failed\n");
		goto fail;
	}

	rc = qpnp_adc_tm_enable_if_channel_meas();
	if (rc < 0)
		pr_err("re-enabling measurement failed\n");

fail:
	mutex_unlock(&adc_tm->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_adc_tm_disable_chan_meas);

int32_t qpnp_adc_tm_usbid_configure(struct qpnp_adc_tm_btm_param *param)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;

	if (!adc_tm || !adc_tm->adc_tm_initialized)
		return -ENODEV;

	if (adc_tm->usb_id_ext_pull_up)
		param->channel = LR_MUX10_USB_ID_LV;
	else
		param->channel = LR_MUX10_PU2_AMUX_USB_ID_LV;

	return qpnp_adc_tm_channel_measure(param);
}
EXPORT_SYMBOL(qpnp_adc_tm_usbid_configure);

int32_t qpnp_adc_tm_usbid_end(void)
{
	struct qpnp_adc_tm_btm_param param;

	return qpnp_adc_tm_disable_chan_meas(&param);
}
EXPORT_SYMBOL(qpnp_adc_tm_usbid_end);

int32_t qpnp_adc_tm_is_ready(void)
{
	struct qpnp_adc_tm_drv *adc_tm = qpnp_adc_tm;

	if (!adc_tm || !adc_tm->adc_tm_initialized)
		return -EPROBE_DEFER;
	else
		return 0;
}
EXPORT_SYMBOL(qpnp_adc_tm_is_ready);

static int qpnp_adc_tm_probe(struct spmi_device *spmi)
{
	struct device_node *node = spmi->dev.of_node, *child;
	struct qpnp_adc_tm_drv *adc_tm;
	struct qpnp_adc_drv *adc_qpnp;
	int32_t count_adc_channel_list = 0, rc, sen_idx = 0;
	u8 thr_init = 0;

	if (!node)
		return -EINVAL;

	if (qpnp_adc_tm) {
		pr_err("adc-tm already in use\n");
		return -EBUSY;
	}

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	adc_tm = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_adc_tm_drv) +
			(count_adc_channel_list *
			sizeof(struct qpnp_adc_tm_sensor)),
				GFP_KERNEL);
	if (!adc_tm) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	qpnp_adc_tm = adc_tm;
	adc_qpnp = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_adc_drv),
			GFP_KERNEL);
	if (!adc_qpnp) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		rc = -ENOMEM;
		goto fail;
	}

	adc_tm->adc = adc_qpnp;

	rc = qpnp_adc_get_devicetree_data(spmi, adc_tm->adc);
	if (rc) {
		dev_err(&spmi->dev, "failed to read device tree\n");
		goto fail;
	}
	mutex_init(&adc_tm->adc->adc_lock);

	/* Register the ADC peripheral interrupt */
	adc_tm->adc->adc_high_thr_irq = spmi_get_irq_byname(spmi,
						NULL, "high-thr-en-set");
	if (adc_tm->adc->adc_high_thr_irq < 0) {
		pr_err("Invalid irq\n");
		rc = -ENXIO;
		goto fail;
	}

	adc_tm->adc->adc_low_thr_irq = spmi_get_irq_byname(spmi,
						NULL, "low-thr-en-set");
	if (adc_tm->adc->adc_low_thr_irq < 0) {
		pr_err("Invalid irq\n");
		rc = -ENXIO;
		goto fail;
	}

	rc = devm_request_irq(&spmi->dev, adc_tm->adc->adc_irq_eoc,
				qpnp_adc_tm_isr, IRQF_TRIGGER_RISING,
				"qpnp_adc_tm_interrupt", adc_tm);
	if (rc) {
		dev_err(&spmi->dev,
			"failed to request adc irq with error %d\n", rc);
		goto fail;
	} else {
		enable_irq_wake(adc_tm->adc->adc_irq_eoc);
	}

	rc = devm_request_irq(&spmi->dev, adc_tm->adc->adc_high_thr_irq,
				qpnp_adc_tm_high_thr_isr,
		IRQF_TRIGGER_RISING, "qpnp_adc_tm_high_interrupt", adc_tm);
	if (rc) {
		dev_err(&spmi->dev, "failed to request adc irq\n");
		goto fail;
	} else {
		enable_irq_wake(adc_tm->adc->adc_high_thr_irq);
	}

	rc = devm_request_irq(&spmi->dev, adc_tm->adc->adc_low_thr_irq,
				qpnp_adc_tm_low_thr_isr,
		IRQF_TRIGGER_RISING, "qpnp_adc_tm_low_interrupt", adc_tm);
	if (rc) {
		dev_err(&spmi->dev, "failed to request adc irq\n");
		goto fail;
	} else {
		enable_irq_wake(adc_tm->adc->adc_low_thr_irq);
	}

	for_each_child_of_node(node, child) {
		char name[25];
		int btm_channel_num;
		bool thermal_node = false;

		rc = of_property_read_u32(child,
				"qcom,btm-channel-number", &btm_channel_num);
		if (rc) {
			pr_err("Invalid btm channel number\n");
			goto fail;
		}
		adc_tm->sensor[sen_idx].btm_channel_num = btm_channel_num;
		adc_tm->sensor[sen_idx].vadc_channel_num =
				adc_tm->adc->adc_channels[sen_idx].channel_num;
		adc_tm->sensor[sen_idx].sensor_num = sen_idx;
		pr_debug("btm_chan:%x, vadc_chan:%x\n", btm_channel_num,
			adc_tm->adc->adc_channels[sen_idx].channel_num);
		thermal_node = of_property_read_bool(child,
					"qcom,thermal-node");
		if (thermal_node) {
			/* Register with the thermal zone */
			pr_debug("thermal node%x\n", btm_channel_num);
			adc_tm->sensor[sen_idx].mode = THERMAL_DEVICE_DISABLED;
			adc_tm->sensor[sen_idx].thermal_node = true;
			snprintf(name, sizeof(name),
				adc_tm->adc->adc_channels[sen_idx].name);
			adc_tm->sensor[sen_idx].meas_interval =
				QPNP_ADC_TM_MEAS_INTERVAL;
			adc_tm->sensor[sen_idx].low_thr =
						QPNP_ADC_TM_M0_LOW_THR;
			adc_tm->sensor[sen_idx].high_thr =
						QPNP_ADC_TM_M0_HIGH_THR;
			adc_tm->sensor[sen_idx].tz_dev =
				thermal_zone_device_register(name,
				ADC_TM_TRIP_NUM,
				&adc_tm->sensor[sen_idx],
				&qpnp_adc_tm_thermal_ops, 0, 0, 0, 0);
			if (IS_ERR(adc_tm->sensor[sen_idx].tz_dev))
				pr_err("thermal device register failed.\n");
		}
		INIT_WORK(&adc_tm->sensor[sen_idx].work, notify_adc_tm_fn);
		sen_idx++;
	}
	adc_tm->max_channels_available = count_adc_channel_list;
	dev_set_drvdata(&spmi->dev, adc_tm);
	rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_HIGH_THR_INT_EN, thr_init);
	if (rc < 0) {
		pr_err("high thr init failed\n");
		goto fail;
	}

	rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_LOW_THR_INT_EN, thr_init);
	if (rc < 0) {
		pr_err("low thr init failed\n");
		goto fail;
	}

	rc = qpnp_adc_tm_write_reg(QPNP_ADC_TM_MULTI_MEAS_EN, thr_init);
	if (rc < 0) {
		pr_err("multi meas en failed\n");
		goto fail;
	}

	adc_tm->usb_id_ext_pull_up = of_property_read_bool(node,
						"usb-id-ext-pull-up");

	adc_tm->adc_tm_initialized = true;

	pr_debug("OK\n");
	return 0;
fail:
	qpnp_adc_tm = NULL;
	return rc;
}

static void qpnp_adc_tm_remove(struct spmi_device *spmi)
{
	struct qpnp_adc_tm_drv *adc_tm = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		thermal_zone_device_unregister(adc_tm->sensor[i].tz_dev);
		i++;
	}

	adc_tm->adc_tm_initialized = false;
	dev_set_drvdata(&spmi->dev, NULL);
}

static const struct of_device_id qpnp_adc_tm_match_table[] = {
	{	.compatible = "qcom,qpnp-adc-tm" },
	{}
};

static struct spmi_driver qpnp_adc_tm_driver = {
	.driver		= {
		.name	= "qcom,qpnp-adc-tm",
		.of_match_table = qpnp_adc_tm_match_table,
	},
	.probe		= qpnp_adc_tm_probe,
	.remove		= qpnp_adc_tm_remove,
};

static int __init qpnp_adc_tm_init(void)
{
	return spmi_driver_register(&qpnp_adc_tm_driver);
}
module_init(qpnp_adc_tm_init);

static void __exit qpnp_adc_tm_exit(void)
{
	spmi_driver_unregister(&qpnp_adc_tm_driver);
}
module_exit(qpnp_adc_tm_exit);

MODULE_DESCRIPTION("QPNP PMIC ADC Threshold Monitoring driver");
MODULE_LICENSE("GPL v2");
