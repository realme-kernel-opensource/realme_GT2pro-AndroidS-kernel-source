/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SOC_QCOM_LLCC_PERFMON_H_
#define _SOC_QCOM_LLCC_PERFMON_H_

#define LLCC_VER2			(21)
#define VER_CHK(v)			(v == LLCC_VER2)

/* COMMON */
#define LLCC_COMMON_HW_INFO(v)		((v == LLCC_VER2) ? 0x34000 : 0x30000)
#define LLCC_COMMON_STATUS0(v)		((v == LLCC_VER2) ? 0x3400C : 0x3000C)

/* FEAC */
#define FEAC_PROF_FILTER_0_CFG3(v)	(VER_CHK(v) ? 0x4300C : 0x03700C)
#define FEAC_PROF_FILTER_0_CFG5(v)	(VER_CHK(v) ? 0x43014 : 0x037014)
#define FEAC_PROF_FILTER_0_CFG6(v)	(VER_CHK(v) ? 0x43018 : 0x037018)
#define FEAC_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x43060 : 0x037060) \
					+ 4 * (n))
#define FEAC_PROF_CFG(v)		(VER_CHK(v) ? 0x430A0 : 0x0370A0)

/* FERC */
#define FERC_PROF_FILTER_0_CFG0(v)	(VER_CHK(v) ? 0x49000 : 0x03B000)
#define FERC_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x49020 : 0x03B020) \
					+ 4 * (n))
#define FERC_PROF_CFG(v)		(VER_CHK(v) ? 0x49060 : 0x03B060)

/* FEWC */
#define FEWC_PROF_FILTER_0_CFG0(v)	(VER_CHK(v) ? 0x39000 : 0x033000)
#define FEWC_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x39020 : 0x033020) \
					+ 4 * (n))

/* BEAC */
#define BEAC0_PROF_FILTER_0_CFG5(v)	(VER_CHK(v) ? 0x61014 : 0x049014)
#define BEAC0_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x61040 : 0x049040) \
					+ 4 * (n))
#define BEAC0_PROF_CFG(v)		(VER_CHK(v) ? 0x61080 : 0x049080)
#define BEAC_INST_OFF			(0x4000)

/* BERC */
#define BERC_PROF_FILTER_0_CFG0(v)	(VER_CHK(v) ? 0x3D000 : 0x039000)
#define BERC_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x3D020 : 0x039020) \
					+ 4 * (n))
#define BERC_PROF_CFG(v)		(VER_CHK(v) ? 0x3D060 : 0x039060)

/* TRP */
#define TRP_PROF_FILTER_0_CFG1		(0x024004)
#define TRP_PROF_FILTER_0_CFG2		(0x024008)
#define TRP_PROF_EVENT_n_CFG(n)		(0x024020 + 4 * (n))
#define TRP_SCID_n_STATUS(n)		(0x000004 + 0x1000 * (n))

/* DRP */
#define DRP_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x51010 : 0x044010) \
					+ 4 * (n))
#define DRP_PROF_CFG(v)			(VER_CHK(v) ? 0x51050 : 0x044050)

/* PMGR */
#define PMGR_PROF_EVENT_n_CFG(v, n)	((VER_CHK(v) ? 0x4D000 : 0x03F000) \
					+ 4 * (n))

#define PERFMON_COUNTER_n_CONFIG(v, n)	((VER_CHK(v) ? 0x36020 : 0x031020) \
					+ 4 * (n))
#define PERFMON_MODE(v)			(VER_CHK(v) ? 0x3600C : 0x03100C)
#define PERFMON_DUMP(v)			(VER_CHK(v) ? 0x36010 : 0x031010)
#define LLCC_COUNTER_n_VALUE(v, n)	((VER_CHK(v) ? 0x36060 : 0x031060) \
					+ 4 * (n))

#define EVENT_NUM_MAX			(128)
#define SCID_MAX			(32)

