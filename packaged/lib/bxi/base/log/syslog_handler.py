# -*- coding: utf-8 -*-


"""
@file syslog_handler.py bxilog syslog handler
@authors Pierre Vignéras <pierre.vigneras@bull.net>
@copyright 2013  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.syslog_handler bxilog syslog handler

"""


import sys
import syslog

import bxi.base as bxibase
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_handler(configobj, section_name, c_config):
    """
    Add a syslog handler configured from the given section in configobj to the c_config

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section_name the section name in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to
    """
    section = configobj[section_name]
    filters_str = section['filters']
    facility_str = section['facility']

    filters = bxilogfilter.parse_filters(filters_str)
    facility = __FFI__.cast('int', eval('syslog.%s' % facility_str))

    identity = __FFI__.new('char[]', sys.argv[0])
    option = __FFI__.cast('int', syslog.LOG_PID)
    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_SYSLOG_HANDLER,
                                               filters._cstruct,
                                               identity,
                                               option,
                                               facility)
