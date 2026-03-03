/*
 * application library for ch372/ch374/ch375/ch376/ch378 etc.
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
 * Cross-compile with cross-gcc -I /path/to/cross-kernel/include
 *
 * Update Log:
 * V1.0 - initial version
 * V1.1 - add support for more endpoints
        - add support for synchronous transmission
 * V1.2 - add support for endpoint management
 *      - add support for start/stop endpoint buffered mode seperately
 *
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "ch37x_lib.h"

#define IOCTL_MAGIC 'W'

#define GetVersion     _IOR(IOCTL_MAGIC, 0x80, uint16_t)
#define GetDeviceID    _IOR(IOCTL_MAGIC, 0x81, uint16_t)
#define GetDeviceEpMsg _IOR(IOCTL_MAGIC, 0x82, uint16_t)
#define GetDeviceDes   _IOR(IOCTL_MAGIC, 0x83, uint16_t)

#define CH37X_SET_EXCLUSIVE   _IOW(IOCTL_MAGIC, 0xa0, uint16_t)
#define START_BUFFERED_UPLOAD _IOW(IOCTL_MAGIC, 0xa1, uint16_t)
#define STOP_BUFFERED_UPLOAD  _IOW(IOCTL_MAGIC, 0xa2, uint16_t)
#define QWERY_BUFFERED_UPLOAD _IOWR(IOCTL_MAGIC, 0xa3, uint16_t)

#define CH37X_BULK_ASYNC_READ  _IOWR(IOCTL_MAGIC, 0xb0, uint16_t)
#define CH37X_BULK_ASYNC_WRITE _IOW(IOCTL_MAGIC, 0xb1, uint16_t)

#define CH37X_SET_TIMEOUT     _IOW(IOCTL_MAGIC, 0xc0, uint16_t)
#define CH37X_BULK_SYNC_READ  _IOWR(IOCTL_MAGIC, 0xc1, uint16_t)
#define CH37X_BULK_SYNC_WRITE _IOW(IOCTL_MAGIC, 0xc2, uint16_t)
#define CH37X_CTRL_SYNC_READ  _IOWR(IOCTL_MAGIC, 0xc3, uint16_t)
#define CH37X_CTRL_SYNC_WRITE _IOW(IOCTL_MAGIC, 0xc4, uint16_t)
#define CH37X_INT_SYNC_READ   _IOWR(IOCTL_MAGIC, 0xc5, uint16_t)
#define CH37X_INT_SYNC_WRITE  _IOW(IOCTL_MAGIC, 0xc6, uint16_t)

static struct usb_ch37x gusbch37x[CH37X_MAX_NUMBER];

/**
 * get_devindex - get device index via file descriptor
 * @fd: file descriptor of device
 *
 * The function return device index if successful, negative if fail.
 */
static int get_devindex(int fd)
{
	int i;

	for (i = 0; i < CH37X_MAX_NUMBER; i++) {
		if (gusbch37x[i].fd == fd) {
			return i;
		}
		continue;
	}

	return -1;
}

/**
 * CH37XGetObject - get global device object
 *
 * The function return pointer to device.
 */
struct usb_ch37x *CH37XGetObject(int fd)
{
	int i = get_devindex(fd);

	if (i < 0)
		return NULL;

	return &gusbch37x[i];
}

/**
 * CH37XScanDevices - scan devices in /dev directory
 *
 * The function return device amounts.
 */
int CH37XScanDevices()
{
	char devname[20];
	int num = 0;
	int i = 0;

	while (i < 20) {
		memset(devname, 0, sizeof(devname));
		sprintf(devname, "%s%d", "/dev/ch37x", i);
		if (!access(devname, F_OK | W_OK | R_OK)) {
			num++;
		}
		i++;
	}

	return num;
}

/**
 * CH37XAddFd - store new device messages
 * @fd: file descriptor of device
 */
static void CH37XAddFd(int fd)
{
	int i;

	for (i = 0; i < CH37X_MAX_NUMBER; i++) {
		if (gusbch37x[i].fd <= 0) {
			gusbch37x[i].fd = fd;
			break;
		}
		continue;
	}
	gusbch37x[i].epmsg_bulkin.eptype = EPTYPE_BULKIN;
	gusbch37x[i].epmsg_bulkout.eptype = EPTYPE_BULKOUT;
	gusbch37x[i].epmsg_intin.eptype = EPTYPE_INTIN;
	gusbch37x[i].epmsg_intout.eptype = EPTYPE_INTOUT;
}

/**
 * CH37XDelFd - remove device
 * @fd: file descriptor of device
 */
