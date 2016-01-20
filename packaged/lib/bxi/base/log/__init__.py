#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
@file log.py Python binding of the BXI High Performance Logging Library
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log Python binding of the BXI High Performance Logging Library

This module exposes a simple Python binding over the BXI High Performance Logging Library.

Overview
=========

This module provides a much simpler API than the standard Python logging module but
with much better overall performances, and a similar API for basic usage.

Therefore, in most cases, the following code should just work:

    import bxi.base.log as logging

    _LOGGER = logging.getLogger('my.logger')

Configuring the logging system is done using the ::basicConfig()
function which offers as far as possible the same API than Python own's
logging.basicConfig().

Differences with Python logging module
======================================

Initialization
---------------------------------

The main difference with the Python logging module is that the logging initialization
can be done only once in the whole process. The logging system is initialized as soon
as a log is performed. Therefore the following sequence of instructions will lead to
an exception:

    import bxi.base.log as logging

    logging.output("This is a log")   # This actually initialize the logging system
                                      # (unless another log has already been emitted
                                      # from another module of course).

    # This will raise a BXILogConfigError
    logging.basicConfig(filename='+') # Configure the logging system so all logs go
                                      # to the standard error

The reason is that the first log, will initialize the logging system so all messages
must be displayed on the standard output (by default).

The second call, ask the logging system to change the configuration so all logs now
must be displayed on the standard error. This leads in the program to a dynamic
change in the logging outputs which is usually undesired. Hence, an exception is raised.
If this is really the behavior expected, you might try the following:

    try:
        logging.basicConfig(filename='+')
    except BXILogConfigError:
        logging.cleanup()
        logging.basicConfig(filename='+')
        logging.output("Dynamic reconfiguration of the logging system")

Configuration
---------------

This module does not provide a hierarchy of loggers as per the Python logging API.
Each logger holds its own logging level and this has no relationship with any other
loggers.

However, the configuration of the logging system is based on prefix matching.


Log levels
------------

This API provides a much richer set of logging levels, inspired by the standard POSIX
syslog facility (from ::PANIC to ::NOTICE), and enhanced with
lower detailed levels (from ::OUT to ::LOWEST).
See the ::bxilog_level_e documentation of the underlying C API log.h
for details on those levels.

"""
from __future__ import print_function
# Try to find other BXI packages in other folders
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)

import atexit
import os
import sys
import warnings
import unittest
import tempfile
import traceback
import cStringIO as StringIO

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr


# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


# WARNING, in the following, the formatting of the documentation should remain as it is
# in order to be correctly processed by doxygen. This is a doxygen bug.
# You can change it, if you have verified first that the new format (using docstrings
# normally, as other functions documentations) appears correctly in the doxygen generated
# documentation.

# # @see ::BXILOG_OFF
OFF = __BXIBASE_CAPI__.BXILOG_OFF

# # @see ::BXILOG_PANIC
PANIC = __BXIBASE_CAPI__.BXILOG_PANIC

# # @see ::BXILOG_ALERT
ALERT = __BXIBASE_CAPI__.BXILOG_ALERT  # #!< See foo.

# # @see ::BXILOG_CRITICAL
CRITICAL = __BXIBASE_CAPI__.BXILOG_CRITICAL

# # @see ::BXILOG_ERROR
ERROR = __BXIBASE_CAPI__.BXILOG_ERROR

# # @see ::BXILOG_WARNING
WARNING = __BXIBASE_CAPI__.BXILOG_WARNING

# # @see ::BXILOG_NOTICE
NOTICE = __BXIBASE_CAPI__.BXILOG_NOTICE

# # @see ::BXILOG_OUTPUT
OUTPUT = __BXIBASE_CAPI__.BXILOG_OUTPUT

# # @see ::BXILOG_INFO
INFO = __BXIBASE_CAPI__.BXILOG_INFO

# # @see ::BXILOG_DEBUG
DEBUG = __BXIBASE_CAPI__.BXILOG_DEBUG

# # @see ::BXILOG_FINE
FINE = __BXIBASE_CAPI__.BXILOG_FINE

# # @see ::BXILOG_TRACE
TRACE = __BXIBASE_CAPI__.BXILOG_TRACE

# # @see ::BXILOG_LOWEST
LOWEST = __BXIBASE_CAPI__.BXILOG_LOWEST

# The .h defines an alias but CFFI
# does understand it
ALL = __BXIBASE_CAPI__.BXILOG_LOWEST


# If True,  bxilog_init() has already been called
_INITIALIZED = False

# Can be set by basicConfig()
_CONFIG = None
_INIT_CALLER = None

# The default logger name
_ROOT_LOGGER_NAME = ''

# The default logger.
_DEFAULT_LOGGER = None

""" 
Colors Theme.

