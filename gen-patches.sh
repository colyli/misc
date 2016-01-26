#!/bin/bash -x

prog=gen-patches

if [ "$#" != 2 ];then
	echo "usage: $prog rev_list_file output_dir"
	exit 1
fi

rev_list_file=$1
output_dir=$2

if ! [ -f "$rev_list_file" ];then
	echo "rev list file $rev_list_file does not exist"
	exit 1
fi

if ! [ -d "$output_dir" ];then
	echo "output dir $output_dir does not exist"
	exit 1
fi

num=1
for commit in `cat "$rev_list_file"`;do
	git format-patch -o "$output_dir" --start-number $num -1 $commit
	num=`expr $num + 1`
done
