#ifndef __SEC_LOG_BUF_H__
#define __SEC_LOG_BUF_H__

#define SEC_LOG_BUF_FLAG_SIZE		(4 * 1024)
#define SEC_LOG_BUF_DATA_SIZE		(1 << CONFIG_LOG_BUF_SHIFT)
#define SEC_LOG_BUF_SIZE		\
	(SEC_LOG_BUF_FLAG_SIZE + SEC_LOG_BUF_DATA_SIZE)
#define SEC_LOG_BUF_START		(0xA5000000 - SEC_LOG_BUF_SIZE)
#define SEC_LOG_BUF_MAGIC		0x404C4F47	/* @LOG */

struct sec_log_buf {
	unsigned int *flag;
	unsigned int *count;
	char *data;
};

void sec_log_buf_init(void);

void __init sec_log_buf_reserve_mem(void);

#endif /* __SEC_LOG_BUF_H__ */
