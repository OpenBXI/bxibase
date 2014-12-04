#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Sébastien Miquée <sebastien.miquee@bull.net>
###############################################################################
# Copyright (C) 2013  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################
import tempfile

"""Unit tests of BXI Log Python library.
"""

import bxi.base.log as bxilog
from bxi.base.err import BXICError

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
        bxilog.cleanup()
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

    def test_existing_file(self):
        """Test logging into an existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")

        # bxilog.output("One log on standard output")
        print("File output: %s" % name)
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)

        bxilog.basicConfig(filename=name)

        bxilog.output("One log on file: %s", name)

        self.assertNotEquals(os.stat(name).st_size, 0)

        os.remove(name)

    def test_non_existing_file(self):
        """Test logging into a non existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")
        print("Using file %s" % name)
        os.close(fd)
        os.remove(name)

        bxilog.basicConfig(filename=name)

        bxilog.output("One log on non-existent (deleted) file: %s", name)

        bxilog.cleanup()

        self.assertNotEquals(os.stat(name).st_size, 0)

    def test_non_existing_dir(self):
        """Test logging into a non existing tmpdir - this should raise an error"""
        tmpdir = tempfile.mkdtemp(".bxilog", "test_")
        os.rmdir(tmpdir)
        name = os.path.join(tmpdir, 'dummy.bxilog')
        bxilog.basicConfig(filename=name)

        self.assertRaises(BXICError, bxilog.output,
                          "One log on non-existent (deleted) directory: %s", name)

        bxilog.cleanup()

        self.assertFalse(os.path.exists(name))




###############################################################################

if __name__ == "__main__":
    unittest.main()
