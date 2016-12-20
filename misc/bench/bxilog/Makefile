CFLAGS=-W -Wall -ansi -pedantic -O3 -g -mtune=native -fPIC -fomit-frame-pointer -std=c99 -D_POSIX_C_SOURCE=200809L 

all: bxilog_bench zlog_bench

clean:
	rm -f bxilog_bench zlog_bench

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

bxilog_bench: bxilog_bench.o common.o
	${CC} -o $@ $^ \
			$(CFLAGS) \
			-lpthread -lzmq -lbxibase \
			-I ../packaged/src/ -L ../packaged/lib/ \
			-D_GNU_SOURCE

# Use this if zlog is to be used from source code and adapt the Makefile accordingly
#C_INCLUDE_PATH=~/dev/scm/zlog/src/:$C_INCLUDE_PATH 
#LIBRARY_PATH=~/dev/scm/zlog/src:$LIBRARY_PATH  

zlog_bench: zlog_bench.o common.o
	${CC} -o $@ $^ \
			$(CFLAGS) \
			-lpthread -lzlog -lbxibase
			
