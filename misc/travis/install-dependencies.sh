#!/bin/bash

wget https://github.com/OpenBXI/backtrace/archive/1.2.0-Bull.6.0.tar.gz
tar xvf 1.2.0-Bull.6.0.tar.gz
cd backtrace*
./boostrap.sh;./configure;make -j;make install
