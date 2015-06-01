/*
 * PASS (Power Aware System Service) - Collect CPUs data
 *
 *  Copyright (C)  2013-2015 Samsung Electronics co. ltd
 *    Chanwoo Choi <cw00.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>

#define MAX_CLUSTER	2
#define CLUSTER_0	0
#define CLUSTER_1	1
#define CLUSTER_0_FIRST_CPU	0	/* CPU0 */
#define CLUSTER_1_FIRST_CPU	4	/* CPU4 */

#define SAMPLING_RATE	10

struct runqueue_data {
	unsigned int avg_nr_runnings;
	int64_t last_time;
	int64_t total_time;
};

static spinlock_t lock;
static struct delayed_work gwork;
static struct runqueue_data rq_data[MAX_CLUSTER];

static cpumask_var_t cluster0_cpus;
static cpumask_var_t cluster1_cpus;

int get_avg_nr_runnings(unsigned int cpu)
{
	unsigned int avg_nr_runnings;
	unsigned long flags = 0;
	int cluster_id;

	spin_lock_irqsave(&lock, flags);

	if (cpu == CLUSTER_0_FIRST_CPU)
		cluster_id = CLUSTER_0;
	else if (cpu == CLUSTER_1_FIRST_CPU)
		cluster_id = CLUSTER_1;
	else
		return 0;

	avg_nr_runnings = rq_data[cluster_id].avg_nr_runnings;
	rq_data[cluster_id].avg_nr_runnings = 0;

	spin_unlock_irqrestore(&lock, flags);

	return avg_nr_runnings;
}
EXPORT_SYMBOL(get_avg_nr_runnings);

static void calculate_nr_running(struct runqueue_data *rq, cpumask_var_t cpus)
{
	int64_t time_diff = 0;
	int64_t avg_nr_runnings = 0;
	int64_t curr_time = ktime_to_ms(ktime_get());

	if (!rq->last_time)
		rq->last_time = curr_time;
	if (!rq->avg_nr_runnings)
		rq->total_time = 0;

	smp_rmb();

	avg_nr_runnings = nr_running_cpumask(cpus) * 100;
	time_diff = curr_time - rq->last_time;

	if (time_diff && rq->total_time != 0) {
		avg_nr_runnings = (avg_nr_runnings * time_diff) +
			(rq->avg_nr_runnings * rq->total_time);
		do_div(avg_nr_runnings, rq->total_time + time_diff);
	}

	rq->avg_nr_runnings = avg_nr_runnings;
	rq->last_time = curr_time;
	rq->total_time += time_diff;
}

static void avg_nr_runnings_work(struct work_struct *work)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lock, flags);
	calculate_nr_running(&rq_data[CLUSTER_0], cluster0_cpus);
	if (NR_CPUS > CLUSTER_1_FIRST_CPU)
		calculate_nr_running(&rq_data[CLUSTER_1], cluster1_cpus);
	spin_unlock_irqrestore(&lock, flags);

	schedule_delayed_work(&gwork, msecs_to_jiffies(SAMPLING_RATE));
}

static int __init pass_stats_init(void)
{
	int i;

	for (i = 0; i < MAX_CLUSTER; i++) {
		rq_data[i].avg_nr_runnings = 0;
		rq_data[i].last_time = 0;
		rq_data[i].total_time = 0;
	}

	alloc_cpumask_var(&cluster0_cpus, GFP_KERNEL);
	if (NR_CPUS > CLUSTER_1_FIRST_CPU)
		alloc_cpumask_var(&cluster1_cpus, GFP_KERNEL);

	for (i = 0; i < NR_CPUS; i++) {
		if (i < CLUSTER_1_FIRST_CPU)
			cpumask_set_cpu(i, cluster0_cpus);
		else
			cpumask_set_cpu(i, cluster1_cpus);
	}

	spin_lock_init(&lock);
	INIT_DEFERRABLE_WORK(&gwork, avg_nr_runnings_work);
	schedule_delayed_work(&gwork, msecs_to_jiffies(SAMPLING_RATE));

	return 0;
}

static void __exit pass_stats_exit(void)
{
	cancel_delayed_work_sync(&gwork);
}
module_init(pass_stats_init);
module_exit(pass_stats_exit);
