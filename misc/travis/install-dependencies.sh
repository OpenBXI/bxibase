#!/bin/bash

TMPDIR=${TMPDIR:-/tmp}

cat /proc/cpuinfo

wget https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz && \
    tar xvf zeromq-* -C ${TMPDIR} && \
    cd ${TMPDIR}/zeromq* && \
    ./configure && \
    make && \
    sudo make install && \
    wget https://github.com/OpenBXI/backtrace/releases/download/1.2.0-Bull.6.0/backtrace-1.2.0.tar.gz && \
    tar xvf backtrace-1.2.0.tar.gz -C ${TMPDIR} && \
    /tmp/backtrace-1.2.0/configure  && \
    make  && \
    sudo make install && \
    sudo ldconfig

