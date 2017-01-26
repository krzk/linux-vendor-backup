/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/string.h>
#include <uprobe/swap_uaccess.h>
#include <us_manager/sspt/sspt.h>
#include <us_manager/probes/probe_info_new.h>
#include "wsp.h"
#include "wsp_res.h"
#include "wsp_msg.h"


struct wsp_probe {
	const char *name;
	struct probe_new probe;
};

struct wsp_bin {
	const char *name;
	unsigned long cnt;
	struct wsp_probe *probe_array;
};


static char *webapp_path = NULL;
static char *ewebkit_path = NULL;


#define WSP_PROBE_MAKE(__name, __desc)	\
{					\
	.name = __name,			\
	.probe.offset = 0,		\
	.probe.desc = __desc		\
}


static void do_res_processing_begin(void *data, void *ptr, enum wsp_res_t type)
{
	struct wsp_res **save_res = (struct wsp_res **)data;
	struct wsp_res *res;

	res = wsp_res_find(ptr, type);

	/* save res pointer */
	*save_res = res;
	if (res) {
		wsp_msg(WSP_RES_PROC_BEGIN, res->id, NULL);
		wsp_res_stat_set_next(res, WRS_ADD_DATA);
	}
}

static void do_res_processing_end(struct wsp_res *res)
{
	wsp_msg(WSP_RES_PROC_END, res->id, NULL);
}

static void do_res_finish(struct wsp_res *res)
{
	wsp_msg(WSP_RES_LOAD_END, res->id, NULL);
	wsp_res_stat_set_next(res, WRS_FINISH);
	wsp_res_del(res);
}


/*
 * soup_req
 */
static int soup_req_handle(struct uprobe *p, struct pt_regs *regs)
{
	enum { max_str_len = 512 };
	const char __user *user_s;
	const char *path;
	struct wsp_res *res;

	res = wsp_res_last();
	if (res == NULL) {
		pr_err("last wsp_res is not found\n");
		return 0;
	}

	user_s = (const char __user *)swap_get_uarg(regs, 1);
	path = strdup_from_user(user_s, GFP_ATOMIC);
	if (path == NULL) {
		pr_warn("soup_req_handle: invalid path\n");
		return 0;
	}

	wsp_msg(WSP_RES_LOAD_BEGIN, res->id, path);
	wsp_res_stat_set_next(res, WRS_SOUP_REQ);
	kfree(path);

	return 0;
}

static struct probe_desc soup_req = MAKE_UPROBE(soup_req_handle);


/*
 * main_res_req
 */
static int mres_req_handle(struct uprobe *p, struct pt_regs *regs)
{
	void *ptr = (void *)swap_get_uarg(regs, 0);
	struct wsp_res *res;

	res = wsp_res_new(ptr, WR_MAIN);
	if (res)
		wsp_res_stat_set_next(res, WRS_WILL_REQ);

	return 0;
}

static struct probe_desc mres_req = MAKE_UPROBE(mres_req_handle);


/*
 * main_res_add_data
 */

static int mres_adata_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	void *ptr = (void *)swap_get_uarg(regs, 0);

	do_res_processing_begin(ri->data, ptr, WR_MAIN);

	return 0;
}

static int mres_adata_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct wsp_res *res = *(struct wsp_res **)ri->data;

	if (res)
		do_res_processing_end(res);

	return 0;
}

static struct probe_desc mres_adata =
		MAKE_URPROBE(mres_adata_eh, mres_adata_rh,
			     sizeof(struct wsp_res *));


/*
 * main_res_finish
 */
static int mres_finish_handle(struct uprobe *p, struct pt_regs *regs)
{
	void *ptr = (void *)swap_get_uarg(regs, 0);
	struct wsp_res *res;

	res = wsp_res_find(ptr, WR_MAIN);
	if (res) {
		wsp_current_set_stat(TDS_FINISH_MAIN_LOAD);
		do_res_finish(res);
	}

	return 0;
}

static struct probe_desc mres_finish = MAKE_UPROBE(mres_finish_handle);


/*
 * res_request
 */
static int res_request_handle(struct uprobe *p, struct pt_regs *regs)
{
	void *ptr = (void *)swap_get_uarg(regs, 0);
	struct wsp_res *res;

	res = wsp_res_last();
	if (res) {
		if (res->type == WR_MAIN && res->stat == WRS_WILL_REQ)
			/* skip */
			return 0;
	}

	res = wsp_res_new(ptr, WR_ANY);
	if (res)
		wsp_res_stat_set_next(res, WRS_WILL_REQ);

	return 0;
}

