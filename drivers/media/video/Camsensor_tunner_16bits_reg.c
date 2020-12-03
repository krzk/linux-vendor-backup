#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <media/v4l2-int-device.h>

#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include "Camsensor_tunner_16bits_reg.h"

typedef struct tagCamReg16 {
	u8 addr;
	u8 value;
} CAM_REG16_T, *PCAM_REG16_T;

typedef struct tagCamReg16Package {
	CAM_REG16_T* reg16;
	int num;
	int nDynamicLoading;
} CAM_REG16_PACKAGE_T, *PCAM_REG16_PACKAGE_T;

#define DEFAULT_REG_SET_AMOUNT 100

typedef enum
{
	CAMERA_CMD_TYPE_NONE,
	CAMERA_CMD_TYPE_READ,
	CAMERA_CMD_TYPE_WRITE
} Camera_Cmd_Parser_type;

#define CAMTUNING_INIT	"TUNING_INIT"
#define CAMTUNING_VT_INIT	"TUNING_VT_INIT"
#define CAMTUNING_PREVIEW_SIZE_640_480	"TUNING_PREVIEW_SIZE_640_480"
#define CAMTUNING_PREVIEW_SIZE_320_240	"TUNING_PREVIEW_SIZE_320_240"
#define CAMTUNING_PREVIEW_SIZE_176_144	"TUNING_PREVIEW_SIZE_176_144"
#define CAMTUNING_BRIGHTNESS_M_4	"TUNING_BRIGHTNESS_M_4"
#define CAMTUNING_BRIGHTNESS_M_3	"TUNING_BRIGHTNESS_M_3"
#define CAMTUNING_BRIGHTNESS_M_2	"TUNING_BRIGHTNESS_M_2"
#define CAMTUNING_BRIGHTNESS_M_1	"TUNING_BRIGHTNESS_M_1"
#define CAMTUNING_BRIGHTNESS_0	"TUNING_BRIGHTNESS_0"
#define CAMTUNING_BRIGHTNESS_P_1	"TUNING_BRIGHTNESS_P_1"
#define CAMTUNING_BRIGHTNESS_P_2	"TUNING_BRIGHTNESS_P_2"
#define CAMTUNING_BRIGHTNESS_P_3	"TUNING_BRIGHTNESS_P_3"
#define CAMTUNING_BRIGHTNESS_P_4	"TUNING_BRIGHTNESS_P_4"
#define CAMTUNING_CONTRAST_M_5	"TUNING_CONTRAST_M_5"
#define CAMTUNING_CONTRAST_M_4	"TUNING_CONTRAST_M_4"
#define CAMTUNING_CONTRAST_M_3	"TUNING_CONTRAST_M_3"
#define CAMTUNING_CONTRAST_M_2	"TUNING_CONTRAST_M_2"
#define CAMTUNING_CONTRAST_M_1	"TUNING_CONTRAST_M_1"
#define CAMTUNING_CONTRAST_0	"TUNING_CONTRAST_0"
#define CAMTUNING_CONTRAST_P_1	"TUNING_CONTRAST_P_1"
#define CAMTUNING_CONTRAST_P_2	"TUNING_CONTRAST_P_2"
#define CAMTUNING_CONTRAST_P_3	"TUNING_CONTRAST_P_3"
#define CAMTUNING_CONTRAST_P_4	"TUNING_CONTRAST_P_4"
#define CAMTUNING_CONTRAST_P_5	"TUNING_CONTRAST_P_5"
#define CAMTUNING_WHITE_BALANCE_AUTO	"TUNING_WHITE_BALANCE_AUTO"
#define CAMTUNING_WHITE_BALANCE_DAYLIGHT	"TUNING_WHITE_BALANCE_DAYLIGHT"
#define CAMTUNING_WHITE_BALANCE_CLOUDY	"TUNING_WHITE_BALANCE_CLOUDY"
#define CAMTUNING_WHITE_BALANCE_FLUORESCENT	"TUNING_WHITE_BALANCE_FLUORESCENT"
#define CAMTUNING_WHITE_BALANCE_INCANDESCENT	"TUNING_WHITE_BALANCE_INCANDESCENT"
#define CAMTUNING_EFFECT_OFF	"TUNING_EFFECT_OFF"
#define CAMTUNING_EFFECT_GRAY	"TUNING_EFFECT_GRAY"
#define CAMTUNING_EFFECT_SEPIA	"TUNING_EFFECT_SEPIA"
#define CAMTUNING_EFFECT_NEGATIVE	"TUNING_EFFECT_NEGATIVE"
#define CAMTUNING_EFFECT_AQUA	"TUNING_EFFECT_AQUA"
#define CAMTUNING_BLUR_0	"TUNING_BLUR_0"
#define CAMTUNING_BLUR_P_1	"TUNING_BLUR_P_1"
#define CAMTUNING_BLUR_P_2	"TUNING_BLUR_P_2"
#define CAMTUNING_BLUR_P_3	"TUNING_BLUR_P_3"
#define CAMTUNING_AUTO_FPS	"TUNING_AUTO_FPS"
#define CAMTUNING_7_FPS	"TUNING_7_FPS"
#define CAMTUNING_10_FPS	"TUNING_10_FPS"
#define CAMTUNING_15_FPS	"TUNING_15_FPS"
#define CAMTUNING_FLIP_NONE	"TUNING_FLIP_NONE"
#define CAMTUNING_FLIP_MIRROR	"TUNING_FLIP_MIRROR"
#define CAMTUNING_VT_FLIP_NONE	"TUNING_VT_FLIP_NONE"
#define CAMTUNING_VT_FLIP_MIRROR	"TUNING_VT_FLIP_MIRROR"
#define CAMTUNING_DTP_ON	"TUNING_DTP_ON"
#define CAMTUNING_DTP_OFF	"TUNING_DTP_OFF"

