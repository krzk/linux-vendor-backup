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
#include "fimc-is-backend.h"
#include "fimc-is.h"
#include "fimc-is-cmd.h"
#include "fimc-is-fw.h"

/**
 * @brief Default set of MCUCTL_HIC_REG and MCUCTL_IHC_REG
 *
 * This is required as those two sets of registers are necessary
 * to properly initialize the communcication with FIMC IS
 * Those to are basic (mandatory) registers and their offsets
 * should ramin the same between differen firmware revisions
 */
static const struct mcuctl_sreg_desc mcuctl_sregs_default[] = {
        [MCUCTL_HIC_REG         ] =
                MCUCTL_SHARED_REG(0x00,  6, 4,MCUCTL_SHARED_REG_RW),
        [MCUCTL_IHC_REG         ] =
                MCUCTL_SHARED_REG(0x024, 7, 4, MCUCTL_SHARED_REG_RW),
};

/**
 * @breif Set of supported MCUCTL Shared registers
 * 	  for  FW FIMC_IS_FW_V120-driven FIMC IS
 */
static const struct mcuctl_sreg_desc fw_120_mcuctl_sregs[] = {
        [MCUCTL_HIC_REG		] =
                MCUCTL_SHARED_REG(0x000, 6, 4, MCUCTL_SHARED_REG_RW),
        [MCUCTL_IHC_REG		] =
                MCUCTL_SHARED_REG(0x024, 7, 4, MCUCTL_SHARED_REG_RW),
        [MCUCTL_ISP_REG		] =
                MCUCTL_SHARED_REG(0x04c, 4, 2, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SCC_REG		] =
                MCUCTL_SHARED_REG(0x06c, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_DNR_REG		] =
                MCUCTL_SHARED_REG(0x08c, 4, 2, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SCP_REG		] =
                MCUCTL_SHARED_REG(0x0ac, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_YUV_REG		] =
                MCUCTL_SHARED_REG(0x0c4, 4, 2, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SHOT_REG	] =
                MCUCTL_SHARED_REG(0x0d8, 4, 2, MCUCTL_SHARED_REG_RW),
        [MCUCTL_META_REG	] =
                MCUCTL_SHARED_REG(0x0ec, 3, 1, MCUCTL_SHARED_REG_RW),
        [MCUCTL_FRAME_COUNT_REG	] =
                MCUCTL_SHARED_REG(0x0fc, 1, 1, MCUCTL_SHARED_REG_RW),
};

/**
 * @brief Set of supported MCUCTL Shared registers
 * 	  for  FW FIMC_IS_FW_V130-driven FIMC IS
 */
static const struct mcuctl_sreg_desc fw_130_mcuctl_sregs[] = {
        [MCUCTL_HIC_REG		] =
                MCUCTL_SHARED_REG(0x00,  6, 4, MCUCTL_SHARED_REG_RW),
        [MCUCTL_IHC_REG		] =
                MCUCTL_SHARED_REG(0x024, 7, 4, MCUCTL_SHARED_REG_RW),
        [MCUCTL_3AA0C_REG	] =
                MCUCTL_SHARED_REG(0x04c, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_3AA1C_REG	] =
                MCUCTL_SHARED_REG(0x064, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SCC_REG		] =
                MCUCTL_SHARED_REG(0x080, 6, 4, MCUCTL_SHARED_REG_RW),
        [MCUCTL_DIS_REG		] =
                MCUCTL_SHARED_REG(0x08c, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SCP_REG		] =
                MCUCTL_SHARED_REG(0x0ac, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_SHOT_REG	] =
                MCUCTL_SHARED_REG(0x0d8, 5, 3, MCUCTL_SHARED_REG_RW),
        [MCUCTL_FRAME_COUNT_REG	] =
                MCUCTL_SHARED_REG(0x0f0, 4, 4, MCUCTL_SHARED_REG_RW),
};


