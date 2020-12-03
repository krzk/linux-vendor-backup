#include <mach/hardware.h>
#include <plat/mux.h>

#ifndef _SWITCH_OMAP_GPIO_H_
#define _SWITCH_OMAP_GPIO_H_

#define HEADSET_DISCONNET			0
#define HEADSET_3POLE				2 
#define HEADSET_4POLE_WITH_MIC	1

extern short int get_headset_status(void);

#define EAR_MIC_BIAS_GPIO OMAP_GPIO_EAR_MICBIAS_EN
#define EAR_KEY_GPIO OMAP_GPIO_EAR_KEY
#define EAR_DET_GPIO OMAP_GPIO_JACK_NINT
#if ( defined( CONFIG_MACH_SAMSUNG_LATONA ) && ( (CONFIG_SAMSUNG_EMU_HW_REV == 0) || (CONFIG_SAMSUNG_REL_HW_REV >= 2) ) || defined(CONFIG_CHN_KERNEL_STE_LATONA))  // emul 0.0
#define EAR_DETECT_INVERT_ENABLE 1
#else
#define EAR_DETECT_INVERT_ENABLE 0
#endif

#define EAR_KEY_INVERT_ENABLE 1

#if ( defined( CONFIG_MACH_SAMSUNG_LATONA ) && ( CONFIG_SAMSUNG_REL_HW_REV >= 1 ) && defined(CONFIG_CHN_KERNEL_STE_LATONA) ) // real 0.1
//#define EAR_ADC_SEL_GPIO OMAP_GPIO_EARPATH_SEL
//#define USE_ADC_SEL_GPIO 0
#endif

#if ( defined( CONFIG_MACH_SAMSUNG_HERON ) && ( CONFIG_SAMSUNG_REL_HW_REV >= 1 ) ) // real 0.1
#define EAR_ADC_SEL_GPIO OMAP_GPIO_EARPATH_SEL
#define USE_ADC_SEL_GPIO 0 // 1	Heekwon Ko EARPATH_SEL is not used.
#endif

#if ( defined( CONFIG_MACH_SAMSUNG_LATONA ) && ( CONFIG_SAMSUNG_REL_HW_REV == 1 ) ) // real 0.1
#define USE_ONLY_AS_4POLE	1
#endif

#if ( defined( CONFIG_MACH_SAMSUNG_HERON ) && ( CONFIG_SAMSUNG_REL_HW_REV == 1 ) ) // real 0.1
#define USE_ONLY_AS_4POLE	1
#endif



#endif//_SWITCH_OMAP_GPIO_H_
