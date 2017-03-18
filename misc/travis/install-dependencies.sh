#!/bin/bash

TMPDIR=${TMPDIR:-/tmp}
CPUS_NB=$(($(grep -c ^processor /proc/cpuinfo)+1))

wget https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz && \
    tar xvf zeromq-* -C ${TMPDIR} && \
    cd ${TMPDIR}/zeromq* && \
    ./configure && \
    make -j${CPUS_NB} && \
    sudo make -j${CPUS_NB} install && \
    wget https://github.com/OpenBXI/backtrace/releases/download/1.2.0-Bull.6.0/backtrace-1.2.0.tar.gz && \
    tar xvf backtrace-1.2.0.tar.gz -C ${TMPDIR} && \
    cd ${TMPDIR}/backtrace-1.2.0 && \
    ./configure  && \
    make  && \
    sudo make -j${CPUS_NB} install && \
    sudo ldconfig