#define MCUCTL_SHARED_REG_VALIDATE(regs_desc, reg_range_id, reg_type) \
        (((reg_range_id) < MCUCTL_END_REG) 			      \
        && (regs_desc[reg_range_id].type&reg_type))


/**
 * @brief Retrieve the descriptor for given set of
 * 	  MCUCTL shared registers.
 *
 * @param[in] mcuctl_desc_id       - MCUCTL shared registers map ID
 * @param[in] mcuctl_sreg_id       - the ID of shared regs set
 * @parma[in/out] mcuctl_sreg_desc - requested registers descriptor
 *
 * @return - 0 upon success, EINVAL otherwise
 */

int mcuctl_sreg_get_desc(struct fimc_is_fw_data *fw_data,
                         unsigned int mcuctl_sreg_id,
                         struct mcuctl_sreg_desc *mcuctl_shared_desc)
{
        const struct mcuctl_sreg_desc *regs_map = fw_data->mcuctl_sreg_map;

        if (!mcuctl_shared_desc || !regs_map)
                return -EINVAL;

        if ((MCUCTL_SHARED_REG_VALIDATE(regs_map, mcuctl_sreg_id,
                                        MCUCTL_SHARED_REG_RW))){
                memcpy(mcuctl_shared_desc, &regs_map[mcuctl_sreg_id],
                       sizeof(struct mcuctl_sreg_desc));
                return 0;
        }
        return -EINVAL;
}

static int mcuctl_sreg_get_info(struct fimc_is_fw_data *fw_data,
                                unsigned int mcuctl_sreg_id,
                                unsigned int info_offset,
                                unsigned long *info)
{
        const struct mcuctl_sreg_desc *regs_desc = fw_data->mcuctl_sreg_map;

        if (!info || !regs_desc)
                return -EINVAL;

        if (MCUCTL_SHARED_REG_VALIDATE(regs_desc,
                        mcuctl_sreg_id, MCUCTL_SHARED_REG_RW)){
                *info = *( ((char*)&regs_desc[mcuctl_sreg_id] )+ info_offset);
                return 0;
        }
        return -EINVAL;
}

/**
 * @breif Retrive the base offset of given set of registers
 * 	  relative to the begining of MCUCTL shared area
 *
 * @param[in] mcuctl_desc_id - MCUCTL shared registers map ID
 * @param[in] mcuctl_sreg_id - the ID of shared regs set
 * @param[in/out] offset     - base offset of requested set of registers
 *
 * @return - 0 upon success, EINVAL otherwise
 */
int mcuctl_sreg_get_offset(struct fimc_is_fw_data *fw_data,
                           unsigned int mcuctl_sreg_id,
                           unsigned long * offset)
{
        return mcuctl_sreg_get_info(fw_data,mcuctl_sreg_id,
                offsetof(struct mcuctl_sreg_desc, base_offset), offset);
}

/**
 * @brief Get the range of requested set of MCUCTL shared registers
 *
 * @parma[in] mcuctl_desc_id - MCUCTL shared registers map ID
 * @param[in] mcuctl_sreg_id - the ID of shared regs set
 * @param[in/out] range      - range of requested set of registers
 *
 * @return - 0 upon success, EINVAL otherwise
 */

int mcuctl_sreg_get_range(struct fimc_is_fw_data *fw_data,
                          unsigned int mcuctl_sreg_id,
                          unsigned long *range)
{

         return mcuctl_sreg_get_info(fw_data,mcuctl_sreg_id,
		offsetof(struct mcuctl_sreg_desc, range),
                range);

}

/**
 * @brief Get the range of data registers
 * 	  for given set of MCUCTL shared registers
 *
 * @param[in] mcuctl_desc_id - MCUCTL shared registers map ID
 * @param[in] mcuctl_sreg_id - the ID of shared regs set
 * @param[in/out] data_range - range of data registers from within
 * 			       requested set
 *
 * @return - 0 upon success, EINVAL otherwise
 */

