#
##
#

$(abs_top_builddir)/packaged/lib/bxi/base:
	mkdir -p $@

$(abs_top_builddir)/packaged/lib/bxi/__init__.py: $(abs_top_builddir)/packaged/lib/bxi/base
	touch $@
	
$(abs_top_builddir)/packaged/lib/bxi/base/__init__.py:$(abs_top_builddir)/packaged/lib/bxi/base
	touch $@
	
# Generated files for CFFI
bxi/base/log_h.py: $(top_srcdir)/packaged/include/bxi/base/log.h\
				   $(abs_top_builddir)/packaged/lib/bxi/base/__init__.py\
				   $(abs_top_builddir)/packaged/lib/bxi/__init__.py 
	C_INCLUDE_PATH=$(top_srcdir)/packaged/include $(top_srcdir)/h2py.sh $< -o $@

bxi/base/err_h.py: $(top_srcdir)/packaged/include/bxi/base/err.h\
				   $(abs_top_builddir)/packaged/lib/bxi/base/__init__.py\
				   $(abs_top_builddir)/packaged/lib/bxi/__init__.py
	C_INCLUDE_PATH=$(top_srcdir)/packaged/include $(top_srcdir)/h2py.sh $< -o $@


# For installation
nobase_python_PYTHON=\
                  $(srcdir)/bxi/__init__.py \
                  bxi/base/err_h.py \
                  bxi/base/log_h.py \
                  $(srcdir)/bxi/base/__init__.py \
                  bxi/base/err.py \
                  bxi/base/log.py
                  


# For generation
all-local: $(nobase_python_PYTHON)


#
##
#
CLEANFILES=bxi/base/log_h.py bxi/base/err_h.py
