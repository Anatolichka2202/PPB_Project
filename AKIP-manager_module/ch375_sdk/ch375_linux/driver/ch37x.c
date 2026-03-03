/*
 * USB device driver for ch372/ch374/ch375/ch376/ch378 etc.
 *
 * Copyright (C) 2023 Nanjing Qinheng Microelectronics Co., Ltd.
 * Web:      http://wch.cn
 * Author:   WCH <tech@wch.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Update Log:
 *
 * V1.0 - initial version
 * V1.1 - improve efficiency and add support for more endpoints
        - add support for synchronous transmission
 * V1.2 - add support for higher linux kernel
 *      - add memory management for seprate endpoint
 *
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif

#define DRIVER_AUTHOR "WCH"
#define DRIVER_DESC   "USB device driver for USB chip ch372/ch374/ch375/ch376/ch378 etc."
#define VERSION_DESC  "V1.2 On 2022.11"

/* Get a minor range for your devices from the usb maintainer */
#define USB_MINOR_BASE 200

/* USB transfer in and out URB amount */
#define CH37X_NW 16
#define CH37X_NR 16

/* USB rx buffer amounts for bulk in endpoint */
#define USB_RX_BUF_UNIT 100 * 1

/* Maxium of endpoints amounts */
#define MAX_EP_NUM 8

/* ioctl commands for interaction between driver and application */
#define IOCTL_MAGIC 'W'

#define GetVersion     _IOR(IOCTL_MAGIC, 0x80, u16)
#define GetDeviceID    _IOR(IOCTL_MAGIC, 0x81, u16)
#define GetDeviceEpMsg _IOR(IOCTL_MAGIC, 0x82, u16)
#define GetDeviceDes   _IOR(IOCTL_MAGIC, 0x83, u16)

#define CH37X_SET_EXCLUSIVE   _IOW(IOCTL_MAGIC, 0xa0, u16)
#define START_BUFFERED_UPLOAD _IOW(IOCTL_MAGIC, 0xa1, u16)
#define STOP_BUFFERED_UPLOAD  _IOW(IOCTL_MAGIC, 0xa2, u16)
#define QWERY_BUFFERED_UPLOAD _IOWR(IOCTL_MAGIC, 0xa3, u16)

#define CH37X_BULK_ASYNC_READ  _IOWR(IOCTL_MAGIC, 0xb0, u16)
#define CH37X_BULK_ASYNC_WRITE _IOW(IOCTL_MAGIC, 0xb1, u16)
#define CH37X_SET_TIMEOUT      _IOW(IOCTL_MAGIC, 0xc0, u16)
#define CH37X_BULK_SYNC_READ   _IOWR(IOCTL_MAGIC, 0xc1, u16)
#define CH37X_BULK_SYNC_WRITE  _IOW(IOCTL_MAGIC, 0xc2, u16)
#define CH37X_CTRL_SYNC_READ   _IOWR(IOCTL_MAGIC, 0xc3, u16)
#define CH37X_CTRL_SYNC_WRITE  _IOW(IOCTL_MAGIC, 0xc4, u16)
#define CH37X_INT_SYNC_READ    _IOWR(IOCTL_MAGIC, 0xc5, u16)
#define CH37X_INT_SYNC_WRITE   _IOW(IOCTL_MAGIC, 0xc6, u16)

/* Table of devices that work with this driver */
static const struct usb_device_id ch37x_table[] = {
	{ USB_DEVICE(0x1a86, 0x5537) }, /* ch375 device mode */
	{ USB_DEVICE(0x4348, 0x5537) }, /* ch375 device mode */
	{ USB_DEVICE(0x1a86, 0x5576) }, /* ch376 device mode */
	{ USB_DEVICE(0x4348, 0x5538) }, /* ch378 device mode */
	{ USB_DEVICE(0x4348, 0x55e0) }, /* usb module */
	{ USB_DEVICE(0x1a86, 0x55e0) }, /* usb module */
	{ USB_DEVICE(0x1a86, 0x55e1) }, /* usb module */
	{ USB_DEVICE(0x1a86, 0x55e2) }, /* usb device */
	{ USB_DEVICE(0x1a86, 0x55e3) }, /* usb device */
	{ USB_DEVICE(0x1a86, 0xe035) }, /* usb module */
	{ USB_DEVICE(0x1a86, 0xe036) }, /* usb module */
	{ USB_DEVICE(0x1a86, 0xe037) }, /* usb device0 */
	{ USB_DEVICE(0x1a86, 0xe038) }, /* usb device1 */
	{ USB_DEVICE(0x1a86, 0x55de) }, /* usb2.0 to jtag/spi device */
	{}				/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, ch37x_table);

/* structure for usb rx buffer */
struct usb_rx_buf {
	struct list_head list;
	u8 *data;
	u32 length;
	u32 pos;
};

struct ch37x_wb {
	unsigned char *buf;
	dma_addr_t dmah;
	int len;
	int use;
	struct urb *urb;
	struct usb_ch37x *instance;
	int epindex;
};

struct ch37x_rb {
	unsigned char *base;
	dma_addr_t dma;
	int index;
	struct usb_ch37x *instance;
	int epindex;
};

/* structure for usb bulk in */
struct ch37x_bulkepin {
	char *gusb_rx_buf;		   /* pointer to rx buffers */
	struct list_head usb_rx_list_free; /* list of free usb_rx_buf */
	struct list_head usb_rx_list_used; /* list of used usb_rx_buf */

	unsigned long read_urbs_free;		/* urb free status */
	struct urb *read_urbs[CH37X_NR];	/* urbs for bulk transfer in */
	struct ch37x_rb read_buffers[CH37X_NR]; /* usb read structure array */
	int rx_buflimit;			/* usb rx buffers limit */

	int rx_endpoint;       /* endpoint pipe */
	atomic_t free_entries; /* free entries amount */

	bool bsync;	  /* sync transfer flag */
	bool bread_valid; /* ready for read operation */
	bool boverflow;	  /* receive data overflow flag */
	bool buploaded;	  /* buffered mode status */
	bool ballocated;  /* rx buffers allocated status */

	spinlock_t throttle_lock;
	unsigned int throttled : 1; /* actually throttled */

	u8 epaddr;    /* endpoint address */
	u32 readsize; /* urb transfer size */
	int epsize;   /* endpoint size */

	wait_queue_head_t bulk_in_wait; /* to wait for an ongoing read */
};

/* structure for usb bulk out */
struct ch37x_bulkepout {
	struct ch37x_wb wb[CH37X_NW]; /* usb write structure array */
	int transmitting;	      /* usb transmit ongoing amounts */
	int tx_endpoint;	      /* endpoint pipe */
	u8 epaddr;		      /* endpoint address */
	u32 writesize;		      /* max packet size for the output bulk endpoint */
};

/* enumerator for endpoint type */
enum {
	EPTYPE_BULKIN = 0,
	EPTYPE_BULKOUT,
	EPTYPE_INTIN,
	EPTYPE_INTOUT,
};

struct ch37x_epmsg {
	u8 epnum;
	u8 epaddr[MAX_EP_NUM];
};

