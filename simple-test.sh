#!/bin/sh

set -x

[[ -z $1 ]] && mtpt=/mnt || mtpt="$1"


domount() {
	mount/mount_o9fs '10.0.2.1!5648' $mtpt || exit 1
}

dounmount() {
	umount -f $mtpt || exit 1
}

cwd=`pwd`

{
	testdir=$mtpt/tmp/testdir
	testfile=$testdir/file0	
	domount
	mkdir $testdir || exit 1
	ls -al $testdir || exit 1
	touch $testfile || exit 1
	echo abc > .xx || exit 1
	cat .xx > $testfile || exit 1
	cat $testfile > .xx2 || exit 1
	cmp .xx .xx2 || exit 1
	echo def > .xx || exit 1
	cat .xx >> $testfile || exit 1
	cat <<! >.xx || exit 1
abc
def
!
	cat $testfile > .xx2 || exit 1
	cmp .xx .xx2 || exit 1
	rm .xx .xx2 || exit 1
	rm $testfile || exit 1
	rmdir $testdir || exit 1
	dounmount
	cd $cwd
}

{
	testdir=./testdir
	testfile=./file0
	domount
	cd $mtpt/tmp
	mkdir $testdir || exit 1
	cd $testdir || exit 1
	ls -al || exit 1
	touch $testfile || exit 1
	echo abc > /tmp/.xx || exit 1
	cat /tmp/.xx > $testfile || exit 1
	cat $testfile > /tmp/.xx2 || exit 1
	cmp /tmp/.xx /tmp/.xx2 || exit 1
	rm /tmp/.xx /tmp/.xx2 || exit 1
	rm $testfile || exit 1
	cd .. || exit 1
	rmdir $testdir || exit 1
	dounmount
	cd $cwd
}

{
	testdir=$mtpt/tmp/testdir
	testfile=$testdir/file0
	domount
	mkdir $testdir || exit 1
	dd if=/dev/random of=$testfile bs=8192 count=1000 || exit 1
	rm -rf $testdir || exit 1
	dounmount
	cd $cwd
}


