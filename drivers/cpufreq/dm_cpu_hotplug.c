#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sort.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>

#define	COLD_THRESHOLD	20
#define	SUPERMAX_FREQ	2100000
#define NORMALMAX_FREQ	1900000
#define NORMALMIN_FREQ	250000
#define POLLING_MSEC	100

struct cpu_load_info {
	cputime64_t cpu_idle;
	cputime64_t cpu_iowait;
	cputime64_t cpu_wall;
	cputime64_t cpu_nice;
};

static DEFINE_PER_CPU(struct cpu_load_info, cur_cpu_info);

static int cpu_util[NR_CPUS];
static struct pm_qos_request max_cpu_qos_hotplug;
static unsigned int cur_load_freq = 0;
static bool lcd_is_on;

enum hotplug_mode {
	CHP_NORMAL,
	CHP_LOW_POWER,
#ifdef CONFIG_EXYNOS5_MAX_CPU_HOTPLUG
	CHP_HIGH_PERF,
#endif
};

static enum hotplug_mode prev_mode;
static unsigned int delay = POLLING_MSEC;

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static inline struct file* file_open(const char* path, int flags, int rights)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}

	return filp;
}

static inline void file_close(struct file* file)
{
	filp_close(file, NULL);
}

static inline int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);
	set_fs(oldfs);

	return ret;
}

