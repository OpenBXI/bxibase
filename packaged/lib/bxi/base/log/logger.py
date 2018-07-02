#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
@file logger.py Logger class
@author Pierre Vignéras <<pierre.vigneras@bull.net>>
@copyright 2018 Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaures - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.logger Logger class

"""

from __future__ import print_function
# Optimization (after profiling)
# If you change this below, better know what you are doing! This has been
# implemented this way after profiling (see misc/profile)
import sys
from sys import _getframe        # Remove '.' since it is time consuming in sys._getframe
import os
from os.path import normcase, basename, dirname, join               # Remove '.' here too
import traceback

import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog
from bxi.base.err import BXICError

# For compatibility between python 2 and 3
try:
    isinstance("", unicode)
except:
    from builtins import str as unicode


# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()

bxierr_isko = __BXIBASE_CAPI__.bxierr_isko                                         # idem
bxilog_logger_is_enabled_for = __BXIBASE_CAPI__.bxilog_logger_is_enabled_for       # idem
bxilog_logger_log_rawstr = __BXIBASE_CAPI__.bxilog_logger_log_rawstr               # idem


# _SRCFILE is used when walking the stack to check when we've got the first
# caller stack frame.
if hasattr(sys, 'frozen'):  # support for py2exe
    _SRCFILE = "bxilog%s__init__%s" % (os.sep, __file__[-4:])
elif __file__[-4:].lower() in ['.pyc', '.pyo']:
    _SRCFILE = __file__[:-4] + '.py'
else:
    _SRCFILE = __file__

_SRCFILE = normcase(_SRCFILE)


def _FindCaller():
    """Find caller filename, linenumber and function name.

    @return a triplet (file name, line number, function name)
    """
    # WARNING: this function is a hotspot!
    frame = _getframe(0)
    if frame is not None:
        frame = frame.f_back
    while hasattr(frame, "f_code"):
        co = frame.f_code
        filename = normcase(co.co_filename)
        if filename == _SRCFILE:
            frame = frame.f_back
            continue
        return (co.co_filename, frame.f_lineno, co.co_name)
    return "(unknown file)", 0, "(unknown function)"


def _bxierr_report(bxierr_p):
    """
    Report and destroy the given C bxierr_report() on standard error.

    @param[in] bxierr_p the error to report and destroy
    """
    # Warning: this function is a hotspot!
    bxierr_pp = __FFI__.new('bxierr_p[1]')
    bxierr_pp[0] = bxierr_p
    __BXIBASE_CAPI__.bxierr_report(bxierr_pp, 2)


def _get_usable_filename_from(fullfilename):
    """
    Try to find a usable filename from the given filename

    Currently handle only when filename is __init__.py,
    it returns: (module)/__init__.py instead.
    """
    # Warning: this function is a hotspot!
    filename = basename(fullfilename)
    if filename == '__init__.py':
        directory = dirname(fullfilename)
        module = basename(directory)
        return join(module, filename)
    return filename


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
        # Warning this function is a hotspot!
        if not bxilog._INITIALIZED:
            bxilog.init()
        clogger = self.clogger              # Remote the dot for performance reason
        if bxilog_logger_is_enabled_for(clogger, level):
            msg_str = msg % args if len(args) > 0 else str(msg)
            fullfilename, lineno, funcname = _FindCaller()
            filename = _get_usable_filename_from(fullfilename)
            filename_len = len(filename) + 1
            funcname_len = len(funcname) + 1
            msg_str_len = len(msg_str) + 1
            if isinstance(msg_str, unicode):
                msg_str = msg_str.encode('utf-8', 'replace')
            if isinstance(filename, unicode):
                filename = filename.encode('utf-8', 'replace')
            if isinstance(funcname, unicode):
                funcname = funcname.encode('utf-8', 'replace')
            bxierr_p = bxilog_logger_log_rawstr(clogger,
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
            if bxierr_isko(bxierr_p):
                _bxierr_report(bxierr_p)

    def _report(self, ei, level, msg, *args, **kwargs):
        """
        Report an error at the given level
        """
        if not bxilog._INITIALIZED:
            bxilog.init()

        report_c = __BXIBASE_CAPI__.bxierr_report_new()

        while True:
            clazz = ei[0]
            value = ei[1]
            if isinstance(value, bxierr.BXICError):
                # Ensure that even when the add_to_report function is NULL,
                # we can still create the report in one way or another
                if value.bxierr_pp[0].add_to_report != __FFI__.NULL:
                    value.bxierr_pp[0].add_to_report(value.bxierr_pp[0],
                                                     report_c,
                                                     64)
                else:
                    __BXIBASE_CAPI__.bxierr_report_add_from_limit(value.bxierr_pp[0],
                                                                  report_c,
                                                                  64)
                break

            if isinstance(value, bxierr.BXIError):
                err_msg = value.msg
                err_bt = value.traceback_str
                __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                                   err_msg.encode('utf-8', 'replace'), len(err_msg) + 1,
                                                   err_bt.encode('utf-8', 'replace'), len(err_bt) + 1)
            else:
                err_msg = "".join(x[:-1] for x in traceback.format_exception_only(clazz,
                                                                                  value))
                if ei[2] is not None:
                    err_bt = bxibase.traceback2str(ei[2])

                    if isinstance(err_msg, unicode):
                        err_msg = err_msg.encode('utf-8', 'replace')
                    if isinstance(err_bt, unicode):
                        err_bt = err_bt.encode('utf-8', 'replace')

                    __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                                       err_msg,
                                                       len(err_msg) + 1,
                                                       err_bt,
                                                       len(err_bt) + 1)

            if not hasattr(value, 'cause') or value.cause is None:
                break
            caused_by_str = __BXIBASE_CAPI__.BXIERR_CAUSED_BY_STR
            caused_by_str_len = __BXIBASE_CAPI__.BXIERR_CAUSED_BY_STR_LEN
            __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                               caused_by_str, caused_by_str_len,
                                               "".encode('utf-8', 'replace'),
                                               len("".encode('utf-8', 'replace')) + 1)

            # Workaround for Python2/3 compatibility issue :
            # Using a bytes object for initializing a C string (char*) seems mandatory in
            # Python 3.
            # In Python 2, an str object is sufficient.
            try:
                cause_str = bytes(str(value.cause), 'utf-8')
            except TypeError:
                cause_str = str(value.cause)

            __BXIBASE_CAPI__.bxierr_report_add(report_c,
                                               cause_str, len(cause_str) + 1,
                                               "".encode('utf-8', 'replace'),
                                               len("".encode('utf-8', 'replace')) + 1)
            ei = (type(value.cause), value.cause, None)

        msg_str = msg % args if len(args) > 0 else str(msg)
        fullfilename, lineno, funcname = _FindCaller()
        filename = _get_usable_filename_from(fullfilename)

        if isinstance(msg_str, unicode):
            msg_str = msg_str.encode('utf-8', 'replace')
        if isinstance(filename, unicode):
            filename = filename.encode('utf-8', 'replace')
        if isinstance(funcname, unicode):
            funcname = funcname.encode('utf-8', 'replace')

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
        __BXIBASE_CAPI__.bxierr_report_free(report_c)

    @property
    def name(self):
        """
        Return the logger name
        @return logger name
        """
        return __FFI__.string(self.clogger.name)

    @property
    def level(self):
        """
        Return the filter level for the current logger
        @return logger filtered level
        """
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
        if isinstance(msg_str, unicode):
            msg_str = msg_str.encode('utf-8', 'replace')
        cmsg_str = __FFI__.new("char []", msg_str)
        filename, lineno, funcname = _FindCaller()
        filename_len = len(filename) + 1
        funcname_len = len(funcname) + 1
        __BXIBASE_CAPI__.bxilog_report_keep(self.clogger,
                                            level,
                                            err.bxierr_pp,
                                            filename.encode('utf-8', 'replace'),
                                            filename_len,
                                            funcname.encode('utf-8', 'replace'),
                                            funcname_len,
                                            lineno,
                                            "%s".encode('utf-8', 'replace'),
                                            cmsg_str)

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
