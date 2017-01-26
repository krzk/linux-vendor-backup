#ifndef __FILE_OPS__
#define __FILE_OPS__

#include <linux/types.h>
#include <kprobe/swap_kprobes_deps.h>

bool file_ops_is_init(void);
int file_ops_init(void);
void file_ops_exit(void);

#endif /* __FILE_OPS__ */
