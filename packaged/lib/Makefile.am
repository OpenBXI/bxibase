#
##
#

$(abs_builddir)/bxi/__init__.py:
	touch  $@

$(abs_builddir)/bxi/base/__init__.py: $(abs_builddir)/bxi/__init__.py
	touch  $@



# For installation
nodist_python_PYTHON=\
                  bxi/base/cffi_h.py
nobase_python_PYTHON=\
                  bxi/ffi.py \
                  bxi/base/sign.py \
                  bxi/base/err.py \
                  bxi/base/log.py


#File present inside build and source folder should be install with a special rule
bxibasedir=$(pythondir)/bxi/base
nodist_bxibase_PYTHON=\
			   $(srcdir)/bxi/base/__init__.py
bxidir=$(pythondir)/bxi
nodist_bxi_PYTHON=\
				  $(srcdir)/bxi/__init__.py 

dist-hook:${bxibase_PYTHON} ${bxi_PYTHON}
		cp ${bxibase_PYTHON} $(distdir)/bxi/base/
		   ${bxi_PYTHON} $(distdir)/bxi

# For generation
all-local: $(nobase_python_PYTHON) $(abs_builddir)/bxi/base/__init__.py

clean-local:
	if test $(srcdir) != $(builddir);\
		then\
		rm -f $(builddir)/bxi/base/__init__.py $(builddir)/bxi/__init__.py;\
	fi

#
##
#
CLEANFILES=${nodist_python_PYTHON}
