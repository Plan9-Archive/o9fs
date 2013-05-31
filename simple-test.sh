#!/bin/ksh

[[ -z $1 ]] && mtpt=/mnt || mtpt="$1"

testdir=$mtpt/tmp/testdir
testfile=$testdir/file0

try() {
	$* || exit 1 
}

domount() {
	try mount/mount_o9fs '10.0.2.1!5648' $mtpt
}

dounmount() {
	try umount $mtpt
}

readdir() {
	try ls $testdir
}

lreaddir() {
	try ls -l $testdir
}

create() {
	>$testfile
}

mkdir() {
	try mkdir $testdir
}

xwrite() {
	echo abc > .xx
	try dd if=.xx of=$testfile
	rm .xx
}

xread() {
	try dd if=$testfile of=.xx
	cat .xx
	rm .xx
}

remove() {
	try rm $testfile
}

rmdir() {
	try rmdir $testdir
}

domount
#mkdir
#create
#readdir
#lreaddir
#xwrite
#xread
#remove
#readdir
#lreaddir
#rmdir
#dounmount
