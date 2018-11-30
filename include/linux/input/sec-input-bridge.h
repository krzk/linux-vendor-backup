#ifndef SEC_INPUT_BRIDGE_H
#define SEC_INPUT_BRIDGE_H

#include <linux/kobject.h>

extern struct class *sec_class;

enum mkey_check_option {
	MKEY_CHECK_AUTO,
	MKEY_CHECK_AWAYS
};

struct sec_input_bridge_mkey {
	unsigned int type;
	unsigned int code;
	enum mkey_check_option option;
};

struct sec_input_bridge_onekey {
	char *onekey_env_str;
	char *onekey_env_value;
	int key_cnt;
	int debug_low_enable;
};

struct sec_input_bridge_mmap {
	struct sec_input_bridge_mkey *mkey_map;
	bool enable;
	unsigned int num_mkey;
	char *uevent_env_str;
	char *uevent_env_value;
	unsigned char enable_uevent;
	void (*pre_event_func)(void *event_data);
	enum kobject_action uevent_action;
};

struct sec_input_bridge_platform_data {
	void *event_data;

	struct sec_input_bridge_mmap *mmap;
	unsigned int num_map;
	char **support_dev_name;
	unsigned int support_dev_num;
	struct sec_input_bridge_onekey *onekey_map;
	int detected_onekey;
	unsigned int num_onekey;
	unsigned int onekey_timeout;
};

#endif /* LINUX_INPUT_SEC_INPUT_BRIDGE_H */

