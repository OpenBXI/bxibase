#!/bin/bash

if test $# -ne 3; then echo "Usage: $(basename $0) bench_cmd threads duration"
  exit 1
fi

if test $1 == "bxilog_bench";then
  gcc -o bxilog_bench bxilog_bench.c -O2 -lpthread -lzmq -lbxi -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
fi

bench=$1
threads=$2
duration=$3
output=/tmp/$1.log


rm -f $output
sync
./$1 $threads $duration 

echo -e "$bench\t$threads\t$duration\t$(cat $output | wc -l)\t$(du $output|cut -f 1)" >> /tmp/$bench.bench
