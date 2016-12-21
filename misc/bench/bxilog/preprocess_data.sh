#!/bin/bash

if test $# -ne 1; then 
	echo "Usage: $(basename $0) bench_directory"
	exit 1
fi

dir=$1

max_threads=1
for i in $dir/*.bench;do 
    echo "Preprocessing $i"
    label=$(basename $i|sed 's/_*\.*bench-*//g')
    cat $i| while read line ;do
        threads=$(echo $line|cut -d ' ' -f 2)
        if ((threads > max_threads));then 
            max_threads=$threads
        fi
        duration=$(echo $line|cut -d ' ' -f 3)
        logs=$(echo $line|cut -d ' ' -f 4)
        throughput=$(echo "scale=2;$logs/$duration/1000"|bc)
        echo $throughput >> $label-${threads}_t.val
    done;
done;
