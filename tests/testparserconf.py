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
        
        cmd = os.path.basename(sys.argv[0])
        path = os.path.join(os.path.expanduser('~'),
                            '.config',
                            parserconf.DEFAULT_CONFIG_DIRNAME)
        filename = os.path.join(path, cmd + parserconf.DEFAULT_CONFIG_SUFFIX)
        if os.path.exists(filename):
            os.unlink(filename)

    def test_default_config(self):
        parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
        parserconf.addargs(parser)
        args = parser.parse_args()
        
        suffix = os.path.join(parserconf.DEFAULT_CONFIG_DIRNAME,
                              parserconf.DEFAULT_CONFIG_FILENAME)
        self.assertTrue(args.config_file.endswith(suffix), args.config_file)
        
    def test_specific_dir_config(self):
        parser = posless.ArgumentParser(formatter_class=parserconf.FilteredHelpFormatter)
        parserconf.addargs(parser, config_dirname='foo')
        args = parser.parse_args()

        suffix = os.path.join('foo', parserconf.DEFAULT_CONFIG_FILENAME)
        self.assertTrue(args.config_file.endswith(suffix))
        
    def test_default_cmd_config(self):
        path_created = False
        file_created = False

        try:
            path = os.path.join(os.path.expanduser('~'),
                                '.config',
                                parserconf.DEFAULT_CONFIG_DIRNAME)
            cmd = os.path.basename(sys.argv[0])
            filename = os.path.join(path, cmd + parserconf.DEFAULT_CONFIG_SUFFIX)

            if not os.path.exists(path):
                os.makedirs(path)
                path_created = True
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
                
    def test_include_config(self):
        default_cfg = configobj.ConfigObj()
        default_cfg['foo0'] = 'bar0'
        default_cfg['foo1'] = 'bar1'
        default_cfg['foo2'] = 'bar2'
        default_filename = NamedTemporaryFile().name
        default_cfg.filename = default_filename
        default_cfg.write()
        
        cfg1 = configobj.ConfigObj()
        cfg1['foo1'] = 'bar1.1'
        cfg1['foo1.1'] = 'bar1.1'
        cfg1['include'] = default_filename
        cfg1_filename = NamedTemporaryFile().name
        cfg1.filename = cfg1_filename
        cfg1.write()
        
        cfg2 = configobj.ConfigObj()
        cfg2['foo2'] = 'bar1.2'
        cfg2['foo2.2'] = 'bar2.2'
        cfg2['include'] = cfg1_filename
        cfg2_filename = NamedTemporaryFile().name
        cfg2.filename = cfg2_filename
        cfg2.write()
           
            
        result = parserconf._get_config_from_include_file(cfg2.filename)
        self.assertEquals(result['foo0'], 'bar0')
        self.assertEquals(result['foo1'], 'bar1.1')
        self.assertEquals(result['foo1.1'], 'bar1.1')
        self.assertEquals(result['foo2'], 'bar1.2')
        self.assertEquals(result['foo2.2'], 'bar2.2')

###############################################################################

if __name__ == "__main__":
    unittest.main()