static struct probe_desc res_request = MAKE_UPROBE(res_request_handle);


/*
 * res_finish
 */
static int res_finish_ehandle(struct uretprobe_instance *ri,
			      struct pt_regs *regs)
{
	void *ptr = (void *)swap_get_uarg(regs, 0);

	do_res_processing_begin(ri->data, ptr, WR_ANY);

	return 0;
}

static int res_finish_rhandle(struct uretprobe_instance *ri,
			      struct pt_regs *regs)
{
	struct wsp_res *res = *(struct wsp_res **)ri->data;

	if (res) {
		do_res_processing_end(res);
		do_res_finish(res);
	}

	return 0;
}

static struct probe_desc res_finish =
		MAKE_URPROBE(res_finish_ehandle, res_finish_rhandle,
			     sizeof(struct wsp_res *));


/*
 * redraw
 */
static int redraw_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	enum tdata_stat stat;

	stat = wsp_current_get_stat();

	if (stat == TDS_FINISH_MAIN_LOAD)
		wsp_msg(WSP_DRAW_BEGIN, 0, NULL);

	return 0;
}

static int redraw_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	if (wsp_current_get_stat() == TDS_FINISH_MAIN_LOAD) {
		wsp_current_set_stat(TDS_DRAW);
		wsp_msg(WSP_DRAW_END, 0, NULL);
	}

	return 0;
}

static struct probe_desc redraw = MAKE_URPROBE(redraw_eh, redraw_rh, 0);


static struct wsp_probe ewebkit_probe_array[] = {
	/* plt */
	/* soup_requester_request@plt */
	WSP_PROBE_MAKE("soup_requester_request@plt", &soup_req),

	/* main_res */
	/* WebCore::MainResourceLoader::willSendRequest(WebCore::ResourceRequest&, WebCore::ResourceResponse const&) */
	WSP_PROBE_MAKE("_ZN7WebCore18MainResourceLoader15willSendRequestERNS_15ResourceRequestERKNS_16ResourceResponseE", &mres_req),
	/* WebCore::MainResourceLoader::addData(char const*, int, bool) */
	WSP_PROBE_MAKE("_ZN7WebCore18MainResourceLoader7addDataEPKcib", &mres_adata),
	/* WebCore::MainResourceLoader::didFinishLoading(double) */
	WSP_PROBE_MAKE("_ZN7WebCore18MainResourceLoader16didFinishLoadingEd", &mres_finish),


	/* res */
	/* WebCore::ResourceLoader::willSendRequest(WebCore::ResourceRequest&, WebCore::ResourceResponse const&) */
	WSP_PROBE_MAKE("_ZN7WebCore14ResourceLoader15willSendRequestERNS_15ResourceRequestERKNS_16ResourceResponseE", &res_request),
	/* WebCore::ResourceLoader::didFinishLoading(WebCore::ResourceHandle*, double) */
	WSP_PROBE_MAKE("_ZN7WebCore14ResourceLoader16didFinishLoadingEPNS_14ResourceHandleEd", &res_finish),


	/* redraw */
	/* WebKit::LayerTreeCoordinator::flushPendingLayerChanges() */
	WSP_PROBE_MAKE("_ZN6WebKit20LayerTreeCoordinator24flushPendingLayerChangesEv", &redraw),
};

enum {
	ewebkit_probes_cnt =
		sizeof(ewebkit_probe_array) / sizeof(struct wsp_probe)
};

static struct wsp_bin ewebkit = {
	.name = NULL,
	.cnt = ewebkit_probes_cnt,
	.probe_array = ewebkit_probe_array
};


/* check ewebkit_probe_array on init address */
static bool wsp_is_addr_init(void)
{
	int i;

	for (i = 0; i < ewebkit_probes_cnt; ++i)
		if (ewebkit_probe_array[i].probe.offset == 0)
			return false;

	return true;
}


static int wsp_probe_register(struct pf_group *pfg, struct dentry *dentry,
			      struct wsp_probe *wsp_probe)
{
	struct probe_new *probe_new = &wsp_probe->probe;

	return pin_register(probe_new, pfg, dentry);
}

static void wsp_probe_unregister(struct pf_group *pfg, struct dentry *dentry,
				 struct wsp_probe *wsp_probe)
{
	struct probe_new *probe_new = &wsp_probe->probe;

	pin_unregister(probe_new, pfg, dentry);
}




