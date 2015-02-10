
#file to be installed
cffi_h_files=\
			 bxi/base/mem.h\
			 bxi/base/str.h\
			 bxi/base/err.h\
			 bxi/base/log.h

nobase_include_HEADERS = \
						 ${cffi_h_files}\
						 bxi/base/time.h\
						 bxi/base/zmq.h

../lib/bxi/base/cffi_h.py: $(cffi_h_files) 
	mkdir -p $$(dirname $@)
	touch  $$(dirname $@)/__init__.py
	touch  $$(dirname $$(dirname $@))/__init__.py
	for i in $^; do \
		echo "#include <$$i>";\
		done > cffi.h
	C_INCLUDE_PATH=$(top_srcdir)/packaged/include $(top_srcdir)/h2py.sh -I. -I$(srcdir) cffi.h $@


all-local: ../lib/bxi/base/cffi_h.py

.PHONY: ../lib/bxi/base/cffi_h.py
