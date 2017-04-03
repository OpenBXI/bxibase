#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""
@file config.py bxilog configuration utilities
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.config bxilog configuration utilities

"""

import importlib

import bxi.base as bxibase

# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_handler(configobj, section_name, c_config):
    """
    Add a bxilog handler to the given configuration.

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section the section in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to 
    """
    section = configobj[section_name]
    module_name = section['module']
    module = importlib.import_module(module_name)
    module.add_handler(configobj, section_name, c_config)

