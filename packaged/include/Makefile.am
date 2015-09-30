
#file to be installed
cffi_files=\
			 bxi/base/mem.h\
			 bxi/base/str.h\
			 bxi/base/err.h\
			 bxi/base/log.h\
			 bxi/base/log_file_handler.h\
			 bxi/base/log_null_handler.h
			 bxi/base/log_console_handler.h
			 
#file to be install
nobase_include_HEADERS = \
						 bxi/base/version.h\
						 bxi/base/mem.h\
						 bxi/base/str.h\
						 bxi/base/err.h\
						 bxi/base/log.h\
						 bxi/base/time.h\
						 bxi/base/log_file_handler.h\
						 bxi/base/log_console_handler.h\
						 bxi/base/log_null_handler.h\
						 bxi/base/zmq.h

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
