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

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.log.filter as bxilogfilter

# Find the C library
__FFI__ = bxiffi.get_ffi()
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

    url = __FFI__.new('char[]', section['url'])
    bind = __FFI__.cast('bool', section.as_bool('bind'))
    sub_nb = __FFI__.cast('int', int(section['sub_nb']))
    __BXIBASE_CAPI__.bxilog_config_add_handler(c_config,
                                               __BXIBASE_CAPI__.BXILOG_REMOTE_HANDLER,
                                               filters,
                                               url,
                                               bind,
                                               sub_nb)


def remote_recv_async_start(urls, sync_nb=0, bind=False):
    param_p = __FFI__.new('bxilog_remote_recv_s[1]')

    param_p[0].nb_urls = len(urls)
    curls = __FFI__.new('char*[%s]' % len(urls))

    param_p[0].urls = curls
    param_p[0].bind = bind
    param_p[0].sync_nb = sync_nb

    gc_keeper_urls = list() # Required to prevent GC
    for i in xrange(len(urls)):
        url = __FFI__.new('char[]', urls[i])
        gc_keeper_urls.append(url)
        param_p[0].urls[i] = url

    binded_curls = __FFI__.new('char*[%s]' % len(urls))
    binded_curls_p = __FFI__.new('char**[1]')
    binded_curls_p[0] = binded_curls
    __BXIBASE_CAPI__.bxilog_remote_recv_async_start(param_p, binded_curls_p)
    result = []

    for i in xrange(len(urls)):
        result.append(__FFI__.string(binded_curls[i])) # TODO: how to gc the C memory?
    return result


def remote_recv_async_stop():
    __BXIBASE_CAPI__.bxilog_remote_recv_async_stop()