/* structure to hold all of our device specific stuff */
struct usb_ch37x {
	struct usb_device *udev;	 /* the usb device for this device */
	struct usb_interface *interface; /* the interface for this device */
	struct usb_anchor submitted;	 /* in case we need to retract our submissions */
	u16 ch37x_id[2];		 /* device vid and pid */

	struct ch37x_bulkepin bulkepin[MAX_EP_NUM];   /* bulk in endpoint structure array */
	struct ch37x_bulkepout bulkepout[MAX_EP_NUM]; /* bulk out endpoint structure array */
	int int_rx_endpoint[MAX_EP_NUM];	      /* interrupt in endpoint pipe array */
	int int_tx_endpoint[MAX_EP_NUM];	      /* interrupt out endpoint pipe array */
	int readtimeout;			      /* usb read timeout */
	int writetimeout;			      /* usb write timeout */

	struct ch37x_epmsg epmsg_bulkin;  /* structure used to transfer to application */
	struct ch37x_epmsg epmsg_bulkout; /* structure used to transfer to application */
	struct ch37x_epmsg epmsg_intin;	  /* structure used to transfer to application */
	struct ch37x_epmsg epmsg_intout;  /* structure used to transfer to application */

	unsigned int f_flags; /* file access modes */

	int errors;	       /* the last request tanked */
	spinlock_t err_lock;   /* lock for errors */
	struct kref kref;      /* reference to this stuff */
	struct mutex io_mutex; /* synchronize I/O with disconnect */
	spinlock_t read_lock;
	spinlock_t write_lock;

	bool bexclusive; /* device exclusive access flag */
};

#define to_ch37x_dev(d) container_of(d, struct usb_ch37x, kref)

static struct usb_driver ch37x_driver;
static void ch37x_draw_down(struct usb_ch37x *usbch37x);
static int ch37x_read_buffers_alloc(struct usb_ch37x *usbch37x, int epindex);
static void ch37x_read_buffers_free(struct usb_ch37x *usbch37x, int epindex);
static void ch37x_read_list_free(struct usb_ch37x *usbch37x, int epindex);
static void ch37x_bulk_read_throttle(struct usb_ch37x *usbch37x, int epindex);
static void ch37x_bulk_read_unthrottle(struct usb_ch37x *usbch37x, int epindex);

static void ch37x_delete_dev(struct kref *kref)
{
	struct usb_ch37x *usbch37x = to_ch37x_dev(kref);

	usb_put_dev(usbch37x->udev);
	kfree(usbch37x);
}

static void ch37x_delete(struct kref *kref)
{
	struct usb_ch37x *usbch37x = to_ch37x_dev(kref);
	int epindex;
	int i;

	usb_put_dev(usbch37x->udev);

	for (epindex = 0; epindex < usbch37x->epmsg_bulkin.epnum; epindex++) {
		if (usbch37x->bulkepin[epindex].ballocated == true) {
			for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
				usb_free_urb(usbch37x->bulkepin[epindex].read_urbs[i]);
			ch37x_read_buffers_free(usbch37x, epindex);
			ch37x_read_list_free(usbch37x, epindex);
			usbch37x->bulkepin[epindex].ballocated = false;
		}
	}

	kfree(usbch37x);
}

static int ch37x_open(struct inode *inode, struct file *file)
{
	struct usb_ch37x *usbch37x;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);

	interface = usb_find_interface(&ch37x_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n", __func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	usbch37x = usb_get_intfdata(interface);
	if (!usbch37x) {
		retval = -ENODEV;
		goto exit;
	}

	if (usbch37x->bexclusive) {
		retval = -EBUSY;
		goto exit;
	}

	retval = usb_autopm_get_interface(interface);
	if (retval)
		goto exit;

	/* increment our usage count for the device */
	kref_get(&usbch37x->kref);

	/* save our object in the file's private structure */
	file->private_data = usbch37x;

	/* save file descriptor flag */
	usbch37x->f_flags = file->f_flags;

exit:
	return retval;
}

static int ch37x_release(struct inode *inode, struct file *file)
{
	struct usb_ch37x *usbch37x;

	usbch37x = file->private_data;
	if (usbch37x == NULL)
		return -ENODEV;

	/* allow the device to be autosuspended */
	mutex_lock(&usbch37x->io_mutex);

	if (usbch37x->interface)
		usb_autopm_put_interface(usbch37x->interface);
	mutex_unlock(&usbch37x->io_mutex);

	if (usbch37x->bexclusive)
		usbch37x->bexclusive = false;

	/* decrement the count on our device */
	kref_put(&usbch37x->kref, ch37x_delete);

	return 0;
}

static int ch37x_flush(struct file *file, fl_owner_t id)
{
	struct usb_ch37x *usbch37x;
	int res;

	usbch37x = file->private_data;
	if (usbch37x == NULL)
		return -ENODEV;

	/* wait for io to stop */
	mutex_lock(&usbch37x->io_mutex);
	ch37x_draw_down(usbch37x);

	/* read out errors, leave subsequent opens a clean state */
	spin_lock_irq(&usbch37x->err_lock);
	res = usbch37x->errors ? (usbch37x->errors == -EPIPE ? -EPIPE : -EIO) : 0;
	usbch37x->errors = 0;
	spin_unlock_irq(&usbch37x->err_lock);

	mutex_unlock(&usbch37x->io_mutex);

	return res;
}

static int ch37x_submit_read_urb(struct usb_ch37x *usbch37x, int epindex, int i, gfp_t mem_flags)
{
	int res;

	if (!test_and_clear_bit(i, &usbch37x->bulkepin[epindex].read_urbs_free))
		return 0;

	res = usb_submit_urb(usbch37x->bulkepin[epindex].read_urbs[i], mem_flags);
	if (res) {
		if (res != -EPERM) {
			dev_err(&usbch37x->interface->dev, "%s - usb_submit_urb failed: %d\n", __func__, res);
		}
		set_bit(i, &usbch37x->bulkepin[epindex].read_urbs_free);
		return res;
	}

	return 0;
}

static int ch37x_submit_read_urbs(struct usb_ch37x *usbch37x, int epindex, gfp_t mem_flags)
{
	int res;
	int i;

	for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; ++i) {
		res = ch37x_submit_read_urb(usbch37x, epindex, i, mem_flags);
		if (res)
			return res;
	}

	return 0;
}

static void ch37x_process_read_urb(struct usb_ch37x *usbch37x, struct urb *urb, int epindex)
{
	struct usb_rx_buf *pusb_rx;
	unsigned long flags;

	if (!urb->actual_length)
		return;

	spin_lock_irqsave(&usbch37x->read_lock, flags);
	if (!list_empty(&usbch37x->bulkepin[epindex].usb_rx_list_free)) {
		pusb_rx = list_entry(usbch37x->bulkepin[epindex].usb_rx_list_free.next, struct usb_rx_buf, list);
		list_del(usbch37x->bulkepin[epindex].usb_rx_list_free.next);
		atomic_dec(&usbch37x->bulkepin[epindex].free_entries);
		spin_unlock_irqrestore(&usbch37x->read_lock, flags);
		if (pusb_rx != NULL) {
			pusb_rx->length = urb->actual_length;
			pusb_rx->pos = 0;
			memcpy(pusb_rx->data, urb->transfer_buffer, urb->actual_length);
			spin_lock_irqsave(&usbch37x->read_lock, flags);
			list_add_tail(&pusb_rx->list, &usbch37x->bulkepin[epindex].usb_rx_list_used);
			spin_unlock_irqrestore(&usbch37x->read_lock, flags);
		}
	} else {
		if (usbch37x->bulkepin[epindex].boverflow == false) {
			pr_info("endpoint %d read buffer overflow!\n", epindex);
			usbch37x->bulkepin[epindex].boverflow = true;
		}
		spin_unlock_irqrestore(&usbch37x->read_lock, flags);
	}
}