Defining new color themes is really very simple.

See ::bxilog_colors_p for details.
"""
COLORS = {'none': __BXIBASE_CAPI__.BXILOG_COLORS_NONE,
          '216_dark': __BXIBASE_CAPI__.BXILOG_COLORS_216_DARK,
          'truecolor_dark': __BXIBASE_CAPI__.BXILOG_COLORS_TC_DARK,
          'truecolor_light': __BXIBASE_CAPI__.BXILOG_COLORS_TC_LIGHT,
          'truecolor_darkgray': __BXIBASE_CAPI__.BXILOG_COLORS_TC_DARKGRAY,
          'truecolor_lightgray': __BXIBASE_CAPI__.BXILOG_COLORS_TC_LIGHTGRAY,
          }

"""
Default configuration items.

@see ::basicConfig()
"""
DEFAULT_FILTERS = ':lowest'

"""
Default configuration of the console handler

@see ::basicConfig()
@see ::bxilog_handler_p
@see ::BXILOG_CONSOLE_HANDLER
"""
DEFAULT_CONSOLE_HANDLER_OPTIONS = (':output',
                                   WARNING,
                                   __BXIBASE_CAPI__.BXILOG_COLORS_216_DARK)

"""
Specifies that file handler filters must be computed automatically.

@see ::basicConfig()
@see ::bxilog_handler_p
@see ::BXILOG_FILE_HANDLER
"""
FILE_HANDLER_FILTERS_AUTO = 'auto'


def is_configured():
    """
    Check if the logs have already been configured.

    @return A boolean indicating the configuration state of the logs
    """
    return _INITIALIZED


def get_level_from_str(level_str):
    """
    Return the ::bxilog_level_e related to the given string.
    """
    level_p = __FFI__.new('bxilog_level_e[1]')
    err = __BXIBASE_CAPI__.bxilog_get_level_from_str(level_str, level_p)

    bxierr.BXICError.raise_if_ko(err)
    return level_p[0]


def fileConfig(**kwargs):
    """
    """
    pass


def basicConfig(**kwargs):
    """
    Configure the whole bxilog module.

    Parameter kwargs can contain following parameters:
        - `'console'`: None or a list of console handler arguments
        - `'filename'`: None or a file name
        - `'file'`: None or a list of file handler arguments
        - `'syslog'`: None or a list of syslog handler arguments
        - `'setsighandler'`: if True, set signal handlers, default is True

    If 'filename' is specified, it overwrites 'file' parameters.

    @param[in] kwargs named parameters as described above

    @return
    @exception bxi.base.err.BXILogConfigError if the logging system as
               already been initialized
    """
    if _INITIALIZED:
        raise bxierr.BXILogConfigError("The bxilog has already been initialized. "
                                       "Its configuration cannot be changed."
                                       "\nAvailable solutions:"
                                       "\n\t1. Do not perform a log at module level, "
                                       "and the configuration afterwards;"
                                       "\n\t2. Do no perform a log unless is_configured()"
                                       " returns True;"
                                       "\n\t3. Call cleanup() to reinitialize the whole "
                                       "bxilog library (Note: you might need a reconfiguration)."
                                       "\nFor your convenience, "
                                       "the following stacktrace might help finding out where "
                                       "the first _init() call  was made:\n %s" % _INIT_CALLER,
                                       kwargs)
    global _CONFIG

    if 'console' not in kwargs:
        kwargs['console'] = DEFAULT_CONSOLE_HANDLER_OPTIONS

    if 'filename' in kwargs:
        kwargs['file'] = (FILE_HANDLER_FILTERS_AUTO, kwargs['filename'], True)

    if 'setsighandler' not in kwargs:
        kwargs['setsighandler'] = True

    _CONFIG = kwargs


def bxilog_excepthook(type_, value, traceback):
    """
    The exception hook called on uncaught exception.
    """
    kwargs = {'exc_info': (type_, value, traceback)}
    critical('Uncaught Exception: %s' % value, **kwargs)


def _init():
    """
    Initialize the underlying C library

    @return
    """
    global _INITIALIZED
    global _CONFIG
    global _INIT_CALLER

    if _CONFIG is None:
        _CONFIG = {'console': DEFAULT_CONSOLE_HANDLER_OPTIONS,
                   'filename': None,
                   'syslog': None,
                   'setsighandler': True,
                   }

    from . import config as bxilogconfig
    from . import filter as bxilogfilter

    handler_filters = set()
    config = __BXIBASE_CAPI__.bxilog_config_new(sys.argv[0])

    console_handler_args = _CONFIG.get('console', DEFAULT_CONSOLE_HANDLER_OPTIONS)
    if console_handler_args is not None:
        console_filters = bxilogconfig.add_console_config(console_handler_args, config)
        handler_filters.add(console_filters)

    file_handler_args = _CONFIG.get('file', None)
    if file_handler_args is not None:
        # Use the same filters than console_handlers
        filters = console_filters if console_handler_args is not None else None
        file_filters = bxilogconfig.add_file_config(file_handler_args, config,
                                                    filters)
        handler_filters.add(file_filters)

    syslog_handler_args = _CONFIG.get('syslog', None)
    if syslog_handler_args is not None:
        syslog_filters = bxilogconfig.add_syslog_config(syslog_handler_args, config)
        handler_filters.add(syslog_filters)

    if config.handlers_nb == 0:
        print("Warning: no handler defined in bxi logging library configuration!")
        null_filters = bxilogconfig.add_null_handler(config)
        handler_filters.add(null_filters)

    loggers_filters = bxilogfilter.merge_filters(handler_filters)
    filters_p = __FFI__.new('bxilog_filters_p[1]')
    filters_p[0] = loggers_filters
    __BXIBASE_CAPI__.bxilog_registry_set_filters(filters_p)

    err_p = __BXIBASE_CAPI__.bxilog_init(config)
    _INITIALIZED = True
    bxierr.BXICError.raise_if_ko(err_p)
    atexit.register(cleanup)
    if _CONFIG['setsighandler']:
        err_p = __BXIBASE_CAPI__.bxilog_install_sighandler()
        bxierr.BXICError.raise_if_ko(err_p)

    sio = StringIO.StringIO()
    traceback.print_stack(file=sio)
    _INIT_CALLER = sio.getvalue()
    sio.close()

    sys.excepthook = bxilog_excepthook


def get_config():
    """
    Return the current bxilog configuration.
    """
    global _CONFIG
    return _CONFIG


def get_logger(name):
    """
    Return the BXILogger instance with the given name.

    If such a logger instance does not exist yet, it is created,
    registered to the underlying C library and returned.

    @param[in] name the logger name

    @return the BXILogger instance with the given name
    """
    for logger in get_all_loggers_iter():
        if __FFI__.string(logger.clogger.name) == name:
            return logger

    logger_p = __FFI__.new('bxilog_logger_p[1]')
    err_p = __BXIBASE_CAPI__.bxilog_registry_get(name, logger_p)
    bxierr.BXICError.raise_if_ko(err_p)

    from . import logger as bxilogger
    return bxilogger.BXILogger(logger_p[0])


def cleanup(flush=True):
    """
    Called at exit time to cleanup the underlying BXI C library.

    @param[in] flush if true, do a flush before releasing all resources.
    @return
    """
    global _INITIALIZED
    global _CONFIG
    global _DEFAULT_LOGGER
    if _INITIALIZED:
        err_p = __BXIBASE_CAPI__.bxilog_finalize(flush)
        bxierr.BXICError.raise_if_ko(err_p)
    _INITIALIZED = False
    _DEFAULT_LOGGER = None
    _CONFIG = None


def flush():
    """
    Flush all pending logs.

    @return
    """
    err_p = __BXIBASE_CAPI__.bxilog_flush()
    bxierr.BXICError.raise_if_ko(err_p)


def get_all_level_names_iter():
    """
    Return an iterator over all level names.

    @return
    """
    names = __FFI__.new("char ***")
    nb = __BXIBASE_CAPI__.bxilog_get_all_level_names(names)
    for i in xrange(nb):
        yield __FFI__.string(names[0][i])


def get_all_loggers_iter():
    """
    Return an iterator over all loggers.

    @return
    """
    loggers = __FFI__.new("bxilog_logger_p **")
    nb = __BXIBASE_CAPI__.bxilog_registry_getall(loggers)
    from . import logger as bxilogger
    for i in xrange(nb):
        yield bxilogger.BXILogger(loggers[0][i])


def get_default_logger():
    """
    Return the root logger.

    @return
    """
    global _DEFAULT_LOGGER
    if _DEFAULT_LOGGER is None:
        _DEFAULT_LOGGER = getLogger(_ROOT_LOGGER_NAME)
    return _DEFAULT_LOGGER


def off(msg, *args, **kwargs):
    """
    Do not log given message!

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().off(msg, *args, **kwargs)


