#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
@authors Jean-Noel Quintin <jean-noel.quintin@atos.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.builder cffi builder

"""

from cffi import FFI

import bxi.base_cffi_def as cdef
libname = 'bxibase'
modulename = 'bxi.base.cffi_h'

ffibuilder = FFI()
ffibuilder.set_source(modulename,
                      None,
                      libraries=[libname])   # or a list of libraries to link with
ffibuilder.cdef(cdef.C_DEF)


if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