static void CH37XDelFd(int fd)
{
	int i;

	for (i = 0; i < CH37X_MAX_NUMBER; i++) {
		if (gusbch37x[i].fd == fd) {
			gusbch37x[i].fd = 0;
			break;
		}
		continue;
	}
}

/**
 * CH37XOpenDevice - open device
 * @pathname: device path in /dev directory
 * @nonblock: access device in nonblock mode if true, blocked if false
 *
 * The function return file descriptor if successful, others if fail.
 */
int CH37XOpenDevice(const char *pathname, bool nonblock)
{
	int fd;

	if (nonblock)
		fd = open(pathname, O_RDWR | O_NONBLOCK);
	else
		fd = open(pathname, O_RDWR);

	if (fd > 0)
		CH37XAddFd(fd);

	return fd;
}

/**
 * CH37XCloseDevice - close device
 * @fd: file descriptor of device
 *
 * The function return true if successful, false if fail.
 */
bool CH37XCloseDevice(int fd)
{
	if (close(fd) == 0) {
		CH37XDelFd(fd);
		return true;
	}

	return false;
}

/**
 * CH37XSetExclusive - set device exclusive access
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 *
 * The function return true if successful, false if fail.
 */
bool CH37XSetExclusive(int fd, bool enable)
{
	return ioctl(fd, CH37X_SET_EXCLUSIVE, (unsigned long)&enable) == 0 ? true : false;
}

/**
 * CH37XGetDriverVersion - get driver verion
 * @fd: file descriptor of device
 * @oBuffer: pointer to version buffer
 *
 * The function return true if successful, false if fail.
 */
bool CH37XGetDriverVersion(int fd, void *oBuffer)
{
	return ioctl(fd, GetVersion, (unsigned long)oBuffer) == 0 ? true : false;
}

/**
 * CH37XGetDeviceID - get device vid and pid
 * @fd: file descriptor of device
 * @id: pointer to store id which contains vid and pid
 *
 * The function return true if successful, false if fail.
 */
bool CH37XGetDeviceID(int fd, uint32_t *id)
{
	return ioctl(fd, GetDeviceID, (unsigned long)id) == 0 ? true : false;
}

/**
 * CH37XGetDeviceEpMsg - get device endpoint messages and save to global device structure
 * @fd: file descriptor of device
 *
 * The function return true if successful, false if fail.
 */
bool CH37XGetDeviceEpMsg(int fd)
{
	int retval;
	uint8_t index;
	int i = get_devindex(fd);

	if (i < 0)
		return false;

	retval = ioctl(fd, GetDeviceEpMsg, (unsigned long)&gusbch37x[i].epmsg_bulkin);
	if (retval)
		goto exit;
	retval = ioctl(fd, GetDeviceEpMsg, (unsigned long)&gusbch37x[i].epmsg_bulkout);
	if (retval)
		goto exit;
	retval = ioctl(fd, GetDeviceEpMsg, (unsigned long)&gusbch37x[i].epmsg_intin);
	if (retval)
		goto exit;
	retval = ioctl(fd, GetDeviceEpMsg, (unsigned long)&gusbch37x[i].epmsg_intout);
	if (retval)
		goto exit;

	if (gusbch37x[i].epmsg_bulkin.epnum > 0) {
		for (index = 0; index < gusbch37x[i].epmsg_bulkin.epnum; index++)
			gusbch37x[i].map_bulkin[index] = gusbch37x[i].epmsg_bulkin.epaddr[index];
	}

	if (gusbch37x[i].epmsg_bulkout.epnum > 0) {
		for (index = 0; index < gusbch37x[i].epmsg_bulkout.epnum; index++)
			gusbch37x[i].map_bulkout[index] = gusbch37x[i].epmsg_bulkout.epaddr[index];
	}

	if (gusbch37x[i].epmsg_intin.epnum > 0) {
		for (index = 0; index < gusbch37x[i].epmsg_intin.epnum; index++)
			gusbch37x[i].map_intin[index] = gusbch37x[i].epmsg_intin.epaddr[index];
	}

	if (gusbch37x[i].epmsg_intout.epnum > 0) {
		for (index = 0; index < gusbch37x[i].epmsg_intout.epnum; index++)
			gusbch37x[i].map_intout[index] = gusbch37x[i].epmsg_intout.epaddr[index];
	}

	return true;

exit:
	return false;
}

/**
 * CH37XGetDeviceDescr - get device descriptor
 * @fd: file descriptor of device
 * @buffer: pointer to descripotor buffer
 *
 * The function return true if successful, false if fail.
 */
