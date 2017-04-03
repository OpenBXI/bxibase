#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
@file zmq.py C binding of usefull additionnal functions for pyzmq
@authors Jean-Noel Quintin <jean-noel.quintin@atos.net>
@copyright 2017  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.zmq Python BXI ZMQ additionnal functions

This module exposes additionnal function for the usage of zmq sockets

"""

import bxi.base as bxibase
import bxi.base.err as bxierr

# Find the C library
__FFI__ = bxibase.get_ffi()
__BXIBASE_CAPI__ = bxibase.get_capi()


def sync_pub(pub_zocket, sync_zocket, sync_url, timeout=1.0):
    """
    Wrapper on the c synchronization function for PUB/SUB socket
    @param[in] pub_zocket to be synchronized
    @param[in] sync_zocket REP/REQ socket for communicating with the client
    @param[in] sync_url on which the sync_socket is binded
    @param[in] timeout for the synchronization
    @return
    """
    cpub_zocket = __FFI__.cast("void *", pub_zocket.underlying)
    csync_zocket = __FFI__.cast("void *", sync_zocket.underlying)
    err = __BXIBASE_CAPI__.bxizmq_sync_pub(cpub_zocket,
                                           csync_zocket,
                                           sync_url,
                                           len(sync_url),
                                           timeout)
    bxierr.BXICError.raise_if_ko(err)


def sync_sub(ctx, sub_zocket, timeout=1.0):
    """
    Wrapper on the c synchronization function for PUB/SUB socket
    @param[in] ctx the zmq context used for the creation of the SUB socket
    @param[in] sub_zocket the SUB socket to be synchronized
    @param[in] timeout for the synchronization
    @return
    """
    cctx = __FFI__.cast("void *", ctx.underlying)
    csub_zocket = __FFI__.cast("void *", sub_zocket.underlying)
    err = __BXIBASE_CAPI__.bxizmq_sync_sub(cctx,
                                           csub_zocket,
                                           timeout)
    bxierr.BXICError.raise_if_ko(err)