int mcuctl_sreg_get_data_range(struct fimc_is_fw_data *fw_data,
                               unsigned int mcuctl_sreg_id,
                               unsigned int *data_range)
{
        return mcuctl_sreg_get_info(fw_data,mcuctl_sreg_id,
		offsetof(struct mcuctl_sreg_desc, data_range),
                (unsigned long*)data_range);
}

/**
 * @breif Validate requested shared registers ser
 *
 * @param[in] mcuctl_desc_id - MCUCTL shared registers map ID
 * @param[in] mcuctl_sreg_id - the ID of shared regs set
 *
 * @return -	1 case given registers set is valid (readable/writable)
 * 		0 otherwise
 */

int mcuctl_sreg_is_valid(struct fimc_is_fw_data *fw_data,
                         unsigned int mcuctl_sreg_id)
{
        const struct mcuctl_sreg_desc *regs_desc = fw_data->mcuctl_sreg_map;
        if (regs_desc) {
                return MCUCTL_SHARED_REG_VALIDATE(regs_desc,
                        mcuctl_sreg_id, MCUCTL_SHARED_REG_RW);
        }
        return 0;
}

static const unsigned int fw_intrsrc_map_default[] = {
        INTR_GENERAL,
};

static const unsigned int fw_120_intrsrc_map[] = {
        INTR_GENERAL,
        INTR_ISP_DONE,
        INTR_SCC_DONE,
        INTR_DNR_DONE,
        INTR_SCP_DONE,
        INTR_ISP_YUV_DONE,
        INTR_META_DONE,
        INTR_SHOT_DONE
};

static const unsigned int fw_130_intrsrc_map[] = {
        INTR_GENERAL,
        INTR_ISP_DONE,
        INTR_3A0C_DONE,
        INTR_3A1C_DONE,
        INTR_SCC_DONE,
        INTR_DIS_DONE,
        INTR_SCP_DONE,
        INTR_SHOT_DONE,
};

static struct fimc_is_fw_data fw_internal[] = {
        {
                .version = FIMC_IS_FW_VER_UNKNOWN,
                .mcuctl_sreg_map = mcuctl_sregs_default,
                .instruction_set = {
                        .hic_bitmap = 0x3ffffffffULL,
                        .ihc_bitmap = 0xffULL,
                },
                .interrupt_map = fw_intrsrc_map_default,
                .interrupt_map_size =  ARRAY_SIZE(fw_intrsrc_map_default),


        },
        {
                .version = FIMC_IS_FW_V120,
                .instruction_set =  {
                        .hic_bitmap = 0x7ff7fffULL,
                        .ihc_bitmap = 0x7fULL,
                },
                .mcuctl_sreg_map = fw_120_mcuctl_sregs,
                .interrupt_map  = fw_120_intrsrc_map,
                .interrupt_map_size = ARRAY_SIZE(fw_120_intrsrc_map),
        },{
                .version = FIMC_IS_FW_V130,
                .instruction_set =  {
                        .hic_bitmap = 0x3ffffffffULL,
                        .ihc_bitmap = 0xffULL,
                },
                .mcuctl_sreg_map = fw_130_mcuctl_sregs,
                .interrupt_map  = fw_130_intrsrc_map,
                .interrupt_map_size = ARRAY_SIZE(fw_130_intrsrc_map),
        },
        {}
};


void fimc_is_fw_set_default(struct fimc_is_fw_data **fw_data)
{
        BUG_ON(!fw_data);
        *fw_data = &fw_internal[0];
}

int fimc_is_fw_config(struct fimc_is_fw_data **fw_data, unsigned int fw_version)
{
        int i;
        /*
         * Drop the last digit from the firmware version :
         * e.g.: ver. 130-139 should fall back to 130
         */
        fw_version -= fw_version%10;

        for (i = 0; i < ARRAY_SIZE(fw_internal); ++i) {
                if (fw_version == fw_internal[i].version) {
                        *fw_data = &fw_internal[i];
                        return 0;
                }
        }
        return -EINVAL;
}
