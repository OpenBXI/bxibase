#
##
#
if HAVE_PYTHON

$(abs_builddir)/bxi/__init__.py:
	touch  $@

$(abs_builddir)/bxi/base/__init__.py: $(abs_builddir)/bxi/__init__.py
	touch  $@



# For installation
nobase_nodist_python_PYTHON=\
							bxi/base/version.py\
							bxi/base/cffi_h.py

nobase_python_PYTHON=\
                  bxi/ffi.py \
                  bxi/base/sign.py \
                  bxi/base/err.py \
                  bxi/base/log/__init__.py\
                  bxi/base/log/config.py\
                  bxi/base/log/logger.py\
                  bxi/base/log/filter.py\
                  bxi/base/log/console_handler.py\
                  bxi/base/log/file_handler.py\
                  bxi/base/log/syslog_handler.py\
                  bxi/base/log/netsnmp_handler.py



#File present inside build and source folder should be install with a special rule
bxibasedir=$(pythondir)/bxi/base
nodist_bxibase_PYTHON=\
			   $(srcdir)/bxi/base/__init__.py

bxidir=$(pythondir)/bxi
nodist_bxi_PYTHON=\
				  $(srcdir)/bxi/__init__.py

bxi/%/version.py:${abs_top_builddir}/version.py $(abs_top_builddir)/packaged/lib/bxi/base
	cp $< $@
BUILT_SOURCES=bxi/base/version.py

dist-hook:
		cp ${nodist_bxibase_PYTHON} $(distdir)/bxi/base/
		cp ${nodist_bxi_PYTHON} $(distdir)/bxi

# For generation
all-local: $(nobase_nodist_python_PYTHON) $(abs_builddir)/bxi/base/__init__.py

clean-local:
	if test $(srcdir) != $(builddir);\
		then\
		rm -f $(builddir)/bxi/base/__init__.py $(builddir)/bxi/__init__.py;\
	fi

#
##
#
CLEANFILES=${nodist_python_PYTHON}
endif
