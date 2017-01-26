
#ifdef CONFIG_SWAP_ENERGY_HOOKS
#include <asm/unistd32.h>
#include <kprobe/swap_kprobes.h>

unsigned long get_nr_compat_read(void)
{
	printk("SWAP __NR_read = %u\n", __NR_read);
	return __NR_read;
}

unsigned long get_nr_compat_write(void)
{
	printk("SWAP __NR_write = %u\n", __NR_write);
	return __NR_write;
}
#endif
