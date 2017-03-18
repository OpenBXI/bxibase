#!/bin/bash

BUILD_DIR=${TMPDIR}/build
wget https://github.com/OpenBXI/backtrace/archive/1.2.0-Bull.6.0.tar.gz && \
tar xvf 1.2.0-Bull.6.0.tar.gz -C ${TMPDIR} && \
mkdir -p ${BUILD_DIR} && \
cd ${BUILD_DIR}; && \
/tmp/backtrace*/bootstrap.sh && \
/tmp/backtrace*/configure  && \
make -j && \
make install
