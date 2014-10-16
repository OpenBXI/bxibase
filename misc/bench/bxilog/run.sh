#!/bin/bash


gcc -o bxilog_bench bxilog_bench.c -O3 -lpthread -lzmq -lbxi -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
rm -f /tmp/bxilog_bench.log;sync;/usr/bin/time -v ./bxilog_bench 5 60;wc -l /tmp/bxilog_bench.log;ls -l /tmp/bxilog_bench.log
