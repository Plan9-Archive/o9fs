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

rdir() {
	echo Reading directory
	try ls $mtpt/net
}

rdirlong() {
	echo Reading directory
	try ls -l $mtpt/net
}



rdir
rdirlong
exit 0
	
readdir="ls -l $mtpt"
readdir2="(cd $mtpt; ls -l)"
stat="ls -ld $mtpt"
mkdir="mkdir $testdir"
create="touch $testfile"
write="echo test0 > $testfile"
read="cat $testfile"
remove="rm $testfile"

echo "o9fs mounted on $mtpt"
echo "readdir, $readdir"
$readdir
echo "readdir2, $readdir2"
$readdir2
#echo "stat(), $stat"
#$stat
#echo "mkdir(), $mkdir"
#$mkdir
#echo "create(), $create"
#$create
#echo "write(), $write"
#$write
#echo "read(), $read"
#$read
#echo "remove(), $remove"
#$remove
