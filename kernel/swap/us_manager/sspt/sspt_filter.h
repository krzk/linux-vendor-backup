#ifndef __SSPT_FILTER_H__
#define __SSPT_FILTER_H__

struct pf_group;

struct sspt_filter {
	struct list_head list;
	struct pf_group *pfg;
};

struct sspt_filter *sspt_filter_create(struct pf_group *pfg);
void sspt_filter_free(struct sspt_filter *fl);

#endif /* __SSPT_FILTER_H__ */