extern CAM_REG16_PACKAGE_T CAM_REG16SET_INIT;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_VT_INIT;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_PREVIEW_SIZE_640_480;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_PREVIEW_SIZE_320_240;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_PREVIEW_SIZE_176_144;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_M_4;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_M_3;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_M_2;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_M_1;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_0;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_P_1;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_P_2;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_P_3;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BRIGHTNESS_P_4;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_M_5;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_M_4;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_M_3;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_M_2;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_M_1;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_0;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_P_1;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_P_2;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_P_3;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_P_4;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_CONTRAST_P_5;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_WHITE_BALANCE_AUTO;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_WHITE_BALANCE_DAYLIGHT;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_WHITE_BALANCE_CLOUDY;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_WHITE_BALANCE_FLUORESCENT;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_WHITE_BALANCE_INCANDESCENT;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_EFFECT_OFF;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_EFFECT_GRAY;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_EFFECT_SEPIA;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_EFFECT_NEGATIVE;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_EFFECT_AQUA;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BLUR_0;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BLUR_P_1;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BLUR_P_2;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_BLUR_P_3;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_AUTO_FPS;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_7_FPS;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_10_FPS;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_15_FPS;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_FLIP_NONE;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_FLIP_MIRROR;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_VT_FLIP_NONE;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_VT_FLIP_MIRROR;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_DTP_ON;
extern CAM_REG16_PACKAGE_T CAM_REG16SET_DTP_OFF;

u8 StringToHexFor16Bit(char *ss)
{
	int len = strlen (ss);
	int i = 0 ;
	u8 val = 0;   

	//To delete 0x or 0X characters
	if (!strncmp(L"0x", ss, 2) ||!strncmp(L"0X", ss, 2)) 
		ss[1] = '0';

	for (i = 0 ; i < len ; i++)
	{    
		if (ss[i] >='0' && ss[i] <='9')
			val = val*16 + (ss[i] - '0');
		else if(ss[i] >='a' && ss[i] <='f') 
			val = val*16 + (ss[i] - 'a'+10);
		else if(ss[i] >='A' && ss[i] <='F') 
			val = val*16 + (ss[i] - 'A'+10);
		else
			break;
	}

	return val;
}


