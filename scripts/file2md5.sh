#!/bin/bash 

usage()
{
	echo "file2md5 <input_file>"
	exit 1
}

if [ $# != 1 ];then
	usage
fi

in_file=$1

file_made=0
for i in `seq 10`;do
        out_file=out_file.$i
        if [ -e $out_file ];then
                echo "output file '$out_file' exists, skip .."
                continue
        fi
        file_made=1
        break
done

if [ $file_made == 0 ];then
        echo "fail to create output file, out_file.[1-10] all exist"
        exit 1
fi


while read line
do
	# visible
#	echo -en $line | sed "s/\r//g" |md5sum | sed "s/  -//" | tee $out_file
	# faster
	echo -en $line | sed "s/\r//g" |md5sum | sed "s/  -//" >> $out_file
done < $in_file
