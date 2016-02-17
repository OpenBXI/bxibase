# -*- coding: utf-8 -*-

"""
@file err.py Defines all Python signal handling for all BXI modules
@authors Sébastien Miquée <sebastien.miquee@bull.net>
@copyright 2014  Bull S.A.S.  -  All rights reserved.\n
           This is not Free or Open Source software.\n
           Please contact Bull SAS for details about its license.\n
           Bull - Rue Jean Jaurès - B.P. 68 - 78340 Les Clayes-sous-Bois
@namespace bxi.base.sign Python BXI Signal handling

Signal handling"""


import signal
import bxi.base.log as bxilog


_LOGGER = bxilog.getLogger(bxilog.LIB_PREFIX + 'bxilog.signals')


SIGNALS = ['None',
           'SIGHUP',
           'SIGINT',
           'SIGQUIT',
           'SIGILL',
           'SIGTRAP',
           'SIGABRT',
           'SIGBUS',
           'SIGFPE',
           'SIGKILL',
           'SIGUSR1',
           'SIGSEGV',
           'SIGUSR2',
           'SIGPIPE',
           'SIGALRM',
           'SIGTERM',
           'SIGSTKFLT',
           'SIGCHLD',
           'SIGCONT',
           'SIGSTOP',
           'SIGTSTP',
           'SIGTTIN',
           'SIGTTOU',
           'SIGURG',
           'SIGXCPU',
           'SIGXFSZ',
           'SIGVTALRM',
           'SIGPROF',
           'SIGWINCH',
           'SIGIO',
           'SIGPWR',
           'SIGSYS',
           'SIGRTMIN',
           'SIGRTMIN+1',
           'SIGRTMIN+2',
           'SIGRTMIN+3',
           'SIGRTMIN+4',
           'SIGRTMIN+5',
           'SIGRTMIN+6',
           'SIGRTMIN+7',
           'SIGRTMIN+8',
           'SIGRTMIN+9',
           'SIGRTMIN+10',
           'SIGRTMIN+11',
           'SIGRTMIN+12',
           'SIGRTMIN+13',
           'SIGRTMIN+14',
           'SIGRTMIN+15',
           'SIGRTMAX-14',
           'SIGRTMAX-13',
           'SIGRTMAX-12',
           'SIGRTMAX-11',
           'SIGRTMAX-10',
           'SIGRTMAX-9',
           'SIGRTMAX-8',
           'SIGRTMAX-7',
           'SIGRTMAX-6',
           'SIGRTMAX-5',
           'SIGRTMAX-4',
           'SIGRTMAX-3',
           'SIGRTMAX-2',
           'SIGRTMAX-1',
           'SIGRTMAX',
           ]


def default_handler(signum, frame):
    """
    Default signal handler.

    @param[in] signum Signal number
    @param[in] frame Signal data

    @return
    """
    _LOGGER.debug('Received signal %s (%s)', signum, SIGNALS[signum])


def set_handler(sig, function):
    """
    Set a handler function on a given signal (numeric or name).

    @param[in] sig Signal number or string
    @param[in] function The function to be executed

    @return
    """
    signum = -1
    if type(sig) == int:
        if sig > 0 and sig < len(SIGNALS):
            signum = sig
        else:
            _LOGGER.error("Not a valid signal to handle: %d", sig)
    elif type(sig) == str:
        try:
            signum = SIGNALS.index(sig)
        except ValueError:
            _LOGGER.error("Not a valid signal to handle: %s", sig)

    if signum != -1:
        _LOGGER.debug('Setting handler for signal: %d (%s)', signum, SIGNALS[signum])
        signal.signal(signum, function)


def mask_all():
    """
    Set the default signal handler for all signals.

    @return
    """
    for i in xrange(1, len(SIGNALS)):
        if i != 9 and i != 19 and i != 32 and i != 33:
            _LOGGER.debug('Setting default handler for signal: %d (%s)', i, SIGNALS[i])
            signal.signal(i, default_handler)
