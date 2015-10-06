lib_LTLIBRARIES=lib/libbxibase.la

lib_libbxibase_la_SOURCES=\
		  src/version.c\
		  src/mem.c\
		  src/str.c\
		  src/err.c\
		  src/time.c\
		  src/zmq.c\
		  src/log.c\
		  src/log/logger.c\
		  src/log/assert.c\
		  src/log/config.c\
		  src/log/exit.c\
		  src/log/fork.c\
		  src/log/handler.c\
		  src/log/report.c\
		  src/log/signal.c\
		  src/log/thread.c\
		  src/log/tsd.c\
		  src/log/registry.c\
		  src/log/file_handler.c\
		  src/log/console_handler.c\
		  src/log/syslog_handler.c\
		  src/log/snmplog_handler.c\
		  src/log/null_handler.c

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
		   src/log/config_impl.h\
		   src/log/fork_impl.h\
		   src/log/handler_impl.h\
		   src/log/log_impl.h\
		   src/log/registry_impl.h\
		   src/log/tsd_impl.h

