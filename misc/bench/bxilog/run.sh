#!/bin/bash

if test $# -ne 1; then echo "Usage: $(basename $0) bench_cmd"
  exit 1
fi

if test $1 == "bxilog_bench";then
  gcc -o bxilog_bench bxilog_bench.c -O3 -lpthread -lzmq -lbxi -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
fi

output=/tmp/$1.log

rm -f $output;sync;/usr/bin/time -v $1 5 60;wc -l $output;ls -l $output
