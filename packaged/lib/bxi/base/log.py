#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
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


import atexit
import cStringIO as StringIO
import os
import sys
import traceback
import warnings
import unittest
import tempfile

import bxi.ffi as bxiffi
import bxi.base as bxibase
from bxi.base.err import BXICError, BXILogConfigError


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

#
# _SRCFILE is used when walking the stack to check when we've got the first
# caller stack frame.
#
if hasattr(sys, 'frozen'):  # support for py2exe
    _SRCFILE = "bxilog%s__init__%s" % (os.sep, __file__[-4:])
elif __file__[-4:].lower() in ['.pyc', '.pyo']:
    _SRCFILE = __file__[:-4] + '.py'
else:
    _SRCFILE = __file__
_SRCFILE = os.path.normcase(_SRCFILE)

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


def _FindCaller():
    """Find caller filename, linenumber and function name.

    @return a triplet (file name, line number, function name)
    """
    frame = sys._getframe(0)
    if frame is not None:
        frame = frame.f_back
    rv = "(unknown file)", 0, "(unknown function)"
    while hasattr(frame, "f_code"):
        co = frame.f_code
        filename = os.path.normcase(co.co_filename)
        if filename == _SRCFILE:
            frame = frame.f_back
            continue
        rv = (co.co_filename, frame.f_lineno, co.co_name)
        break
    return rv


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
    bxierr = __BXIBASE_CAPI__.bxilog_get_level_from_str(level_str, level_p)

    BXICError.raise_if_ko(bxierr)
    return level_p[0]


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
        raise BXILogConfigError("The bxilog has already been initialized. "
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


def parse_filters(filter_str):
    """
    Parse the given string and return the corresponding set of filters as
    a bxilog_filter_p

    @param[in] filters_str a string representing filters as defined in ::bxilog_filters_parse()

    @return a ::bxilog_filter_p object
    """
    filters_p = __FFI__.new('bxilog_filters_p[1]')
    err = __BXIBASE_CAPI__.bxilog_filters_parse(filter_str, filters_p)
    BXICError.raise_if_ko(err)
    return filters_p[0]


def new_detailed_filters(filters, delta=2, min_level=ERROR):
    """
    Create a copy of the given filters with a level increase of delta.
    """
    result_p = __FFI__.new('bxilog_filters_p[1]')
    result_p[0] = __BXIBASE_CAPI__.bxilog_filters_new()
    for i in xrange(filters.nb):
        new_level = min(filters.list[i].level + delta, LOWEST)
        # At least error messages
        new_level = max(new_level, min_level)
        __BXIBASE_CAPI__.bxilog_filters_add(result_p, filters.list[i].prefix,
                                            new_level)
    return result_p[0]


def merge_filters(filters_set):
    """
    Merge the given set of filters.
    """
    prefixes = dict()
    for filters in filters_set:
        if isinstance(filters, str):
            filters
        for i in xrange(filters.nb):
            prefix = __FFI__.string(filters.list[i].prefix)
            level = filters.list[i].level
            if prefix in prefixes:
                prefixes[prefix] = max(level, prefixes[prefix])
            else:
                prefixes[prefix] = level

    result_p = __FFI__.new('bxilog_filters_p[1]')
    result_p[0] = __BXIBASE_CAPI__.bxilog_filters_new()
    for prefix in sorted(prefixes):
        __BXIBASE_CAPI__.bxilog_filters_add(result_p, prefix, prefixes[prefix])

    return result_p[0]


def bxilog_excepthook(type_, value, traceback):
    """
    The exception hook called on uncaught exception.
    """
    kwargs = {'exc_info': (type_, value, traceback)}
    critical('Uncaught Exception: %s' % value, **kwargs)


def _add_console_config(console_handler_args, config):
    console_filters = parse_filters(console_handler_args[0])
    stderr_level = __FFI__.cast('int', console_handler_args[1])
    colors = console_handler_args[2]
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_CONSOLE_HANDLER,
                                               console_filters,
                                               stderr_level, colors)
    return console_filters


def _add_file_config(file_handler_args, config, console_filters):
    if file_handler_args[0] == FILE_HANDLER_FILTERS_AUTO:
        # Compute file filters automatically according to console handler filters
        if console_filters is not None:
            file_filters = new_detailed_filters(console_filters)
        else:
            file_filters = __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_ALL
    else:
        file_filters = parse_filters(file_handler_args[0])
    progname = __FFI__.new('char[]', sys.argv[0])
    filename = __FFI__.new('char[]', file_handler_args[1])
    open_flags = __FFI__.cast('int',
                              os.O_CREAT |
                              (os.O_APPEND if file_handler_args[2] else os.O_TRUNC))
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_FILE_HANDLER,
                                               file_filters,
                                               progname,
                                               filename,
                                               open_flags)
    return file_filters


