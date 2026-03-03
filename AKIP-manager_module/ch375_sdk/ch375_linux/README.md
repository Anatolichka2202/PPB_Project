# ch37x linux USB device SDK

## Description

Linux driver and application for USB devices ch372/ch374/ch375/ch376/ch378 etc.

The SDK is used to operate the devices above, which supports multi-endpoints communication, supports usb control/bulk/interrupt transfer and sync/async mode for bulk transfer.

The directory contains demo, driver and lib directories.

demo: source file for the application and development dynamic library and related header files
driver: USB device driver

## Driver Operating Overview

1. Open "Terminal"
2. Switch to "driver" directory
3. Compile the driver using "make", you will see the module "ch37x.ko" if successful
4. Type "sudo make load" or "sudo insmod ch37x.ko" to load the driver dynamically
5. Type "sudo make unload" or "sudo rmmod ch37x.ko" to unload the driver
6. Type "sudo make install" to make the driver work permanently
7. Type "sudo make uninstall" to remove the driver

Before the driver works, you should make sure that the usb device has been plugged in and is working properly, you can use shell command "lsusb" or "dmesg" to confirm that, USB VID of these devices are [1A86] or [4348], you can view all IDs from the id table which defined in "ch37x.c".

If the device works well, the driver will created tty devices named "ch37x*" in /dev directory.

## Application Operating Overview

1. Switch to "demo" directory
2. Type "g++ demo_cmd.c ch37x_lib.c -o app" to compile the demo

## Note

Any question, you can send feedback to mail: tech@wch.cn
