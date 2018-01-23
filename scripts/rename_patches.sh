#!/bin/bash -x

if [ $# -lt 1 ] || [ $# -gt 2 ];then
	echo "usage: $0 name-list <start_id>"
	echo "   name-list is a patch name list without ID"
	exit 0
fi

list=$1

nr=1
if [ $# == 2 ];then
	nr=$2
fi
for p in `cat $list`;do
	f=$p
	p=`echo $p|sed "s/^[0-9]\{4\}-//"`
	ls *"$p"*
	if [ $? != 0 ];then
		echo "$p does not exist"
		continue
	fi

	# in case $f has seq ID
	id=`printf %.4d $nr`
	nr=`expr $nr + 1`
	name=$id"-"$p
	mv $f $name
done
