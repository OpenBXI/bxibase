#
##
#

$(builddir)/bxi/base:
	mkdir -p $@

$(builddir)/bxi/__init__.py: $(builddir)/bxi/base
	touch $@
	
$(builddir)/bxi/base/__init__.py:$(builddir)/bxi/base
	touch $@
	
# Generated files for CFFI
bxi/base/log_h.py: $(top_srcdir)/packaged/include/bxi/base/log.h $(builddir)/bxi/base/__init__.py $(builddir)/bxi/__init__.py 
	$(top_srcdir)/h2py.sh $< -o $@

bxi/base/err_h.py: $(top_srcdir)/packaged/include/bxi/base/err.h $(builddir)/bxi/base/__init__.py $(builddir)/bxi/__init__.py
	$(top_srcdir)/h2py.sh $< -o $@


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
