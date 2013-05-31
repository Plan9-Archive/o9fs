#!/bin/sh

if [[ $1 == "-v" ]]; then
	try() {
		$* || exit $?
    }
	shift
else
	try() {
		$* >/dev/null 2>&1 || exit $?
    }
fi


[[ -z $1 ]] && mtpt=/mnt || mtpt="$1"

testdir=$mtpt/tmp/testdir
testfile=$testdir/test0

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
mkdir
create
readdir
lreaddir
xwrite
xread
remove
readdir
lreaddir
rmdir
dounmount