static int wsp_bin_register(struct pf_group *pfg, struct wsp_bin *bin)
{
	int i, ret;
	struct dentry *dentry;

	dentry = dentry_by_path(bin->name);
	if (dentry == NULL) {
		pr_err("dentry not found (path='%s'\n", bin->name);
		return -EINVAL;
	}

	for (i = 0; i < bin->cnt; ++i) {
		struct wsp_probe *p = &bin->probe_array[i];

		ret = wsp_probe_register(pfg, dentry, p);
		if (ret) {
			pr_err("ERROR: wsp_probe_register, addr=%lx ret=%d\n",
			       p->probe.offset, ret);
			return ret;
		}
	}

	return 0;
}

static void wsp_bin_unregister(struct pf_group *pfg, struct wsp_bin *bin)
{
	int i;
	struct dentry *dentry;

	dentry = dentry_by_path(bin->name);
	for (i = 0; i < bin->cnt; ++i)
		wsp_probe_unregister(pfg, dentry, &bin->probe_array[i]);
}

static void do_set_path(char **dest, char *path, size_t len)
{
	*dest = kmalloc(len, GFP_KERNEL);
	if (*dest == NULL) {
		printk("Not enough memory to init path\n");
		return;
	}

	strncpy(*dest, path, len);
}

static void do_free_path(char **dest)
{
	kfree(*dest);
	*dest = NULL;
}


static struct pf_group *g_pfg;

static int wsp_app_register(void)
{
	struct dentry *dentry;

	if (webapp_path == NULL || ewebkit_path == NULL) {
		printk("WSP: some required paths are not set!\n");
		return -EINVAL;
	}

	ewebkit.name = ewebkit_path;

	dentry = dentry_by_path(webapp_path);
	if (dentry == NULL) {
		pr_err("dentry not found (path='%s'\n", webapp_path);
		return -EINVAL;
	}

	g_pfg = get_pf_group_by_dentry(dentry, (void *)dentry);
	if (g_pfg == NULL) {
		pr_err("g_pfg is NULL (by dentry=%p)\n", dentry);
		return -ENOMEM;
	}

	return wsp_bin_register(g_pfg, &ewebkit);
}

static void wsp_app_unregister(void)
{
	if (ewebkit.name != NULL) {
		printk("WSP: ewebkit path is not initialized\n");
		return;
	}

	wsp_bin_unregister(g_pfg, &ewebkit);
	put_pf_group(g_pfg);
}


static int do_wsp_on(void)
{
	int ret;

	ret = wsp_res_init();
	if (ret)
		return ret;

	ret = wsp_app_register();
	if (ret)
		wsp_res_exit();

	return ret;
}

static void do_wsp_off(void)
{
	wsp_app_unregister();
	wsp_res_exit();
	do_free_path(&webapp_path);
	do_free_path(&ewebkit_path);
}


static enum wsp_mode g_mode = WSP_OFF;
static DEFINE_MUTEX(g_mode_mutex);

int wsp_set_addr(const char *name, unsigned long offset)
{
	int i, ret = 0;

	if (mutex_trylock(&g_mode_mutex) == 0)
		return -EBUSY;

	for (i = 0; i < ewebkit_probes_cnt; ++i) {
		if (0 == strcmp(name, ewebkit_probe_array[i].name)) {
			ewebkit_probe_array[i].probe.offset = offset;
			goto unlock;
		}
	}

	ret = -EINVAL;

unlock:
	mutex_unlock(&g_mode_mutex);
	return ret;
}

int wsp_set_mode(enum wsp_mode mode)
{
	int ret = 0;

	mutex_lock(&g_mode_mutex);
	switch (mode) {
	case WSP_ON:
		if (g_mode == WSP_ON) {
			ret = -EBUSY;
			goto unlock;
		}

		ret = wsp_is_addr_init() ? do_wsp_on() : -EPERM;
		break;
	case WSP_OFF:
		if (g_mode == WSP_OFF) {
			ret = -EBUSY;
			goto unlock;
		}

		do_wsp_off();
		break;
	default:
		ret = -EINVAL;
		break;
	}

unlock:
	if (ret == 0)
		g_mode = mode;

	mutex_unlock(&g_mode_mutex);
	return ret;
}

enum wsp_mode wsp_get_mode(void)
{
	return g_mode;
}

void wsp_set_webapp_path(char *path, size_t len)
{
	do_free_path(&webapp_path);
	do_set_path(&webapp_path, path, len);
}

void wsp_set_ewebkit_path(char *path, size_t len)
{
	do_free_path(&ewebkit_path);
	do_set_path(&ewebkit_path, path, len);
}

int wsp_init(void)
{
	return 0;
}

void wsp_exit(void)
{
	wsp_set_mode(WSP_OFF);
}
