#ifndef _CH37X_LIB_H
#define _CH37X_LIB_H

#include <stdint.h>
#include <sys/ioctl.h>
#include <map>

using namespace std;

#define CH37X_MAX_NUMBER 16
#define MAX_EP_NUM	 8

enum EPTYPE {
	EPTYPE_BULKIN = 0,
	EPTYPE_BULKOUT,
	EPTYPE_INTIN,
	EPTYPE_INTOUT,
};

struct ch37x_epmsg {
	uint8_t eptype;
	uint8_t epnum;
	uint8_t epaddr[MAX_EP_NUM];
};

struct usb_ch37x {
	struct ch37x_epmsg epmsg_bulkin;
	struct ch37x_epmsg epmsg_bulkout;
	struct ch37x_epmsg epmsg_intin;
	struct ch37x_epmsg epmsg_intout;

	map<uint8_t, uint8_t> map_bulkin;
	map<uint8_t, uint8_t> map_bulkout;
	map<uint8_t, uint8_t> map_intin;
	map<uint8_t, uint8_t> map_intout;

	int fd;

	bool rbufmode[MAX_EP_NUM];
	bool wbufmode[MAX_EP_NUM];
};

/**
 * CH37XGetObject - get global device object
 *
 * The function return pointer to device.
 */
extern struct usb_ch37x *CH37XGetObject(int fd);

/**
 * CH37XScanDevices - scan usb devices in /dev directory
 *
 * The function return device amounts.
 */
extern int CH37XScanDevices();

/**
 * CH37XOpenDevice - open device
 * @pathname: device path in /dev directory
 * @nonblock: access device in nonblock mode if true, blocked if false
 *
 * The function return file descriptor if successful, others if fail.
 */
extern int CH37XOpenDevice(const char *pathname, bool nonblock);
/**
 * CH37XCloseDevice - close device
 * @fd: file descriptor of device
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XCloseDevice(int fd);

/**
 * CH37XSetExclusive - set device exclusive access
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XSetExclusive(int fd, bool enable);

/**
 * CH37XGetDriverVersion - get driver verion
 * @fd: file descriptor of device
 * @oBuffer: pointer to version buffer
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XGetDriverVersion(int fd, void *oBuffer);

/**
 * CH37XGetDeviceID - get device vid and pid
 * @fd: file descriptor of device
 * @id: pointer to store id which contains vid and pid
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XGetDeviceID(int fd, uint32_t *id);

/**
 * CH37XGetDeviceEpMsg - get device endpoint messages and save to global device structure
 * @fd: file descriptor of device
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XGetDeviceEpMsg(int fd);

/**
 * CH37XGetDeviceDescr - get device descriptor
 * @fd: file descriptor of device
 * @buffer: pointer to descripotor buffer
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XGetDeviceDescr(int fd, uint8_t *buffer);

/**
 * CH37XSetTimeout - set USB data upload and download timeout
 * @fd: file descriptor of device
 * @iWriteTimeout: data download timeout in milliseconds
 * @iReadTimeout: data upload timeout in milliseconds
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XSetTimeout(int fd, uint32_t iWriteTimeout, uint32_t iReadTimeout);

/**
 * CH37XSetBufUpload - set USB data to upload in buffered mode or not
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 * @eptype: endpoint type
 * @epaddr: address of endpoint
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XSetBufUpload(int fd, bool enable, EPTYPE eptype, uint8_t epaddr, uint32_t urblen);

/**
 * CH37XSetBufDownload - set USB data to download in buffered mode or not
 * @fd: file descriptor of device
 * @enable: true on enable, false on disable
 * @eptype: endpoint type
 * @epaddr: address of endpoint
 *
 * The function return true if successful, false if fail.
 */
extern bool CH37XSetBufDownload(int fd, bool enable, EPTYPE eptype, uint8_t epaddr);

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
extern bool CH37XReadData(int fd, EPTYPE eptype, uint8_t epaddr, void *oBuffer, uint32_t *ioLength);

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
extern bool CH37XWriteData(int fd, EPTYPE eptype, uint8_t epaddr, void *iBuffer, uint32_t *ioLength);

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
extern int CH37XControlIn(int fd, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, uint8_t *data,
		   uint16_t size);

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
extern int CH37XControlOut(int fd, uint8_t request, uint8_t requesttype, uint16_t value, uint16_t index, uint8_t *data,
		    uint16_t size);

#endif
