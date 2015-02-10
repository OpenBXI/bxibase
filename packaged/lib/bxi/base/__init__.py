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
from bxi.base.cffi_h import C_DEF as LOG_DEF

__bxibase_ffi__ = FFI()
__bxibase_ffi__.cdef(LOG_DEF)
__bxibase_api__ = __bxibase_ffi__.dlopen('libbxibase.so')

def get_bxibase_ffi():
    return __bxibase_ffi__

def get_bxibase_api():
    return __bxibase_api__

