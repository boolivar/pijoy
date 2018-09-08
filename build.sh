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
  echo "No kernel source found"
  exit 1
fi

cd ${BUILD_DIR}
make ARCH=arm CROSS_COMPILE=${CCPREFIX} oldconfig && \
  make ARCH=arm CROSS_COMPILE=${CCPREFIX} modules_prepare