bool CH37XGetDeviceDescr(int fd, uint8_t *buffer)
{
	return ioctl(fd, GetDeviceDes, (unsigned long)buffer) == 0 ? true : false;
}

/**
 * CH37XSetTimeout - set USB data upload and download timeout
 * @fd: file descriptor of device
 * @iWriteTimeout: data download timeout in milliseconds
 * @iReadTimeout: data upload timeout in milliseconds
 *
 * The function return true if successful, false if fail.
 */
bool CH37XSetTimeout(int fd, uint32_t iWriteTimeout, uint32_t iReadTimeout)
{
	struct _timeout {
		uint32_t read;
		uint32_t write;
	} __attribute__((packed));

	struct _timeout timeout;

	timeout.read = iReadTimeout;
	timeout.write = iWriteTimeout;

	return ioctl(fd, CH37X_SET_TIMEOUT, (unsigned long)&timeout) == 0 ? true : false;
}

/**
 * get_epindex - get endpoint index for endpoint with specified type and address
 * @eptype: endpoint type
 * @epaddr: address of endpoint
 *
 * The function return endpoint index if successful, negative if fail.
 */
int get_epindex(int fd, EPTYPE eptype, uint8_t epaddr)
{
	bool found = false;
	map<uint8_t, uint8_t>::iterator it;
	int i = get_devindex(fd);

	if (i < 0)
		return -1;

	switch (eptype) {
	case EPTYPE_BULKIN:
		for (it = gusbch37x[i].map_bulkin.begin(); it != gusbch37x[i].map_bulkin.end(); it++) {
			if (it->second == epaddr) {
				found = true;
				break;
			}
		}
		break;
	case EPTYPE_BULKOUT:
		for (it = gusbch37x[i].map_bulkout.begin(); it != gusbch37x[i].map_bulkout.end(); it++) {
			if (it->second == epaddr) {
				found = true;
				break;
			}
		}
		break;
	case EPTYPE_INTIN:
		for (it = gusbch37x[i].map_intin.begin(); it != gusbch37x[i].map_intin.end(); it++) {
			if (it->second == epaddr) {
				found = true;
				break;
			}
		}
		break;
	case EPTYPE_INTOUT:
		for (it = gusbch37x[i].map_intout.begin(); it != gusbch37x[i].map_intout.end(); it++) {
			if (it->second == epaddr) {
				found = true;
				break;
			}
		}
		break;
	default:
		break;
	}

	if (found)
		return it->first;

	return -1;
}

/**
 * CH37XSetBufUpload - set USB data to upload in buffered mode or not
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 * @eptype: endpoint type
 * @epaddr: address of endpoint
 *
 * The function return true if successful, false if fail.
 */
bool CH37XSetBufUpload(int fd, bool enable, EPTYPE eptype, uint8_t epaddr, uint32_t urblen)
{
	struct _bulkUp {
		uint8_t epindex;
		uint32_t urblen;
	} __attribute__((packed));

	struct _bulkUp bulkUp;
	int retval;
	int epindex;
	int i = get_devindex(fd);

	if (i < 0)
		return false;

	epindex = get_epindex(fd, eptype, epaddr);
	if (epindex < 0)
		return false;

	if (gusbch37x[i].rbufmode[epindex] && enable)
		return false;

	bulkUp.epindex = epindex;
	bulkUp.urblen = urblen;
	gusbch37x[i].rbufmode[epindex] = enable;

	if (enable)
		retval = ioctl(fd, START_BUFFERED_UPLOAD, (unsigned long)&bulkUp);
	else
		retval = ioctl(fd, STOP_BUFFERED_UPLOAD, (unsigned long)&bulkUp);

	return retval == 0 ? true : false;
}

/**
 * CH37XSetBufDownload - set USB data to download in buffered mode or not
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 * @eptype: endpoint type
 * @epaddr: address of endpoint
 *
 * The function return true if successful, false if fail.
 */
bool CH37XSetBufDownload(int fd, bool enable, EPTYPE eptype, uint8_t epaddr)
{
	int retval;
	int epindex;
	int i = get_devindex(fd);

	if (i < 0)
		return false;

	epindex = get_epindex(fd, eptype, epaddr);
	if (epindex < 0)
		return false;

	if (gusbch37x[i].wbufmode[epindex] && enable)
		return false;

	gusbch37x[i].wbufmode[epindex] = enable;

	return true;
}

