
#file to be installed
cffi_files=\
			 bxi/base/mem.h\
			 bxi/base/str.h\
			 bxi/base/err.h\
			 bxi/base/log.h\
			 bxi/base/log/file_handler.h\
			 bxi/base/log/null_handler.h\
			 bxi/base/log/syslog_handler.h\
			 bxi/base/log/console_handler.h
			 
#file to be install
nobase_include_HEADERS = \
						 bxi/base/version.h\
						 bxi/base/mem.h\
						 bxi/base/str.h\
						 bxi/base/err.h\
						 bxi/base/zmq.h\
						 bxi/base/time.h\
						 bxi/base/log.h\
						 bxi/base/log/level.h\
						 bxi/base/log/filter.h\
						 bxi/base/log/logger.h\
						 bxi/base/log/assert.h\
						 bxi/base/log/config.h\
						 bxi/base/log/exit.h\
						 bxi/base/log/handler.h\
						 bxi/base/log/report.h\
						 bxi/base/log/signal.h\
						 bxi/base/log/thread.h\
						 bxi/base/log/registry.h\
						 bxi/base/log/file_handler.h\
						 bxi/base/log/console_handler.h\
						 bxi/base/log/syslog_handler.h\
						 bxi/base/log/snmplog_handler.h\
						 bxi/base/log/null_handler.h


if HAVE_PYTHON
../lib/bxi/base/cffi_h.py: $(cffi_files) 
	mkdir -p $$(dirname $@)
	for i in $^; do \
		echo "#include <$$i>";\
		done > cffi.h
	C_INCLUDE_PATH="$(top_srcdir)/packaged/include:$(abs_top_builddir)/packaged/include:\
	${C_INCLUDE_PATH}" \
	$(top_srcdir)/h2py.sh cffi.h $@;\
	rm cffi.h


all-local: ../lib/bxi/base/cffi_h.py

.PHONY: ../lib/bxi/base/cffi_h.py
endif
