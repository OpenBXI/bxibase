#!/usr/bin/env python

import sys
import bxi.base.log as bxilog
import bxi.base.posless as posless
import bxi.base.parserconf as bxiparserconf

if __name__ == '__main__':
    parser = posless.ArgumentParser(description='BXI Log Posless Example',
                                   formatter_class=bxiparserconf.FilteredHelpFormatter)
    bxiparserconf.addargs(parser)
    args = parser.parse_args()
        
    one = bxilog.getLogger('one.logger')
    other = bxilog.getLogger('another.logger')
    history = bxilog.getLogger('history') 
    
    # This log will go only to the history according to the configuration
    history.out("This process just starts, arguments: %s", sys.argv)

    # All log below will be sent to all other handlers
    bxilog.out("Starting")
    one.info("Something")
    other.debug("Something else")
    
    one.error("Something wrong happened?")
    other.critical("And here, something even worse!")
    
    history.out("This process ends normally")