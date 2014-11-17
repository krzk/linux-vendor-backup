#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <uapi/linux/dma-buf-test.h>

struct dmabuf_file {
	struct device *dev;
	struct dma_buf *buf;
	dma_addr_t phys;
	size_t size;
	void *virt;
	int fd;
};

static int dmabuf_attach(struct dma_buf *buf, struct device *dev,
			 struct dma_buf_attachment *attach)
{
	return 0;
}

static void dmabuf_detach(struct dma_buf *buf,
			  struct dma_buf_attachment *attach)
{
}

static struct sg_table *dmabuf_map_dma_buf(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct dmabuf_file *priv = attach->dmabuf->priv;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
		kfree(sgt);
		return NULL;
	}

	sg_dma_address(sgt->sgl) = priv->phys;
	sg_dma_len(sgt->sgl) = priv->size;

	return sgt;
}

static void dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void dmabuf_release(struct dma_buf *buf)
{
}

static int dmabuf_begin_cpu_access(struct dma_buf *buf, size_t size,
				   size_t length,
				   enum dma_data_direction direction)
{
	struct dmabuf_file *priv = buf->priv;

	dma_sync_single_for_cpu(priv->dev, priv->phys, priv->size, direction);

	return 0;
}

static void dmabuf_end_cpu_access(struct dma_buf *buf, size_t size,
				  size_t length,
				  enum dma_data_direction direction)
{
	struct dmabuf_file *priv = buf->priv;

	dma_sync_single_for_device(priv->dev, priv->phys, priv->size,
				   direction);
}

static void *dmabuf_kmap_atomic(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void dmabuf_kunmap_atomic(struct dma_buf *buf, unsigned long page,
				 void *vaddr)
{
}

static void *dmabuf_kmap(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void dmabuf_kunmap(struct dma_buf *buf, unsigned long page, void *vaddr)
{
}

static void dmabuf_vm_open(struct vm_area_struct *vma)
{
}

static void dmabuf_vm_close(struct vm_area_struct *vma)
{
}

static int dmabuf_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return 0;
}

static const struct vm_operations_struct dmabuf_vm_ops = {
	.open = dmabuf_vm_open,
	.close = dmabuf_vm_close,
	.fault = dmabuf_vm_fault,
};

static int dmabuf_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	pgprot_t prot = vm_get_page_prot(vma->vm_flags);
	struct dmabuf_file *priv = buf->priv;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &dmabuf_vm_ops;
	vma->vm_private_data = priv;
	vma->vm_page_prot = pgprot_writecombine(prot);

	return remap_pfn_range(vma, vma->vm_start, priv->phys >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static void *dmabuf_vmap(struct dma_buf *buf)
{
	return NULL;
}

static void dmabuf_vunmap(struct dma_buf *buf, void *vaddr)
{
}

static const struct dma_buf_ops dmabuf_ops = {
	.attach = dmabuf_attach,
	.detach = dmabuf_detach,
	.map_dma_buf = dmabuf_map_dma_buf,
	.unmap_dma_buf = dmabuf_unmap_dma_buf,
	.release = dmabuf_release,
	.begin_cpu_access = dmabuf_begin_cpu_access,
	.end_cpu_access = dmabuf_end_cpu_access,
	.kmap_atomic = dmabuf_kmap_atomic,
	.kunmap_atomic = dmabuf_kunmap_atomic,
	.kmap = dmabuf_kmap,
	.kunmap = dmabuf_kunmap,
	.mmap = dmabuf_mmap,
	.vmap = dmabuf_vmap,
	.vunmap = dmabuf_vunmap,
};

static int dmabuf_file_open(struct inode *inode, struct file *file)
{
	struct dmabuf_file *priv;
	int ret = 0;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	file->private_data = priv;

	return ret;
}

static int dmabuf_file_release(struct inode *inode, struct file *file)
{
	struct dmabuf_file *priv = file->private_data;
	int ret = 0;

	if (priv->virt)
		dma_free_writecombine(priv->dev, priv->size, priv->virt,
				      priv->phys);

	if (priv->buf)
		dma_buf_put(priv->buf);

	kfree(priv);

	return ret;
}

static int dmabuf_ioctl_create(struct dmabuf_file *priv,
			       const void __user *data)
{
	struct dmabuf_create args;
	int ret = 0;

	if (priv->buf || priv->virt)
		return -EBUSY;

	if (copy_from_user(&args, data, sizeof(args)))
		return -EFAULT;

	priv->virt = dma_alloc_writecombine(priv->dev, args.size, &priv->phys,
					    GFP_KERNEL | __GFP_NOWARN);
	if (!priv->virt)
		return -ENOMEM;

	args.flags |= O_RDWR;

	priv->buf = dma_buf_export(priv, &dmabuf_ops, args.size, args.flags,
				   NULL);
	if (!priv->buf) {
		ret = -ENOMEM;
		goto free;
	}

	if (IS_ERR(priv->buf)) {
		ret = PTR_ERR(priv->buf);
		goto free;
	}

	priv->size = args.size;

	return 0;

free:
	dma_free_writecombine(NULL, priv->size, priv->virt, priv->phys);
	priv->virt = NULL;
	return ret;
}

static int dmabuf_ioctl_delete(struct dmabuf_file *priv, unsigned long flags)
{
	dma_free_writecombine(NULL, priv->size, priv->virt, priv->phys);
	priv->virt = NULL;
	priv->phys = 0;
	priv->size = 0;

	dma_buf_put(priv->buf);
	priv->buf = NULL;

	return 0;
}

static int dmabuf_ioctl_export(struct dmabuf_file *priv, unsigned long flags)
{
	int err;

	struct dmabuf_create *buf = (struct dmabuf_create *)flags;
	get_dma_buf(priv->buf);

	err = dma_buf_fd(priv->buf, flags);
	if (err < 0)
		dma_buf_put(priv->buf);

	priv->fd = err;

	if (copy_to_user(&buf->fd, &err, 4))
		return -EFAULT;

	return 0;
}

static long dmabuf_file_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	struct dmabuf_file *priv = file->private_data;
	long ret = 0;

	switch (cmd) {
	case DMABUF_IOCTL_CREATE:
		ret = dmabuf_ioctl_create(priv, (const void __user *)arg);
		break;

	case DMABUF_IOCTL_DELETE:
		ret = dmabuf_ioctl_delete(priv, arg);
		break;

	case DMABUF_IOCTL_EXPORT:
		ret = dmabuf_ioctl_export(priv, arg);
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static const struct file_operations dmabuf_fops = {
	.owner = THIS_MODULE,
	.open = dmabuf_file_open,
	.release = dmabuf_file_release,
	.unlocked_ioctl = dmabuf_file_ioctl,
};

static struct miscdevice dmabuf_device = {
	.minor = 128,
	.name = "dmabuf",
	.fops = &dmabuf_fops,
};

static int __init dmabuf_init(void)
{
	return misc_register(&dmabuf_device);
}
module_init(dmabuf_init);

static void __exit dmabuf_exit(void)
{
	misc_deregister(&dmabuf_device);
}
module_exit(dmabuf_exit);

MODULE_AUTHOR("Thierry Reding <treding at nvidia.com>");
MODULE_DESCRIPTION("DMA-BUF test driver");
MODULE_LICENSE("GPL v2");
