#!/bin/sh
# installs o9fs stuff in kernel source under /usr/src/sys
# written by Iruata Souza
# See LICENSE
# JUL/2007

o9fsroot=miscfs/o9fs

addfiles() {
	cp /usr/src/sys/conf/files /usr/src/sys/conf/files.old

	[[ -f files.o9fs ]] && rm -f files.o9fs

	for f in *.c; do
		[[ "$f" == "o9fs_convD2M.c" ]] && continue
		echo "file $o9fsroot/$f		o9fs" >> files.o9fs
	done

	exist=$(grep o9fs /usr/src/sys/conf/files)
	[[ ! -z $exist ]] && exit 0

	echo >> /usr/src/sys/conf/files
	cat files.o9fs >> /usr/src/sys/conf/files
}

addfiles