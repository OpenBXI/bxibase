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

This module exposes a simple Python binding over the BXI High Performance library.

"""

import atexit
import os
import sys
from bxi.base import get_bxibase_ffi
from bxi.base import get_bxibase_api
from bxi.base.err import BXICError

# Find the C library
_bxibase_ffi = get_bxibase_ffi()
_bxibase_api = get_bxibase_api()
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

# The root logger name
_ROOT_LOGGER_NAME = 'root'

# The root logger
_ROOT_LOGGER = None

"""
Default configuration items.

@see basicConfig()
"""
DEFAULT_CFG_ITEMS = [('', _bxibase_api.BXILOG_LOWEST)]

def basicConfig(**kwargs):
    """
    Configure the whole bxilog module.

    Parameter kwargs can contain following parameters:
        - `'filename'`: the file output name ('-': stdout, '+': stderr)
        - `'setsighandler'`: if True, set signal handlers
        - `'cfg_items'`: a list of tuples (prefix, level)

    @param kwargs named parameters as described above

    @return None
    """
    if _INITIALIZED:
        raise RuntimeError("The bxilog has already been initialized! "
                           "Its configuration cannot be changed yet. "
                           "Call cleanup() before.")
    global _CONFIG
        
    if 'filename' not in kwargs:
        kwargs['filename'] = '-'

    if 'cfg_items' not in kwargs:
        kwargs['cfg_items'] = DEFAULT_CFG_ITEMS
    else:
        kwargs['cfg_items'] = sorted(kwargs['cfg_items'])

    if 'setsighandler' not in kwargs:
        kwargs['setsighandler'] = True

    _CONFIG = kwargs

def getLogger(name):
    """
    Return the BXILogger instance with the given name.

    If such a logger instance does not exist yet, it is created,
    registered to the underlying C library and returned.

    @param name the logger name

    @return the BXILogger instance with the given name
    """
    global _INITIALIZED
    global _CONFIG

    if not _INITIALIZED:
        if _CONFIG is None:
            _CONFIG = {'filename':'-',
                       'cfg_items': DEFAULT_CFG_ITEMS,
                       'setsighandler': True,
                       }

        bxierr_p = _bxibase_api.bxilog_init(os.path.basename(sys.argv[0]),
                                       _CONFIG['filename'])
        BXICError.raise_if_ko(bxierr_p)
        atexit.register(_bxibase_api.bxilog_finalize)
        _INITIALIZED = True
        if _CONFIG['setsighandler']:
            bxierr_p = _bxibase_api.bxilog_install_sighandler()
            BXICError.raise_if_ko(bxierr_p)

    for logger in getAllLoggers_iter():
        if _bxibase_ffi.string(logger.clogger.name) == name:
            return logger
    logger = _bxibase_api.bxilog_new(name)
    _bxibase_api.bxilog_register(logger)

    cfg_items = _CONFIG['cfg_items']
    cffi_items = _bxibase_ffi.new('bxilog_cfg_item_s[]', len(cfg_items))
    for i in xrange(len(cfg_items)):
        cffi_items[i].prefix = _bxibase_ffi.new('char[]', cfg_items[i][0])
        cffi_items[i].level = cfg_items[i][1]

    bxierr_p = _bxibase_api.bxilog_cfg_registered(len(cfg_items), cffi_items)
    BXICError.raise_if_ko(bxierr_p)

    return BXILogger(logger)


def cleanup():
    """Called at exit time to cleanup the underlying BXI C library.

    @return
    """
    global _INITIALIZED
    global _CONFIG
    global _ROOT_LOGGER
    if _INITIALIZED:
        bxierr_p = _bxibase_api.bxilog_finalize()
        BXICError.raise_if_ko(bxierr_p)
    _INITIALIZED = False
    _ROOT_LOGGER = None
    _CONFIG = None

def flush():
    bxierr_p = _bxibase_api.bxilog_flush()
    BXICError.raise_if_ko(bxierr_p)

def getAllLevelNames_iter():
    names = _bxibase_ffi.new("char ***")
    nb = _bxibase_api.bxilog_get_all_level_names(names)
    for i in xrange(nb):
        yield _bxibase_ffi.string(names[0][i])

def getAllLoggers_iter():
    loggers = _bxibase_ffi.new("bxilog_p **")
    nb = _bxibase_api.bxilog_get_registered(loggers)
    for i in xrange(nb):
        yield BXILogger(loggers[0][i])


def findCaller():
    """Find caller filename, linenumber and function name.

    @return a triplet (file name, line number, function name)
    """
    f = sys._getframe(0)
    if f is not None:
        f = f.f_back
    rv = "(unknown file)", 0, "(unknown function)"
    while hasattr(f, "f_code"):
        co = f.f_code
        filename = os.path.normcase(co.co_filename)
        if filename == _SRCFILE:
            f = f.f_back
            continue
        rv = (co.co_filename, f.f_lineno, co.co_name)
        break
    return rv

def _get_root_logger():
    global _ROOT_LOGGER
    if _ROOT_LOGGER is None:
        _ROOT_LOGGER = getLogger(_ROOT_LOGGER_NAME)
    return _ROOT_LOGGER

def panic(msg, *args, **kwargs):
    _get_root_logger().panic(msg, *args, **kwargs)

def alert(msg, *args, **kwargs):
    _get_root_logger().alert(msg, *args, **kwargs)

def critical(msg, *args, **kwargs):
    _get_root_logger().critical(msg, *args, **kwargs)

def error(msg, *args, **kwargs):
    _get_root_logger().error(msg, *args, **kwargs)

def warning(msg, *args, **kwargs):
    _get_root_logger().warning(msg, *args, **kwargs)

def notice(msg, *args, **kwargs):
    _get_root_logger().notice(msg, *args, **kwargs)

def output(msg, *args, **kwargs):
    _get_root_logger().output(msg, *args, **kwargs)

def info(msg, *args, **kwargs):
    _get_root_logger().info(msg, *args, **kwargs)

def debug(msg, *args, **kwargs):
    _get_root_logger().debug(msg, *args, **kwargs)

def fine(msg, *args, **kwargs):
    _get_root_logger().fine(msg, *args, **kwargs)

def trace(msg, *args, **kwargs):
    _get_root_logger().trace(msg, *args, **kwargs)

def lowest(msg, *args, **kwargs):
    _get_root_logger().lowest(msg, *args, **kwargs)



class BXILogger(object):
    """A BXILogger instance provides various methods for logging.

    This class provides a thin layer on top of the underlying C module.
    """

    def __init__(self, clogger):
        """Wraps the given C logger

        @param[in] clogger the C underlying logger instance.

        @return a BXILogger instance
        """
        self.clogger = clogger

    def _log(self, level, msg, *args, **kwargs):
        """Log the given message at the given level

        @param[in] level the level at which the given message should be logged
        @param[msg] the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg

        @raise
        """
        if _bxibase_api.bxilog_is_enabled_for(self.clogger, level):
            s = msg % args
            filename, lineno, funcname = findCaller()
            bxierr_p = _bxibase_api.bxilog_log_nolevelcheck(self.clogger,
                                                       level,
                                                       filename,
                                                       len(filename) + 1,
                                                       funcname,
                                                       len(funcname) + 1,
                                                       lineno,
                                                       s)
            BXICError.raise_if_ko(bxierr_p)


    def panic(self, msg, *args, **kwargs):
        """Documentation for a function.

        @param[in] msg the message to log at the panic level
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return Nothing

        """

        self._log(_bxibase_api.BXILOG_PANIC, msg, *args, **kwargs)

    def alert(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_ALERT, msg, *args, **kwargs)

    def critical(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_CRITICAL, msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_ERROR, msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_WARNING, msg, *args, **kwargs)

    def notice(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_NOTICE, msg, *args, **kwargs)

    def output(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_OUTPUT, msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_INFO, msg, *args, **kwargs)

    def debug(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_DEBUG, msg, *args, **kwargs)

    def fine(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_FINE, msg, *args, **kwargs)

    def trace(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_TRACE, msg, *args, **kwargs)

    def lowest(self, msg, *args, **kwargs):
        self._log(_bxibase_api.BXILOG_LOWEST, msg, *args, **kwargs)