/**
 * CH37XASyncReadData - USB data trasfer in asynchronously
 * @fd: file descriptor of device
 * @epindex: index of endpoint
 * @oBuffer: pointer to the data to receive
 * @ioLength: pointer to length in bytes of the data to receive, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
static bool CH37XASyncReadData(int fd, uint32_t epindex, void *oBuffer, uint32_t *ioLength)
{
	struct _bulkUp {
		uint32_t len;
		uint8_t epindex;
		uint8_t data[0];
	} __attribute__((packed));

	struct _bulkUp *bulkUp;
	int retval;

	bulkUp = (struct _bulkUp *)malloc(sizeof(struct _bulkUp) + *ioLength);

	bulkUp->len = *ioLength;
	bulkUp->epindex = epindex;

	retval = ioctl(fd, CH37X_BULK_ASYNC_READ, (unsigned long)bulkUp);
	printf("ioctl: %d\n", retval);
	if (retval < 0) {
		goto exit;
	}

	*ioLength = bulkUp->len;

	memcpy((uint8_t *)oBuffer, bulkUp->data, bulkUp->len);

exit:
	free(bulkUp);
	return retval == 0 ? true : false;
}

/**
 * CH37XSyncReadData - USB data trasfer in synchronously
 * @fd: file descriptor of device
 * @eptype: endpoint type
 * @epindex: index of endpoint
 * @oBuffer: pointer to the data to receive
 * @ioLength: pointer to length in bytes of the data to receive, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
static bool CH37XSyncReadData(int fd, EPTYPE eptype, uint8_t epindex, void *oBuffer, uint32_t *ioLength)
{
	struct _usbUp {
		uint32_t len;
		uint8_t epindex;
		uint8_t data[0];
	} __attribute__((packed));

	struct _usbUp *usbUp;
	int retval = -1;

	usbUp = (struct _usbUp *)malloc(sizeof(struct _usbUp) + *ioLength);

	usbUp->len = *ioLength;
	usbUp->epindex = epindex;

	if (eptype == EPTYPE_BULKIN)
		retval = ioctl(fd, CH37X_BULK_SYNC_READ, (unsigned long)usbUp);
	else if (eptype == EPTYPE_INTIN)
		retval = ioctl(fd, CH37X_INT_SYNC_READ, (unsigned long)usbUp);
	if (retval < 0) {
		goto exit;
	}

	*ioLength = usbUp->len;
	memcpy((uint8_t *)oBuffer, usbUp->data, usbUp->len);

exit:
	free(usbUp);
	return retval == 0 ? true : false;
}

/**
 * CH37XReadData - USB data trasfer in
 * @fd: file descriptor of device
 * @eptype: endpoint type
 * @epindex: index of endpoint
 * @oBuffer: pointer to the data to receive
 * @ioLength: pointer to length in bytes of the data to receive, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
bool CH37XReadData(int fd, EPTYPE eptype, uint8_t epaddr, void *oBuffer, uint32_t *ioLength)
{
	int epindex;
	int i = get_devindex(fd);

	if (i < 0)
		return false;

	epindex = get_epindex(fd, eptype, epaddr);
	if (epindex < 0)
		return false;

	if (gusbch37x[i].rbufmode[epindex])
		return CH37XASyncReadData(fd, epindex, oBuffer, ioLength);
	else
		return CH37XSyncReadData(fd, eptype, epindex, oBuffer, ioLength);
}

/**
 * CH37XASyncWriteData - USB data trasfer out asynchronously
 * @fd: file descriptor of device
 * @epindex: index of endpoint
 * @iBuffer: pointer to the data to send
 * @ioLength: pointer to length in bytes of the data to send, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
static bool CH37XASyncWriteData(int fd, uint8_t epindex, void *iBuffer, uint32_t *ioLength)
{
	struct _bulkDown {
		uint32_t len;
		uint8_t epindex;
		uint8_t data[0];
	} __attribute__((packed));

	struct _bulkDown *bulkDown;
	int retval;

	bulkDown = (struct _bulkDown *)malloc(sizeof(struct _bulkDown) + *ioLength);

	bulkDown->len = *ioLength;
	bulkDown->epindex = epindex;
	memcpy(bulkDown->data, iBuffer, *ioLength);

	retval = ioctl(fd, CH37X_BULK_ASYNC_WRITE, (unsigned long)bulkDown);
	if (retval < 0) {
		goto exit;
	}

	*ioLength = bulkDown->len;

exit:
	free(bulkDown);
	return retval == 0 ? true : false;
}

/**
 * CH37XSyncWriteData - USB data trasfer out synchronously
 * @fd: file descriptor of device
 * @eptype: endpoint type
 * @epindex: index of endpoint
 * @iBuffer: pointer to the data to send
 * @ioLength: pointer to length in bytes of the data to send, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
static bool CH37XSyncWriteData(int fd, EPTYPE eptype, uint8_t epindex, void *iBuffer, uint32_t *ioLength)
{
	struct _usbDown {
		uint32_t len;
		uint8_t epindex;
		uint8_t data[0];
	} __attribute__((packed));

	struct _usbDown *usbDown;
	int retval = -1;

	usbDown = (struct _usbDown *)malloc(sizeof(struct _usbDown) + *ioLength);

	usbDown->len = *ioLength;
	usbDown->epindex = epindex;
	memcpy(usbDown->data, iBuffer, *ioLength);

	if (eptype == EPTYPE_BULKOUT)
		retval = ioctl(fd, CH37X_BULK_SYNC_WRITE, (unsigned long)usbDown);
	else if (eptype == EPTYPE_INTOUT)
		retval = ioctl(fd, CH37X_INT_SYNC_WRITE, (unsigned long)usbDown);
	if (retval < 0) {
		goto exit;
	}

	*ioLength = usbDown->len;

exit:
	free(usbDown);
	return retval == 0 ? true : false;
}

/**
 * CH37XWriteData - USB data trasfer out
 * @fd: file descriptor of device
 * @eptype: endpoint type
 * @epaddr: endpoint address
 * @iBuffer: pointer to the data to send
 * @ioLength: pointer to length in bytes of the data to send, the length will be updated when successful
 *
 * The function return true if successful, false if fail.
 */
