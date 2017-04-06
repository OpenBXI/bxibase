#
##
#
if HAVE_PYTHON
# For installation

nobase_python_PYTHON=\
		  bxi/base/sign.py \
		  bxi/base/err.py \
		  bxi/base/zmq.py \
		  bxi/base/posless.py\
		  bxi/base/parserconf.py\
		  bxi/base/summary.py\
		  bxi/base/log/__init__.py\
		  bxi/base/log/config.py\
		  bxi/base/log/logger.py\
		  bxi/base/log/filter.py\
		  bxi/base/log/null_handler.py\
		  bxi/base/log/console_handler.py\
		  bxi/base/log/file_handler.py\
		  bxi/base/log/syslog_handler.py\
		  bxi/base/log/netsnmp_handler.py\
		  bxi/base/log/remote_handler.py\
		  bxi/base/log/remote_receiver.py


MODULE_NAME=base
include Makefile_spec

#
##
#
CLEANFILES=${nobase_nodist_python_PYTHON}
endif
