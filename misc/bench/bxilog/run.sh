#!/bin/bash

if test $# -ne 3; then echo "Usage: $(basename $0) bench_cmd threads duration"
  exit 1
fi

#if test $1 == "bxilog_bench";then
#  gcc -o bxilog_bench bxilog_bench.c -O2 -lpthread -lzmq -lbxibase -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
#fi

bench=$1
threads=$2
duration=$3
output="/tmp/$1.log /tmp/$(basename -s .py $1).trace.*"


rm -f $output
sync
/usr/bin/time --format="%e %S %U %M %F %R %c %w %O"  -o /tmp/$bench.time -a ./$1 $threads $duration >> /tmp/$bench.print 2>&1 
# /usr/bin/time -v -o /tmp/$bench.time -a ./$1 $threads $duration 

echo -en "$bench\t$threads\t"\
        "$(tail -n 1 /tmp/$bench.time 2>/dev/null|cut -d ' ' -f 1)\t"\
        "$(cat $output 2>/dev/null|wc -l)\t"\
        "$(du $output 2>/dev/null |cut -f 1)\n" >> /tmp/$bench.bench
rm -f $output