/* Perfmon */
#define CLEAR_ON_ENABLE			BIT(31)
#define CLEAR_ON_DUMP			BIT(30)
#define FREEZE_ON_SATURATE		BIT(29)
#define CHAINING_EN			BIT(28)
#define COUNT_CLOCK_EVENT		BIT(24)

#define EVENT_SELECT_SHIFT		(16)
#define PERFMON_EVENT_SELECT_MASK	GENMASK(EVENT_SELECT_SHIFT + 4,\
						EVENT_SELECT_SHIFT)
#define PORT_SELECT_SHIFT		(0)
#define PERFMON_PORT_SELECT_MASK	GENMASK(PORT_SELECT_SHIFT + 3,\
						PORT_SELECT_SHIFT)

#define MANUAL_MODE			(0)
#define TIMED_MODE			(1)
#define TRIGGER_MODE			(2)
#define MONITOR_EN_SHIFT		(15)
#define MONITOR_EN			BIT(MONITOR_EN_SHIFT)
#define PERFMON_MODE_MONITOR_EN_MASK	GENMASK(MONITOR_EN_SHIFT + 0,\
						MONITOR_EN_SHIFT)
#define MONITOR_MODE_SHIFT		(0)
#define PERFMON_MODE_MONITOR_MODE_MASK	GENMASK(MONITOR_MODE_SHIFT + 0,\
						MONITOR_MODE_SHIFT)

#define MONITOR_DUMP			BIT(0)

/* COMMON */
#define BYTE_SCALING			(1024)
#define BEAT_SCALING			(32)
#define LB_CNT_SHIFT			(28)
#define LB_CNT_MASK			GENMASK(LB_CNT_SHIFT + 3, \
						LB_CNT_SHIFT)
#define NUM_MC_SHIFT			(10)
#define NUM_MC_MASK			GENMASK(NUM_MC_SHIFT + 1, \
						NUM_MC_SHIFT)

#define BYTE_SCALING_SHIFT		(16)
#define PROF_CFG_BYTE_SCALING_MASK	GENMASK(BYTE_SCALING_SHIFT + 11,\
						BYTE_SCALING_SHIFT)
#define BEAT_SCALING_SHIFT		(8)
#define PROF_CFG_BEAT_SCALING_MASK	GENMASK(BEAT_SCALING_SHIFT + 7,\
						BEAT_SCALING_SHIFT)
#define PROF_EN_SHIFT			(0)
#define PROF_EN				BIT(PROF_EN_SHIFT)
#define PROF_CFG_EN_MASK		GENMASK(PROF_EN_SHIFT + 0,\
						PROF_EN_SHIFT)

#define FILTER_EN_SHIFT			(31)
#define FILTER_EN			BIT(FILTER_EN_SHIFT)
#define FILTER_EN_MASK			GENMASK(FILTER_EN_SHIFT + 0,\
						FILTER_EN_SHIFT)
#define FILTER_0			(0)
#define FILTER_0_MASK			GENMASK(FILTER_0 + 0, \
						FILTER_0)
#define FILTER_1			(1)
#define FILTER_1_MASK			GENMASK(FILTER_1 + 0, \
						FILTER_1)

#define FILTER_SEL_SHIFT		(16)
#define FILTER_SEL_MASK			GENMASK(FILTER_SEL_SHIFT + 0,\
						FILTER_SEL_SHIFT)
#define EVENT_SEL_SHIFT			(0)
#define EVENT_SEL_MASK			GENMASK(EVENT_SEL_SHIFT + 5,\
						EVENT_SEL_SHIFT)
#define EVENT_SEL_MASK7			GENMASK(EVENT_SEL_SHIFT + 6,\
						EVENT_SEL_SHIFT)

#define CACHEALLOC_MASK_SHIFT		(16)
#define CACHEALLOC_MASK_MASK		GENMASK(CACHEALLOC_MASK_SHIFT + 3, \
					CACHEALLOC_MASK_SHIFT)
