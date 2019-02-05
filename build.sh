#!/bin/sh

if [ $1 ]; then
  cd ${KERNEL_DIR} && rm -rf ./*

  kernel_ref=`curl -L https://github.com/raspberrypi/firmware/raw/$1/extra/git_hash`

  echo "Fetch kernel from https://github.com/raspberrypi/linux/tree/${kernel_ref}"

  curl -L https://github.com/raspberrypi/linux/archive/${kernel_ref}.tar.gz -o kernel.tar.gz && \
      tar -xzf kernel.tar.gz && \
      find . -mindepth 2 -maxdepth 2 -exec mv -t . '{}' +

  rm -rf linux-${kernel_ref}
  rm kernel.tar.gz
  curl -L https://github.com/raspberrypi/firmware/raw/$1/extra/Module.symvers -o Module.symvers
fi

if [ ! "$(ls ${KERNEL_DIR})" ]; then
  echo "No kernel source found. Try to pass your firmware version tag (e.g. '1.20181112') or commit hash from https://github.com/raspberrypi/firmware repo"
  exit 1
fi

cp ${BUILD_DIR}/.config ${KERNEL_DIR}
cd ${KERNEL_DIR} && \
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} oldconfig && \
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} modules_prepare && \
  cd ${BUILD_DIR} && \
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} -C ${KERNEL_DIR} M=$(pwd)
