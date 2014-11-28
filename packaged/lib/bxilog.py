#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@bull.net>
# Contributors:
###############################################################################
# Copyright (C) 2013  Bull S.A.S.  -  All rights reserved
# Bull
# Rue Jean Jaurès
# B.P. 68
# 78340 Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull SAS for details about its license.
###############################################################################
"""BXI High Performance Logging Library"""

import atexit
from cffi import FFI
import os
import sys

from bxilog_h import C_DEF as LOG_DEF

#
# _srcfile is used when walking the stack to check when we've got the first
# caller stack frame.
#
if hasattr(sys, 'frozen'):  # support for py2exe
    _srcfile = "bxilog%s__init__%s" % (os.sep, __file__[-4:])
elif __file__[-4:].lower() in ['.pyc', '.pyo']:
    _srcfile = __file__[:-4] + '.py'
else:
    _srcfile = __file__
_srcfile = os.path.normcase(_srcfile)


ffi = FFI()

ffi.cdef(LOG_DEF)

capi = ffi.dlopen('libbxibase.so')

_INITIALIZED = False
_CONFIG = None

def getLogger(name):
    """
    Return the logger instance with the given name.

    A new logger is created if it does not exist yet.
    """
    global _INITIALIZED
    global _CONFIG

    if not _INITIALIZED:
        if _CONFIG is None:
            _CONFIG = {'output':'-',
                       'cfg_items': [('', capi.BXILOG_LOWEST)],
                       'setsighandler': True}

        capi.bxilog_init(os.path.basename(sys.argv[0]), _CONFIG['output'])
        atexit.register(capi.bxilog_finalize)
        _INITIALIZED = True
        if _CONFIG['setsighandler']:
            capi.bxilog_install_sighandler()

    for logger in getAllLoggers_iter():
        if logger.clogger.name == name:
            return logger
    logger = capi.bxilog_new(name)
    capi.bxilog_register(logger)

    cfg_items = _CONFIG['cfg_items']
    cffi_items = ffi.new('bxilog_cfg_item_s[]', len(cfg_items))
    for i in xrange(len(cfg_items)):
        cffi_items[i].prefix = ffi.new('char[]', cfg_items[i][0])
        cffi_items[i].level = cfg_items[i][1]

    capi.bxilog_cfg_registered(len(cfg_items), cffi_items)

    return _BXILoggerWrapper(logger)


def close_logs():
    """Function called for closing the BXI C log"""
    capi.bxilog_finalize()

def getAllLevelNames_iter():
    names = ffi.new("char ***")
    nb = capi.bxilog_get_all_level_names(names)
    print(nb)
    for i in xrange(nb):
        yield ffi.string(names[0][i])

def getAllLoggers_iter():
    loggers = ffi.new("bxilog_p **")
    nb = capi.bxilog_get_registered(loggers)
    for i in xrange(nb):
        yield _BXILoggerWrapper(loggers[0][i])


def findCaller():
    """
    Find the stack frame of the caller so that we can note the source
    file name, line number and function name.
    """
    f = sys._getframe(0)
    if f is not None:
        f = f.f_back
    rv = "(unknown file)", 0, "(unknown function)"
    while hasattr(f, "f_code"):
        co = f.f_code
        filename = os.path.normcase(co.co_filename)
        if filename == _srcfile:
            f = f.f_back
            continue
        rv = (co.co_filename, f.f_lineno, co.co_name)
        break
    return rv

def panic(msg, *args, **kwargs):
    getLogger('root').panic(msg, *args, **kwargs)

def alert(msg, *args, **kwargs):
    getLogger('root').alert(msg, *args, **kwargs)

def critical(msg, *args, **kwargs):
    getLogger('root').critical(msg, *args, **kwargs)

def error(msg, *args, **kwargs):
    getLogger('root').error(msg, *args, **kwargs)

def warning(msg, *args, **kwargs):
    getLogger('root').warning(msg, *args, **kwargs)

def notice(msg, *args, **kwargs):
    getLogger('root').notice(msg, *args, **kwargs)

def output(msg, *args, **kwargs):
    getLogger('root').output(msg, *args, **kwargs)

def info(msg, *args, **kwargs):
    getLogger('root').info(msg, *args, **kwargs)

def debug(msg, *args, **kwargs):
    getLogger('root').debug(msg, *args, **kwargs)

def fine(msg, *args, **kwargs):
    getLogger('root').fine(msg, *args, **kwargs)

def trace(msg, *args, **kwargs):
    getLogger('root').trace(msg, *args, **kwargs)

def lowest(msg, *args, **kwargs):
    getLogger('root').lowest(msg, *args, **kwargs)



class _BXILoggerWrapper(object):

    def __init__(self, clogger):
        self.clogger = clogger

    def _log(self, level, msg, *args, **kwargs):
        if capi.bxilog_is_enabled_for(self.clogger, level):
            s = msg % args
            filename, lineno, funcname = findCaller()
            capi.bxilog_log_nolevelcheck(self.clogger,
                                         level,
                                         filename,
                                         len(filename) + 1,
                                         funcname,
                                         len(funcname) + 1,
                                         lineno,
                                         s)


    def panic(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_PANIC, msg, *args, **kwargs)

    def alert(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_ALERT, msg, *args, **kwargs)

    def critical(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_CRITICAL, msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_ERROR, msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_WARNING, msg, *args, **kwargs)

    def notice(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_NOTICE, msg, *args, **kwargs)

    def output(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_OUTPUT, msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_INFO, msg, *args, **kwargs)

    def debug(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_DEBUG, msg, *args, **kwargs)

    def fine(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_FINE, msg, *args, **kwargs)

    def trace(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_TRACE, msg, *args, **kwargs)

    def lowest(self, msg, *args, **kwargs):
        self._log(capi.BXILOG_LOWEST, msg, *args, **kwargs)