static void ch37x_read_bulk_callback(struct urb *urb)
{
	struct ch37x_rb *rb = urb->context;
	struct usb_ch37x *usbch37x = rb->instance;
	int status = urb->status;
	int epindex = rb->epindex;
	unsigned long flags;
	bool is_entries_free;

	if (!usbch37x->udev) {
		set_bit(rb->index, &usbch37x->bulkepin[epindex].read_urbs_free);
		dev_info(&usbch37x->interface->dev, "%s - disconnected\n", __func__);
		goto exit;
	}

	if (status) {
		set_bit(rb->index, &usbch37x->bulkepin[epindex].read_urbs_free);
		if (!(status == -ENOENT) || (status == -ECONNRESET) || (status == -ESHUTDOWN))
			dev_err(&usbch37x->interface->dev, "%s - nonzero read bulk status received: %d\n", __func__,
				status);
		usbch37x->errors = urb->status;
		goto exit;
	}

	usb_mark_last_busy(usbch37x->udev);
	ch37x_process_read_urb(usbch37x, urb, epindex);
	set_bit(rb->index, &usbch37x->bulkepin[epindex].read_urbs_free);

	/* throttle device if requested */
	spin_lock_irqsave(&usbch37x->bulkepin[epindex].throttle_lock, flags);
	if (!usbch37x->bulkepin[epindex].throttled) {
		spin_unlock_irqrestore(&usbch37x->bulkepin[epindex].throttle_lock, flags);
		ch37x_submit_read_urb(usbch37x, epindex, rb->index, GFP_ATOMIC);
	} else {
		spin_unlock_irqrestore(&usbch37x->bulkepin[epindex].throttle_lock, flags);
	}

	is_entries_free = atomic_read(&usbch37x->bulkepin[epindex].free_entries) < USB_RX_BUF_UNIT * 2 / 10 ? false :
													      true;

	if (!is_entries_free) {
		if (!usbch37x->bulkepin[epindex].throttled)
			ch37x_bulk_read_throttle(usbch37x, epindex);
	}

exit:
	spin_lock_irq(&usbch37x->err_lock);
	usbch37x->bulkepin[epindex].bread_valid = true;
	spin_unlock_irq(&usbch37x->err_lock);
	wake_up_interruptible(&usbch37x->bulkepin[epindex].bulk_in_wait);
}

/*
 * Finish write. Caller must hold write_lock
 */
static void ch37x_write_done(struct usb_ch37x *usbch37x, struct ch37x_wb *wb, int epindex)
{
	wb->use = 0;
	usbch37x->bulkepout[epindex].transmitting--;
	usb_autopm_put_interface_async(usbch37x->interface);
}

/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */
static int ch37x_wb_alloc(struct usb_ch37x *usbch37x, int epindex)
{
	int i, wbn;
	struct ch37x_wb *wb;

	wbn = 0;
	i = 0;
	for (;;) {
		wb = &usbch37x->bulkepout[epindex].wb[wbn];
		if (!wb->use) {
			wb->use = 1;
			return wbn;
		}
		wbn = (wbn + 1) % CH37X_NW;
		if (++i >= CH37X_NW)
			return -1;
	}
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */
static int ch37x_start_wb(struct usb_ch37x *usbch37x, struct ch37x_wb *wb, int epindex)
{
	int rc;

	usbch37x->bulkepout[epindex].transmitting++;

	wb->urb->transfer_buffer = wb->buf;
	wb->urb->transfer_dma = wb->dmah;
	wb->urb->transfer_buffer_length = wb->len;
	wb->urb->dev = usbch37x->udev;

	usb_anchor_urb(wb->urb, &usbch37x->submitted);
	rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
	if (rc < 0) {
		dev_err(&usbch37x->interface->dev, "%s - usb_submit_urb(write bulk) failed: %d\n", __func__, rc);
		ch37x_write_done(usbch37x, wb, epindex);
	}
	return rc;
}

static void ch37x_write_bulk(struct urb *urb)
{
	struct ch37x_wb *wb = urb->context;
	struct usb_ch37x *usbch37x = wb->instance;
	unsigned long flags;

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT || urb->status == -ECONNRESET || urb->status == -ESHUTDOWN))
			dev_err(&usbch37x->interface->dev, "%s - nonzero write bulk status received: %d\n", __func__,
				urb->status);

		spin_lock(&usbch37x->err_lock);
		usbch37x->errors = urb->status;
		spin_unlock(&usbch37x->err_lock);
	}
	spin_lock_irqsave(&usbch37x->write_lock, flags);
	ch37x_write_done(usbch37x, wb, wb->epindex);
	spin_unlock_irqrestore(&usbch37x->write_lock, flags);
}

static int ch37x_read_list_alloc(struct usb_ch37x *usbch37x, int epindex)
{
	char *pmem;
	int i;

	INIT_LIST_HEAD(&usbch37x->bulkepin[epindex].usb_rx_list_free);
	INIT_LIST_HEAD(&usbch37x->bulkepin[epindex].usb_rx_list_used);
	atomic_set(&usbch37x->bulkepin[epindex].free_entries, 0);

	usbch37x->bulkepin[epindex].gusb_rx_buf =
		(char *)kmalloc(sizeof(struct usb_rx_buf) * USB_RX_BUF_UNIT, GFP_KERNEL);
	if (!usbch37x->bulkepin[epindex].gusb_rx_buf) {
		dev_err(&usbch37x->interface->dev, "Could not allocate gusb_rx_buf\n");
		goto error;
	}

	pmem = usbch37x->bulkepin[epindex].gusb_rx_buf;
	for (i = 0; i < USB_RX_BUF_UNIT; i++) {
		struct usb_rx_buf *pusb_rx = (struct usb_rx_buf *)pmem;
		pusb_rx->data = kmalloc(usbch37x->bulkepin[epindex].readsize, GFP_KERNEL);
		if (!pusb_rx->data) {
			dev_err(&usbch37x->interface->dev, "Could not kmalloc pusb_rx data\n");
			goto error;
		}
		list_add_tail(&pusb_rx->list, &usbch37x->bulkepin[epindex].usb_rx_list_free);
		atomic_inc(&usbch37x->bulkepin[epindex].free_entries);
		pmem += sizeof(struct usb_rx_buf);
	}

	return 0;
error:
	ch37x_read_list_free(usbch37x, epindex);

	return -1;
}

