#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Pierre Vignéras <pierre.vigneras@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################


"""Unit tests of BXIError Python library.
"""

import ctypes
import random
import unittest

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr

__FFI__ = bxiffi.get_ffi()


class TestBXIError(unittest.TestCase):
    def test_bxierr_fields(self):
        null = __FFI__.NULL
        err = bxibase.__CAPI__.bxierr_new(42, null, null, null, null, "Foo")
        self.assertEquals(err.code, 42)
    
    def test_bxierr_list(self):
        pass
    
    def test_bxierr_set(self):
        bxierr_set = bxibase.__CAPI__.bxierr_set_new()
        for i in range(10):
            rc = random.randrange(0, 5)
            err_p = __FFI__.new('bxierr_p *')
            err_p[0] = bxibase.__CAPI__.bxierr_new(rc,
                                                   __FFI__.NULL,
                                                   __FFI__.NULL,
                                                   __FFI__.NULL,
                                                   __FFI__.NULL,
                                                    "A random error: rc=%d",
                                                    __FFI__.cast('int', rc))
            bxibase.__CAPI__.bxierr_set_add(bxierr_set, err_p);
        
        func = __FFI__.cast('void (*)(void*)', bxibase.__CAPI__.bxierr_set_free)

        err = bxibase.__CAPI__.bxierr_new(10, 
                                          bxierr_set,
                                          func,
                                          bxibase.__CAPI__.bxierr_list_add_to_report,
                                          __FFI__.NULL,
                                          "An error from an errset")
        self.assertTrue(bxierr.BXICError.is_ko(err))

    

###############################################################################

if __name__ == "__main__":
    unittest.main()