void CreateReg16Buffer(PCAM_REG16_PACKAGE_T pRegPackage, CAM_REG16_T* pRegBuf, int iDataAmount)
{   
	CAM_REG16_T* pRegTemp = NULL;
	int i = 0;

	pRegTemp = (CAM_REG16_T*)kmalloc(iDataAmount * sizeof(CAM_REG16_T), GFP_KERNEL);

	if (NULL == pRegTemp)
	{
		printk("[CAMSENSOR_TUNNER_REG16] CreateRegBuffer() - malloc() failed!!! \n");
	}

	memcpy(pRegTemp, pRegBuf, iDataAmount * sizeof(CAM_REG16_T));

	pRegPackage->reg16= pRegTemp;
	pRegPackage->num= iDataAmount;
	pRegPackage->nDynamicLoading = 1;

	printk("########## [CAMSENSOR_TUNNER_REG16] CreateRegBuffer : iDataAmount = %d ##########\n", iDataAmount);
}

void DeleteReg16Buffer(PCAM_REG16_PACKAGE_T pRegPackage)
{
	if (pRegPackage->nDynamicLoading == 1)
	{
		if (pRegPackage->reg16 != NULL)
		{
			kfree(pRegPackage->reg16);
		}
	}

	pRegPackage->reg16 = NULL;
	pRegPackage->num = 0;
	pRegPackage->nDynamicLoading = 0;
}

void Copy2Reg16Buffer(char* pszStr, CAM_REG16_T* pRegBuf, int iDataAmount)
{
	if (0 == iDataAmount)
	{
		return;
	}

	if (!strcmp(pszStr, CAMTUNING_INIT)) {
		CreateReg16Buffer(&CAM_REG16SET_INIT, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_VT_INIT)) {
		CreateReg16Buffer(&CAM_REG16SET_VT_INIT, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_PREVIEW_SIZE_640_480)) {
		CreateReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_640_480, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_PREVIEW_SIZE_320_240)) {
		CreateReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_320_240, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_PREVIEW_SIZE_176_144)) {
		CreateReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_176_144, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_M_4)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_4, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_M_3)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_3, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_M_2)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_2, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_M_1)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_1, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_0)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_0, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_P_1)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_1, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_P_2)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_2, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_P_3)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_3, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BRIGHTNESS_P_4)) {
		CreateReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_4, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_M_5)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_M_5, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_M_4)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_M_4, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_M_3)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_M_3, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_M_2)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_M_2, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_M_1)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_M_1, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_0)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_0, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_P_1)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_P_1, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_P_2)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_P_2, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_P_3)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_P_3, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_P_4)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_P_4, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_CONTRAST_P_5)) {
		CreateReg16Buffer(&CAM_REG16SET_CONTRAST_P_5, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_WHITE_BALANCE_AUTO)) {
		CreateReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_AUTO, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_WHITE_BALANCE_DAYLIGHT)) {
		CreateReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_DAYLIGHT, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_WHITE_BALANCE_CLOUDY)) {
		CreateReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_CLOUDY, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_WHITE_BALANCE_FLUORESCENT)) {
		CreateReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_FLUORESCENT, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_WHITE_BALANCE_INCANDESCENT)) {
		CreateReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_INCANDESCENT, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_EFFECT_OFF)) {
		CreateReg16Buffer(&CAM_REG16SET_EFFECT_OFF, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_EFFECT_GRAY)) {
		CreateReg16Buffer(&CAM_REG16SET_EFFECT_GRAY, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_EFFECT_SEPIA)) {
		CreateReg16Buffer(&CAM_REG16SET_EFFECT_SEPIA, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_EFFECT_NEGATIVE)) {
		CreateReg16Buffer(&CAM_REG16SET_EFFECT_NEGATIVE, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_EFFECT_AQUA)) {
		CreateReg16Buffer(&CAM_REG16SET_EFFECT_AQUA, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BLUR_0)) {
		CreateReg16Buffer(&CAM_REG16SET_BLUR_0, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BLUR_P_1)) {
		CreateReg16Buffer(&CAM_REG16SET_BLUR_P_1, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BLUR_P_2)) {
		CreateReg16Buffer(&CAM_REG16SET_BLUR_P_2, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_BLUR_P_3)) {
		CreateReg16Buffer(&CAM_REG16SET_BLUR_P_3, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_AUTO_FPS)) {
		CreateReg16Buffer(&CAM_REG16SET_AUTO_FPS, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_7_FPS)) {
		CreateReg16Buffer(&CAM_REG16SET_7_FPS, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_10_FPS)) {
		CreateReg16Buffer(&CAM_REG16SET_10_FPS, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_15_FPS)) {
		CreateReg16Buffer(&CAM_REG16SET_15_FPS, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_FLIP_NONE)) {
		CreateReg16Buffer(&CAM_REG16SET_FLIP_NONE, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_FLIP_MIRROR)) {
		CreateReg16Buffer(&CAM_REG16SET_FLIP_MIRROR, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_VT_FLIP_NONE)) {
		CreateReg16Buffer(&CAM_REG16SET_VT_FLIP_NONE, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_VT_FLIP_MIRROR)) {
		CreateReg16Buffer(&CAM_REG16SET_VT_FLIP_MIRROR, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_DTP_ON)) {
		CreateReg16Buffer(&CAM_REG16SET_DTP_ON, pRegBuf, iDataAmount);
	} else if (!strcmp(pszStr, CAMTUNING_DTP_OFF)) {
		CreateReg16Buffer(&CAM_REG16SET_DTP_OFF, pRegBuf, iDataAmount);
	}
}


