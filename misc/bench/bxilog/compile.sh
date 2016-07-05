#!/bin/bash

CC=${CC:-gcc}

$CC -o bxilog_bench bxilog_bench.c -O3 -g -mtune=native -fPIC -fomit-frame-pointer -lpthread -lzmq -lbxibase -I ../packaged/src/ -L ../packaged/lib/ -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE


C_INCLUDE_PATH=~/dev/scm/zlog/src/:$C_INCLUDE_PATH 
LIBRARY_PATH=~/dev/scm/zlog/src:$LIBRARY_PATH  

$CC -o zlog_bench zlog_bench.c -O3 -lzlog  -lbxibase -std=c99 -D_POSIX_C_SOURCE=200809L  -lpthread


