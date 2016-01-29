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
import cStringIO as StringIO

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog

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

    def _report_exception(self, level,
                          filename, filename_len,
                          funcname, funcname_len,
                          lineno, ei):

        tbs = []
        msgs = []
        while True:
            if isinstance(ei[1], bxierr.BXICError):
                backtrace = __FFI__.string(ei[1].bxierr_pp[0].backtrace)
                tb = "Low Level C back trace:\n" + backtrace
            else:
                tb = ""
            lines = traceback.format_exception(ei[0], ei[1], ei[2], None)
            tb += ''.join(lines[0:len(lines)-1])
            tbs.insert(0, tb[:-1]) # Remove final '\n'
            msg = lines[-1][:-1] # Remove final '\n'
            msgs.insert(0, msg)

            if not hasattr(ei[1], 'cause') or ei[1].cause is None:
                break

            ei = (type(ei[1].cause), ei[1].cause, ei[1].traceback)

        for i in xrange(len(msgs)):
            bxierr_p = __BXIBASE_CAPI__.bxilog_logger_log_rawstr(self.clogger,
                                                                 bxilog.TRACE, 
                                                                 filename, filename_len,
                                                                 funcname, funcname_len,
                                                                 lineno, 
                                                                 tbs[i],
                                                                 len(tbs[i]) + 1)
            # Recursive call: this will raise an exception that might be catched
            # again by the logging library and so on...
            # So instead of:
            # bxierr.BXICError.raise_if_ko(bxierr_p)
            # We use directly the bxierr C reporting to stderr (fd=2)
            _bxierr_report(bxierr_p)
            bxierr_p = __BXIBASE_CAPI__.bxilog_logger_log_rawstr(self.clogger,
                                                                 level, 
                                                                 filename, filename_len, 
                                                                 funcname, funcname_len, 
                                                                 lineno,
                                                                 msgs[i],
                                                                 len(msgs[i]) + 1)
            # Recursive call: this will raise an exception that might be catched
            # again by the logging library and so on...
            # So instead of:
            # bxierr.BXICError.raise_if_ko(bxierr_p)
            # We use directly the bxierr C reporting to stderr (fd=2)
            _bxierr_report(bxierr_p)

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
            exc_s = ''
            ei = kwargs.get('exc_info', None)
            if ei is not None:
                self._report_exception(level, 
                                       filename, filename_len,
                                       funcname, funcname_len,
                                       lineno, ei)

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
            level = bxilog.ERROR
        self.log(level, msg, *args, **kwargs)

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

