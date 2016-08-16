#!/bin/bash -x

LINUX_GIT=/home/colyli/source/linux/linux
SUSE_GIT=/home/colyli/source/suse-kernel/kernel-source
WORK_DIR=/home/colyli/source/work-backport
START_ID=v4.4
END_ID=HEAD
TARGET_DIR=drivers/md

dir_made=0
for i in `seq 10`;do
	workdir="$WORK_DIR".$i
	if [ -e $workdir ];then
		echo "work directory '$workdir' exists, skip .."
		continue
	fi
	mkdir $workdir
	if [ $? != 0 ];then
		echo "create work dir $workdir failed."
		exit 1
	fi
	dir_made=1
	break
done

if [ $dir_made == 0 ];then
	echo "fail to create work dir, "$WORK_DIR".[1-10] all exist"
	exit 1
fi

pushd $LINUX_GIT
if [ $? != 0 ];then
	echo "cd $LINUX_GIT failed"
	exit 1
fi

git rev-list --reverse "$START_ID".."$END_ID" "$TARGET_DIR" > $workdir/rev-list
if [ $? != 0 ];then
	echo "git rev-list failed"
	exit 1
fi
popd

pushd $SUSE_GIT
if [ $? != 0 ];then
	echo "cd $SUSE_GIT failed"
	exit 1
fi

rm -f $workdir/backport-list
for id in `cat $workdir/rev-list`;do
	grep -nr $id $SUSE_GIT >/dev/null
	if [ $? == 0 ];then
		echo "merged in SUSE kernel, id $id"
	else
		echo "not merged, id $id"
		echo $id >> $workdir/backport-list
	fi
done
popd

pushd $LINUX_GIT
num=1
for id in `cat $workdir/backport-list`;do
	git format-patch -o $workdir --start-number $num -1 $id
	if [ $? != 0 ];then
		echo "git format-patch failed, id $id"
		exit 1
	fi
	num=`expr $num + 1`
done

cd $workdir

# popd
