# -*- coding: utf-8 -*-
###############################################################################
# Author: Sébastien Miquée <sebastien.miquee@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################

"""Unit tests of BXI Log Python library.
"""

import bxilog

import sys
import os

import unittest


###############################################################################
class BXILogTest(unittest.TestCase):
    """Unit tests for the BXILog
    """

    def setUp(self):
        """Definition of the context.
        """
        pass

    def tearDown(self):
        """Cleaning up for each test.
        """
        pass


###############################################################################
    def test_default_logger(self):
        """Test default logging functions"""
        for level in bxilog.getAllLevelNames_iter():
            getattr(bxilog, level)("Some stuff with noarg")
            getattr(bxilog, level)("Some stuff with args: %d %f %s", 2, 3.14, 'a string')


    def test_basic_log(self):
        """Test normal logging"""
        def _produce_logs(logger, level):
            getattr(logger, level)("Some stuff with noarg")
            getattr(logger, level)("Some stuff with args: %d %f %s", 2, 3.14, 'a string')

        logger1 = bxilog.getLogger("foo")
        for level in bxilog.getAllLevelNames_iter():
            _produce_logs(logger1, level)

        logger2 = bxilog.getLogger("bar")
        for level in bxilog.getAllLevelNames_iter():
            _produce_logs(logger2, level)

        logger3 = bxilog.getLogger("foo.bar")
        for level in bxilog.getAllLevelNames_iter():
            _produce_logs(logger3, level)

    def test_error_log(self):
        """Test error while logging"""
        logger = bxilog.getLogger("foo")
        self.assertRaises(TypeError,
                          logger.output, "Testing not enough args: %d %d %s", 1)
        self.assertRaises(TypeError,
                          logger.output,
                           "Testing too many args: %d %d %s", 1, 2, 3, 'toto', 'tata')
        self.assertRaises(TypeError,
                          logger.output,
                           "Testing wrong types: %d %d %s", 'foo', 2.5, 3, 'toto')



###############################################################################

if __name__ == "__main__":
    unittest.main()
