#!/bin/sh

dirs="certs crypto firmware init ipc net samples scripts security sound tools virt"

arch_dirs="arc arm64 blackfin cris h8300 ia64 m68k microblaze mn10300 nios2 parisc powerpc score s390 sh sparc tile unicore32 alpha arm avr32 c6x frv hexagon m32r metag mips openrisc um x86/xen x86/ia32 x86/lguest x86/power xtensa"

drivers_dirs="accessibility acpi amba android ata atm auxdisplay base bcma block/aoe block/parideblock/xen-blkback bluetooth bus cdrom char clk clocksource connector cpufreq cpuidle crypto dax dca devfreq block/drbd edac disa eisa extcon firewire firmware fmc fpga gpio gpu hid hsi hv hwmon hwspinlock hwtracing i2c ide idle ieee802154 iio infiniband input iommu ipack irqchip isdn leds lguest lightnvm macintosh mailbox mca mcb media memory memstick message mfd misc mmc mtd net nfc ntb nubus of oprofile parisc parport pci pcmcia phy pinctrl platform pnp power powercap pps ps3 ptp pwm rapidio ras regulator remoteproc reset rpmsg rtc s390 sbus sfi sh sn soc spi ssb spmi staging target tc telephony thermal thunderbolt tty uio usb uwb vfio vhost video virt virtio vlynq vme w1 watchdog xen zorro"

fs_dirs="9p adfs affs afs autofs4 befs bfs btrfs cachefiles ceph cifs coda cramfs devpts dlm dmapi ecryptfs efivarfs efs exofs exportfs ext2 ext3 f2fs fat freevxfs fscache fuse gfs2 hfs hfsplus hostfs hpfs hppfs hugetlbfs isofs jbd jffs2 jfs lockd logfs minix ncpfs nfs nfs_common nfsd nilfs2 nls notify ntfs ocfs2 omfs openpromfs overlayfs overlayfs-old pstore qnx4 qnx6 quota ramfs reiserfs romfs squashfs squashfs3 sysv ubifs udf ufs xfs"

include_dirs="acpi clocksource crypto drm dt-bindings generated keys kvm math-emu media misc net pcmcia ras rdma rxrpc soc sound target trace video xen"

for d in $dirs;do
	rm -rf $d
done

for d in $arch_dirs;do
	rm -rf arch/$d
done

for d in $drivers_dirs;do
	rm -rf drivers/$d
done

for d in $fs_dirs;do
	rm -rf fs/$d
done

for d in $include_dirs;do
	rm -rf include/$d
done
