/*
 * llid.c
 *
 * Low-Level Interface Driver
 *
 * COPYRIGHT(C) Samsung Electronics Co.Ltd. 2009-2010 All Right Reserved.  
 *
 * 2009.02 - First editing by SungHwan.yun <sunghwan.yun@samsung.com> @LDK@
 * 
 * 2009.03 - Currently managed by  SungHwan.yun <sunghwan.yun@samsung.com> @LDK@
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include "j4fs.h"

#if defined(J4FS_USE_XSR)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <xsr/Inc/XsrTypes.h>
#include <xsr/Inc/STL.h>
#else
#include <txsr/Inc/XsrTypes.h>
#include <txsr/Inc/STL.h>
#endif

#elif defined(J4FS_USE_FSR)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <tfsr/Inc/FSR.h>
#include <tfsr/Inc/FSR_STL.h>
#else
#include <fsr/Inc/FSR.h>
#include <fsr/Inc/FSR_STL.h>
#endif

#else
'compile error'
#endif

extern j4fs_device_info device_info;
extern unsigned int j4fs_traceMask;

/*
  * Fills the specified buffer with the number of bytes defined by length from the device's absolute physical address
  * parameters
  *   offset  : start physical page number
  *   length  : bytes to write
  *   buffer  : buffer to write data
  * return value
  *   The return code should indicate ERROR (1) or OK(0). If the return code indicates ERROR, the dev_ptr->status field should indicate 
  *   the return error code.
  */
int FlashDevRead(j4fs_device_info *dev_ptr, DWORD offset, DWORD length, BYTE *buffer)
{
    	DWORD nVol=0;
	int ret=-1;
	int part_id=dev_ptr->device;
	
#if defined(J4FS_USE_XSR)
	ret = STL_Read(nVol, part_id, offset/512, length/512, buffer);
	if (ret != STL_SUCCESS) {
		T(J4FS_TRACE_ALWAYS,("%s %d: Error(offset,length,j4fs_end,nErr)=(0x%x,0x%x,0x%x,0x%x)\n",__FUNCTION__,__LINE__,offset,length,device_info.j4fs_end,ret));
   		return J4FS_FAIL;
	}
#elif defined(J4FS_USE_FSR)
	ret = FSR_STL_Read(nVol, part_id, offset/512, length/512, buffer, FSR_STL_FLAG_DEFAULT);
	if (ret != FSR_STL_SUCCESS) {
		T(J4FS_TRACE_ALWAYS,("%s %d: Error(offset,length,j4fs_end,nErr)=(0x%x,0x%x,0x%x,0x%x)\n",__FUNCTION__,__LINE__,offset,length,device_info.j4fs_end,ret));
   		return J4FS_FAIL;
	}
#else
'compile error'
#endif

	return J4FS_SUCCESS;
}

/*
  * This function writes length bytes of data from a specified buffer to the destination address within the device
  * parameters
  *   offset  : start physical page number
  *   length  : bytes to write
  *   buffer  : buffer to write data
  * return value
  *   The return code should indicate ERROR (1) or OK(0). If the return code indicates ERROR, the dev_ptr->status field should indicate 
  *   the return error code.
  */
int FlashDevWrite(j4fs_device_info *dev_ptr, DWORD offset, DWORD length, BYTE *buffer)
{
    	DWORD nVol=0;
	int ret=-1;
	int part_id=dev_ptr->device;
		
#if defined(J4FS_USE_XSR)
	ret = STL_Write(nVol, part_id, offset/512, length/512, buffer);
	if (ret != STL_SUCCESS) {
		T(J4FS_TRACE_ALWAYS,("%s %d: Error(offset,length,j4fs_end,nErr)=(0x%x,0x%x,0x%x,0x%x)\n",__FUNCTION__,__LINE__,offset,length,device_info.j4fs_end,ret));
   		return J4FS_FAIL;
	}
#elif defined(J4FS_USE_FSR)
	ret = FSR_STL_Write(nVol, part_id, offset/512, length/512, buffer, FSR_STL_FLAG_DEFAULT);
	if (ret != FSR_STL_SUCCESS) {
		T(J4FS_TRACE_ALWAYS,("%s %d: Error(offset,length,j4fs_end,nErr)=(0x%x,0x%x,0x%x,0x%x)\n",__FUNCTION__,__LINE__,offset,length,device_info.j4fs_end,ret));
   		return J4FS_FAIL;
	}
#else
'compile error'
#endif

	return J4FS_SUCCESS;	
}

/* 
  * In order to reuse the flash media, an erase command must be provided for the FSD. This command erases a single flash erase-block beginning 
  * at the address specified by the aux field in the DEVICE_INFO structure. The blocksize field of the DEVICE_INFO structure is used to force the aux ptr 
  * to a block boundary.
  * parameters
  *   dev_ptr->aux : start block address to be erased
  */
int FlashDevErase(j4fs_device_info *dev_ptr)
{
	// TODO
	return J4FS_SUCCESS;
}

int FlashDevSpecial(j4fs_device_info *dev_ptr, DWORD scmd)
{
	switch(scmd)
	{
		case J4FS_INIT:
			// Initialize the internal FSD structures to use a device
			FlashDevMount();
			
			break;

		case J4FS_GET_INFO:
			break;

		default:
			break;
	}

	return J4FS_SUCCESS;
}

int FlashDevMount()
{
	DWORD media_status_table_size=1;		//  Media Status Table occupys 1 block

	device_info.device=J4FS_PARTITION_ID;
	device_info.blocksize=PHYSICAL_BLOCK_SIZE;
	device_info.pagesize=PHYSICAL_PAGE_SIZE;
	device_info.j4fs_offset=media_status_table_size*device_info.blocksize;	 // j4fs_offset follows the Media Status Table.
	return J4FS_SUCCESS;
}