#define CACHEALLOC_MATCH_SHIFT		(12)
#define CACHEALLOC_MATCH_MASK		GENMASK(CACHEALLOC_MATCH_SHIFT + 3, \
					CACHEALLOC_MATCH_SHIFT)
#define OPCODE_MASK_SHIFT		(28)
#define OPCODE_MASK_MASK		GENMASK(OPCODE_MASK_SHIFT + 3, \
					OPCODE_MASK_SHIFT)
#define OPCODE_MATCH_SHIFT		(24)
#define OPCODE_MATCH_MASK		GENMASK(OPCODE_MATCH_SHIFT + 3, \
					OPCODE_MATCH_SHIFT)
#define MID_MASK_SHIFT			(16)
#define MID_MASK_MASK			GENMASK(MID_MASK_SHIFT + 15, \
						MID_MASK_SHIFT)
#define MID_MATCH_SHIFT			(0)
#define MID_MATCH_MASK			GENMASK(MID_MATCH_SHIFT + 15, \
						MID_MATCH_SHIFT)
#define SCID_MASK_SHIFT			(16)
#define SCID_MASK_MASK			GENMASK(SCID_MASK_SHIFT + 15, \
						SCID_MASK_SHIFT)
#define SCID_MATCH_SHIFT		(0)
#define SCID_MATCH_MASK			GENMASK(SCID_MATCH_SHIFT + 15, \
						SCID_MATCH_SHIFT)
#define SCID_MULTI_MATCH_SHIFT		(0)
#define SCID_MULTI_MATCH_MASK		GENMASK(SCID_MULTI_MATCH_SHIFT + 31, \
						SCID_MULTI_MATCH_SHIFT)
#define PROFTAG_MASK_SHIFT		(2)
#define PROFTAG_MASK_MASK		GENMASK(PROFTAG_MASK_SHIFT + 1,\
						PROFTAG_MASK_SHIFT)
#define PROFTAG_MATCH_SHIFT		(0)
#define PROFTAG_MATCH_MASK		GENMASK(PROFTAG_MATCH_SHIFT + 1,\
						PROFTAG_MATCH_SHIFT)
/* FEAC */
#define FEAC_SCALING_FILTER_SEL_SHIFT	(2)
#define FEAC_SCALING_FILTER_SEL_MASK	GENMASK(FEAC_SCALING_FILTER_SEL_SHIFT \
					+ 0, \
					FEAC_SCALING_FILTER_SEL_SHIFT)
#define FEAC_SCALING_FILTER_EN_SHIFT	(1)
#define FEAC_SCALING_FILTER_EN		BIT(FEAC_SCALING_FILTER_EN_SHIFT)
#define FEAC_SCALING_FILTER_EN_MASK	GENMASK(FEAC_SCALING_FILTER_EN_SHIFT \
					+ 0, \
					FEAC_SCALING_FILTER_EN_SHIFT)

#define FEAC_WR_BEAT_FILTER_SEL_SHIFT	(29)
#define FEAC_WR_BEAT_FILTER_SEL_MASK	GENMASK(FEAC_WR_BEAT_FILTER_SEL_SHIFT \
					+ 0, \
					FEAC_WR_BEAT_FILTER_SEL_SHIFT)
#define FEAC_WR_BEAT_FILTER_EN_SHIFT	(28)
#define FEAC_WR_BEAT_FILTER_EN_MASK	GENMASK(FEAC_WR_BEAT_FILTER_EN_SHIFT \
					+ 0, \
					FEAC_WR_BEAT_FILTER_EN_SHIFT)
#define FEAC_WR_BEAT_FILTER_EN		BIT(FEAC_WR_BEAT_FILTER_EN_SHIFT)
#define FEAC_WR_BYTE_FILTER_SEL_SHIFT	(6)
#define FEAC_WR_BYTE_FILTER_SEL_MASK	GENMASK(FEAC_WR_BYTE_FILTER_SEL_SHIFT \
					+ 0, \
					FEAC_WR_BYTE_FILTER_SEL_SHIFT)
