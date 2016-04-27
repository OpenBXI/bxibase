#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""
@file logger.py Logger class
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.logger Logger class

"""

import sys
import os
import traceback

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog
from bxi.base.err import BXICError

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


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


def _bxierr_report(bxierr_p):
    """
    Report and destroy the given C bxierr_report() on standard error.

    @param[in] bxierr_p the error to report and destroy
    """
    if bxierr_p is None:
        return
    bxierr_pp = __FFI__.new('bxierr_p[1]')
    bxierr_pp[0] = bxierr_p
    __BXIBASE_CAPI__.bxierr_report(bxierr_pp, 2)


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
        if not bxilog._INITIALIZED:
            bxilog._init()
        if __BXIBASE_CAPI__.bxilog_logger_is_enabled_for(self.clogger, level):
            msg_str = msg % args if len(args) > 0 else str(msg)
            filename, lineno, funcname = _FindCaller()
            filename_len = len(filename) + 1
            funcname_len = len(funcname) + 1
            msg_str_len = len(msg_str) + 1
            bxierr_p = __BXIBASE_CAPI__.bxilog_logger_log_rawstr(self.clogger,
                                                                 level,
                                                                 filename,
                                                                 filename_len,
                                                                 funcname,
                                                                 funcname_len,
                                                                 lineno,
                                                                 msg_str,
                                                                 msg_str_len)
            # Recursive call: this will raise an exception that might be catched
            # again by the logging library and so on...
            # So instead of:
            # bxierr.BXICError.raise_if_ko(bxierr_p)
            # We use directly the bxierr C reporting to stderr (fd=2)
            _bxierr_report(bxierr_p)

    def _report(self, ei, level, msg, *args, **kwargs):
        if not bxilog._INITIALIZED:
            bxilog._init()

        report_c = __BXIBASE_CAPI__.bxierr_report_new()

        while True:
            clazz = ei[0]
            value = ei[1]
            if isinstance(value, bxierr.BXICError):
                __BXIBASE_CAPI__.bxierr_report_add_from(value.bxierr_pp[0], report_c)
                break

            if isinstance(value, bxierr.BXIError):
                err_msg = value.msg
                err_bt = value.traceback_str
                __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                                   err_msg, len(err_msg) + 1,
                                                   err_bt, len(err_bt) + 1)
            else:
                err_msg = "".join(x[:-1] for x in traceback.format_exception_only(clazz,
                                                                                  value))
                if ei[2] is None:
                    err_bt = "-- Backtrace unavailable --"
                else:
                    err_bt = bxibase.traceback2str(ei[2])
                __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                                   err_msg, len(err_msg) + 1,
                                                   err_bt, len(err_bt) + 1)

            if not hasattr(value, 'cause') or value.cause is None:
                break

            cause_str = __BXIBASE_CAPI__.BXIERR_CAUSED_BY_STR
            cause_str_len = __BXIBASE_CAPI__.BXIERR_CAUSED_BY_STR_LEN
            __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                               cause_str, cause_str_len,
                                               "", len("") + 1)
            ei = (type(value.cause), value.cause, None)

        msg_str = msg % args if len(args) > 0 else str(msg)
        filename, lineno, funcname = _FindCaller()
        filename_len = len(filename) + 1
        funcname_len = len(funcname) + 1
        msg_str_len = len(msg_str) + 1

        __BXIBASE_CAPI__.bxilog_report_raw(report_c,
                                           self.clogger,
                                           level,
                                           filename,
                                           filename_len,
                                           funcname,
                                           funcname_len,
                                           lineno,
                                           msg_str,
                                           msg_str_len)

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
        self.log(bxilog.OFF, msg, *args, **kwargs)

    def panic(self, msg, *args, **kwargs):
        """
        Log at the ::PANIC level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.PANIC, msg, *args, **kwargs)

    def alert(self, msg, *args, **kwargs):
        """
        Log at the ::ALERT level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.ALERT, msg, *args, **kwargs)

    def critical(self, msg, *args, **kwargs):
        """
        Log at the ::CRITICAL level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.CRITICAL, msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        """
        Log at the ::ERROR level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.ERROR, msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        """
        Log at the ::WARNING level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.WARNING, msg, *args, **kwargs)

    def notice(self, msg, *args, **kwargs):
        """
        Log at the ::NOTICE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.NOTICE, msg, *args, **kwargs)

    def output(self, msg, *args, **kwargs):
        """
        Log at the ::OUT level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.OUTPUT, msg, *args, **kwargs)

    def info(self, msg, *args, **kwargs):
        """
        Log at the ::INFO level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.INFO, msg, *args, **kwargs)

    def debug(self, msg, *args, **kwargs):
        """
        Log at the ::DEBUG level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.DEBUG, msg, *args, **kwargs)

    def fine(self, msg, *args, **kwargs):
        """
        Log at the ::FINE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.FINE, msg, *args, **kwargs)

    def trace(self, msg, *args, **kwargs):
        """
        Log at the ::TRACE level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.TRACE, msg, *args, **kwargs)

    def lowest(self, msg, *args, **kwargs):
        """
        Log at the ::LOWEST level the given msg.

        @param[in] msg the message to log
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        self.log(bxilog.LOWEST, msg, *args, **kwargs)

    def exception(self, msg, *args, **kwargs):
        """
        Log the current exception with the given message.

        @note the logging level can be specified:
                exception(bxierr,
                          "An error occured on %s with %s", foo, bar,
                          level=WARNING)

        @param[in] msg the message to log with the given exception
        @param[in] args an array of parameters for string substitution in msg
        @param[in] kwargs a dict of named parameters for string substitution in msg
        @return

        """
        if 'level' in kwargs:
            level = kwargs['level']
            del kwargs['level']
        else:
            level = bxilog.ERROR
        self._report(sys.exc_info(), level, msg, *args, **kwargs)

    def report_bxierr(self, err, level=bxilog.ERROR, msg="", *args, **kwargs):
        """
        Report a BXICError or a bxierr_p object.

        @note the logging level can be specified:
                report_bxierr(bxierr,
                              "An error occured on %s with %s", foo, bar,
                              level=WARNING)

        @param[in] err an instance of a BXICError or a bxierr_p 
        @param[in] msg the message to display along with the error report
        @param[in] args message arguments if any
        @param[in] kwargs message arguments if any

        @return
        """
        if not isinstance(err, BXICError) and \
           not __FFI__.typeof(bxierr) is __FFI__.typeof('bxierr_p'):

            raise NotImplementedError('Bad object type, expected BXICError or bxierr_p,'
                                      ' got: %r. Use bxilog.exception() instead?' % err)

        msg_str = msg % args if len(args) > 0 else str(msg)
        filename, lineno, funcname = _FindCaller()
        filename_len = len(filename) + 1
        funcname_len = len(funcname) + 1
        __BXIBASE_CAPI__.bxilog_report_keep(self.clogger,
                                            level,
                                            err.bxierr_pp,
                                            filename,
                                            filename_len,
                                            funcname,
                                            funcname_len,
                                            lineno,
                                            "%s",
                                            msg_str)

    @staticmethod
    def flush():
        """Convenience method for flushing all logs

        @return
        """
        bxilog.flush()

    # Provide a compatible API with the standard Python logging module
    setLevel = set_level

    # Provide a compatible API with the standard Python logging module
    isEnabledFor = is_enabled_for

    # Provide a compatible API with the standard Python logging module
    warn = warning
    out = output

