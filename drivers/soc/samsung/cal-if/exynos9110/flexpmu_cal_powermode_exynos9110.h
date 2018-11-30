struct cpu_inform pmucal_cpuinform_list[] = {
	PMUCAL_CPU_INFORM(0, 0x11860000, 0x0860),
	PMUCAL_CPU_INFORM(1, 0x11860000, 0x0864),
};
unsigned int cpu_inform_list_size = ARRAY_SIZE(pmucal_cpuinform_list);
