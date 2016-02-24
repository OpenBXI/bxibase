#!/usr/bin/env python
import bxi.base.log as bxilog
import multiprocessing
import os,sys

# [BXI Log and Python multiprocessing module]
@bxilog.multiprocessing_target
def f():
    # Using the bxilog.multiprocessing_target decorator guarantees that 
    # uncaught exception will be reported by the logging system
    # and that the logging system will be cleanup properly (and therefore
    # flushed).
    bxilog.out("In subprocess")

    try: 
        raise ValueError("An expected exception in subprocess")
    except:
        bxilog.exception("Handling an exception in subprocess")

    # This will be catched thanks to the bxilog.multiprocessing_target() decorator
    # Otherwise, the exception will appear on the standard error as usual
    raise ValueError("An unexpected exception in subprocess")
# [BXI Log and Python multiprocessing module]

if __name__ == '__main__':
    filename = os.path.basename(sys.argv[0]) + '.bxilog'
    if os.path.exists(filename):
        os.unlink(filename)
    
    bxilog.basicConfig(filename=filename, filemode='w')
    p = multiprocessing.Process(target=f)
    p.start()
    p.join()
    
    with open(filename) as f:
        lines = f.readlines()
        assert lines[-1].endswith("An unexpected exception in subprocess\n")