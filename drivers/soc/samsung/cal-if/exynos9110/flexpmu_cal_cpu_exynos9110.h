#ifndef ACPM_FRAMEWORK

/* individual sequence descriptor for each core - on, off, status */
struct pmucal_seq core00_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP2_INTR_BID_ENABLE", 0x11870000, 0x0200, (0x1 << 4), (0x0 << 4), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_RETRY, "GRP2_INTR_BID_CLEAR", 0x11870000, 0x020c, (0x1 << 4), (0x1 << 4), 0x11870000, 0x0208, (0x1 << 4), (0x1 << 4) | (0x1 << 4)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP2", 0x11860000, 0x7f08, (0x1 << 4), (0x0 << 4), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq core00_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP2_INTR_BID_ENABLE", 0x11870000, 0x0200, (0x1 << 4), (0x1 << 4), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_RETRY, "GRP1_INTR_BID_CLEAR", 0x11870000, 0x010c, (0x1 << 4), (0x1 << 4), 0x11870000, 0x0108, (0x1 << 4), (0x1 << 4) | (0x1 << 4)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP2", 0x11860000, 0x7f08, (0x1 << 4), (0x1 << 4), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq core00_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_CPU0_STATUS", 0x11860000, 0x2024, (0xf << 0), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq core01_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP2_INTR_BID_ENABLE", 0x11870000, 0x0200, (0x1 << 5), (0x0 << 5), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_RETRY, "GRP2_INTR_BID_CLEAR", 0x11870000, 0x020c, (0x1 << 5), (0x1 << 5), 0x11870000, 0x0208, (0x1 << 5), (0x1 << 5) | (0x1 << 5)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP2", 0x11860000, 0x7f08, (0x1 << 5), (0x0 << 5), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq core01_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP2_INTR_BID_ENABLE", 0x11870000, 0x0200, (0x1 << 5), (0x1 << 5), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_RETRY, "GRP1_INTR_BID_CLEAR", 0x11870000, 0x010c, (0x1 << 5), (0x1 << 5), 0x11870000, 0x0108, (0x1 << 5), (0x1 << 5) | (0x1 << 5)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP2", 0x11860000, 0x7f08, (0x1 << 5), (0x1 << 5), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq core01_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_CPU1_STATUS", 0x11860000, 0x202c, (0xf << 0), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq cluster0_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_NONCPU_STATUS", 0x11860000, 0x204c, (0xf << 0), 0, 0, 0, 0xffffffff, 0),
};
enum pmucal_cpu_corenum {
	CPU_CORE00,
	CPU_CORE01,
	PMUCAL_NUM_CORES,
};

struct pmucal_cpu pmucal_cpu_list[PMUCAL_NUM_CORES] = {
	[CPU_CORE00] = {
		.id = CPU_CORE00,
		.release = 0,
		.on = core00_on,
		.off = core00_off,
		.status = core00_status,
		.num_release = 0,
		.num_on = ARRAY_SIZE(core00_on),
		.num_off = ARRAY_SIZE(core00_off),
		.num_status = ARRAY_SIZE(core00_status),
	},
	[CPU_CORE01] = {
		.id = CPU_CORE01,
		.release = 0,
		.on = core01_on,
		.off = core01_off,
		.status = core01_status,
		.num_release = 0,
		.num_on = ARRAY_SIZE(core01_on),
		.num_off = ARRAY_SIZE(core01_off),
		.num_status = ARRAY_SIZE(core01_status),
	},
};
unsigned int pmucal_cpu_list_size = ARRAY_SIZE(pmucal_cpu_list);

enum pmucal_cpu_clusternum {
	CPU_CLUSTER0,
	PMUCAL_NUM_CLUSTERS,
};

struct pmucal_cpu pmucal_cluster_list[PMUCAL_NUM_CLUSTERS] = {
	[CPU_CLUSTER0] = {
		.id = CPU_CLUSTER0,
		.on = 0,
		.off = 0,
		.status = cluster0_status,
		.num_on = 0,
		.num_off = 0,
		.num_status = ARRAY_SIZE(cluster0_status),
	},
};
unsigned int pmucal_cluster_list_size = ARRAY_SIZE(pmucal_cluster_list);

enum pmucal_opsnum {
	NUM_PMUCAL_OPTIONS,
};

struct pmucal_cpu pmucal_pmu_ops_list[] = {};

unsigned int pmucal_option_list_size = 0;

#else