#define NONE_MODE       		0
#define READ_MODE       		1
#define WRITE_MODE     		2

#define REG_SET_COUNT		100
#define MAX_PATH                    256
#define CAMIF_CONFIGURE_FILE			"/sdcard/CamTuning_16Bit.txt"

/*
#define START_STR		"CAM_REG16_T SR030PC40_"

void Load16BitTuningFile_header()
{
	char*        buffer = NULL;
	int             buf_index = 0;
	unsigned int file_size = 0;

	struct file *filep = NULL;
	mm_segment_t oldfs;    

	char       wch;
	char       szRegAddr[MAX_PATH];
	char       szRegValue[MAX_PATH];
	char 	szCommand[MAX_PATH];

	u8      dwRegAddr = 0;
	u8      dwRegValue = 0;

	char	strTuningCommand[MAX_PATH];
	char       strTuningOutLog[MAX_PATH];

	int nMode = 0;
	int nStartTurning = 0;
	int nStartAddr = 0;
	int nStartValue = 0;
	int nStartHex = 0;
	int nStartCommand = 0;
	int nStartParsing = 0;

	CAM_REG16_T* pRegBuf = NULL;
	CAM_REG16_T* ptmpBuf = NULL;	
	int nRegSetCount = 0;
	int nRegSetMaxCount = 0;

	char pLine[256];
	int len_of_line;
	char *pStartCommand;
	char *pEndCommand;
	

	filep = filp_open(CAMIF_CONFIGURE_FILE, O_RDONLY, 0) ;

	if (filep && (filep!= 0xfffffffe) && (filep!=0xfffffff3)) {
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		file_size = filep->f_op->llseek(filep, 0, SEEK_END);
		filep->f_op->llseek(filep, 0, SEEK_SET);
		buffer = (char*)kmalloc(file_size+1, GFP_KERNEL);
		filep->f_op->read(filep, buffer, file_size, &filep->f_pos);
		buffer[file_size] = '\0';
		filp_close(filep, current->files);
		set_fs(oldfs);

		printk("Load32BitTuningFile : File Size = %d \n", file_size);
	} else {
		printk("Load32BitTuningFile : Do not find %s file !! \n", CAMIF_CONFIGURE_FILE);
		return;
	}

	//fOutTurning = _wfopen(strTurningOutFileName, TEXT("w+"));

	while((file_size - buf_index)> 0)
	{

		if (!nStartCommand) {
			// Read Line
			if (sscanf(buffer + buf_index, "%s", pLine)) len_of_line = strlen(pLine);
			buf_index = buf_index + len_of_line + 1;

			if (pStartCommand = strstr(pLine, START_STR) && pEndCommand = strstr(pLine, "[]")) 
			{
				pLine = pLine + strlen(START_STR);
				*pEndCommand = '\0';
				strcpy(szCommand, pLine);

				if (nStartParsing) {
					Copy2Reg16Buffer(szCommand, pRegBuf, nRegSetCount);

					//free(pRegBuf);
					kfree(pRegBuf);
					pRegBuf = NULL;
				}
				nStartParsing = 0;
				nStartCommand = 1;
				continue;
			}
		} else {
			// Read 1 byte
			wch = buffer[buf_index];
			buf_index = buf_index+1;
		}



		//Command
		if (!strncmp(L"[", &wch, 1)) {
			if (nStartParsing) {
				Copy2Reg16Buffer(szCommand, pRegBuf, nRegSetCount);

				//free(pRegBuf);
				kfree(pRegBuf);
				pRegBuf = NULL;
			}

			nStartParsing = 0;
			nStartCommand = 1;
			memset(szCommand, 0x00, sizeof(char) * MAX_PATH);

			continue;
		}

		if (!strncmp(L"]", &wch, 1)) {
			pRegBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * sizeof(CAM_REG16_T), GFP_KERNEL);

			nStartParsing = 1;
			nStartCommand = 0;

			nRegSetCount = 0;
			nRegSetMaxCount = REG_SET_COUNT;
			continue;
		}

		if (nStartCommand) {
			strncat(szCommand, &wch, 1);
		}

		//Address and Value
		if (nStartParsing)
		{
			if (!strncmp(L"{", &wch, 1))
			{
				nStartTurning = 1;
				nMode           = WRITE_MODE;
				nStartAddr     = 1;
				nStartValue    = 0;
				nStartHex      = 0;

				memset(szRegAddr, 0x00, sizeof(char) * MAX_PATH);
				memset(szRegValue, 0x00, sizeof(char) * MAX_PATH);

				dwRegAddr = 0x00;
				dwRegValue = 0x00;

				continue;
			}

			if (!strncmp(L"}", &wch, 1))
			{
				dwRegAddr = StringToHexFor16Bit(szRegAddr);
				dwRegValue = StringToHexFor16Bit(szRegValue); 

				if(nMode == WRITE_MODE)
				{		
					if (nRegSetCount >= nRegSetMaxCount )
					{
						int inc_num = nRegSetMaxCount /REG_SET_COUNT;

						inc_num++;
						//ptmpBuf = (CAM_REG16_T*) malloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));
						ptmpBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T), GFP_KERNEL);
						memset(ptmpBuf, 0x00, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num--;
						memcpy(ptmpBuf, pRegBuf, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						kfree(pRegBuf);

						inc_num++;
						//pRegBuf = (CAM_REG16_T*) malloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));
						pRegBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T), GFP_KERNEL);
						memset(pRegBuf, 0x00, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num--;
						memcpy(pRegBuf, ptmpBuf, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num++;
						nRegSetMaxCount = REG_SET_COUNT * inc_num;

					}

					pRegBuf[nRegSetCount].addr 	= dwRegAddr;
					pRegBuf[nRegSetCount].value 	= dwRegValue;
					nRegSetCount++;
				}

				nStartTurning = 0;
				continue;
			}

			if (!strncmp(L",", &wch, 1))
			{
				nStartAddr  = 0;
				nStartValue = 1;
				nStartHex   = 0;
				continue;
			}

			if (!nStartHex && !strncmp(L"0", &wch, 1))
				continue;

			if (!strncmp(L"x", &wch, 1))
			{
				nStartHex   = 1;
				continue;
			}

			if (!strncmp(L"X", &wch, 1))
			{
				nStartHex   = 1;
				continue;
			}

			if (!strncmp(L" ", &wch, 1))
				continue;

			if (nStartTurning)
			{
				if(nStartAddr)
					strncat(szRegAddr, &wch, 1);

				if(nStartValue)
					strncat(szRegValue, &wch, 1);
			}
		}

	}

	if (nStartParsing)
	{
		Copy2Reg16Buffer(szCommand, pRegBuf, nRegSetCount);

		kfree(pRegBuf);        
		pRegBuf = NULL;
	}

	kfree(buffer);
	buffer = NULL;  

	return;
}
*/


