#include <linux/list.h>
#include <linux/slab.h>
#include "sspt_filter.h"
#include "sspt_proc.h"

struct sspt_filter *sspt_filter_create(struct pf_group *pfg)
{
	struct sspt_filter *fl;

	fl = kmalloc(sizeof(*fl), GFP_KERNEL);
	if (fl == NULL)
		return NULL;

	INIT_LIST_HEAD(&fl->list);
	fl->pfg = pfg;

	return fl;
}

void sspt_filter_free(struct sspt_filter *fl)
{
	kfree(fl);
}
