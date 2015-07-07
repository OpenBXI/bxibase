lib_LTLIBRARIES=lib/libbxibase.la

lib_libbxibase_la_SOURCES=\
		  src/version.c\
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


SUBDIRS=include lib doc

EXTRA_DIST=\
		   doc/examples/bxilog_err.c\
		   doc/examples/bxilog_cfg.c

TESTS=\
	  doc/examples/bxilog_err\
	  doc/examples/bxilog_cfg

check_PROGRAMS=\
	  doc/examples/bxilog_err\
	  doc/examples/bxilog_cfg

doc_examples_bxilog_err_LDADD=lib/libbxibase.la
doc_examples_bxilog_cfg_LDADD=lib/libbxibase.la

doc_examples_bxilog_err_CFLAGS=\
							   -I$(top_srcdir)/packaged/include


doc_examples_bxilog_cfg_CFLAGS=\
							   -I$(top_srcdir)/packaged/include
