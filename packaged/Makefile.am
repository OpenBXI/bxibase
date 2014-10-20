lib_LTLIBRARIES=lib/libbxibase.la



lib_libbxibase_la_SOURCES=\
		  src/mem.c\
		  src/str.c\
		  src/err.c\
		  src/time.c\
		  src/zmq.c\
		  src/log.c

lib_libbxibase_la_LDFLAGS=\
					 $(ZMQ_LIBS)\
					 $(UUID_LIBS)

lib_libbxibase_la_CFLAGS=\
					 -I$(top_srcdir)/packaged/include\
					 $(ZMQ_CFLAGS)\
					 $(UUID_CFLAGS)\
					 $(LIB_CFLAGS)

lib_libbxibase_la_LIBADD=$(LIB_LIBS)




SUBDIRS=lib include doc

