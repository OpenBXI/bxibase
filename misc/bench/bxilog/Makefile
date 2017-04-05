CFLAGS=-W -Wall -ansi -pedantic -O3 -g -mtune=native -fPIC -fomit-frame-pointer -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS=-lbxibase -lpthread
EXEC=bench-c_bxilog bench-c_zlog

all: $(EXEC)

clean:
	rm -f $(EXEC)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS) $(LDFLAGS)

bench-c_bxilog: bench-c_bxilog.o common.o
	${CC} -o $@ $^ \
			$(CFLAGS) \
			$(LDFLAGS) \
			-D_GNU_SOURCE

# Use this if zlog is to be used from source code and adapt the Makefile accordingly
#C_INCLUDE_PATH=~/dev/scm/zlog/src/:$C_INCLUDE_PATH 
#LIBRARY_PATH=~/dev/scm/zlog/src:$LIBRARY_PATH  

bench-c_zlog: bench-c_zlog.o common.o
	${CC} -o $@ $^ \
			$(CFLAGS) \
			$(LDFLAGS) -lzlog
	