static inline int file_write(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

static inline int file_sync(struct file* file)
{
	vfs_fsync(file, 0);
	return 0;
}

static inline void pm_qos_update_max(int frequency)
{
	if (pm_qos_request_active(&max_cpu_qos_hotplug))
		pm_qos_update_request(&max_cpu_qos_hotplug, frequency);
	else
		pm_qos_add_request(&max_cpu_qos_hotplug, PM_QOS_CPU_FREQ_MAX, frequency);
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lcd_is_on = false;
		pr_info("LCD is off\n");
		break;
	case FB_BLANK_UNBLANK:
		/*
		 * LCD blank CPU qos is set by exynos-ikcs-cpufreq
		 * This line of code release max limit when LCD is
		 * turned on.
		 */
		lcd_is_on = true;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

static void __cpu_hotplug(struct cpumask *be_out_cpus)
{
	struct file *fp_online;
#ifdef DM_HOTPLUG_DEBUG
	struct timeval before, after;
	int dtime[NR_CPUS];
#endif
	char fsys[50];
	int i;

	if (cpumask_weight(be_out_cpus) >= NR_CPUS)
		return;

	for (i=1; i < NR_CPUS; i++) {
#ifdef DM_HOTPLUG_DEBUG
		do_gettimeofday(&before);
#endif
		snprintf(fsys, 50, "/sys/devices/system/cpu/cpu%d/online", i);
		fp_online = file_open(fsys, O_RDWR, S_IWUSR);
		if (cpumask_test_cpu(i, be_out_cpus))
			file_write(fp_online, 0, "0", 2);
		else
			file_write(fp_online, 0, "1", 2);
		file_close(fp_online);
#ifdef DM_HOTPLUG_DEBUG
		do_gettimeofday(&after);
		dtime[i] = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
				(after.tv_usec - before.tv_usec);
#endif
	}

#ifdef DM_HOTPLUG_DEBUG
	printk("hotplug delay : %d %d %d %d\n", dtime[0], dtime[1], dtime[2], dtime[3]);
#endif
	return;
}

static int dynamic_hotplug(enum hotplug_mode mode)
{
	int cpu = 0;
	struct cpumask out_target;
	enum hotplug_mode ret = 0;

	cpumask_clear(&out_target);

	switch (mode) {
	case CHP_LOW_POWER:
		delay = POLLING_MSEC;
		for_each_cpu(cpu, cpu_online_mask) {
			if (cpu == 0)
				continue;
			cpumask_set_cpu(cpu, &out_target);
		}
		pm_qos_update_max(NORMALMAX_FREQ);
		__cpu_hotplug(&out_target);
		break;
#ifdef CONFIG_EXYNOS5_MAX_CPU_HOTPLUG
	case CHP_HIGH_PERF:
#ifndef FROG_OVER
		for_each_cpu(cpu, cpu_online_mask) {
			if (cpu == 0 || cpu_util[cpu] > COLD_THRESHOLD)
				continue;
			if (cpumask_weight(&out_target) < 2)
				cpumask_set_cpu(cpu, &out_target);
		}

		if (cpumask_weight(&out_target) == 2) {
			__cpu_hotplug(&out_target);
			pm_qos_update_max(SUPERMAX_FREQ);
			delay = 10;
		} else
			ret = -EPERM;
#else
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			__cpu_hotplug(&out_target);
		pm_qos_update_max(SUPERMAX_FREQ);
#endif
		break;
#endif
	case CHP_NORMAL:
	default:
		delay = POLLING_MSEC;
		pm_qos_update_max(NORMALMAX_FREQ);
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			__cpu_hotplug(&out_target);
		break;
	}

	return ret;
}

static int low_stay = 0;
static int high_stay = 0;

static enum hotplug_mode diagnose_condition(void)
{
	int ret;
#if defined(CONFIG_EXYNOS5_MAX_CPU_HOTPLUG) && !defined(FROG_OVER)
	int i;
	int cold_cpus;
#endif

	ret = CHP_NORMAL;

	if (cur_load_freq > NORMALMIN_FREQ)
		low_stay = 0;
	else if (cur_load_freq <= NORMALMIN_FREQ && low_stay <= 5)
		low_stay++;
	if (low_stay > 5 && !lcd_is_on)
		ret = CHP_LOW_POWER;

	if (cur_load_freq < NORMALMAX_FREQ)
		high_stay = 0;
	else if (cur_load_freq >= NORMALMAX_FREQ)
		high_stay++;

#ifdef CONFIG_EXYNOS5_MAX_CPU_HOTPLUG
#ifndef FROG_OVER
	for (i = 1, cold_cpus=0; i < NR_CPUS; i++)
		if (cpu_util[i] <= COLD_THRESHOLD)
			cold_cpus++;

	if (high_stay > 3 && cold_cpus >= 2) {
		ret = CHP_HIGH_PERF;
		high_stay = 0;
	}
#else
	if (high_stay > 5) {
		ret = CHP_HIGH_PERF;
		high_stay = 0;
	}
#endif
#endif

	return ret;
}

static void calc_load(void)
{
	struct cpufreq_policy *policy;
	unsigned int cpu_util_sum = 0;
	int cpu = 0;
	unsigned int i;

	policy = cpufreq_cpu_get(cpu);

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = 0;

	for_each_cpu(i, policy->cpus) {
		struct cpu_load_info	*i_load_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;

		i_load_info = &per_cpu(cur_cpu_info, i);

		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(i, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - i_load_info->cpu_wall);
		i_load_info->cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - i_load_info->cpu_idle);
		i_load_info->cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - i_load_info->cpu_iowait);
		i_load_info->cpu_iowait = cur_iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		cpu_util[i] = load;
		cpu_util_sum += load;

		load_freq = load * policy->cur;

		if (policy->cur > cur_load_freq)
			cur_load_freq = policy->cur;
	}

	cpufreq_cpu_put(policy);
	return;
}

static int thread_run_flag;

static int on_run(void *data)
{
	int on_cpu = 0;
	enum hotplug_mode exe_mode;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	prev_mode = CHP_NORMAL;
	thread_run_flag = 1;

	while (thread_run_flag) {
		calc_load();

		exe_mode = diagnose_condition();
		if (exe_mode != prev_mode) {
#ifdef DM_HOTPLUG_DEBUG
			pr_debug("frequency info : %d, %s\n", cur_load_freq
				, (exe_mode<1)?"NORMAL":((exe_mode<2)?"LOW":"HIGH"));
#endif
			if (dynamic_hotplug(exe_mode) < 0)
				exe_mode = prev_mode;
		}

		prev_mode = exe_mode;
		msleep_interruptible(delay);
	}

	return 0;
}

void dm_cpu_hotplug_exit(void)
{
	thread_run_flag = 0;
}

void dm_cpu_hotplug_init(void)
{
	struct task_struct *k;

	k = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (!IS_ERR(k))
		pr_err("Failed in creation of thread.\n");

	fb_register_client(&fb_block);
	lcd_is_on = true;
}

