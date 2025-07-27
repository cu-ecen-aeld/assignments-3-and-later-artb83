#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

#Exit if failed to create outdir directory
if [ ! -d "${OUTDIR}" ]; then
	echo "Could not create a "$OUTDIR" directory"
	exit 1
else
	echo $OUTDIR" created successfully"
	echo
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION} 
    # TODO: Add your kernel build steps here
    # Deep clean
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
	# Generate default configuration for QEMU
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig 
	# Build kernel image
    make -j6 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
	# Build kernel modules - skipped in this (3 part 2) assignment
    # make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
	# Build kernel device tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs  
fi

echo "Adding the Image in outdir "${OUTDIR}
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
sudo mkdir -p rootfs
if [ ! -d ./rootfs ]; then
	echo "Could not create a "$OUTDIR" directory"
	exit 1
else
	cd rootfs
fi
echo
echo "Creating necessary base directories"
echo

sudo mkdir -p ./bin ./dev ./etc ./lib ./lib64 ./proc ./sys ./sbin ./tmp 
sudo mkdir -p ./home/conf ./usr/bin ./usr/sbin ./usr/lib	
sudo mkdir -p ./var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
	git clone https://git.busybox.net/busybox
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Configure busybox"
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Make and install busybox"
make -j6 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} busybox
sudo make -j6 CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=arm64 CROSS_COMPILE=/usr/local/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu- install
cd ${OUTDIR}/rootfs
echo
echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Adding library dependencies to rootfs"
sudo cp /usr/local/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib

sudo cp /usr/local/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6  ${OUTDIR}/rootfs/lib64

sudo cp /usr/local/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2  ${OUTDIR}/rootfs/lib64

sudo cp /usr/local/arm-cross-compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6  ${OUTDIR}/rootfs/lib64


# TODO: Make device nodes
echo "Making device nodes"
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility"
cd ${FINDER_APP_DIR}
sudo cp writer ${OUTDIR}/rootfs/home

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy the finder related scripts and executables to the /home directory on the target rootfs"

sudo cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
sudo cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
sudo cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf
sudo cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf
sudo cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home

echo "Show outdir rootfs and rootfs/home content"
echo ${OUTDIR}"/rootfs/home content"
ls -l ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
sudo chown --recursive root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
sudo find . | /usr/bin/cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
echo ${OUTDIR}"/rootfs content"
ls -l 
cd ${OUTDIR}
gzip -f initramfs.cpio
sudo chown --recursive root:root initramfs.cpio.gz
echo ${OUTDIR}" content - End of manual-linux.sh"
ls -l