bool CH37XWriteData(int fd, EPTYPE eptype, uint8_t epaddr, void *iBuffer, uint32_t *ioLength)
{
	int epindex;
	int i = get_devindex(fd);

	if (i < 0)
		return false;

	epindex = get_epindex(fd, eptype, epaddr);
	if (epindex < 0)
		return false;

	if (eptype == EPTYPE_BULKOUT) {
		if (gusbch37x[i].wbufmode[epindex])
			return CH37XASyncWriteData(fd, epindex, iBuffer, ioLength);
		else
			return CH37XSyncWriteData(fd, eptype, epindex, iBuffer, ioLength);
	} else if (eptype == EPTYPE_INTOUT) {
		return CH37XSyncWriteData(fd, eptype, epindex, iBuffer, ioLength);
	}

	return false;
}

/**
 * CH37XControlIn - control trasfer in
 * @fd: file descriptor of device
 * @request: USB message request value
 * @requesttype: USB message request type value
 * @value: USB message value
 * @index: USB message index value
 * @data: pointer to the data to send
 * @size: length in bytes of the data to send
 *
 * The function return 0 if successful, others if fail.
 */
int CH37XControlIn(int fd, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, uint8_t *data,
		   uint16_t size)
{
	struct _controlmsg {
		uint8_t request;
		uint8_t requesttype;
		uint16_t value;
		uint16_t index;
		uint16_t size;
		uint8_t data[0];
	} __attribute__((packed));

	int ret;
	struct _controlmsg *controlmsg;

	controlmsg = (struct _controlmsg *)malloc(sizeof(struct _controlmsg) + size);

	controlmsg->request = request;
	controlmsg->requesttype = requesttype;
	controlmsg->value = value;
	controlmsg->index = index;
	controlmsg->size = size;

	ret = ioctl(fd, CH37X_CTRL_SYNC_READ, (unsigned long)controlmsg);
	if (ret < 0) {
		goto exit;
	}

	memcpy(data, controlmsg->data, ret);

exit:
	free(controlmsg);
	return ret;
}

/**
 * CH37XControlOut - control trasfer out
 * @fd: file descriptor of device
 * @request: USB message request value
 * @requesttype: USB message request type value
 * @value: USB message value
 * @index: USB message index value
 * @data: pointer to the data to send
 * @size: length in bytes of the data to send
 *
 * The function return 0 if successful, others if fail.
 */
int CH37XControlOut(int fd, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, uint8_t *data,
		    uint16_t size)
{
	struct _controlmsg {
		uint8_t request;
		uint8_t requesttype;
		uint16_t value;
		uint16_t index;
		uint16_t size;
		uint8_t data[0];
	} __attribute__((packed));

	int ret;
	struct _controlmsg *controlmsg;

	controlmsg = (struct _controlmsg *)malloc(sizeof(struct _controlmsg) + size);

	controlmsg->request = request;
	controlmsg->requesttype = requesttype;
	controlmsg->value = value;
	controlmsg->index = index;
	controlmsg->size = size;
	memcpy(controlmsg->data, data, size);

	ret = ioctl(fd, CH37X_CTRL_SYNC_WRITE, (unsigned long)controlmsg);

	free(controlmsg);
	return ret;
}
