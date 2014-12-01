#
##
#


# Generated files for CFFI
bxilog_h.py: $(top_srcdir)/packaged/include/bxi/base/log.h
	$(top_srcdir)/h2py.sh $< -o $@


# For installation
nobase_python_PYTHON=\
                  bxilog_h.py \
                  bxilog.py


# For generation
all-local: $(nobase_python_PYTHON)


#
##
#
CLEANFILES=bxilog_h.py