#define FEAC_WR_BYTE_FILTER_EN_SHIFT	(5)
#define FEAC_WR_BYTE_FILTER_EN_MASK	GENMASK(FEAC_WR_BYTE_FILTER_EN_SHIFT \
					+ 0, \
					FEAC_WR_BYTE_FILTER_EN_SHIFT)
#define FEAC_WR_BYTE_FILTER_EN		BIT(FEAC_WR_BYTE_FILTER_EN_SHIFT)
#define FEAC_RD_BEAT_FILTER_SEL_SHIFT	(4)
#define FEAC_RD_BEAT_FILTER_SEL_MASK	GENMASK(FEAC_RD_BEAT_FILTER_SEL_SHIFT \
					+ 0, \
					FEAC_RD_BEAT_FILTER_SEL_SHIFT)
#define FEAC_RD_BEAT_FILTER_EN_SHIFT	(3)
#define FEAC_RD_BEAT_FILTER_EN_MASK	GENMASK(FEAC_RD_BEAT_FILTER_EN_SHIFT \
					+ 0, \
					FEAC_RD_BEAT_FILTER_EN_SHIFT)
#define FEAC_RD_BEAT_FILTER_EN		BIT(FEAC_RD_BEAT_FILTER_EN_SHIFT)
#define FEAC_RD_BYTE_FILTER_SEL_SHIFT	(2)
#define FEAC_RD_BYTE_FILTER_SEL_MASK	GENMASK(FEAC_RD_BYTE_FILTER_SEL_SHIFT \
					+ 0, \
					FEAC_RD_BYTE_FILTER_SEL_SHIFT)
#define FEAC_RD_BYTE_FILTER_EN_SHIFT	(1)
#define FEAC_RD_BYTE_FILTER_EN_MASK	GENMASK(FEAC_RD_BYTE_FILTER_EN_SHIFT \
					+ 0, \
					FEAC_RD_BYTE_FILTER_EN_SHIFT)
#define FEAC_RD_BYTE_FILTER_EN		BIT(FEAC_RD_BYTE_FILTER_EN_SHIFT)
/* BEAC */
#define BEAC_PROFTAG_MASK_SHIFT		(14)
#define BEAC_PROFTAG_MASK_MASK		GENMASK(BEAC_PROFTAG_MASK_SHIFT + 1,\
						BEAC_PROFTAG_MASK_SHIFT)
#define BEAC_PROFTAG_MATCH_SHIFT	(12)
#define BEAC_PROFTAG_MATCH_MASK		GENMASK(BEAC_PROFTAG_MATCH_SHIFT + 1,\
						BEAC_PROFTAG_MATCH_SHIFT)
#define BEAC_MC_PROFTAG_SHIFT		(1)
#define BEAC_MC_PROFTAG_MASK		GENMASK(BEAC_MC_PROFTAG_SHIFT + 1,\
					BEAC_MC_PROFTAG_SHIFT)
#define BEAC_WR_BEAT_FILTER_SEL_SHIFT	(6)
#define BEAC_WR_BEAT_FILTER_SEL_MASK	GENMASK(BEAC_WR_BEAT_FILTER_SEL_SHIFT \
					+ 0, \
					BEAC_WR_BEAT_FILTER_SEL_SHIFT)
#define BEAC_WR_BEAT_FILTER_EN_SHIFT	(5)
#define BEAC_WR_BEAT_FILTER_EN_MASK	GENMASK(BEAC_WR_BEAT_FILTER_EN_SHIFT \
					+ 0, \
					BEAC_WR_BEAT_FILTER_EN_SHIFT)
