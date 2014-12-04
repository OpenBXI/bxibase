#
##
#


# Generated files for CFFI
bxi/base/log_h.py: $(top_srcdir)/packaged/include/bxi/base/log.h
	mkdir -p bxi/base/
	echo "from pkgutil import extend_path" > bxi/__init__.py
	echo "__path__ = extend_path(__path__, __name__)" >> bxi/__init__.py
	$(top_srcdir)/h2py.sh $< -o $@

bxi/base/err_h.py: $(top_srcdir)/packaged/include/bxi/base/err.h
	mkdir -p bxi/base/
	echo "from pkgutil import extend_path" > bxi/base/__init__.py
	echo "__path__ = extend_path(__path__, __name__)" >> bxi/base/__init__.py
	$(top_srcdir)/h2py.sh $< -o $@


# For installation
nobase_python_PYTHON=\
                  bxi/__init__.py \
                  bxi/base/err_h.py \
                  bxi/base/log_h.py \
                  bxi/base/__init__.py \
                  bxi/base/err.py \
                  bxi/base/log.py
                  


# For generation
all-local: $(nobase_python_PYTHON)


#
##
#
CLEANFILES=bxi/base/log_h.py bxi/base/err_h.py