static void ch37x_read_list_free(struct usb_ch37x *usbch37x, int epindex)
{
	struct usb_rx_buf *pusb_rx;
	unsigned long flags;

	spin_lock_irqsave(&usbch37x->read_lock, flags);
	while (!list_empty(&usbch37x->bulkepin[epindex].usb_rx_list_used)) {
		pusb_rx = list_entry(usbch37x->bulkepin[epindex].usb_rx_list_used.next, struct usb_rx_buf, list);
		list_del(usbch37x->bulkepin[epindex].usb_rx_list_used.next);
		if (pusb_rx != NULL) {
			list_add_tail(&pusb_rx->list, &usbch37x->bulkepin[epindex].usb_rx_list_free);
		}
	}
	while (!list_empty(&usbch37x->bulkepin[epindex].usb_rx_list_free)) {
		pusb_rx = list_entry(usbch37x->bulkepin[epindex].usb_rx_list_free.next, struct usb_rx_buf, list);
		list_del(usbch37x->bulkepin[epindex].usb_rx_list_free.next);
		if (pusb_rx != NULL) {
			if (pusb_rx->data) {
				kfree(pusb_rx->data);
				pusb_rx->data = NULL;
			}
		}
	}
	atomic_set(&usbch37x->bulkepin[epindex].free_entries, 0);
	spin_unlock_irqrestore(&usbch37x->read_lock, flags);

	if (usbch37x->bulkepin[epindex].gusb_rx_buf) {
		kfree(usbch37x->bulkepin[epindex].gusb_rx_buf);
		usbch37x->bulkepin[epindex].gusb_rx_buf = NULL;
	}
}

/* Little helpers: write/read buffers free */
static void ch37x_write_buffers_free(struct usb_ch37x *usbch37x)
{
	int i;
	struct ch37x_wb *wb;
	struct usb_device *usb_dev = interface_to_usbdev(usbch37x->interface);
	int epindex;

	for (epindex = 0; epindex < usbch37x->epmsg_bulkin.epnum; epindex++) {
		for (wb = &usbch37x->bulkepout[epindex].wb[0], i = 0; i < CH37X_NW; i++, wb++)
			usb_free_coherent(usb_dev, usbch37x->bulkepout[epindex].writesize, wb->buf, wb->dmah);
	}
}

static void ch37x_read_buffers_free(struct usb_ch37x *usbch37x, int epindex)
{
	int i;

	for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
		usb_free_coherent(usbch37x->udev, usbch37x->bulkepin[epindex].readsize,
				  usbch37x->bulkepin[epindex].read_buffers[i].base,
				  usbch37x->bulkepin[epindex].read_buffers[i].dma);
}

/* Little helper: write buffers allocate */
static int ch37x_write_buffers_alloc(struct usb_ch37x *usbch37x)
{
	int i;
	struct ch37x_wb *wb;
	int epindex;

	for (epindex = 0; epindex < usbch37x->epmsg_bulkout.epnum; epindex++) {
		for (wb = &usbch37x->bulkepout[epindex].wb[0], i = 0; i < CH37X_NW; i++, wb++) {
			wb->buf = usb_alloc_coherent(usbch37x->udev, usbch37x->bulkepout[epindex].writesize, GFP_KERNEL,
						     &wb->dmah);
			if (!wb->buf) {
				while (i != 0) {
					--i;
					--wb;
					usb_free_coherent(usbch37x->udev, usbch37x->bulkepout[epindex].writesize,
							  wb->buf, wb->dmah);
				}
				return -ENOMEM;
			}
		}
	}

	return 0;
}

/* Async endpoint bulk transfer */
static ssize_t ch37x_bulk_async_read(struct usb_ch37x *usbch37x, int epindex, void *pbuf, unsigned long count)
{
	struct usb_rx_buf *pusb_rx;
	int rv;
	int bytesRead;
	int pos = 0;
	int perlen;
	unsigned long flags;
	bool is_entries_free;

	/* if we cannot read at all, return EOF */
	if (count <= 0) {
		return -EINVAL;
	}

retry:
	/* errors must be reported */
	rv = usbch37x->errors;
	if (rv < 0) {
		/* any error is reported once */
		usbch37x->errors = 0;
		/* to preserve notifications about reset */
		rv = (rv == -EPIPE) ? rv : -EIO;
		/* report it */
		pr_info("errors here\n");
		return rv;
	}

	spin_lock_irqsave(&usbch37x->read_lock, flags);
	if (!list_empty(&usbch37x->bulkepin[epindex].usb_rx_list_used)) {
		pusb_rx = list_entry(usbch37x->bulkepin[epindex].usb_rx_list_used.next, struct usb_rx_buf, list);
		spin_unlock_irqrestore(&usbch37x->read_lock, flags);
		perlen = pusb_rx->length;
		if ((count - pos) < perlen) {
			/* read particial data and do not delete the entry */
			bytesRead = count - pos;
			rv = copy_to_user((u8 __user *)pbuf + pos, pusb_rx->data + pusb_rx->pos, bytesRead);
			if (rv < 0)
				goto exit;
			else if (rv > 0) {
				pusb_rx->length -= bytesRead - rv;
				pusb_rx->pos += bytesRead - rv;
				pos += bytesRead - rv;
			} else {
				pusb_rx->length -= bytesRead;
				pusb_rx->pos += bytesRead;
				pos += bytesRead;
			}
		} else {
			/* read all data and delete the entry */
			bytesRead = perlen;
			rv = copy_to_user((u8 __user *)pbuf + pos, pusb_rx->data + pusb_rx->pos, bytesRead);
			if (rv < 0)
				goto exit;
			else if (rv > 0) {
				pos += bytesRead - rv;
			} else {
				pos += bytesRead;
				spin_lock_irqsave(&usbch37x->read_lock, flags);
				list_del(usbch37x->bulkepin[epindex].usb_rx_list_used.next);
				list_add_tail(&pusb_rx->list, &usbch37x->bulkepin[epindex].usb_rx_list_free);
				atomic_inc(&usbch37x->bulkepin[epindex].free_entries);
				spin_unlock_irqrestore(&usbch37x->read_lock, flags);
			}
			if ((count - pos) != 0)
				goto retry;
		}
	} else {
		spin_unlock_irqrestore(&usbch37x->read_lock, flags);
		spin_lock_irq(&usbch37x->err_lock);
		usbch37x->bulkepin[epindex].bread_valid = false;
		spin_unlock_irq(&usbch37x->err_lock);
		if (usbch37x->f_flags & O_NONBLOCK) {
			goto exit;
		}
		rv = wait_event_interruptible_timeout(usbch37x->bulkepin[epindex].bulk_in_wait,
						      usbch37x->bulkepin[epindex].bread_valid,
						      msecs_to_jiffies(usbch37x->readtimeout));
		if (rv <= 0) {
			goto exit;
		}
		goto retry;
	}
exit:
	if (pos > 0) {
		is_entries_free = atomic_read(&usbch37x->bulkepin[epindex].free_entries) < USB_RX_BUF_UNIT * 2 / 10 ?
					  false :
					  true;

		if (is_entries_free) {
			if (usbch37x->bulkepin[epindex].throttled)
				ch37x_bulk_read_unthrottle(usbch37x, epindex);
		}
	}

	return pos > 0 ? pos : rv;
}

