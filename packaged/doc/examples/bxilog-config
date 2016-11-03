#!/usr/bin/env python
import bxi.base.log as bxilog
import sys
import configobj

if __name__ == '__main__':
    
    conf = {'handlers': ['console', 'syslog', 'trace', 'history'], 
            'setsighandler': True,  # Catch signals automagically
            'console':{'module': 'bxi.base.log.console_handler',
                       'filters': ':output,history:off',
                       },
            'syslog':{'module': 'bxi.base.log.syslog_handler',
                      'filters': ':warning,history:off',
                      'facility': 'LOG_LOCAL7'
                      },
            'trace':{'module': 'bxi.base.log.file_handler',
                     'filters': ':trace,history:off',
                     'path': '/tmp/trace.log',
                     'append': False},
            'history':{'module': 'bxi.base.log.file_handler',
                       'filters': ':off,history:lowest',
                       'path': '/tmp/history.log',
                       'append': False},
            }
    bxilog.set_config(configobj.ConfigObj(conf))
    
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