def panic(msg, *args, **kwargs):
    """
    Log the given message at the ::PANIC logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().panic(msg, *args, **kwargs)


def alert(msg, *args, **kwargs):
    """
    Log the given message at the ::ALERT logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().alert(msg, *args, **kwargs)


def critical(msg, *args, **kwargs):
    """
    Log the given message at the ::CRITICAL logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().critical(msg, *args, **kwargs)


def error(msg, *args, **kwargs):
    """
    Log the given message at the ::ERROR logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().error(msg, *args, **kwargs)


def warning(msg, *args, **kwargs):
    """
    Log the given message at the ::WARNING logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().warning(msg, *args, **kwargs)


def notice(msg, *args, **kwargs):
    """
    Log the given message at the ::NOTICE logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().notice(msg, *args, **kwargs)


def output(msg, *args, **kwargs):
    """
    Log the given message at the ::OUT logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().output(msg, *args, **kwargs)

out = output


def info(msg, *args, **kwargs):
    """
    Log the given message at the ::INFO logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().info(msg, *args, **kwargs)


def debug(msg, *args, **kwargs):
    """
    Log the given message at the ::DEBUG logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().debug(msg, *args, **kwargs)


def fine(msg, *args, **kwargs):
    """
    Log the given message at the ::FINE logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().fine(msg, *args, **kwargs)


