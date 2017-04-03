# -*- coding: utf-8 -*-

"""
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.cffi Python BXI CFFI instance and tools

"""

import time

__FFI__ = None


def include_for_type(ctype, ffi):
    '''
    Include the given ffi, only if given ctype isn't include yet.

    Warning: this doesn't work for function ctype !

    @return None
    '''
    start = time.time()
    # pylint: disable=W0603
    global __FFI__
    if __FFI__ is None:
        __FFI__ = ffi

    try:
        __FFI__.getctype(ctype)
    except __FFI__.error:
        __FFI__.include(ffi)

    print ("##### loading time: %s ###### %s ######"
           % (str((time.time() - start)), ctype))


def get_ffi():
    """
    Return the ffi object used by this module to interact with the C backend.

    @return the ffi object used by this module to interact with the C backend.
    """
    return __FFI__
