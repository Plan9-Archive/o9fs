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
testfile="$mntp/test0"

readdir="ls -l $mntp"
stat="stat $mntp"
create="touch $testfile"
write="echo test0 > $testfile"
read="cat $testfile"
remove="rm $testfile"

echo "Testing o9fs mounted on $mntp"
echo "readdir(), $readdir"
$readdir
echo
echo "stat(), $stat"
$stat
echo
echo "create(), $create"
$create
echo
echo "write(), $write"
$write
echo
echo "read(), $read"
$read
echo
echo "remove(), $remove"
$remove
echo