def trace(msg, *args, **kwargs):
    """
    Log the given message at the ::TRACE logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().trace(msg, *args, **kwargs)


def lowest(msg, *args, **kwargs):
    """
    Log the given message at the ::LOWEST logging level using the default logger.

    @param[in] msg the message to log
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().lowest(msg, *args, **kwargs)


def exception(msg="", *args, **kwargs):
    """
    Log the current exception.

    @param[in] msg a message to display before the exception itself
    @param[in] args the message arguments if any
    @param[in] kwargs the message arguments if any

    @return
    @see get_default_logger
    """
    get_default_logger().exception(msg, *args, **kwargs)


# Provide a compatible API with the standard Python logging module
getLogger = get_logger


# Provide a compatible API with the standard Python logging module
warn = warning

# Warnings integration - taken from the standard Python logging module
_warnings_showwarning = None


def _showwarning(message, category, filename, lineno, _file=None, line=None):
    """
    Implementation of showwarnings which redirects to logging, which will first
    check to see if the file parameter is None. If a file is specified, it will
    delegate to the original warnings implementation of showwarning. Otherwise,
    it will call warnings.formatwarning and will log the resulting string to a
    warnings logger named "py.warnings" with level logging.WARNING.
    """
    if _file is not None:
        if _warnings_showwarning is not None:
            _warnings_showwarning(message, category, filename, lineno, _file, line)
    else:
        warning_msg = warnings.formatwarning(message, category, filename, lineno, line)
        getLogger("py.warnings").warning("%s", warning_msg)


def captureWarnings(capture):
    """
    If capture is true, redirect all warnings to the logging package.
    If capture is False, ensure that warnings are not redirected to logging
    but to their original destinations.
    """
    global _warnings_showwarning
    if capture:
        if _warnings_showwarning is None:
            _warnings_showwarning = warnings.showwarning
            warnings.showwarning = _showwarning
    else:
        if _warnings_showwarning is not None:
            warnings.showwarning = _warnings_showwarning
            _warnings_showwarning = None


class FileLike(object):
    """
    A file like object that can be used for writing backed by the logging api.
    """
    def __init__(self, logger, level=OUTPUT):
        self.logger = logger
        self.level = level
        self.buf = None

    def close(self):
        self._newline()
        self.flush()

    def _newline(self):
        if self.buf is not None:
            self.logger.log(self.level, self.buf)
            self.buf = None

    def flush(self):
        self.logger.flush()

    def write(self, s):
        if self.buf is None:
            self.buf = s
        else:
            # Yes, we use '+' instead of StringIO, format or other tuning
            # since:
            #        1. we do not expect *lots* of write() call without '\n' and
            #           for such few number of operations, the difference is
            #           between various technics is meaningless
            #        2. benchmark shows quite good performance of the '+' operator
            #           nowadays, except with Pypy. See the string_concat.py bench
            #           for details
            self.buf += s
        if self.buf[-1] == '\n':
            self.buf = self.buf[:-1]
            self._newline()

    def writelines(self, sequence):
        for line in sequence:
            self.write(line)


class TestCase(unittest.TestCase):
    BXILOG_FILENAME = os.path.join(tempfile.gettempdir(),
                                   "%s.bxilog" % os.path.basename(sys.argv[0]))

    APPEND = False

    @classmethod
    def setUpClass(cls):
        basicConfig(console=None,
                    file=(':lowest', cls.BXILOG_FILENAME, TestCase.APPEND))

    @classmethod
    def tearDownClass(cls):
        cleanup()
