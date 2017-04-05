#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
"""

@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.netsnmp_handler net-snmp log handler

"""

import bxi.ffi as bxiffi
import bxi.base.err as bxierr
import bxi.base as bxibase
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_handler(configobj, section_name, c_config):
    """
    Add a net-snmp handler configured from the given section in configobj to the c_config

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section_name the section name in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to 
    """
    section = configobj[section_name]
    filters_str = section['filters']

    filters = bxilogfilter.parse_filters(filters_str)

    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_SNMPLOG_HANDLER,
                                               filters._cstruct)
