#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
@file err.py Defines all Python exceptions classes for all BXI modules
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.err Python BXI Exceptions

This module exposes all BXI Exception classes.

"""
import sys

try:
    from builtins import range
except ImportError:
    pass

import bxi.base as bxibase

# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


class BXIError(Exception):
    """
    The root class of all BXI exceptions
    """
    def __init__(self, msg, cause=None):
        """
        Create a new BXIError instance.

        @param[in] msg a message
        @param[in] cause the cause of the error
        """
        super(BXIError, self).__init__(msg)
        self.msg = msg
        self._cause = cause
        tb = sys.exc_info()
        if tb == (None, None, None):
            self.traceback_str = ""
        else:
            self.traceback_str = bxibase.traceback2str(tb[2])

    def __str__(self):
        return self.msg + ("" if self.cause is None
                           else "\n caused by: " + str(self.cause))

    @property
    def cause(self):
        """
        Get the cause of the error
        @return python cause
        """
        return self._cause


class BXICError(BXIError, bxibase.Wrapper):
    """
    Wrap a ::bxierr_p into a Python exception
    """
    def __init__(self, bxierr_p, gc=True):
        """
        Create a new instance for the given ::bxierr_p

        @param[in] bxierr_p a ::bxierr_p C pointer
        @param[in] gc if bxierr_destroy should be called
        """
#        err_msg = __FFI__.string(__BXIBASE_CAPI__.bxierr_str(bxierr_p))
        err_msg = __FFI__.string(bxierr_p.msg)
        super(BXICError, self).__init__(err_msg)
        self.bxierr_pp = __FFI__.new('bxierr_p[1]')
        self.bxierr_pp[0] = bxierr_p
        if gc:
            self.bxierr_pp = __FFI__.gc(self.bxierr_pp,
                                        __BXIBASE_CAPI__.bxierr_destroy)

    def __str__(self):
        c_report = __BXIBASE_CAPI__.bxierr_report_new()
        self.bxierr_pp[0].add_to_report(self.bxierr_pp[0], c_report, 20)
        lines = []
        for i in range(0, c_report.err_nb):
            lines.append(__FFI__.string(c_report.err_msgs[i]))
        result = "\n".join(lines)
        __BXIBASE_CAPI__.bxierr_report_free(c_report)
        return result

    @property
    def cause(self):
        """
        Generate the cause python object only if needed
        @return python object of the cause
        """
        if self._cause is not None:
            return self._cause

        cause_ = self.bxierr_pp[0].cause
        self._cause = BXICError(cause_, gc=False) if cause_ != __FFI__.NULL else None
        return self._cause

    @staticmethod
    def chain(cause, err):
        """
        Chain the err with the cause and change cause for the global err

        @param[inout] cause the original error
        @param[in]    err the last error generated
        @return
        """
        err_p = __FFI__.new('bxierr_p[1]')
        err_p[0] = err
        cause_p = __FFI__.new('bxierr_p[1]')
        cause_p[0] = cause
        __BXIBASE_CAPI__.bxierr_chain(cause_p, err_p)
        cause = cause_p[0]

    @staticmethod
    def is_ko(bxierr_p):
        """
        Test if the given ::bxierr_p is ko.

        @param[in] bxierr_p the ::bxierr_p
        @return boolean
        """
        return __BXIBASE_CAPI__.bxierr_isko(bxierr_p)

    @staticmethod
    def is_ok(bxierr_p):
        """
        Test if the given ::bxierr_p is ok.

        @param[in] bxierr_p the ::bxierr_p
        @return boolean
        """
        return __BXIBASE_CAPI__.bxierr_isok(bxierr_p)

    @classmethod
    def raise_if_ko(cls, bxierr_p):
        """
        Raise a BXICError if the given ::bxierr_p is ko.

        @param[in] cls invoking class, to be raised if ko
        @param[in] bxierr_p the ::bxierr_p
        @return
        @exception BXICError the corresponding BXICError if the given ::bxierr_p is ko.
        """
        if __BXIBASE_CAPI__.bxierr_isko(bxierr_p):
            raise cls(bxierr_p)

    @staticmethod
    def errno2bxierr(msg):
        """
        Return the BXICError related to the value of POSIX errno.

        @param[in] msg a string representing the message before the errno related message

        @return the BXICError related to the value of POSIX errno.
        """
        return __BXIBASE_CAPI__.bxierr_fromidx(__BXIBASE_CAPI__.errno,
                                               __FFI__.NULL, "%s", msg)


class BXILogConfigError(BXIError):
    """
    Raised on error during bxilog configuration.
    """
    def __init__(self, msg, cfg):
        """
        Create a new instance with the given message.

        @param[in] msg the exception message
        @param[in] cfg the bxi log configuration as a dictionary.
        """
        super(BXILogConfigError, self).__init__(msg)
        self.config = cfg

    def get_config(self):
        """
        Return the configuration

        @return the configuration
        """
        return self.config
