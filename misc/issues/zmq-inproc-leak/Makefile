all: leak test

CFLAGS=-O0 -g
#CFLAGS=-O3 -g -mtune=native -fPIC -fomit-frame-pointer

test: test.c 
	${CC} -o test test.c \
			$(CFLAGS) \
			 -lpthread -lzmq \
			-I ../packaged/src/ -L ../packaged/lib/ \
			-std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE

leak: leak.c
	${CC} -o leak leak.c \
			$(CFLAGS) \
			-lpthread -lzmq -lbxibase \
			-I ../packaged/src/ -L ../packaged/lib/ \
			-std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
clean:
	rm -f test leak
