#!/bin/bash

nr=1;for f in `cat namelist`;do n=`printf %.4d $nr`;echo $n;nr=`expr $nr + 1`;name=`echo $f| sed "s/^.*[0-9]-/$n-/"`;cp $f tmp/$name;done
