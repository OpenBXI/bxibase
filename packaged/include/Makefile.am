
#file to be installed
cffi_h_files=\
			 ${abs_top_srcdir}/packaged/include/bxi/base/mem.h\
			 ${abs_top_srcdir}/packaged/include/bxi/base/str.h\
			 ${abs_top_srcdir}/packaged/include/bxi/base/err.h\
			 ${abs_top_srcdir}/packaged/include/bxi/base/log.h

nobase_include_HEADERS = \
						 bxi/base/version.h\
						 bxi/base/mem.h\
						 bxi/base/str.h\
						 bxi/base/err.h\
						 bxi/base/log.h\
						 bxi/base/time.h\
						 bxi/base/zmq.h

../lib/bxi/base/cffi_h.py: $(cffi_h_files) 
	mkdir -p $$(dirname $@)
	touch  $$(dirname $@)/__init__.py
	touch  $$(dirname $$(dirname $@))/__init__.py
	for i in $^; do \
		echo "#include <$$i>";\
		done > cffi.h
	C_INCLUDE_PATH=$(top_srcdir)/packaged/include $(top_srcdir)/h2py.sh -I${srcdir} -I. cffi.h $@


all-local: ../lib/bxi/base/cffi_h.py

.PHONY: ../lib/bxi/base/cffi_h.py