#define BEAC_WR_BEAT_FILTER_EN		BIT(BEAC_WR_BEAT_FILTER_EN_SHIFT)
#define BEAC_RD_BEAT_FILTER_SEL_SHIFT	(4)
#define BEAC_RD_BEAT_FILTER_SEL_MASK	GENMASK(BEAC_RD_BEAT_FILTER_SEL_SHIFT \
					+ 0, \
					BEAC_RD_BEAT_FILTER_SEL_SHIFT)
#define BEAC_RD_BEAT_FILTER_EN_SHIFT	(3)
#define BEAC_RD_BEAT_FILTER_EN_MASK	GENMASK(BEAC_RD_BEAT_FILTER_EN_SHIFT \
					+ 0, \
					BEAC_RD_BEAT_FILTER_EN_SHIFT)
#define BEAC_RD_BEAT_FILTER_EN		BIT(BEAC_RD_BEAT_FILTER_EN_SHIFT)
/* TRP */
#define TRP_SCID_MATCH_SHIFT		(0)
#define TRP_SCID_MATCH_MASK		GENMASK(TRP_SCID_MATCH_SHIFT + 4,\
						TRP_SCID_MATCH_SHIFT)
#define TRP_SCID_MASK_SHIFT		(8)
#define TRP_SCID_MASK_MASK		GENMASK(TRP_SCID_MASK_SHIFT + 4,\
						TRP_SCID_MASK_SHIFT)
#define TRP_WAY_ID_MATCH_SHIFT		(16)
#define TRP_WAY_ID_MATCH_MASK		GENMASK(TRP_WAY_ID_MATCH_SHIFT + 3,\
						TRP_WAY_ID_MATCH_SHIFT)
#define TRP_WAY_ID_MASK_SHIFT		(20)
#define TRP_WAY_ID_MASK_MASK		GENMASK(TRP_WAY_ID_MASK_SHIFT + 3,\
						TRP_WAY_ID_MASK_SHIFT)
#define TRP_PROFTAG_MATCH_SHIFT		(24)
#define TRP_PROFTAG_MATCH_MASK		GENMASK(TRP_PROFTAG_MATCH_SHIFT + 1,\
						TRP_PROFTAG_MATCH_SHIFT)
#define TRP_PROFTAG_MASK_SHIFT		(28)
#define TRP_PROFTAG_MASK_MASK		GENMASK(TRP_PROFTAG_MASK_SHIFT + 1,\
						TRP_PROFTAG_MASK_SHIFT)

#define TRP_SCID_STATUS_ACTIVE_SHIFT		(0)
#define TRP_SCID_STATUS_ACTIVE_MASK		GENMASK( \
						TRP_SCID_STATUS_ACTIVE_SHIFT \
						+ 0, \
						TRP_SCID_STATUS_ACTIVE_SHIFT)
#define TRP_SCID_STATUS_DEACTIVE_SHIFT		(1)
#define TRP_SCID_STATUS_CURRENT_CAP_SHIFT	(16)
#define TRP_SCID_STATUS_CURRENT_CAP_MASK	GENMASK( \
					TRP_SCID_STATUS_CURRENT_CAP_SHIFT \
					+ 14, \
					TRP_SCID_STATUS_CURRENT_CAP_SHIFT)

#define MAJOR_VER_MASK			(0xFF000000)
#define BRANCH_MASK			(0x00FF0000)
#define MINOR_MASK			(0x0000FF00)
#define LLCC_VERSION_1			(0x01010200)
#define LLCC_VERSION_2			(0x02000000)
#define LLCC_VERSION_3			(0x03000000)
#define	MAJOR_REV_NO(v)			((v & MAJOR_VER_MASK) >> 24)
#define	BRANCH_NO(v)			((v & BRANCH_MASK) >> 16)
#define	MINOR_NO(v)			((v & MINOR_MASK) >> 8)
#define REV_0				(0x0)
#define REV_1				(0x1)
#define REV_2				(0x2)
#define BANK_OFFSET			(0x80000)

#endif /* _SOC_QCOM_LLCC_PERFMON_H_ */
