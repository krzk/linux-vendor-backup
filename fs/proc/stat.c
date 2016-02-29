#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <asm/cputime.h>
#include <linux/tick.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/suspend.h>
#include <linux/power/sleep_monitor.h>
#endif /* CONFIG_SLEEP_MONITOR */

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}

#endif

static int show_stat(struct seq_file *p, void *v)
{
	int i, j;
	unsigned long jif;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	struct timespec boottime;

	user = nice = system = idle = iowait =
		irq = softirq = steal = 0;
	guest = guest_nice = 0;
	getboottime(&boottime);
	jif = boottime.tv_sec;

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += get_idle_time(i);
		iowait += get_iowait_time(i);
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		sum += kstat_cpu_irqs_sum(i);
		sum += arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	seq_puts(p, "cpu ");
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(user));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(nice));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(system));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(idle));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(iowait));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(irq));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(softirq));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(steal));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest));
	seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest_nice));
	seq_putc(p, '\n');

	for_each_online_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle = get_idle_time(i);
		iowait = get_iowait_time(i);
		irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal = kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest = kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		seq_printf(p, "cpu%d", i);
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(user));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(nice));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(system));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(idle));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(iowait));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(irq));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(softirq));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(steal));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest));
		seq_put_decimal_ull(p, ' ', cputime64_to_clock_t(guest_nice));
		seq_putc(p, '\n');
	}
	seq_printf(p, "intr %llu", (unsigned long long)sum);

	/* sum again ? it could be updated? */
	for_each_irq_nr(j)
		seq_put_decimal_ull(p, ' ', kstat_irqs(j));

	seq_printf(p,
		"\nctxt %llu\n"
		"btime %lu\n"
		"processes %lu\n"
		"procs_running %lu\n"
		"procs_blocked %lu\n",
		nr_context_switches(),
		(unsigned long)jif,
		total_forks,
		nr_running(),
		nr_iowait());

	seq_printf(p, "softirq %llu", (unsigned long long)sum_softirq);

	for (i = 0; i < NR_SOFTIRQS; i++)
		seq_put_decimal_ull(p, ' ', per_softirq_sums[i]);
	seq_putc(p, '\n');

	return 0;
}

static int stat_open(struct inode *inode, struct file *file)
{
	unsigned size = 1024 + 128 * num_possible_cpus();
	char *buf;
	struct seq_file *m;
	int res;

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;

	/* don't ask for more than the kmalloc() max size */
	if (size > KMALLOC_MAX_SIZE)
		size = KMALLOC_MAX_SIZE;
	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	res = single_open(file, show_stat, NULL);
	if (!res) {
		m = file->private_data;
		m->buf = buf;
		m->size = ksize(buf);
	} else
		kfree(buf);
	return res;
}

static const struct file_operations proc_stat_operations = {
	.open		= stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_SLEEP_MONITOR
struct proc_stat_cpu_info {
	u64 user;
	u64 nice;
	u64 system;
	u64 idle;
	u32 util;
};

static struct proc_stat_cpu_info *sleep_mon_cpu_info;

static int proc_stat_pm_notifier(struct notifier_block *nb,
					unsigned long event, void *dummy)
{
	int i;
	u32 cpu_util;
	u64 user, nice, system, idle;
	u64 total_user, total_nice, total_system, total_idle;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		total_user = total_nice = total_system = total_idle = 0;
		for_each_possible_cpu(i) {

			total_user += user = kcpustat_cpu(i).cpustat[CPUTIME_USER] - (sleep_mon_cpu_info + i)->user;
			total_nice += nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE] - (sleep_mon_cpu_info + i)->nice;
			total_system += system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM] - (sleep_mon_cpu_info + i)->system;
			total_idle += idle = get_idle_time(i) - (sleep_mon_cpu_info + i)->idle;

			cpu_util = (user + nice + system)*100;

			if (user + nice + system + idle != 0)
				do_div(cpu_util, (user + nice + system + idle));
			(sleep_mon_cpu_info + i)->util = (u32)cpu_util;

			pr_debug("%s:%d:%d:0x%llx:0x%llx:0x%llx:0x%llx:%d\n",
						__func__, (int)event, i, user, nice, system, idle, (int)cpu_util);
		}
		break;
	case PM_POST_SUSPEND:
		for_each_possible_cpu(i) {
			(sleep_mon_cpu_info + i)->user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
			(sleep_mon_cpu_info + i)->nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
			(sleep_mon_cpu_info + i)->system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
			(sleep_mon_cpu_info + i)->idle = get_idle_time(i);
			(sleep_mon_cpu_info + i)->util = 0;

			pr_debug("%s:%d:%d:0x%llx:0x%llx:0x%llx:0x%llx\n",
						__func__, (int)event, i,
						(sleep_mon_cpu_info + i)->user,
						(sleep_mon_cpu_info + i)->nice,
						(sleep_mon_cpu_info + i)->system,
						(sleep_mon_cpu_info + i)->idle);
		}
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block proc_stat_notifier_block = {
	.notifier_call = proc_stat_pm_notifier,
};

static int proc_stat_sleep_monitor_read_cb(void *priv,
							 unsigned int *raw_val, int check_level, int caller_type)
{
	int i;
	u32 cpu_util = 0, total_cpu_util = 0;

	for_each_possible_cpu(i) {
		cpu_util = (sleep_mon_cpu_info + i)->util;

		/* maximum granted core is 4 */
		*raw_val |= ((cpu_util&0xff) << (i*8));

		/* maximum granted core is 2 */
		if (cpu_util < 25)
			cpu_util = 0;
		else if (cpu_util < 50)
			cpu_util = 1;
		else if (cpu_util < 75)
			cpu_util = 2;
		else
			cpu_util = 3;
		total_cpu_util |= (cpu_util << i*2);

		pr_debug("%s:cpu%d:%d:0x%x:0x%x\n",
			__func__, i, cpu_util, total_cpu_util, *raw_val);
	}

	return total_cpu_util;
}

static struct sleep_monitor_ops proc_stat_sleep_monitor_ops = {
	.read_cb_func = proc_stat_sleep_monitor_read_cb,
};
#endif /* CONFIG_SLEEP_MONITOR */

static int __init proc_stat_init(void)
{
#ifdef CONFIG_SLEEP_MONITOR
	int ret;
	int num_cpu;
#endif /* CONFIG_SLEEP_MONITOR */

	proc_create("stat", 0, NULL, &proc_stat_operations);

#ifdef CONFIG_SLEEP_MONITOR
	num_cpu = num_possible_cpus();
	if ((sleep_mon_cpu_info = kzalloc(sizeof(struct proc_stat_cpu_info)*num_cpu, GFP_KERNEL)) == NULL)
		goto fail_alloc;

	ret = register_pm_notifier(&proc_stat_notifier_block);
	if (ret)
		goto fail_pm_register;

	ret = sleep_monitor_register_ops(&sleep_mon_cpu_info,
	   &proc_stat_sleep_monitor_ops,
	   SLEEP_MONITOR_CPU_UTIL);
	if (ret)
		goto fail_slp_mon_register;

	return 0;

fail_pm_register:
	kfree(sleep_mon_cpu_info);
fail_alloc:
fail_slp_mon_register:
#endif /* CONFIG_SLEEP_MONITOR */

	return 0;
}
module_init(proc_stat_init);