void Load16BitTuningFile()
{
	char*        buffer = NULL;
	int             buf_index = 0;
	unsigned int file_size = 0;

	struct file *filep = NULL;
	mm_segment_t oldfs;    

	char       wch;
	char       szRegAddr[MAX_PATH];
	char       szRegValue[MAX_PATH];
	char 	szCommand[MAX_PATH];

	u8      dwRegAddr = 0;
	u8      dwRegValue = 0;

	char	strTuningCommand[MAX_PATH];
	char       strTuningOutLog[MAX_PATH];

	int nMode = 0;
	int nStartTurning = 0;
	int nStartAddr = 0;
	int nStartValue = 0;
	int nStartHex = 0;
	int nStartCommand = 0;
	int nStartParsing = 0;

	CAM_REG16_T* pRegBuf = NULL;
	CAM_REG16_T* ptmpBuf = NULL;	
	int nRegSetCount = 0;
	int nRegSetMaxCount = 0;

	filep = filp_open(CAMIF_CONFIGURE_FILE, O_RDONLY, 0) ;

	if (filep && (filep!= 0xfffffffe) && (filep!=0xfffffff3))
	{
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		file_size = filep->f_op->llseek(filep, 0, SEEK_END);
		filep->f_op->llseek(filep, 0, SEEK_SET);
		buffer = (char*)kmalloc(file_size+1, GFP_KERNEL);
		filep->f_op->read(filep, buffer, file_size, &filep->f_pos);
		buffer[file_size] = '\0';
		filp_close(filep, current->files);
		set_fs(oldfs);

		printk("Load32BitTuningFile : File Size = %d \n", file_size);
	}
	else
	{
		printk("Load32BitTuningFile : Do not find %s file !! \n", CAMIF_CONFIGURE_FILE);
		return;
	}

	//fOutTurning = _wfopen(strTurningOutFileName, TEXT("w+"));

	while((file_size - buf_index)> 0)
	{
		//wch = fgetwc(fTurning);
		wch = buffer[buf_index];
		buf_index = buf_index+1;

		//Command
		if (!strncmp(L"[", &wch, 1)) {
			if (nStartParsing) {
				Copy2Reg16Buffer(szCommand, pRegBuf, nRegSetCount);

				//free(pRegBuf);
				kfree(pRegBuf);
				pRegBuf = NULL;
			}

			nStartParsing = 0;
			nStartCommand = 1;
			memset(szCommand, 0x00, sizeof(char) * MAX_PATH);

			continue;
		}

		if (!strncmp(L"]", &wch, 1)) {
			pRegBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * sizeof(CAM_REG16_T), GFP_KERNEL);

			nStartParsing = 1;
			nStartCommand = 0;

			nRegSetCount = 0;
			nRegSetMaxCount = REG_SET_COUNT;
			continue;
		}

		if (nStartCommand) {
			strncat(szCommand, &wch, 1);
		}



		//Address and Value
		if (nStartParsing)
		{
			if (!strncmp(L"{", &wch, 1))
			{
				nStartTurning = 1;
				nMode           = WRITE_MODE;
				nStartAddr     = 1;
				nStartValue    = 0;
				nStartHex      = 0;

				memset(szRegAddr, 0x00, sizeof(char) * MAX_PATH);
				memset(szRegValue, 0x00, sizeof(char) * MAX_PATH);

				dwRegAddr = 0x00;
				dwRegValue = 0x00;

				continue;
			}

			if (!strncmp(L"}", &wch, 1))
			{
				dwRegAddr = StringToHexFor16Bit(szRegAddr);
				dwRegValue = StringToHexFor16Bit(szRegValue); 

				if(nMode == WRITE_MODE)
				{		
					if (nRegSetCount >= nRegSetMaxCount )
					{
						int inc_num = nRegSetMaxCount /REG_SET_COUNT;

						inc_num++;
						//ptmpBuf = (CAM_REG16_T*) malloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));
						ptmpBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T), GFP_KERNEL);
						memset(ptmpBuf, 0x00, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num--;
						memcpy(ptmpBuf, pRegBuf, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						kfree(pRegBuf);

						inc_num++;
						//pRegBuf = (CAM_REG16_T*) malloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));
						pRegBuf = (CAM_REG16_T*)kmalloc(REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T), GFP_KERNEL);
						memset(pRegBuf, 0x00, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num--;
						memcpy(pRegBuf, ptmpBuf, REG_SET_COUNT * inc_num * sizeof(CAM_REG16_T));

						inc_num++;
						nRegSetMaxCount = REG_SET_COUNT * inc_num;

					}

					pRegBuf[nRegSetCount].addr 	= dwRegAddr;
					pRegBuf[nRegSetCount].value 	= dwRegValue;
					nRegSetCount++;
				}

				nStartTurning = 0;
				continue;
			}

			if (!strncmp(L",", &wch, 1))
			{
				nStartAddr  = 0;
				nStartValue = 1;
				nStartHex   = 0;
				continue;
			}

			if (!nStartHex && !strncmp(L"0", &wch, 1))
				continue;

			if (!strncmp(L"x", &wch, 1))
			{
				nStartHex   = 1;
				continue;
			}

			if (!strncmp(L"X", &wch, 1))
			{
				nStartHex   = 1;
				continue;
			}

			if (!strncmp(L" ", &wch, 1))
				continue;

			if (nStartTurning)
			{
				if(nStartAddr)
					strncat(szRegAddr, &wch, 1);

				if(nStartValue)
					strncat(szRegValue, &wch, 1);
			}
		}

	}

	if (nStartParsing)
	{
		Copy2Reg16Buffer(szCommand, pRegBuf, nRegSetCount);

		kfree(pRegBuf);        
		pRegBuf = NULL;
	}

	kfree(buffer);
	buffer = NULL;  

	return;
}


