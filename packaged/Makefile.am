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
		  src/log/level.c\
		  src/log/filter.c\
		  src/log/config.c\
		  src/log/assert.c\
		  src/log/exit.c\
		  src/log/fork.c\
		  src/log/handler.c\
		  src/log/report.c\
		  src/log/signal.c\
		  src/log/thread.c\
		  src/log/tsd.c\
		  src/log/registry.c\
		  src/log/file_handler.c\
		  src/log/file_handler_stdio.c\
		  src/log/console_handler.c\
		  src/log/syslog_handler.c\
		  src/log/null_handler.c\
		  src/log/remote_handler.c\
		  src/log/remote_receiver.c


if HAVE_SNMP_LOG
lib_libbxibase_la_SOURCES+=\
		  src/log/netsnmp_handler.c

endif

lib_libbxibase_la_LDFLAGS=\
					 $(ZMQ_LIBS)\
					 $(LIBTOOLLDFLAGS)\
					 $(UUID_LIBS)

lib_libbxibase_la_CFLAGS=\
					 -I$(top_builddir)
					 -I$(top_srcdir)/packaged/include\
					 $(ZMQ_CFLAGS)\
					 $(UUID_CFLAGS)\
					 $(LIB_CFLAGS)

lib_libbxibase_la_LIBADD=$(LIB_LIBS)

AM_CFLAGS=\
		  -I$(srcdir)/include\
		  -I$(builddir)/include\
		  $(ZMQ_CFLAGS)

AM_LDFLAGS=\
		  $(ZMQ_CFLAGS)\
		  $(ZMQ_LIBS)


dist_bin_SCRIPTS=\
				 bin/bxilog-producer\
				 bin/bxilog-check-order\
				 bin/bxiconfig\
				 bin/bxilog-parser\
				 bin/bxilog-console

SUBDIRS=include . lib doc

EXTRA_DIST=\
		   src/log/config_impl.h\
		   src/log/fork_impl.h\
		   src/log/handler_impl.h\
		   src/log/log_impl.h\
		   src/log/registry_impl.h\
		   src/log/tsd_impl.h
