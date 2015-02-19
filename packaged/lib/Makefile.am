#
##
#



# For installation
nobase_python_PYTHON=\
                  bxi/ffi.py \
                  bxi/base/cffi_h.py \
                  bxi/base/err.py \
                  bxi/base/log.py



#File present inside build and source folder should be install with a special rule
bxidir==$(pythondir)/bxi
bxibasedir==$(pythondir)/bxi/base
bxi_PYTHON=\
		   $(srcdir)/bxi/__init__.py 
bxibase_PYTHON=\
			   $(srcdir)/bxi/base/__init__.py
# For generation
all-local: $(nobase_python_PYTHON)


#
##
#
CLEANFILES=bxi/base/cffi_h.py