/* Async endpoint bulk transfer */
static ssize_t ch37x_bulk_async_write(struct usb_ch37x *usbch37x, int epindex, void *pbuf, u32 count)
{
	int stat;
	unsigned long flags;
	int wbn;
	struct ch37x_wb *wb;
	int rv;

	/* verify that we actually have some data to write */
	if (!count)
		return 0;

	spin_lock_irqsave(&usbch37x->write_lock, flags);
	wbn = ch37x_wb_alloc(usbch37x, epindex);
	if (wbn < 0) {
		spin_unlock_irqrestore(&usbch37x->write_lock, flags);
		return 0;
	}
	wb = &usbch37x->bulkepout[epindex].wb[wbn];

	if (!usbch37x->udev) {
		wb->use = 0;
		spin_unlock_irqrestore(&usbch37x->write_lock, flags);
		return -ENODEV;
	}

	count = (count > usbch37x->bulkepout[epindex].writesize) ? usbch37x->bulkepout[epindex].writesize : count;
	rv = copy_from_user(wb->buf, (u8 __user *)pbuf, count);
	if (rv < 0) {
		return rv;
	}
	wb->len = count - rv;

	stat = usb_autopm_get_interface_async(usbch37x->interface);
	if (stat) {
		wb->use = 0;
		spin_unlock_irqrestore(&usbch37x->write_lock, flags);
		return stat;
	}

	stat = ch37x_start_wb(usbch37x, wb, epindex);
	spin_unlock_irqrestore(&usbch37x->write_lock, flags);

	if (stat < 0)
		return stat;
	return count;
}

/* Sync endpoint bulk transfer */
static ssize_t ch37x_bulk_sync_read(struct usb_ch37x *usbch37x, int epindex, void *pbuf, u32 count)
{
	int rv;
	int bytes_read = 0;
	ssize_t bytes_to_read = count;
	u8 *buffer;

	buffer = kmalloc(bytes_to_read, GFP_KERNEL);
	if (!buffer) {
		rv = -ENOMEM;
		goto exit;
	}

	rv = usb_bulk_msg(usbch37x->udev, usbch37x->bulkepin[epindex].rx_endpoint, buffer, bytes_to_read, &bytes_read,
			  usbch37x->readtimeout);
	if (!rv) {
		if (copy_to_user((u8 __user *)pbuf, buffer, bytes_read)) {
			rv = -EFAULT;
			goto exit;
		}
	} else
		goto exit;

	kfree(buffer);
	return bytes_read;
exit:
	kfree(buffer);
	return rv;
}

/* Sync endpoint bulk transfer */
static ssize_t ch37x_bulk_sync_write(struct usb_ch37x *usbch37x, int epindex, void *pbuf, u32 count)
{
	int rv;
	int bytes_write = 0;
	ssize_t bytes_to_write = count;
	u8 *buffer = NULL;

	buffer = kmalloc(bytes_to_write, GFP_KERNEL);
	if (!buffer) {
		rv = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(buffer, (u8 __user *)pbuf, bytes_to_write)) {
		rv = -EFAULT;
		goto exit;
	}

	rv = usb_bulk_msg(usbch37x->udev, usbch37x->bulkepout[epindex].tx_endpoint, buffer, bytes_to_write,
			  &bytes_write, usbch37x->writetimeout);
	if (rv)
		goto exit;

	kfree(buffer);
	return bytes_write;
exit:
	kfree(buffer);
	return rv;
}

/* Start endpoint bulk transfer in buffered mode */
static int ch37x_start_read_io(struct usb_ch37x *usbch37x, int epindex, u32 urblen)
{
	int retval;
	int i;

	if (usbch37x->bulkepin[epindex].buploaded || usbch37x->bulkepin[epindex].ballocated)
		return -EAGAIN;

	retval = usb_autopm_get_interface(usbch37x->interface);
	if (retval)
		goto error_get_interface;

	if ((urblen != 0) && ((urblen % usbch37x->bulkepin[epindex].epsize) == 0)) {
		usbch37x->bulkepin[epindex].readsize = urblen;
	} else
		usbch37x->bulkepin[epindex].readsize = usbch37x->bulkepin[epindex].epsize;

	retval = ch37x_read_buffers_alloc(usbch37x, epindex);
	if (retval)
		goto alloc_fail;

	retval = ch37x_read_list_alloc(usbch37x, epindex);
	if (retval)
		goto alloc_fail1;

	spin_lock_irq(&usbch37x->bulkepin[epindex].throttle_lock);
	usbch37x->bulkepin[epindex].throttled = 0;
	spin_unlock_irq(&usbch37x->bulkepin[epindex].throttle_lock);

	retval = ch37x_submit_read_urbs(usbch37x, epindex, GFP_KERNEL);
	if (retval)
		goto error_submit_read_urbs;

	usb_autopm_put_interface(usbch37x->interface);

	usbch37x->bulkepin[epindex].buploaded = true;
	usbch37x->bulkepin[epindex].ballocated = true;

	return 0;

error_submit_read_urbs:
	for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
		usb_kill_urb(usbch37x->bulkepin[epindex].read_urbs[i]);
	usb_autopm_put_interface(usbch37x->interface);
	ch37x_read_list_free(usbch37x, epindex);
alloc_fail1:
	ch37x_read_buffers_free(usbch37x, epindex);
alloc_fail:
error_get_interface:
	return retval;
}

/* Stop endpoint bulk transfer in buffered mode */
static void ch37x_stop_read_io(struct usb_ch37x *usbch37x, int epindex)
{
	int i;

	if (usbch37x->bulkepin[epindex].buploaded == true) {
		for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
			usb_kill_urb(usbch37x->bulkepin[epindex].read_urbs[i]);
		usbch37x->bulkepin[epindex].buploaded = false;
	}
	if (usbch37x->bulkepin[epindex].ballocated == true) {
		for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
			usb_free_urb(usbch37x->bulkepin[epindex].read_urbs[i]);
		ch37x_read_buffers_free(usbch37x, epindex);
		ch37x_read_list_free(usbch37x, epindex);
		usbch37x->bulkepin[epindex].ballocated = false;
	}
}

/* Query buffered size of specified endpoint */
static void ch37x_query_read_buffer(struct usb_ch37x *usbch37x, int epindex, u64 *size)
{
	struct usb_rx_buf *pusb_rx;
	u64 bytesBuffered = 0;
	unsigned long flags;

	spin_lock_irqsave(&usbch37x->read_lock, flags);
	while (!list_empty(&usbch37x->bulkepin[epindex].usb_rx_list_used)) {
		pusb_rx = list_entry(usbch37x->bulkepin[epindex].usb_rx_list_used.next, struct usb_rx_buf, list);
		if (pusb_rx != NULL) {
			bytesBuffered += pusb_rx->length;
		}
	}
	spin_unlock_irqrestore(&usbch37x->read_lock, flags);

	*size = bytesBuffered;
}

