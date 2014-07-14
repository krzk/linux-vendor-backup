/*
 * Samsung EXYNOS5/EXYNOS3250 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef FIMC_IS_BACKEND_H_
#define FIMC_IS_BACKEND_H_

#include "fimc-is-fw.h"
#include <linux/types.h>


/*
 * MCUCTL Shared register types
 */
#define MCUCTL_SHARED_REG_INVAL 0
#define MCUCTL_SHARED_REG_R	(1 << 0)
#define MCUCTL_SHARED_REG_W	(1 << 1)
#define MCUCTL_SHARED_REG_RW	(MCUCTL_SHARED_REG_R | MCUCTL_SHARED_REG_W)

/*
 * Set of supported MCUCTL shared registers.
 * Might be extended if needed.
 */
enum{
        /* | COMMAND ID | SENSOR/GROUP ID | PARAMS |		  */
        MCUCTL_HIC_REG,
        /* | NUMBER 	|			    		  */
        MCUCTL_PW_DONW_REG,
        /* | IFLAGS	| COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_IHC_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_3AA0C_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_3AA1C_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_ISP_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_SCC_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_DNR_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_DIS_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_SCP_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_YUV_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_SHOT_REG,
        /* | IFLAGS     | COMMAND ID | SENSOR/GROUP ID | PARAMS  | */
        MCUCTL_META_REG,
        /* | SENSOR3    | SENSOR2    | SENSOR1         | SENSOR0 | */
        MCUCTL_FRAME_COUNT_REG,
        MCUCTL_END_REG
};

#define MCUCTL_SREG_SIZE	4

/**
 * struct mcuctl_sreg_desc - MCUCTL Shared register descriptor
 * @base_offset: Base offset of logical MCUCTL shared registers range
 * 		 relative to the begining of MCUCTL shared area
 * @range: Number of 32-bits registers within the range
 * @data_range: Number of 32-bits registers dedicated for communication
 * 		parameters
 * @type: type of the registers set: MCUCTL_SHARED_REG_INVAL denotes given set
 * 	  is not supported. MCUCTL_SHARED_REG_R, MCUCTL_SHARED_REG_W indicate
 * 	  readable / writable registers.
 */
struct mcuctl_sreg_desc {
        unsigned long base_offset;
        unsigned int range;
        unsigned int data_range;
        unsigned int type;
};

/*
 * MCUCTL Shared regs layout
 * Single set of logically related registers:
 * OFFSET: | 0x00 | : INTERRUPT FLAGS *OPTIONAL*
 * OFFSET: | 0x04 | : COMMAND ID
 * OFFSET: | 0x08 | : SENSOR  ID / GROUP ID
 * OFFSET: | 0x0c | : COMMAND PARAMETERS (max.4)
 */

#define MCUCTL_SHARED_REG(offset, size, data_size, reg_type)\
{							    \
        .base_offset = offset,				    \
        .range 	     = size,				    \
        .data_range  = data_size,			    \
        .type        = reg_type,			    \
}

/**
 *  @brief Generic set of FIMC IS interrupts
 */
enum fimc_is_interrupt {
        INTR_GENERAL,
        INTR_ISP_DONE,
        INTR_3A0C_DONE,
        INTR_3A1C_DONE,
        INTR_SCC_DONE,
        INTR_DNR_DONE,
        INTR_DIS_DONE,
        INTR_SCP_DONE,
        INTR_ISP_YUV_DONE,
        INTR_META_DONE,
        INTR_SHOT_DONE,
        INTR_MAX_MAP

};

/**
 * @brief Unspecified firmware version
 */
#define FIMC_IS_FW_VER_UNKNOWN	0
/**
 * @brief Generic error code
 */
#define FIMC_IS_FW_INVALID (-EINVAL)

/**
 * @brief Map firmware specific interrupt source id
 * 	  to generic one
 */
#define FIMC_IS_FW_GET_INTR_SRC(fw_data, __intr_src)	\
        ((__intr_src) < (fw_data)->interrupt_map_size	\
                ? (fw_data)->interrupt_map[(__intr_src)]\
                : FIMC_IS_FW_INVALID)

/**
 * @brief Verify if given command is supported by current firmware
 */
#define FIMC_IS_FW_CMD_SUPPORT(fw_data, __cmd) 				\
({									\
        u64 __cmd_bitmap = 0; 						\
        unsigned int __bit = 0; 					\
        if ((__cmd) <= HIC_COMMAND_END) {				\
                __cmd_bitmap = (fw_data)->instruction_set.hic_bitmap;	\
                __bit = (__cmd) -1;					\
        }								\
        else if ((__cmd) <= IHC_COMMAND_END) { 				\
                __cmd_bitmap = (fw_data)->instruction_set.ihc_bitmap; 	\
                __bit = (__cmd) - IHC_COMMAND_BEGIN; 			\
        }								\
        __cmd_bitmap & (1ULL << __bit) ? 1 : 0;				\
})

/**
 * struct fimc_is_fw_commands - internal info on firmware support
 * 				for FIMC IS <-> Host communication commands
 * @hic_bitmap: bitmap of supported HIC commands
 * 		(each command (ID) is mapped to a corresponding bit)
 * @ihc_bitmap: bitmap of supported IHC commands
 * 		(each command (ID) is mapped to a corresponding bit,
 * 		starting with IHC_COMMAND_BEGIN mapped to first bit)
 *
 */
struct fimc_is_fw_commands{
        u64	hic_bitmap;
        u64	ihc_bitmap;
};


/**
 * struct fimc_is_fw_dta -  set of internal data used to control
 * 			    proper information flow between the
 * 			    driver itself and currently used FIMC IS
 * 			    firmware
 * @fw_version: reference firmware version
 * @mcuctl_sreg_map: map of MCUCTL shared registers
 * @instruction_set: bitmap of supported instructions
 * @interrupt_map: FIMC IS fw specific interrupts src map
 */
struct fimc_is_fw_data {
        unsigned int		 		 version;
        const struct fimc_is_fw_commands 	 instruction_set;
        const struct mcuctl_sreg_desc const	*mcuctl_sreg_map;
        const unsigned int const		*interrupt_map;
        unsigned int				 interrupt_map_size;
};

void fimc_is_fw_set_default(struct fimc_is_fw_data **fw_data);
int fimc_is_fw_config(struct fimc_is_fw_data **fw_data, unsigned int fw_version);


int mcuctl_sreg_get_desc(struct fimc_is_fw_data *fw_data,
                         unsigned int mcuctl_sreg_id,
                         struct mcuctl_sreg_desc *mcuctl_sreg_desc);

int mcuctl_sreg_get_offset(struct fimc_is_fw_data *fw_data,
                           unsigned int mcuctl_sreg_id,
                           unsigned long * offset);

int mcuctl_sreg_get_range(struct fimc_is_fw_data *fw_data,
                          unsigned int mcuctl_sreg_id,
                          unsigned long *range);

int mcuctl_sreg_get_data_range(struct fimc_is_fw_data *fw_data,
                               unsigned int mcuctl_sreg_id,
                               unsigned int *data_range);

int mcuctl_sreg_is_valid(struct fimc_is_fw_data *fw_data,
                         unsigned int mcuctl_sreg_id);


#endif /*FIMC_IS_BACKEND_H_*/
