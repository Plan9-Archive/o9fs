#!/bin/sh
# installs o9fs stuff in kernel source under /usr/src/sys
# written by Iruata Souza
# See LICENSE
# JUL/2007

o9fsroot=miscfs/o9fs
srcroot=/usr/src

addfiles() {
	cp $srcroot/sys/conf/files $srcroot/sys/conf/files.old

	[[ -f files.o9fs ]] && rm -f files.o9fs

	for f in *.c; do
		[[ "$f" == "o9fs_lkm.c" ]] && continue
		echo "file $o9fsroot/$f		o9fs" >> files.o9fs
	done

	exist=$(grep o9fs $srcroot/sys/conf/files)
	[[ ! -z $exist ]] && return 

	echo "# o9fs" >> $srcroot/sys/conf/files
	echo "include $o9fsroot/files.o9fs" >> $srcroot/sys/conf/files
}

patch() {
	cd $srcroot
	patch -p1 < sys/$o9fsroot/openbsd-headers.diff
}

echo "Installing o9fs source in a kernel tree under $srcroot...
I will make a symlink $srcroot/$o9fsroot pointing to `pwd` (here)
and add o9fs sources to $srcroot/sys/conf/files
do you want me to continue? [y/n]"
read ans

[[ "$ans" != "y" ]]  && exit 0

ln -s `pwd` $srcroot/sys/$o9fsroot
addfiles

echo
echo "o9fs installed under $srcroot/$o9fsroot
You should now edit your kernel configuration and add

option O9FS
"