/* individual sequence descriptor for each core - on, off, status */
struct pmucal_seq core00_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_CPU0_CONFIGURATION", 0x11860000, 0x2020, (0xf << 16), (0xF << 16), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 4), (0x0 << 4), 0, 0, 0, 0),
};
struct pmucal_seq core00_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_CPU0_CONFIGURATION", 0x11860000, 0x2020, (0xf << 0), (0xF << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_CPU0_CONFIGURATION", 0x11860000, 0x2020, (0xf << 16), (0x0 << 16), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CLUSTER0_CPU0_STATUS", 0x11860000, 0x2024, (0xf << 0), (0xF << 0), 0x11860000, 0x2024, (0xf << 0), (0xF << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP1", 0x11860000, 0x7f04, (0x1 << 4), (0x0 << 4), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 4), (0x0 << 4), 0, 0, 0, 0),
};
struct pmucal_seq core00_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_CPU0_CONFIGURATION", 0x11860000, 0x2020, (0xf << 0), (0x0 << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 4), (0x1 << 4), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP1", 0x11860000, 0x7f04, (0x1 << 4), (0x1 << 4), 0, 0, 0, 0),
};
struct pmucal_seq core00_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_CPU0_STATUS", 0x11860000, 0x2024, (0xf << 0), 0, 0, 0, 0, 0),
};
struct pmucal_seq core01_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_CPU1_CONFIGURATION", 0x11860000, 0x2028, (0xf << 16), (0xF << 16), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 5), (0x0 << 5), 0, 0, 0, 0),
};
struct pmucal_seq core01_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_CPU1_CONFIGURATION", 0x11860000, 0x2028, (0xf << 0), (0xF << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP1", 0x11860000, 0x7f04, (0x1 << 5), (0x0 << 5), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 5), (0x0 << 5), 0, 0, 0, 0),
};
struct pmucal_seq core01_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_CPU1_CONFIGURATION", 0x11860000, 0x2028, (0xf << 0), (0x0 << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GRP1_INTR_BID_ENABLE", 0x11870000, 0x0100, (0x1 << 5), (0x1 << 5), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP1", 0x11860000, 0x7f04, (0x1 << 5), (0x1 << 5), 0, 0, 0, 0),
};
struct pmucal_seq core01_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_CPU1_STATUS", 0x11860000, 0x202c, (0xf << 0), 0, 0, 0, 0, 0),
};
struct pmucal_seq cluster0_on[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_NONCPU_CONFIGURATION", 0x11860000, 0x2048, (0xf << 0), (0xF << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "LPI_CSSYS_CONFIGURATION", 0x11860000, 0x2b60, (0x1 << 0), (0x1 << 0), 0, 0, 0, 0),
};
struct pmucal_seq cluster0_off[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_CPU0_ETC_OPTION", 0x11860000, 0x3c48, (0x1 << 17), (0x0 << 17), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_CPU1_ETC_OPTION", 0x11860000, 0x3c58, (0x1 << 17), (0x0 << 17), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "LPI_CSSYS_CONFIGURATION", 0x11860000, 0x2b60, (0x1 << 0), (0x0 << 0), 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE_WAIT, "CLUSTER0_NONCPU_CONFIGURATION", 0x11860000, 0x2048, (0xf << 0), (0x0 << 0), 0, 0, 0, 0),
};
struct pmucal_seq cluster0_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "CLUSTER0_NONCPU_STATUS", 0x11860000, 0x204c, (0xf << 0), 0, 0, 0, 0, 0),
};
enum pmucal_cpu_corenum {
	CPU_CORE00,
	CPU_CORE01,
	PMUCAL_NUM_CORES,
};

struct pmucal_cpu pmucal_cpu_list[PMUCAL_NUM_CORES] = {
	[CPU_CORE00] = {
		.id = CPU_CORE00,
		.release = core00_release,
		.on = core00_on,
		.off = core00_off,
		.status = core00_status,
		.num_release = ARRAY_SIZE(core00_release),
		.num_on = ARRAY_SIZE(core00_on),
		.num_off = ARRAY_SIZE(core00_off),
		.num_status = ARRAY_SIZE(core00_status),
	},
	[CPU_CORE01] = {
		.id = CPU_CORE01,
		.release = core01_release,
		.on = core01_on,
		.off = core01_off,
		.status = core01_status,
		.num_release = ARRAY_SIZE(core01_release),
		.num_on = ARRAY_SIZE(core01_on),
		.num_off = ARRAY_SIZE(core01_off),
		.num_status = ARRAY_SIZE(core01_status),
	},
};
unsigned int pmucal_cpu_list_size = ARRAY_SIZE(pmucal_cpu_list);

enum pmucal_cpu_clusternum {
	CPU_CLUSTER0,
	PMUCAL_NUM_CLUSTERS,
};

struct pmucal_cpu pmucal_cluster_list[PMUCAL_NUM_CLUSTERS] = {
	[CPU_CLUSTER0] = {
		.id = CPU_CLUSTER0,
		.on = cluster0_on,
		.off = cluster0_off,
		.status = cluster0_status,
		.num_on = ARRAY_SIZE(cluster0_on),
		.num_off = ARRAY_SIZE(cluster0_off),
		.num_status = ARRAY_SIZE(cluster0_status),
	},
};
unsigned int pmucal_cluster_list_size = ARRAY_SIZE(pmucal_cluster_list);

enum pmucal_opsnum {
	NUM_PMUCAL_OPTIONS,
};

struct pmucal_cpu pmucal_pmu_ops_list[] = {};

unsigned int pmucal_option_list_size = 0;

struct cpu_info cpuinfo[] = {
	[0] = {
		.min = 0,
		.max = 1,
		.total = 2,
	},
};
#endif
