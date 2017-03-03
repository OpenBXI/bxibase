# -*- coding: utf-8 -*-

"""
@file remote_receiver.py client side of the bxilog remote handler
@authors Pierre Vignéras <pierre.vigneras@atos.net>
@copyright 2016  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.log.remote_receiver client side of the bxilog remote handler

"""

import bxi.ffi as bxiffi
import bxi.base as bxibase
import bxi.base.err as bxierr

# Find the C library
__FFI__ = bxiffi.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def start_receiving(urls, sync_nb, ctrl_zock=None):
    """
    Start receiving bxilogs from the given set of urls.

    if ctrl_zock is not None, the underlying SUB socket will bind to the given URLs
    instead of connecting. The given ctrl_zock will be used
    to send back to the other side the actual URL the SUB socket has been binded to.

    @note this function immediately returns. Use stop_receiving() to
          stop the underlying thread.

    @param[in] urls the urls to bind/connect to
    @param[in] sync_nb the number of synchronization to perform
    @param[in] ctrl_zock bind instead of connect if not None

    @return the urls on which the SUB socket has been binded to

    @see stop_receiving()
    """
    param = __FFI__.new('bxilog_remote_recv_s *')
    param.nb_urls = len(urls)
    # WARNING: use local variables in order to prevent the GC from
    # freeing allocated space before the C function has been called!
    # This is quite tricky! If you modify this piece of code
    # better be sure on what you are doing!

    tmpref = []
    for url in urls:
        tmpref.append(__FFI__.new('char[]', str(url)))
    c_urls = __FFI__.new('char *[]', tmpref)
    param.urls = c_urls
    param.bind = ctrl_zock is not None
    param.sync_nb = sync_nb
    urls_a = __FFI__.new('char *[1]')
    urls_p = __FFI__.new('char **[1]')
    urls_p[0] = urls_a
    err = __BXIBASE_CAPI__.bxilog_remote_recv_async_start(param, urls_p)
    bxierr.BXICError.raise_if_ko(err)
    result = []
    for i in xrange(len(urls)):
        result.append(__FFI__.string(urls_a[i]))
    # TODO: change this design! The ctrl zocket and the sub zocket are not in the same
    # thread! This has some strange consequences.
    # When binding on SUB, the other end, expect the URL to be sent on the CTRL socket.
    if ctrl_zock is not None:
        ctrl_zock.send_multipart(result)
    return result


def stop_receiving():
    """
    Stop receiving bxilogs.

    @see start_receiving()
    """
    err = __BXIBASE_CAPI__.bxilog_remote_recv_async_stop()
    bxierr.BXICError.raise_if_ko(err)
