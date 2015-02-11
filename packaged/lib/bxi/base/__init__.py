#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base Python BXI Base module

"""
# Try to find other BXI packages in other folders
from pkgutil import extend_path
__path__ = extend_path(__path__, __name__)


from cffi import FFI
from cffi.api import CDefError

from bxi.base.cffi_h import C_DEF

__FFI__ = FFI()
__FFI__.cdef(C_DEF)
__CAPI__ = __FFI__.dlopen('libbxibase.so')


def include_if_required(other_ffi):
    try:
        other_ffi.getctype("bxilog_p")
    except CDefError as ffi:
        other_ffi.cdef(C_DEF)


def get_ffi():
    """
    Return the ffi object used by this module to interact with the C backend.

    @return the ffi object used by this module to interact with the C backend.
    """
    return __FFI__


def get_capi():
    """
    Return the CFFI wrapped C library.

    @return the CFFI wrapped C library.
    """
    return __CAPI__

