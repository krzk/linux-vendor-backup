struct saved_time_in_state_info {
	unsigned int state;
	unsigned int frequency;
	unsigned long long time_before;
	unsigned long long time_in_state;
};

int max_state_num = 10;
extern void la_cpufreq_stats_update(void);
extern void la_gpufreq_stats_update(int clock);
extern void la_devfreq_update_status(char *name, unsigned long freq);

static struct saved_time_in_state_info *cpufreq_time_in_state;
static struct saved_time_in_state_info *gpufreq_time_in_state;
static struct saved_time_in_state_info *miffreq_time_in_state;
static struct saved_time_in_state_info *intfreq_time_in_state;
#define TIME_IN_STATE_INFO_SIZE	\
	(sizeof(struct saved_time_in_state_info ) *max_state_num)

static int la_time_in_state_monitor_init(void){

	cpufreq_time_in_state = vmalloc(TIME_IN_STATE_INFO_SIZE);
	if (cpufreq_time_in_state == NULL)
		return -ENOMEM;

	gpufreq_time_in_state = vmalloc(TIME_IN_STATE_INFO_SIZE);
	if (gpufreq_time_in_state == NULL)
		return -ENOMEM;

	miffreq_time_in_state = vmalloc(TIME_IN_STATE_INFO_SIZE);
	if (miffreq_time_in_state == NULL)
		return -ENOMEM;

	intfreq_time_in_state = vmalloc(TIME_IN_STATE_INFO_SIZE);
	if (intfreq_time_in_state == NULL)
		return -ENOMEM;

	memset(cpufreq_time_in_state, 0, TIME_IN_STATE_INFO_SIZE);
	memset(gpufreq_time_in_state, 0, TIME_IN_STATE_INFO_SIZE);
	memset(miffreq_time_in_state, 0, TIME_IN_STATE_INFO_SIZE);
	memset(intfreq_time_in_state, 0, TIME_IN_STATE_INFO_SIZE);

	return 0;
}

void la_get_time_in_state(unsigned int i, unsigned int frequency, unsigned long long last_time, char *name) {

	if(strstr(name,"CPU")!=NULL){
		cpufreq_time_in_state[i].state = i;
		cpufreq_time_in_state[i].frequency = frequency;
		cpufreq_time_in_state[i].time_in_state= last_time - cpufreq_time_in_state[i].time_before;
		cpufreq_time_in_state[i].time_before = last_time;
	} else if (strstr(name,"GPU")!=NULL) {
		gpufreq_time_in_state[i].state = i;
		gpufreq_time_in_state[i].frequency = frequency;
		gpufreq_time_in_state[i].time_in_state= last_time - gpufreq_time_in_state[i].time_before;
		gpufreq_time_in_state[i].time_before = last_time;
	} else if (strstr(name,"MIF")!=NULL) {
		miffreq_time_in_state[i].state = i;
		miffreq_time_in_state[i].frequency = frequency;
		miffreq_time_in_state[i].time_in_state= last_time - miffreq_time_in_state[i].time_before;
		miffreq_time_in_state[i].time_before = last_time;
	} else if (strstr(name,"INT")!=NULL) {
		intfreq_time_in_state[i].state = i;
		intfreq_time_in_state[i].frequency = frequency;
		intfreq_time_in_state[i].time_in_state= last_time - intfreq_time_in_state[i].time_before;
		intfreq_time_in_state[i].time_before = last_time;
	}

}

static int time_in_state_monitor_read_sub(char *buf, int buf_size)
{
	int ret = 0,i =0;

	/* update current status */
	la_cpufreq_stats_update();
	la_gpufreq_stats_update(saved_load_factor.gpu_freq);
	la_devfreq_update_status("MIF",saved_load_factor.mif_bus_freq);
	la_devfreq_update_status("INT",saved_load_factor.int_bus_freq);

	/* show time_in_state_monitor */
	ret += sprintf(buf + ret,"=========TIME_IN_STATE MONITOR=========\n"
								"           CPUFREQ     TIME<ms>\n");
	ret += sprintf(buf + ret,"[CPU]\n");
	for (i = 0; i < max_state_num; i++) {
		if(cpufreq_time_in_state[i].frequency)
			ret += sprintf(buf + ret,"        %10u\t%llu\n", cpufreq_time_in_state[i].frequency,
							cpufreq_time_in_state[i].time_in_state);
	}
	ret += sprintf(buf + ret,"[GPU]\n");
	for (i = 0; i < max_state_num; i++) {
		if(gpufreq_time_in_state[i].frequency)
			ret += sprintf(buf + ret,"        %10u\t%llu\n", gpufreq_time_in_state[i].frequency*1000,
							gpufreq_time_in_state[i].time_in_state);
	}
	ret += sprintf(buf + ret,"[MIF]\n");
	for (i = 0; i < max_state_num; i++) {
		if(miffreq_time_in_state[i].frequency)
			ret += sprintf(buf + ret,"        %10u\t%llu\n", miffreq_time_in_state[i].frequency,
							miffreq_time_in_state[i].time_in_state);
	}
	ret += sprintf(buf + ret,"[INT]\n");
	for (i = 0; i < max_state_num; i++) {
		if(intfreq_time_in_state[i].frequency)
			ret += sprintf(buf + ret,"        %10u\t%llu\n", intfreq_time_in_state[i].frequency,
							intfreq_time_in_state[i].time_in_state);
	}

	return ret;
}

static ssize_t time_in_state_monitor_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
						,time_in_state_monitor_read_sub);

	return size_for_copy;
}

static const struct file_operations time_in_state_monitor_fops = {
	.owner = THIS_MODULE,
	.read  = time_in_state_monitor_read,
};

void debugfs_time_in_state(struct dentry *d)
{
	if (!debugfs_create_file("time_in_state_monitor", 0600, d, NULL, &time_in_state_monitor_fops))
		pr_err("%s : debugfs_create_file, error\n", "time_in_state_monitor");
}

