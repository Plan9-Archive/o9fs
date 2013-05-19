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

readdir() {
	try ls $mtpt/net
}

lreaddir() {
	try ls -l $mtpt/net
}

create() {
	try touch $mtpt/net/file
}

mkdir() {
	try mkdir $mtpt/net/dir
}

xwrite() {
	echo abc > .xx
	try dd if=.xx of=$mtpt/net/dir
	rm .xx
}

xread() {
	try dd if=$mtpt/net/dir of=.xx
	cat .xx
	rm .xx
}

remove() {
	try rm $mtpt/net/file
}

rmdir() {
	try rmdir $mtpt/net/dir
}

readdir
lreaddir
create
mkdir
rmdir
xwrite
xread
remove
	
