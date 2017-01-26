#ifndef __LOADER_CONTROL_H__
#define __LOADER_CONTROL_H__

struct dentry;

int lc_init(void);
void lc_exit(void);

int lc_add_ignored_binary(char *filename);
int lc_clean_ignored_bins(void);

unsigned int lc_get_ignored_names(char ***filenames_p);
void lc_release_ignored_names(char ***filenames_p);

bool lc_check_dentry_is_ignored(struct dentry *dentry);

#endif /* __LOADER_CONTROL_H__ */