void UnLoad16BitTuningFile()
{
	DeleteReg16Buffer(&CAM_REG16SET_INIT);
	DeleteReg16Buffer(&CAM_REG16SET_VT_INIT);
	DeleteReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_640_480);
	DeleteReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_320_240);
	DeleteReg16Buffer(&CAM_REG16SET_PREVIEW_SIZE_176_144);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_4);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_3);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_2);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_M_1);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_0);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_1);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_2);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_3);
	DeleteReg16Buffer(&CAM_REG16SET_BRIGHTNESS_P_4);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_M_5);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_M_4);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_M_3);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_M_2);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_M_1);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_0);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_P_1);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_P_2);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_P_3);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_P_4);
	DeleteReg16Buffer(&CAM_REG16SET_CONTRAST_P_5);
	DeleteReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_AUTO);
	DeleteReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_DAYLIGHT);
	DeleteReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_CLOUDY);
	DeleteReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_FLUORESCENT);
	DeleteReg16Buffer(&CAM_REG16SET_WHITE_BALANCE_INCANDESCENT);
	DeleteReg16Buffer(&CAM_REG16SET_EFFECT_OFF);
	DeleteReg16Buffer(&CAM_REG16SET_EFFECT_GRAY);
	DeleteReg16Buffer(&CAM_REG16SET_EFFECT_SEPIA);
	DeleteReg16Buffer(&CAM_REG16SET_EFFECT_NEGATIVE);
	DeleteReg16Buffer(&CAM_REG16SET_EFFECT_AQUA);
	DeleteReg16Buffer(&CAM_REG16SET_BLUR_0);
	DeleteReg16Buffer(&CAM_REG16SET_BLUR_P_1);
	DeleteReg16Buffer(&CAM_REG16SET_BLUR_P_2);
	DeleteReg16Buffer(&CAM_REG16SET_BLUR_P_3);
	DeleteReg16Buffer(&CAM_REG16SET_AUTO_FPS);
	DeleteReg16Buffer(&CAM_REG16SET_7_FPS);
	DeleteReg16Buffer(&CAM_REG16SET_10_FPS);
	DeleteReg16Buffer(&CAM_REG16SET_15_FPS);
	DeleteReg16Buffer(&CAM_REG16SET_FLIP_NONE);
	DeleteReg16Buffer(&CAM_REG16SET_FLIP_MIRROR);
	DeleteReg16Buffer(&CAM_REG16SET_VT_FLIP_NONE);
	DeleteReg16Buffer(&CAM_REG16SET_VT_FLIP_MIRROR);
	DeleteReg16Buffer(&CAM_REG16SET_DTP_ON);
	DeleteReg16Buffer(&CAM_REG16SET_DTP_OFF);
}

