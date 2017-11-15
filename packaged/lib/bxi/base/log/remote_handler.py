# -*- coding: utf-8 -*-

"""
@file remote_handler.py bxilog remote handler
@authors Sébastien Miquée <sebastien.miquee@atos.net>
@copyright 2016  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.remote_handler bxilog remote handler

"""

import bxi.base as bxibase
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def add_handler(configobj, section_name, c_config):
    """
    Add a remote handler configured from the given section in configobj to the c_config

    @param[in] configobj the configobj (a dict) representing the whole configuration
    @param[in] section_name the section name in the configobj that must be used
    @param[inout] c_config the bxilog configuration where the handler must be added to
    """
    section = configobj[section_name]

    filters_str = section['filters']

    filters = bxilogfilter.parse_filters(filters_str)


    url = section['url']
    if '%(prog)s' in url:
        import bxi.base.log as bxilog
        import sys
        url = url % {'prog': bxilog._PROGNAME if bxilog._PROGNAME is not None\
                                              else sys.argv[0]}

    url = __FFI__.new('char[]', url.encode("utf-8", "replace"))
    bind = __FFI__.cast('bool', section.as_bool('bind'))
    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_REMOTE_HANDLER,
                                               filters._cstruct,
                                               url,
                                               bind)
