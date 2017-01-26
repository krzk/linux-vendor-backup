#ifndef __PRELOAD_MODULE_H__
#define __PRELOAD_MODULE_H__

struct sspt_ip;
struct dentry;

int pm_uprobe_init(struct sspt_ip *ip);
void pm_uprobe_exit(struct sspt_ip *ip);

int pm_get_caller_init(struct sspt_ip *ip);
void pm_get_caller_exit(struct sspt_ip *ip);
int pm_get_call_type_init(struct sspt_ip *ip);
void pm_get_call_type_exit(struct sspt_ip *ip);
int pm_write_msg_init(struct sspt_ip *ip);
void pm_write_msg_exit(struct sspt_ip *ip);
int pm_set_handler(char *path);

struct dentry *get_dentry(const char *filepath);
void put_dentry(struct dentry *dentry);

#endif /* __PRELOAD_MODULE_H__ */
