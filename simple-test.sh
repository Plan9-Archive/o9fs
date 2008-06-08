#!/bin/sh
# simple tests for o9fs
# written by Iruata Souza
# see LICENSE
# JUL2007

usage() {
	echo "usage: $0 mountpoint"
	exit 1
}

[[ -z $1 ]] && usage

mntp="$1"
testdir="$mntp/testdir"
testfile="$testdir/test0"

readdir="ls -l $mntp"
stat="ls -ld $mntp"
mkdir="mkdir $testdir"
create="touch $testfile"
write="echo test0 > $testfile"
read="cat $testfile"
remove="rm $testfile"

echo "Testing o9fs mounted on $mntp"
echo "readdir(), $readdir"
$readdir
echo "stat(), $stat"
$stat
echo "mkdir(), $mkdir"
$mkdir
echo "create(), $create"
$create
echo "write(), $write"
$write
echo "read(), $read"
$read
echo "remove(), $remove"
$remove