def _add_syslog_config(syslog_handler_args, config):
    syslog_filters = syslog_handler_args[0]
    if isinstance(syslog_filters, str):
        syslog_filters = parse_filters(syslog_filters)
    ident = __FFI__.new('char[]', sys.argv[0])
    option = __FFI__.cast('int', syslog_handler_args[1])
    facility = __FFI__.cast('int', syslog_handler_args[2])
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_SYSLOG_HANDLER,
                                               syslog_filters,
                                               ident, option, facility)
    return syslog_filters


def _add_null_handler(config):
    null_filters = __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_OFF
    __BXIBASE_CAPI__.bxilog_config_add_handler(config,
                                               __BXIBASE_CAPI__.BXILOG_NULL_HANDLER,
                                               __BXIBASE_CAPI__.BXILOG_FILTERS_ALL_OFF)
    return null_filters


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

    handler_filters = set()
    config = __BXIBASE_CAPI__.bxilog_config_new(sys.argv[0])

    console_handler_args = _CONFIG.get('console', DEFAULT_CONSOLE_HANDLER_OPTIONS)
    if console_handler_args is not None:
        console_filters = _add_console_config(console_handler_args, config)
        handler_filters.add(console_filters)

    file_handler_args = _CONFIG.get('file', None)
    if file_handler_args is not None:
        file_filters = _add_file_config(file_handler_args, config,
                                        console_filters if console_handler_args is not None else None)
        handler_filters.add(file_filters)

    syslog_handler_args = _CONFIG.get('syslog', None)
    if syslog_handler_args is not None:
        syslog_filters = _add_syslog_config(syslog_handler_args, config)
        handler_filters.add(syslog_filters)

    if config.handlers_nb == 0:
        print("Warning: no handler defined in bxi logging library configuration!")
        null_filters = _add_null_handler(config)
        handler_filters.add(null_filters)

    loggers_filters = merge_filters(handler_filters)
    filters_p = __FFI__.new('bxilog_filters_p[1]')
    filters_p[0] = loggers_filters
    __BXIBASE_CAPI__.bxilog_registry_set_filters(filters_p)

    bxierr_p = __BXIBASE_CAPI__.bxilog_init(config)
    _INITIALIZED = True
    BXICError.raise_if_ko(bxierr_p)
    atexit.register(cleanup)
    if _CONFIG['setsighandler']:
        bxierr_p = __BXIBASE_CAPI__.bxilog_install_sighandler()
        BXICError.raise_if_ko(bxierr_p)

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
    bxierr_p = __BXIBASE_CAPI__.bxilog_registry_get(name, logger_p)
    BXICError.raise_if_ko(bxierr_p)

    return BXILogger(logger_p[0])


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
        bxierr_p = __BXIBASE_CAPI__.bxilog_finalize(flush)
        BXICError.raise_if_ko(bxierr_p)
    _INITIALIZED = False
    _DEFAULT_LOGGER = None
    _CONFIG = None


def flush():
    """
    Flush all pending logs.

    @return
    """
    bxierr_p = __BXIBASE_CAPI__.bxilog_flush()
    BXICError.raise_if_ko(bxierr_p)


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
    for i in xrange(nb):
        yield BXILogger(loggers[0][i])


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


