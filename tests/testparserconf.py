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
import sys
from os.path import expanduser
from tempfile import NamedTemporaryFile
import tempfile
from shutil import rmtree
import subprocess
import string
import bxi.base.log.console_handler as bxilog_consolehandler
import bxi.base.log.null_handler as bxilog_nullhandler
import bxi.base.parserconf as bxiparserconf



"""Unit tests of BXI parserconf module.
"""

import os
import configobj
import unittest

import bxi.base as bxibase
import bxi.base.err as bxierr
import bxi.base.log as bxilog

import bxi.base.posless as posless
import bxi.base.parserconf as parserconf


class ParserConfTest(unittest.TestCase):
    """Unit tests for the BXI parserconf module
    """
    def setUp(self):
        unittest.TestCase.setUp(self)
        
        try:
            self.bxiconfigdir = tempfile.mkdtemp()
            os.environ['BXICONFIGDIR'] = self.bxiconfigdir
            del os.environ['BXICONFIGFILE']
        except KeyError:
            pass

    def tearDown(self):
        pass
#         if os.path.exist(self.bxiconfigdir):
#             rmtree(self.bxiconfigdir)

    def test_default_config(self):
        parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
        parserconf.addargs(parser)
        args = parser.parse_args()
        
        suffix = os.path.join(self.bxiconfigdir,
                              parserconf.DEFAULT_CONFIG_FILENAME)
        self.assertTrue(args.config_file.endswith(suffix), args.config_file)

    def test_specific_dir_config(self):
        del os.environ['BXICONFIGDIR']
        parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
        parserconf.addargs(parser, config_dirname='foo')
        args = parser.parse_args()

        suffix = os.path.join('foo', parserconf.DEFAULT_CONFIG_FILENAME)
        self.assertTrue(args.config_file.endswith(suffix), args.config_file)

    def test_default_cmd_config(self):
        path_created = False
        file_created = False

        try:
            cmd = os.path.basename(sys.argv[0])
            filename = os.path.join(self.bxiconfigdir, 
                                    cmd + parserconf.DEFAULT_CONFIG_SUFFIX)

            if not os.path.exists(filename):
                with open(filename, 'w'):
                    created = True
            parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
            parserconf.addargs(parser)
            args = parser.parse_args()
            
            self.assertTrue(os.path.exists(filename))
            self.assertEquals(args.config_file, filename)
        finally:
            if file_created:
                os.unlink(filename)
            if path_created:
                os.unlink(path)
                
    def test_domain_config(self):
        path_created = False
        file_created = False

        try:
            filename = os.path.join(self.bxiconfigdir, 
                                    'foo' + parserconf.DEFAULT_CONFIG_SUFFIX)

            if not os.path.exists(filename):
                with open(filename, 'w'):
                    created = True
            parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
            parserconf.addargs(parser, domain_name='foo')
            args = parser.parse_args()
            
            self.assertTrue(os.path.exists(filename))
            self.assertEquals(args.config_file, filename)
        finally:
            if file_created:
                os.unlink(filename)
            if path_created:
                os.unlink(path)

    def test_include_config(self):
        default_cfg = configobj.ConfigObj()
        default_cfg['foo0'] = 'bar0'
        default_cfg['foo1'] = 'bar1'
        default_cfg['foo2'] = 'bar2'
        default_filename = NamedTemporaryFile().name
        default_cfg.filename = default_filename
        default_cfg.write()
        
        cfg11 = configobj.ConfigObj()
        cfg11['foo1'] = 'bar1.1'
        cfg11['includes'] = [default_filename]
        cfg11_filename = NamedTemporaryFile().name
        cfg11.filename = cfg11_filename
        cfg11.write()
        
        cfg12 = configobj.ConfigObj()
        cfg12['foo1'] = 'bar1.2'
        cfg12['includes'] = [default_filename]
        cfg12_filename = NamedTemporaryFile().name
        cfg12.filename = cfg12_filename
        cfg12.write()
        
        cfg2 = configobj.ConfigObj()
        cfg2['foo2'] = 'bar2'
        cfg2['includes'] = [cfg11_filename, cfg12_filename]
        cfg2_filename = NamedTemporaryFile().name
        cfg2.filename = cfg2_filename
        cfg2.write()
           
            
        result = parserconf._get_config_from_file(cfg2.filename)
        self.assertEquals(result['foo0'], 'bar0')
        self.assertEquals(result['foo1'], 'bar1.2')
        self.assertEquals(result['foo2'], 'bar2')
        
        
    def _test_logs(self, param):
        exe = os.path.join(os.path.dirname(__file__), "simple_bxilogger.py")
        
        args = [exe]
        args.extend(param)
        process = subprocess.Popen(args,
                                   stdout=subprocess.PIPE, 
                                   stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
#         print(stdout)
#         print(stderr)
        self.assertEquals(len(stdout.strip().split('\n')), 2) # output and notice lines
        self.assertEquals(len(stderr.strip().split('\n')), 5) # All above warning
        
        args.append("--log-console-filters=:error")
        process = subprocess.Popen(args,
                                   stdout=subprocess.PIPE, 
                                   stderr=subprocess.PIPE)
        stdout, stderr = process.communicate()
#         print(stdout)
#         print(stderr)
        self.assertEquals(len(stdout.strip().split('\n')), 1) # Empty line
        self.assertEquals(len(stdout.strip().split('\n')[0]), 0) # Empty line
        self.assertEquals(len(stderr.strip().split('\n')), 4) # All above error

    def test_log_no_conf(self):
        "Default is console handler when no configuration is found."
        self._test_logs([])

    def test_log_simple_conf(self):
        config = {'handlers': ['console',],
                  'setsighandler': True,
                  'console': {'module': bxilog_consolehandler.__name__,
                              'filters': ':output',
                              'stderr_level': 'WARNING',
                              'colors': '216_dark',
                              },
                 }
        fileconf = os.path.join(self.bxiconfigdir, 'bxilog.conf')
        with open(fileconf, 'w') as f:
            configobj.ConfigObj(config).write(f)
        self._test_logs(['--logcfgfile=%s' % fileconf])
    
    def test_log_directref_conf(self):
        config = {'handlers': ['console',],
                  'setsighandler': True,
                  'console': {'module': bxilog_consolehandler.__name__,
                              'filters': ':output',
                              'stderr_level': 'WARNING',
                              'colors': '216_dark',
                              },
                 }
        fileconf = os.path.join(self.bxiconfigdir, 'bxilog.conf')
        with open(fileconf, 'w') as f:
            configobj.ConfigObj(config).write(f)
        
        globalconfig = {bxiparserconf.BXILOG_DEFAULT_CONFIGFILE_KEY: fileconf}
        globalconf_file = os.path.join(self.bxiconfigdir, 'global.conf')
        with open(globalconf_file, 'w') as f:
            configobj.ConfigObj(globalconfig).write(f)
            
        self._test_logs(['--config-file=%s' % globalconf_file])
        
    def test_log_indirectref_conf(self):
         # This one will be overwritten
        config = {'handlers': ['none',],
                  'setsighandler': True,
                  'none': {'module': bxilog_nullhandler.__name__,
                           'filters': ':trace',
                           },
                 }
        fileconf = os.path.join(self.bxiconfigdir, 'bxilog.conf')
        with open(fileconf, 'w') as f:
            configobj.ConfigObj(config).write(f)
        
        globalconfig = {bxiparserconf.BXILOG_DEFAULT_CONFIGFILE_KEY: fileconf}
        globalconf_file = os.path.join(self.bxiconfigdir, 'global.conf')
        with open(globalconf_file, 'w') as f:
            configobj.ConfigObj(globalconfig).write(f)
        
        # This one will be used
        config = {'handlers': ['console',],
                  'setsighandler': True,
                  'console': {'module': bxilog_consolehandler.__name__,
                              'filters': ':output',
                              'stderr_level': 'WARNING',
                              'colors': '216_dark',
                              },
                 }
        fileconf = os.path.join(self.bxiconfigdir, 'specific.log.conf')
        with open(fileconf, 'w') as f:
            configobj.ConfigObj(config).write(f)
        
        specific_config = {'include': globalconf_file,
                           bxiparserconf.BXILOG_DEFAULT_CONFIGFILE_KEY: fileconf}
        specificconf_file = os.path.join(self.bxiconfigdir, 'specific.conf')
        with open(specificconf_file, 'w') as f:
            configobj.ConfigObj(specific_config).write(f)
            
#         print("Config file: %s" % specificconf_file)
        self._test_logs(['--config-file=%s' % specificconf_file])
    


###############################################################################

if __name__ == "__main__":
    unittest.main()
