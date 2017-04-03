#!/bin/env python
# file "example_build.py"

# Note: we instantiate the same 'cffi.FFI' class as in the previous
# example, but call the result 'ffibuilder' now instead of 'ffi';
# this is to avoid confusion with the other 'ffi' object you get below

import importlib
from cffi import FFI
import argparse as posless
ffibuilder = FFI()



if __name__ == "__main__":
    parser = posless.ArgumentParser(description='CFFI Compiler')
    parser.add_argument("cdef", type=str,
                        help="Module containing the CDEF")
    parser.add_argument("includes", type=str,
                        help="file with the list of include")
    parser.add_argument("library", type=str,
                        help="name of the library")
    parser.add_argument("output", type=str,
                        help="name of the output library")
    args = parser.parse_args()

    includes = open(args.includes, 'r').read()
    ffibuilder.set_source(args.output,
                          #includes,
                          None,
                          libraries=[args.library])   # or a list of libraries to link with
    # (more arguments like setup.py's Extension class:
    # include_dirs=[..], extra_objects=[..], and so on)
    cdef = importlib.import_module(args.cdef)
    ffibuilder.cdef(cdef.C_DEF)
    ffibuilder.compile(verbose=True)
