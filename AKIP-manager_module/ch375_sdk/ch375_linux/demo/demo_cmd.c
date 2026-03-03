/*
 * USB application for ch372/ch374/ch375/ch376/ch378 etc.
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
 * V1.2 - call APIs with new library
 *
 */

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "ch37x_lib.h"

struct ch37x {
	int fd;
	char version[100];
	uint32_t dev_id;
	uint8_t bulkinnum;
	uint8_t bulkoutnum;
	uint8_t intinnum;
	uint8_t intoutnum;
};

static struct ch37x ch37xdevice;

static const char *devname = "/dev/ch37x0";

/**
 * Show_DevMsg - dump device messages
 * @fd: file descriptor of device
 *
 */
void Show_DevMsg(struct ch37x *dev)
{
	bool retval;
	int i;

	memset(dev->version, 0x00, sizeof(dev->version));
	printf("\n");
	printf("Information of %s lists below:\n", devname);
	printf("\n**************************************\n");
	retval = CH37XGetDriverVersion(dev->fd, dev->version);
	if (retval == false) {
		printf("CH37XGetDriverVersion error.\n");
		return;
	}

	retval = CH37XGetDeviceID(dev->fd, &dev->dev_id);
	if (retval == false) {
		printf("CH37XGetDeviceID error.\n");
		return;
	}
	retval = CH37XGetDeviceEpMsg(dev->fd);
	if (retval == false) {
		printf("CH37XGetDeviceEpMsg error.\n");
		return;
	}

	dev->bulkinnum = CH37XGetObject(dev->fd)->epmsg_bulkin.epnum;
	dev->bulkoutnum = CH37XGetObject(dev->fd)->epmsg_bulkout.epnum;
	dev->intinnum = CH37XGetObject(dev->fd)->epmsg_intin.epnum;
	dev->intoutnum = CH37XGetObject(dev->fd)->epmsg_intout.epnum;

	printf("Driver version: %s\n", dev->version);
	printf("Vendor ID: 0x%4x, Product ID: 0x%4x\n", (uint16_t)dev->dev_id, (uint16_t)(dev->dev_id >> 16));
	printf("\n******Interface Endpoint Message Dump******\n");
	printf("\tbulk_in_num: %d\n", dev->bulkinnum);
	if (dev->bulkinnum != 0) {
		for (i = 0; i < dev->bulkinnum; i++)
			printf("\tEndpoint[%d]: 0x%2x\n", i, CH37XGetObject(dev->fd)->epmsg_bulkin.epaddr[i]);
	}
	printf("\tbulk_out_num: %d\n", dev->bulkoutnum);
	if (dev->bulkoutnum != 0) {
		for (i = 0; i < dev->bulkoutnum; i++)
			printf("\tEndpoint[%d]: 0x%2x\n", i, CH37XGetObject(dev->fd)->epmsg_bulkout.epaddr[i]);
	}
	printf("\tinterrupt_in_num: %d\n", dev->intinnum);
	if (dev->intinnum != 0) {
		for (i = 0; i < dev->intinnum; i++)
			printf("\tEndpoint[%d]: 0x%2x\n", i, CH37XGetObject(dev->fd)->epmsg_intin.epaddr[i]);
	}
	printf("\tinterrupt_out_num: %d\n", dev->intoutnum);
	if (dev->intoutnum != 0) {
		for (i = 0; i < dev->intoutnum; i++)
			printf("\tEndpoint[%d]: 0x%2x\n", i, CH37XGetObject(dev->fd)->epmsg_intout.epaddr[i]);
	}
	printf("**************************************\n\n");
}

/**
 * sig_handler_sigint - callback routine for SIGINT
 * @signo: signal value
 *
 */
void sig_handler_sigint(int signo)
{
	exit(0);
}

int main(int argc, char *argv[])
{
	int devnum = 0;
	bool ret;
	char choice, ch;
	uint8_t wbuffer[1024];
	uint8_t rbuffer[1024];
	uint32_t wlen;
	uint32_t rlen;

	/* register callback for sigint */
	signal(SIGINT, sig_handler_sigint);

	/* scan devices in /dev directory */
	devnum = CH37XScanDevices();
	printf("Scan %d devices.\n", devnum);

	/* quit if no device connected */
	if (devnum == 0)
		return 0;

	/* open device in non-block mode */
	ch37xdevice.fd = CH37XOpenDevice(devname, true);
	if (ch37xdevice.fd < 0) {
		printf("CH37XOpenDevice false.\n");
		return -1;
	}

	/* set device not to be exclusive  */
	ret = CH37XSetExclusive(ch37xdevice.fd, false);
	if (ret == false) {
		printf("CH37XSetExclusive false.\n");
		return -1;
	}

	/* set usb read and write timeout in milliseconds */
	ret = CH37XSetTimeout(ch37xdevice.fd, 3000, 3000);
	if (ret == false) {
		printf("CH37XSetTimeout false.\n");
		return -1;
	}

	/* dump message of device */
	Show_DevMsg(&ch37xdevice);

	sleep(1);

	/* set bulk in endpoint in non-buffered mode */
	ret = CH37XSetBufUpload(ch37xdevice.fd, false, EPTYPE_BULKIN,
				CH37XGetObject(ch37xdevice.fd)->epmsg_bulkin.epaddr[0], 1024);
	if (ret == false) {
		printf("CH37XSetBufUpload false.\n");
		return -1;
	}

	/* set bulk out endpoint in non-buffered mode */
	ret = CH37XSetBufDownload(ch37xdevice.fd, false, EPTYPE_BULKOUT,
				  CH37XGetObject(ch37xdevice.fd)->epmsg_bulkout.epaddr[0]);
	if (ret == false) {
		printf("CH37XSetBufUpload false.\n");
		return -1;
	}

	while (1) {
		printf("press r to read data from device, w to write data from host to device, "
		       "q to quit this app.\n");

		scanf("%c", &choice);
		while ((ch = getchar()) != EOF && ch != '\n')
			;
		if (choice == 'q')
			break;

		switch (choice) {
		case 'r':
			printf("input length to read:\n");
			scanf("%d", &rlen);
			while ((ch = getchar()) != EOF && ch != '\n')
				;
			ret = CH37XReadData(ch37xdevice.fd, EPTYPE_BULKIN,
					    CH37XGetObject(ch37xdevice.fd)->epmsg_bulkin.epaddr[0], rbuffer, &rlen);
			if (ret == false) {
				printf("CH37XReadData false.\n");
				break;
			}
			printf("Read %d bytes this time.\n", rlen);
			break;
		case 'w':
			printf("input length to write:\n");
			scanf("%d", &wlen);
			while ((ch = getchar()) != EOF && ch != '\n')
				;
			ret = CH37XWriteData(ch37xdevice.fd, EPTYPE_BULKOUT,
					     CH37XGetObject(ch37xdevice.fd)->epmsg_bulkout.epaddr[0], wbuffer, &wlen);
			if (ret == false) {
				printf("CH37XWriteData false.\n");
				break;
			}
			printf("Wrote %d bytes this time.\n", wlen);
			break;
		default:
			printf("Bad Choice, please retry.\n");
			break;
		}
	}

	/* close device */
	ret = CH37XCloseDevice(ch37xdevice.fd);
	if (ret == false) {
		printf("CH37XCloseDevice false.\n");
	}

	return 0;
}
