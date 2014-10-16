gcc -o bxilog_bench bxilog_bench.c -O3 -mtune=native -fPIC -fomit-frame-pointer -lpthread -lzmq -lbxi -I ../packaged/src/ -L ../packaged/lib/ -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
