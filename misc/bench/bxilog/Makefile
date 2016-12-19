all: bxilog_bench zlog_bench

clean:
	rm bxilog_bench zlog_bench



bxilog_bench: bxilog_bench.c
	${CC} -o bxilog_bench bxilog_bench.c \
			-O3 -g -mtune=native -fPIC -fomit-frame-pointer -lpthread -lzmq -lbxibase \
			-I ../packaged/src/ -L ../packaged/lib/ \
			-std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
	
zlog_bench: zlog_bench.c
	${CC} -o zlog_bench zlog_bench.c \
			-O3 -g -mtune=native -fPIC -fomit-frame-pointer -lpthread -lzlog -lbxibase \
			-std=c99 -D_POSIX_C_SOURCE=200809L