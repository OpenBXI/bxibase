#!/bin/bash

TMPDIR=${TMPDIR:-/tmp}
BUILD_DIR=${TMPDIR}/build

wget https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz && \
    tar xvf zeromq-* -C ${TMPDIR} && \
    cd ${TMPDIR}/zeromq* && \
    ./configure && \
    make && \
    sudo make install && \
    wget https://github.com/OpenBXI/backtrace/archive/1.2.0-Bull.6.0.tar.gz && \
    tar xvf 1.2.0-Bull.6.0.tar.gz -C ${TMPDIR} && \
    mkdir -p ${BUILD_DIR} && \
    cd ${BUILD_DIR} && \
    /tmp/backtrace*/bootstrap.sh && \
    /tmp/backtrace*/configure  && \
    make  && \
    sudo make install && \
    sudo ldconfig