/* USB control transfer in */
static int ch37x_control_msg_in(struct usb_ch37x *usbch37x, u8 request, u8 requesttype, u16 value, u16 index, void *buf,
				unsigned bufsize)
{
	int retval;
	char *buffer;

	buffer = kmalloc(bufsize, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	retval = usb_autopm_get_interface(usbch37x->interface);
	if (retval)
		goto out;

	retval = usb_control_msg(usbch37x->udev, usb_rcvctrlpipe(usbch37x->udev, 0), request, requesttype, value, index,
				 buffer, bufsize, usbch37x->readtimeout);
	if (retval > 0) {
		if (copy_to_user((u8 __user *)buf, buffer, retval)) {
			retval = -EFAULT;
		}
	}

	usb_autopm_put_interface(usbch37x->interface);

out:
	kfree(buffer);
	return retval;
}

/* USB control transfer out */
static int ch37x_control_msg_out(struct usb_ch37x *usbch37x, u8 request, u8 requesttype, u16 value, u16 index,
				 void *buf, unsigned bufsize)
{
	int retval;
	char *buffer;

	buffer = kmalloc(bufsize, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	retval = copy_from_user(buffer, (u8 __user *)buf, bufsize);
	if (retval)
		goto out;

	retval = usb_autopm_get_interface(usbch37x->interface);
	if (retval)
		goto out;

	retval = usb_control_msg(usbch37x->udev, usb_sndctrlpipe(usbch37x->udev, 0), request, requesttype, value, index,
				 buf, bufsize, usbch37x->writetimeout);

	usb_autopm_put_interface(usbch37x->interface);

out:
	kfree(buffer);
	return retval;
}

/* Sync usb interrupt transfer */
static int ch37x_interrupt_msg_in(struct usb_ch37x *usbch37x, int index, void *pbuf, u32 count)
{
	int rv;
	int bytes_read = 0;
	ssize_t bytes_to_read = count;
	u8 *buffer;

	buffer = kmalloc(bytes_to_read, GFP_KERNEL);
	if (!buffer) {
		rv = -ENOMEM;
		goto exit;
	}

	rv = usb_bulk_msg(usbch37x->udev, usbch37x->int_rx_endpoint[index], buffer, bytes_to_read, &bytes_read,
			  usbch37x->readtimeout);
	if (!rv) {
		if (copy_to_user((u8 __user *)pbuf, buffer, bytes_read)) {
			rv = -EFAULT;
			goto exit;
		}
	} else
		goto exit;

	kfree(buffer);
	return bytes_read;
exit:
	kfree(buffer);
	return rv;
}

/* Sync usb interrupt transfer */
static ssize_t ch37x_interrupt_msg_out(struct usb_ch37x *usbch37x, int index, void *pbuf, u32 count)
{
	int rv;
	int bytes_write = 0;
	ssize_t bytes_to_write = count;
	u8 *buffer = NULL;

	buffer = kmalloc(bytes_to_write, GFP_KERNEL);
	if (!buffer) {
		rv = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(buffer, (u8 __user *)pbuf, bytes_to_write)) {
		rv = -EFAULT;
		goto exit;
	}

	rv = usb_bulk_msg(usbch37x->udev, usbch37x->int_tx_endpoint[index], buffer, bytes_to_write, &bytes_write,
			  usbch37x->writetimeout);
	if (rv)
		goto exit;

	kfree(buffer);
	return bytes_write;
exit:
	kfree(buffer);
	return rv;
}

/* Throttle the usb read process in buffered mode */
static void ch37x_bulk_read_throttle(struct usb_ch37x *usbch37x, int epindex)
{
	dev_info(&usbch37x->interface->dev, "%s\n", __func__);

	spin_lock_irq(&usbch37x->bulkepin[epindex].throttle_lock);
	usbch37x->bulkepin[epindex].throttled = 1;
	spin_unlock_irq(&usbch37x->bulkepin[epindex].throttle_lock);
}

/* Unthrottle the usb read process in buffered mode */
static void ch37x_bulk_read_unthrottle(struct usb_ch37x *usbch37x, int epindex)
{
	unsigned int was_throttled;

	dev_info(&usbch37x->interface->dev, "%s\n", __func__);

	spin_lock_irq(&usbch37x->bulkepin[epindex].throttle_lock);
	was_throttled = usbch37x->bulkepin[epindex].throttled;
	usbch37x->bulkepin[epindex].throttled = 0;
	spin_unlock_irq(&usbch37x->bulkepin[epindex].throttle_lock);

	if (was_throttled)
		ch37x_submit_read_urbs(usbch37x, epindex, GFP_KERNEL);
}

/* Ioctl implements for interaction between driver and application */
static long ch37x_ioctl(struct file *ch37x_file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int bytes_ret = 0;
	int bytes_write = 0;
	int bytes_read = 0;
	struct usb_ch37x *usbch37x;
	char *ch37x_version_tmp = VERSION_DESC;
	int readtimeout = 0;
	int writetimeout = 0;
	int epindex = 0;
	int urblen = 0;
	int eptype = 0;
	unsigned long arg1, arg2, arg3, arg4, arg5, arg6;

	usbch37x = (struct usb_ch37x *)ch37x_file->private_data;

	if (mutex_lock_interruptible(&usbch37x->io_mutex))
		return -ERESTARTSYS;

	/* verify that the device wasn't unplugged */
	if (usbch37x->udev == NULL) {
		retval = -ENODEV;
		goto exit;
	}

	switch (cmd) {
	case GetVersion:
		retval = copy_to_user((u8 __user *)arg, ch37x_version_tmp, strlen(VERSION_DESC));
		break;
	case GetDeviceID:
		retval = copy_to_user((u16 __user *)arg, usbch37x->ch37x_id, sizeof(u16) * 2);
		break;
	case GetDeviceEpMsg:
		retval = get_user(eptype, (u8 __user *)arg);
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 1);
		switch (eptype) {
		case EPTYPE_BULKIN:
			retval = copy_to_user((u8 __user *)arg1, &usbch37x->epmsg_bulkin, sizeof(struct ch37x_epmsg));
			break;
		case EPTYPE_BULKOUT:
			retval = copy_to_user((u8 __user *)arg1, &usbch37x->epmsg_bulkout, sizeof(struct ch37x_epmsg));
			break;
		case EPTYPE_INTIN:
			retval = copy_to_user((u8 __user *)arg1, &usbch37x->epmsg_intin, sizeof(struct ch37x_epmsg));
			break;
		case EPTYPE_INTOUT:
			retval = copy_to_user((u8 __user *)arg1, &usbch37x->epmsg_intout, sizeof(struct ch37x_epmsg));
			break;
		default:
			retval = -EINVAL;
			break;
		}
		break;
	case GetDeviceDes:
		retval = usb_get_descriptor(usbch37x->udev, USB_DT_DEVICE, 0x00, &usbch37x->udev->descriptor,
					    sizeof(usbch37x->udev->descriptor));
		if (retval < sizeof(usbch37x->udev->descriptor)) {
			retval = -EPROTO;
			goto exit;
		}
		retval =
			copy_to_user((u8 __user *)arg, &usbch37x->udev->descriptor, sizeof(usbch37x->udev->descriptor));
		break;
	case CH37X_SET_EXCLUSIVE:
		retval = get_user(arg1, (u8 __user *)arg);
		if (retval)
			goto exit;
		if (arg1)
			usbch37x->bexclusive = true;
		else
			usbch37x->bexclusive = false;
		retval = 0;
		break;
	case START_BUFFERED_UPLOAD:
		retval = get_user(epindex, (u8 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(urblen, (u32 __user *)((u8 *)arg + 1));
		if (retval)
			goto exit;
		retval = ch37x_start_read_io(usbch37x, epindex, urblen);
		break;
	case STOP_BUFFERED_UPLOAD:
		retval = get_user(epindex, (u8 __user *)arg);
		if (retval)
			goto exit;
		ch37x_stop_read_io(usbch37x, epindex);
		break;
	case QWERY_BUFFERED_UPLOAD:
		retval = get_user(epindex, (u8 __user *)arg);
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 1);
		ch37x_query_read_buffer(usbch37x, epindex, (u64 *)arg1);
		break;
	case CH37X_BULK_ASYNC_READ:
		retval = get_user(bytes_read, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(epindex, (u8 __user *)arg + 4);
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		bytes_ret = ch37x_bulk_async_read(usbch37x, epindex, (void *)arg1, bytes_read);
		if (bytes_ret >= 0) {
			retval = put_user(bytes_ret, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		} else
			retval = bytes_ret;
		break;
	case CH37X_BULK_ASYNC_WRITE:
		retval = get_user(bytes_write, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(epindex, ((u8 __user *)arg + 4));
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		bytes_ret = ch37x_bulk_async_write(usbch37x, epindex, (void *)arg1, bytes_write);
		if (bytes_ret > 0) {
			retval = put_user(bytes_ret, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		} else
			retval = bytes_ret;
		break;
	case CH37X_SET_TIMEOUT:
		retval = get_user(readtimeout, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(writetimeout, ((u32 __user *)arg + 1));
		if (retval)
			goto exit;
		usbch37x->readtimeout = readtimeout;
		usbch37x->writetimeout = writetimeout;
		break;
	case CH37X_BULK_SYNC_READ:
		retval = get_user(bytes_read, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(epindex, (u8 __user *)arg + 4);
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		retval = ch37x_bulk_sync_read(usbch37x, epindex, (void *)arg1, bytes_read);
		if (retval > 0) {
			retval = put_user(retval, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		}
		break;
	case CH37X_BULK_SYNC_WRITE:
		retval = get_user(bytes_write, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(epindex, ((u8 __user *)arg + 4));
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		retval = ch37x_bulk_sync_write(usbch37x, epindex, (void *)arg1, bytes_write);
		if (retval > 0) {
			retval = put_user(retval, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		}
		break;
	case CH37X_CTRL_SYNC_READ:
		retval = get_user(arg1, (u8 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(arg2, ((u8 __user *)arg + 1));
		if (retval)
			goto exit;
		retval = get_user(arg3, (u16 __user *)((u8 *)arg + 2));
		if (retval)
			goto exit;
		retval = get_user(arg4, (u16 __user *)((u8 *)arg + 4));
		if (retval)
			goto exit;
		retval = get_user(arg5, (u16 __user *)((u8 *)arg + 6));
		if (retval)
			goto exit;
		arg6 = (unsigned long)((u8 __user *)arg + 8);
		retval = ch37x_control_msg_in(usbch37x, (u8)arg1, (u8)arg2, (u16)arg3, (u16)arg4, (u8 __user *)arg6,
					      (u16)arg5);
		break;
	case CH37X_CTRL_SYNC_WRITE:
		retval = get_user(arg1, (u8 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(arg2, ((u8 __user *)arg + 1));
		if (retval)
			goto exit;
		retval = get_user(arg3, (u16 __user *)((u8 *)arg + 2));
		if (retval)
			goto exit;
		retval = get_user(arg4, (u16 __user *)((u8 *)arg + 4));
		if (retval)
			goto exit;
		retval = get_user(arg5, (u16 __user *)((u8 *)arg + 6));
		if (retval)
			goto exit;
		arg6 = (unsigned long)((u8 __user *)arg + 8);
		retval = ch37x_control_msg_out(usbch37x, (u8)arg1, (u8)arg2, (u16)arg3, (u16)arg4, (u8 __user *)arg6,
					       (u16)arg5);
		if (retval != (u16)arg5) {
			retval = -EINVAL;
			goto exit;
		}
		break;
	case CH37X_INT_SYNC_READ:
		retval = get_user(bytes_read, (u32 __user *)arg);
		if (retval)
			goto exit;
		retval = get_user(epindex, (u8 __user *)arg + 4);
		if (retval)
			goto exit;
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		retval = ch37x_interrupt_msg_in(usbch37x, epindex, (void *)arg1, bytes_read);
		if (retval > 0) {
			retval = put_user(retval, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		}
		break;
	case CH37X_INT_SYNC_WRITE:
		retval = get_user(bytes_write, (u32 __user *)arg);
		retval = get_user(epindex, ((u8 __user *)arg + 4));
		arg1 = (unsigned long)((u8 __user *)arg + 5);
		retval = ch37x_interrupt_msg_out(usbch37x, epindex, (void *)arg1, bytes_write);
		if (retval > 0) {
			retval = put_user(retval, (u32 __user *)arg);
			if (retval)
				goto exit;
			retval = 0;
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
exit:
	/* unlock the device */
	mutex_unlock(&usbch37x->io_mutex);
	return retval;
}

static const struct file_operations ch37x_fops = {
	.owner = THIS_MODULE,
	.open = ch37x_open,
	.unlocked_ioctl = ch37x_ioctl,
	.release = ch37x_release,
	.flush = ch37x_flush,
};

/*
 * USB class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver ch37x_class = {
	.name = "ch37x%d",
	.fops = &ch37x_fops,
	.minor_base = USB_MINOR_BASE,
};

static int ch37x_read_buffers_alloc(struct usb_ch37x *usbch37x, int epindex)
{
	int i;
	int num_rx_buf = CH37X_NR;

	for (i = 0; i < num_rx_buf; i++) {
		struct ch37x_rb *rb = &(usbch37x->bulkepin[epindex].read_buffers[i]);
		struct urb *urb;

		rb->base =
			usb_alloc_coherent(usbch37x->udev, usbch37x->bulkepin[epindex].readsize, GFP_KERNEL, &rb->dma);
		if (!rb->base)
			goto out;
		rb->index = i;
		rb->instance = usbch37x;
		rb->epindex = epindex;

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb)
			goto out;

		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		urb->transfer_dma = rb->dma;

		usb_fill_bulk_urb(urb, usbch37x->udev, usbch37x->bulkepin[epindex].rx_endpoint, rb->base,
				  usbch37x->bulkepin[epindex].readsize, ch37x_read_bulk_callback, rb);
		usbch37x->bulkepin[epindex].read_urbs[i] = urb;
		__set_bit(i, &usbch37x->bulkepin[epindex].read_urbs_free);
	}

	return 0;

out:
	return -1;
}

static int ch37x_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_ch37x *usbch37x;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, epindex;
	int retval = -ENOMEM;
	int num_rx_buf = CH37X_NR;

	/* allocate memory for our device state and initialize it */
	usbch37x = kzalloc(sizeof(*usbch37x), GFP_KERNEL);
	if (!usbch37x) {
		dev_err(&interface->dev, "Out of memory");
		goto error;
	}
	kref_init(&usbch37x->kref);

	mutex_init(&usbch37x->io_mutex);
	spin_lock_init(&usbch37x->err_lock);
	spin_lock_init(&usbch37x->read_lock);
	spin_lock_init(&usbch37x->write_lock);
	init_usb_anchor(&usbch37x->submitted);

	usbch37x->udev = usb_get_dev(interface_to_usbdev(interface));
	usbch37x->ch37x_id[0] = usbch37x->udev->descriptor.idVendor;
	usbch37x->ch37x_id[1] = usbch37x->udev->descriptor.idProduct;
	usbch37x->interface = interface;

	/* set up the endpoint information */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(endpoint)) {
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].epaddr = endpoint->bEndpointAddress;
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].epsize = usb_endpoint_maxp(endpoint);
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].rx_endpoint =
				usb_rcvbulkpipe(usbch37x->udev, endpoint->bEndpointAddress);
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].rx_buflimit = num_rx_buf;
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].buploaded = false;
			usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].ballocated = false;
			init_waitqueue_head(&usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].bulk_in_wait);
			spin_lock_init(&usbch37x->bulkepin[usbch37x->epmsg_bulkin.epnum].throttle_lock);
			usbch37x->epmsg_bulkin.epaddr[usbch37x->epmsg_bulkin.epnum] = endpoint->bEndpointAddress;
			usbch37x->epmsg_bulkin.epnum++;
		}

		if (usb_endpoint_is_bulk_out(endpoint)) {
			usbch37x->bulkepout[usbch37x->epmsg_bulkout.epnum].epaddr = endpoint->bEndpointAddress;
			usbch37x->bulkepout[usbch37x->epmsg_bulkout.epnum].writesize = usb_endpoint_maxp(endpoint) * 20;
			usbch37x->bulkepout[usbch37x->epmsg_bulkout.epnum].tx_endpoint =
				usb_sndbulkpipe(usbch37x->udev, endpoint->bEndpointAddress);
			usbch37x->epmsg_bulkout.epaddr[usbch37x->epmsg_bulkout.epnum] = endpoint->bEndpointAddress;
			usbch37x->epmsg_bulkout.epnum++;
		}
		if (usb_endpoint_is_int_in(endpoint)) {
			usbch37x->epmsg_intin.epaddr[usbch37x->epmsg_intin.epnum] = endpoint->bEndpointAddress;
			usbch37x->int_rx_endpoint[usbch37x->epmsg_intin.epnum] =
				usb_rcvintpipe(usbch37x->udev, endpoint->bEndpointAddress);
			usbch37x->epmsg_intin.epnum++;
		}
		if (usb_endpoint_is_int_out(endpoint)) {
			usbch37x->epmsg_intout.epaddr[usbch37x->epmsg_intout.epnum] = endpoint->bEndpointAddress;
			usbch37x->int_tx_endpoint[usbch37x->epmsg_intout.epnum] =
				usb_sndintpipe(usbch37x->udev, endpoint->bEndpointAddress);
			usbch37x->epmsg_intout.epnum++;
		}
	}

	if (ch37x_write_buffers_alloc(usbch37x) < 0)
		goto alloc_fail;

	for (epindex = 0; epindex < usbch37x->epmsg_bulkout.epnum; epindex++) {
		for (i = 0; i < CH37X_NW; i++) {
			struct ch37x_wb *snd = &(usbch37x->bulkepout[epindex].wb[i]);
			snd->epindex = epindex;
			snd->urb = usb_alloc_urb(0, GFP_KERNEL);
			if (snd->urb == NULL)
				goto alloc_fail1;

			usb_fill_bulk_urb(snd->urb, usbch37x->udev, usbch37x->bulkepout[epindex].tx_endpoint, NULL,
					  usbch37x->bulkepout[epindex].writesize, ch37x_write_bulk, snd);
			snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
			snd->instance = usbch37x;
		}
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, usbch37x);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &ch37x_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev, "Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto alloc_fail2;
	}

	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev, "USB device ch37x #%d now attached", interface->minor);
	return 0;

alloc_fail2:
	for (epindex = 0; epindex < usbch37x->epmsg_bulkout.epnum; epindex++) {
		for (i = 0; i < CH37X_NW; i++) {
			usb_free_urb(usbch37x->bulkepout[epindex].wb[i].urb);
		}
	}
alloc_fail1:
	ch37x_write_buffers_free(usbch37x);
alloc_fail:
error:
	/* this frees allocated memory */
	if (usbch37x)
		kref_put(&usbch37x->kref, ch37x_delete_dev);
	return retval;
}

static void ch37x_disconnect(struct usb_interface *interface)
{
	struct usb_ch37x *usbch37x;
	int minor = interface->minor;

	usbch37x = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &ch37x_class);

	/* prevent more I/O from starting */
	mutex_lock(&usbch37x->io_mutex);
	usbch37x->interface = NULL;
	mutex_unlock(&usbch37x->io_mutex);

	usb_kill_anchored_urbs(&usbch37x->submitted);

	/* decrement our usage count */
	kref_put(&usbch37x->kref, ch37x_delete);

	pr_info("USB device ch37x #%d now disconnected", minor);
}

/* Realease related */
static void ch37x_draw_down(struct usb_ch37x *usbch37x)
{
	int time;
	int i;
	int epindex;

	time = usb_wait_anchor_empty_timeout(&usbch37x->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&usbch37x->submitted);

	for (epindex = 0; epindex < usbch37x->epmsg_bulkin.epnum; epindex++) {
		if (usbch37x->bulkepin[epindex].buploaded == true) {
			for (i = 0; i < usbch37x->bulkepin[epindex].rx_buflimit; i++)
				usb_kill_urb(usbch37x->bulkepin[epindex].read_urbs[i]);
			usbch37x->bulkepin[epindex].buploaded = false;
		}
	}
}

static int ch37x_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_ch37x *usbch37x = usb_get_intfdata(intf);

	if (!usbch37x)
		return 0;
	ch37x_draw_down(usbch37x);

	return 0;
}

static int ch37x_resume(struct usb_interface *intf)
{
	return 0;
}

static int ch37x_pre_reset(struct usb_interface *intf)
{
	struct usb_ch37x *usbch37x = usb_get_intfdata(intf);

	mutex_lock(&usbch37x->io_mutex);
	ch37x_draw_down(usbch37x);

	return 0;
}

static int ch37x_post_reset(struct usb_interface *intf)
{
	struct usb_ch37x *usbch37x = usb_get_intfdata(intf);

	/* we are sure no URBs are active - no locking needed */
	usbch37x->errors = -EPIPE;
	mutex_unlock(&usbch37x->io_mutex);

	return 0;
}

static struct usb_driver ch37x_driver = {
	.name = "usb_ch37x",
	.probe = ch37x_probe,
	.disconnect = ch37x_disconnect,
	.suspend = ch37x_suspend,
	.resume = ch37x_resume,
	.pre_reset = ch37x_pre_reset,
	.post_reset = ch37x_post_reset,
	.id_table = ch37x_table,
	.supports_autosuspend = 1,
};

static int __init ch37x_init(void)
{
	int result;

	/* register this driver with the USB subsystem */
	result = usb_register(&ch37x_driver);
	if (result)
		pr_err("usb_register failed. error number %d", result);
	return result;
}

static void __exit ch37x_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&ch37x_driver);
}

module_init(ch37x_init);
module_exit(ch37x_exit);

MODULE_LICENSE("GPL");
