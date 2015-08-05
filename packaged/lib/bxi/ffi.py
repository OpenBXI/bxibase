# -*- coding: utf-8 -*-

"""
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.cffi Python BXI CFFI instance and tools

"""

from cffi import FFI
from cffi.api import CDefError

__FFI__ = FFI()


def add_cdef_for_type(ctype, cdef, packed=False):
    """
    Return the ffi object used by this module to interact with the C backend.

    @return the ffi object used by this module to interact with the C backend.
    """
    try:
        __FFI__.getctype(ctype)
    except CDefError:
        __FFI__.cdef(cdef, packed)


def get_ffi():
    return __FFI__