class BXILogger(object):
    """
    A BXILogger instance provides various methods for logging.

    This class provides a thin layer on top of the underlying C module.
    """

    def __init__(self, clogger):
        """
        Wraps the given C logger.

        @param[in] clogger the C underlying logger instance.

        @return a BXILogger instance
        """
        self.clogger = clogger

    def log(self, level, msg, *args, **kwargs):
        """
        Log the given message at the given level.

        @param[in] level the level at which the given message should be logged
        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        @exception BXICError if an error occurred at the underlying C layer
        """
        if not _INITIALIZED:
            _init()
        if __BXIBASE_CAPI__.bxilog_logger_is_enabled_for(self.clogger, level):
            msg_str = msg % args if len(args) > 0 else str(msg)
            filename, lineno, funcname = _FindCaller()
            bxierr_p = __BXIBASE_CAPI__.bxilog_logger_log_rawstr(self.clogger,
                                                                 level,
                                                                 filename,
                                                                 len(filename) + 1,
                                                                 funcname,
                                                                 len(funcname) + 1,
                                                                 lineno,
                                                                 msg_str,
                                                                 len(msg_str) + 1)
            BXICError.raise_if_ko(bxierr_p)
            exc_s = ''
            ei = kwargs.get('exc_info', None)
            if ei is not None:
                sio = StringIO.StringIO()
                traceback.print_exception(ei[0], ei[1], ei[2], None, sio)
                if isinstance(ei[1], BXICError):
                    backtrace = __FFI__.string(ei[1].bxierr_pp[0].backtrace)
                    sio.write("Low Level C back trace:\n")
                    sio.write(backtrace)
                exc_s = sio.getvalue()
                sio.close()
                if exc_s[-1:] == "\n":
                    exc_s = exc_s[:-1]
                bxierr_p = __BXIBASE_CAPI__.bxilog_logger_log_rawstr(self.clogger,
                                                                     TRACE,
                                                                     filename,
                                                                     len(filename) + 1,
                                                                     funcname,
                                                                     len(funcname) + 1,
                                                                     lineno,
                                                                     exc_s,
                                                                     len(exc_s) + 1)
                BXICError.raise_if_ko(bxierr_p)

    @property
    def name(self):
        return __FFI__.string(self.clogger.name)

    @property
    def level(self):
        return self.clogger.level

    def set_level(self, level):
        """
        Set this logger logging level.

        @param[in] level the new logging level
        @return
        @see ::PANIC
        @see ::ALERT
        @see ::CRITICAL
        @see ::ERROR
        @see ::WARNING
        @see ::NOTICE
        @see ::OUT
        @see ::INFO
        @see ::DEBUG
        @see ::FINE
        @see ::TRACE
        @see ::LOWEST
        """
        __BXIBASE_CAPI__.bxilog_logger_set_level(self.clogger, level)

    def is_enabled_for(self, level):
        """
        Return True if this logger is enabled for the given logging level.

        @param[in] level the logging level to check against
        @return True if this logger is enabled for the given logging level.
        """
        return __BXIBASE_CAPI__.bxilog_logger_is_enabled_for(self.clogger, level)

    def off(self, msg, *args, **kwargs):
        """
        Do not log the given message!

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(OFF, msg, *args, **kwargs)

    def panic(self, msg, *args, **kwargs):
        """
        Log at the ::PANIC level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(PANIC, msg, *args, **kwargs)

    def alert(self, msg, *args, **kwargs):
        """
        Log at the ::ALERT level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(ALERT, msg, *args, **kwargs)

    def critical(self, msg, *args, **kwargs):
        """
        Log at the ::CRITICAL level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(CRITICAL, msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        """
        Log at the ::ERROR level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(ERROR, msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        """
        Log at the ::WARNING level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(WARNING, msg, *args, **kwargs)

    def notice(self, msg, *args, **kwargs):
        """
        Log at the ::NOTICE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(NOTICE, msg, *args, **kwargs)

    def output(self, msg, *args, **kwargs):
        """
        Log at the ::OUT level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(OUTPUT, msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        """
        Log at the ::INFO level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(INFO, msg, *args, **kwargs)

    def debug(self, msg, *args, **kwargs):
        """
        Log at the ::DEBUG level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(DEBUG, msg, *args, **kwargs)

    def fine(self, msg, *args, **kwargs):
        """
        Log at the ::FINE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(FINE, msg, *args, **kwargs)

    def trace(self, msg, *args, **kwargs):
        """
        Log at the ::TRACE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(TRACE, msg, *args, **kwargs)

    def lowest(self, msg, *args, **kwargs):
        """
        Log at the ::LOWEST level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(LOWEST, msg, *args, **kwargs)

    def exception(self, msg, *args, **kwargs):
        """
        Log the current exception with the given message.

        @param[in] msg the message to log with the given exception
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        kwargs['exc_info'] = sys.exc_info()
        if 'level' in kwargs:
            level = kwargs['level']
            del kwargs['level']
        else:
            level = ERROR
        self.log(level, msg, *args, **kwargs)

    @staticmethod
    def flush():
        """Convenience method for flushing all logs

        @return
        """
        flush()

    # Provide a compatible API with the standard Python logging module
    setLevel = set_level

    # Provide a compatible API with the standard Python logging module
    isEnabledFor = is_enabled_for

    # Provide a compatible API with the standard Python logging module
    warn = warning
    out = output


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

    @classmethod
    def setUpClass(cls):
        basicConfig(console=None,
                    file=(':lowest', cls.BXILOG_FILENAME, False))

    @classmethod
    def tearDownClass(cls):
        cleanup()
