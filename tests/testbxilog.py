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
"""Unit tests of BXI Log Python library.
"""
###############################################################################

import os, time
import sys
import tempfile
import unittest

from bxi.base.err import BXICError
import bxi.base.log as bxilog

FILENAME = "%s.bxilog" % __name__


class BXILogTest(unittest.TestCase):
    """Unit tests for the BXILog
    """

    def _check_log_produced(self, file, log_f, *args):
        old_size = os.stat(file).st_size if os.path.exists(file) else 0
        log_f(*args)
        bxilog.flush()
        self.assertTrue(os.stat(file).st_size > old_size)

    def setUp(self):
        """Definition of the context.
        """
        print("Whole unit test suite file outputs: %s" % FILENAME)
        bxilog.basicConfig(filename=FILENAME,
                           setsighandler=True,
                           cfg_items=[('', 'lowest')])
        pass

    def tearDown(self):
        """Cleaning up for each test.
        """
        bxilog.cleanup()
        pass


###############################################################################
    def test_default_logger(self):
        """Test default logging functions"""
        for level in bxilog.get_all_level_names_iter():
            self._check_log_produced(FILENAME, 
                                     getattr(bxilog, level), "Some stuff with noarg")
            self._check_log_produced(FILENAME, 
                                     getattr(bxilog, level), "Some stuff with args: %d %f %s", 2, 3.14, 'a string')

    def test_basic_log(self):
        """Test normal logging"""
        def _produce_logs(logger, level):
            self._check_log_produced(FILENAME,
                                     getattr(logger, level), "Some stuff with noarg")
            self._check_log_produced(FILENAME,
                                     getattr(logger, level), "Some stuff with args: %d %f %s", 2, 3.14, 'a string')

        logger1 = bxilog.getLogger("foo")
        for level in bxilog.get_all_level_names_iter():
            _produce_logs(logger1, level)

        logger2 = bxilog.getLogger("bar")
        for level in bxilog.get_all_level_names_iter():
            _produce_logs(logger2, level)

        logger3 = bxilog.getLogger("foo.bar")
        for level in bxilog.get_all_level_names_iter():
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

        print("Overriding file output to "
              "%s for %s.%s()" % (name, __name__,
                                  BXILogTest.test_existing_file.__name__))
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)

        bxilog.basicConfig(filename=name)

        self._check_log_produced(name, bxilog.output, "One log on file: %s", name)
        os.remove(name)

    def test_non_existing_file(self):
        """Test logging into a non existing file"""
        fd, name = tempfile.mkstemp(".bxilog", "test_")
        print("Overriding file output to "
              "%s for %s.%s()" % (name, __name__,
                                  BXILogTest.test_non_existing_file.__name__))
        self.assertEquals(os.stat(name).st_size, 0)
        os.close(fd)
        os.remove(name)

        bxilog.basicConfig(filename=name)

        self._check_log_produced(name, bxilog.output,
                                 "One log on non-existent (deleted) file: %s", name)
        bxilog.cleanup()

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

    def test_config_file(self):
        """Test logging with configuration items"""

        orig_size = os.stat(FILENAME).st_size

        bxilog.basicConfig(filename=FILENAME,
                           cfg_items=[('','output'),
                                      ('foo', 'debug'),
                                      ('foo.bar','trace')])
        
        foo = bxilog.getLogger('foo')
        bar = bxilog.getLogger('foo.bar')
        any = bxilog.getLogger('bebopalula')
        
        any.info('This message must not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
        bar.lowest('This message must also not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
        foo.fine('This message must not appear in file %s', FILENAME)
        bxilog.flush()
        newsize = os.stat(FILENAME).st_size
        self.assertEquals(newsize, orig_size)
        
        self._check_log_produced(FILENAME, any.output,
                                 'This message must appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, bar.trace,
                                 'This message must also appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, foo.debug,
                                 'This message must also appear in file %s', FILENAME)
        self._check_log_produced(FILENAME, bxilog.output,
                                 "This message must also appear in file %s", FILENAME)

###############################################################################

if __name__ == "__main__":
    unittest.main()
