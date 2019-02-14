/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __EXYNOS_BOOT_MODE_H
#define __EXYNOS_BOOT_MODE_H

/* high 24-bits is tag, low 8-bits is type */
#define REBOOT_FLAG		0x12345670
/* normal boot */
#define BOOT_NORMAL		(REBOOT_FLAG + 0)
/* enter bootloader thor download mode */
#define BOOT_BL_DOWNLOAD	(REBOOT_FLAG + 1)

#endif
