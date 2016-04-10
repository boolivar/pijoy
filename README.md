# pijoy
Sega megadrive gamepad driver for raspberry pi gpio

1. Get running kernel sources
  * find pi firmware git hash:
  ```bash
  zcat /usr/share/doc/raspberrypi-bootloader/changelog.Debian.gz | head
  ```
  * find pi kernel sources git hash:
  ```
  firmware/extra/git_hash
  ```
  * get pi kernel sources
  * setup KERNEL_SRC environment variable:
  ```bash
  export KERNEL_SRC={path-to-kernel-sources}
  ```
  * get Module.symvers from firmware repo:
  ```bash
  cp firmware/extra/Module.symvers ${KERNEL_SRC}
  ```
2. Get running kernel config
  * ensure configs module running:
  ```bash
  modprobe configs
  ```
  * get running kernel config:
  ```bash
  zcat /proc/config.gz > ${KERNEL_SRC}/.config
  ```
3. Setup compiler tools
  * setup CCPREFIX environment variable:
  ```bash
  export CCPREFIX=/{tools-path}/arm-bcm2708/{compiler-path}/bin/arm-linux-gnueabihf-
  ```
  * prepare kernel sources:
  ```bash
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} oldconfig
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} modules_prepare
  ```
4. Build module
  * run make within pijoy module directory:
  ```bash
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} -C ${KERNEL_SRC} M=$(pwd)
  ```
5. Upload and run compiled module
  * upload module to pi:
  ```bash
  scp pijoy.ko pi@raspberry:/home/pi/
  ```
  * install module on pi:
  ```bash
  mv /home/pi/pijoy.ko /lib/modules/$(uname -r)/
  ```
  * load module on pi:
  ```bash
  modprobe pijoy dev1=0,6
  ```

## Links
---

[Building pi kernel](http://elinux.org/Raspberry_Pi_Kernel_Compilation)

### Github

[Pi compiler tools](https://github.com/raspberrypi/tools)

[Pi kernel sources](https://github.com/raspberrypi/linux)

[Pi firmware (kernel+modules) builds](https://github.com/raspberrypi/firmware)

[rpi-update tool for easy firmware update](https://github.com/Hexxeh/rpi-update)

[Pi firmware mirror used by rpi-update tool](https://github.com/Hexxeh/rpi-firmware)

[rpi-source utility that installs the kernel source](https://github.com/notro/rpi-source)
