#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# Author: Alain Cady <alain.cady@atos.net>
# Contributors: Sébastien Miquée <sebastien.miquee@atos.net>
###############################################################################
# Copyright (C) 2012 - 2016  Bull S. A. S.  -  All rights reserved
# Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois
# This is not Free or Open Source software.
# Please contact Bull S. A. S. for details about its license.
###############################################################################


from configobj import ConfigObjError
from configobj import ConfigObj
from gettext import gettext as _
import bxi.base.log as logging
import bxi.base.log.console_handler as bxilog_consolehandler
import bxi.base.log.file_handler as bxilog_filehandler
import configobj
import bxi.base.posless as posless
import tempfile
import os.path
import sys
import bxi.base.err as bxierr

ArgumentError = posless.ArgumentError
ArgumentTypeError = posless.ArgumentTypeError
FileType = posless.FileType
HelpFormatter = posless.HelpFormatter
ArgumentDefaultsHelpFormatter = posless.ArgumentDefaultsHelpFormatter
RawDescriptionHelpFormatter = posless.RawDescriptionHelpFormatter
RawTextHelpFormatter = posless.RawTextHelpFormatter
Namespace = posless.Namespace
Action = posless.Action
ONE_OR_MORE = posless.ONE_OR_MORE
OPTIONAL = posless.OPTIONAL
PARSER = posless.PARSER
REMAINDER = posless.REMAINDER
SUPPRESS = posless.SUPPRESS
ZERO_OR_MORE = posless.ZERO_OR_MORE

DEFAULT_CONFIG_FILENAME = 'default.conf'
DEFAULT_CONFIG_DIRNAME = 'bxi'
DEFAULT_CONFIG_SUFFIX = '.conf'

DEFAULT_CONSOLE_COLORS = '216_dark'
DEFAULT_CONSOLE_FILTERS = ':output'

BXILOG_DEFAULT_CONFIGFILE = 'bxilog.default.configfile'


def get_default_logfile():
    return '%s%s%s.bxilog' % (tempfile.gettempdir(),
                              os.path.sep,
                              os.path.basename(sys.argv[0]))


class LogLevelsAction(posless.Action):

    def __init__(self,
                 option_strings,
                 dest,
                 default=None,
                 GroupTitle=None,
                 mustbeprinted=True,
                 required=False,
                 help=None,
                 nargs=0,
                 setsighandler=True,
                 metavar=None):

        super(LogLevelsAction, self).__init__(option_strings=option_strings,
                                              dest=dest,
                                              nargs=0,
                                              GroupTitle=GroupTitle,
                                              mustbeprinted=mustbeprinted,
                                              const=True,
                                              default=default,
                                              required=required,
                                              help=help)

    def __call__(self, parser, namespace, values, option_string=None):
        names = list(logging.get_all_level_names_iter())
        for i in xrange(len(names)):
            print("%s = %s" % (i, names[i]))
        sys.exit(0)


@staticmethod
def _defaultfiltering(action):
    if action.mustbeprinted:
        return True
    return False


class FilteredHelpFormatter(posless.HelpFormatter):

    getfilterfunction = _defaultfiltering

    def __init__(self,
                 prog,
                 indent_increment=2,
                 max_help_position=24,
                 width=None):
        super(FilteredHelpFormatter, self).__init__(prog,
                                                    indent_increment=indent_increment,
                                                    max_help_position=max_help_position,
                                                    width=width)

    def add_argument(self, action, namespace):
        isactionprinted = FilteredHelpFormatter.getfilterfunction
        if isactionprinted(action):
            super(FilteredHelpFormatter, self).add_argument(action, namespace)


class HelpActionFormatted(posless._HelpAction):

    def __call__(self, parser, namespace, values,
                 option_string=None, filter_function=_defaultfiltering):
        parser.formatter_class = FilteredHelpFormatter
        parser.formatter_class.getfilterfunction = filter_function
        parser.print_help(namespace=namespace)
        parser.exit()


@staticmethod
def _logfiltering(action):
    if action.GroupTitle is None or action.GroupTitle == 'BXI Log options':
        return True
    return False


class _LoggedHelpAction(HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_LoggedHelpAction, self).__call__(parser, namespace, values,
                                                option_string,
                                                filter_function=_logfiltering)


@staticmethod
def _fullfiltering(action):
    return True


class _FullHelpAction(HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_FullHelpAction, self).__call__(parser, namespace, values,
                                              option_string,
                                              filter_function=_fullfiltering)


def _get_config_from_file(filename):
    with open(filename, 'r') as f:
        config = ConfigObj(f, interpolation=False)

        if 'includes' not in config or config['includes'] is None:
            return config

        for include_filename in config['includes']:
            included = _get_config_from_file(include_filename)
            included.merge(config)

        return included


def _add_config(parser,
                default_config_dirname,
                default_config_filename,
                cmd_config_file_suffix):

    if os.getuid() == 0:
        config_dir_prefix = '/etc/'
    else:
        config_dir_prefix = os.path.join(os.path.expanduser('~'), '.config')

    full_config_dir = os.path.join(config_dir_prefix, default_config_dirname)
    full_config_dir = os.getenv('BXICONFIGDIR', full_config_dir)
    cmd_config = os.path.join(full_config_dir,
                              os.path.basename(sys.argv[0]) + cmd_config_file_suffix)
    if not os.path.exists(cmd_config):
        cmd_config = os.path.join(full_config_dir, default_config_filename)

    def _add_config_file_arg(target_parser):
        group = target_parser.add_argument_group('Configuration directory')
        group.add_argument("--config-directory",
                           help="Configuration directory. Default: %(default)s"
                           ", environment variable: %(envvar)s",
                           default=full_config_dir,
                           envvar="BXICONFIGDIR",
                           metavar="DIR")

        group = target_parser.add_argument_group('Configuration file')
        group.add_argument("-C", "--config-file",
                           help="configuration file %(default)s"
                           ", environment variable: %(envvar)s",
                           default=cmd_config,
                           envvar="BXICONFIGFILE",
                           metavar="FILE")

    dummy = posless.ArgumentParser(add_help=False)
    _add_config_file_arg(dummy)
    _add_config_file_arg(parser)

    known_args = dummy.parse_known_args()[0]

    if os.path.exists(known_args.config_file):
        try:
            parser.config = _get_config_from_file(known_args.config_file)
        except IOError as err:
            sys.stderr.write('Configuration file error: %s' % err)
            logging.cleanup()
            sys.exit(1)
        except ConfigObjError as err:
            sys.stderr.write('Configuration file parsing error: %s' % err)
            logging.cleanup()
            sys.exit(2)
    else:
        parser.config = ConfigObj()


def _configure_log(parser):
    def _add_common(target_parser):
        group = target_parser.add_argument_group('BXI Log options')
        group.add_argument("--loglevels",
                           mustbeprinted=False,
                           action=LogLevelsAction,
                           help="displays all log levels and exit.")
        group.add_argument("--output-default-logcfg",
                           action='store_true',
                           mustbeprinted=False,
                           help="Output the default logging configuration.")

        default_logcfgfile = getdefaultvalue(target_parser,
                                             ['Defaults'],
                                             BXILOG_DEFAULT_CONFIGFILE,
                                             None)
        group.add_argument("--logcfgfile", type=posless.FileType('r'),
                           mustbeprinted=False,
                           metavar='logcfgfile',
                           default=default_logcfgfile,
                           help="Logging configuration file "
                           "(overridden by other logging options).")
        return group

    def _add_others(target_parser, args, group):
        if args.logcfgfile is None:
            default = DEFAULT_CONSOLE_FILTERS
        else:
            default = None

        group.add_argument("-l", "--console_filters",
                           mustbeprinted=False,
                           metavar='console_filters',
                           envvar='BXILOG_CONSOLE_FILTERS',
                           default=default,
                           help="define the logging filters for the console handler "
                                "of the default logging configuration. "
                                "If set, the default logging configuration is used. "
                                "Default: '%(default)s'. "
                                "Logging filters are defined by the following format: "
                                "logger_name_prefix:level[,prefix:level]*")
 
        if args.logcfgfile is None:
            default = bxilog_filehandler.FILTERS_AUTO
        else:
            default = None

        group.add_argument("--file_filters",
                           metavar='file_filters',
                           mustbeprinted=False,
                           envvar='BXILOG_FILE_FILTERS',
                           default=default,
                           help="define the logging filters for the file handler "
                                "of the default logging configuration. "
                                "If set to '%s', " % bxilog_filehandler.FILTERS_AUTO +
                                "filters are automatically computed to "
                                "provide two levels more details than console_filters and "
                                "at least error levels and above. "
                                "The format is the one defined by console_filters option. "
                                "If set, the default logging configuration is used. "
                                "Default: '%(default)s'. ")

        if args.logcfgfile is None:
            default = get_default_logfile()
        else:
            default = None

        group.add_argument("--logfile",
                           metavar='FILE',
                           envvar='BXILOGFILE',
                           mustbeprinted=False,
                           default=default,
                           help="define the destination file for the file handler "
                                "of the default logging configuration"
                                "If set, the default logging configuration is used. "
                                "Default: %(default)s")

    dummy = posless.ArgumentParser(add_help=False)
    group1 = _add_common(dummy)
    group = _add_common(parser)

    known_args = dummy.parse_known_args()[0]
    _add_others(dummy, known_args, group1)
    _add_others(parser, known_args, group)

    known_args = dummy.parse_known_args()[0]

    logging.captureWarnings(True)

    # Create the default logging configuration
    console_filters = known_args.console_filters if known_args.console_filters is not None else DEFAULT_CONSOLE_FILTERS
    file_filters = known_args.file_filters if known_args.file_filters is not None else bxilog_filehandler.FILTERS_AUTO
    logfile = known_args.logfile if known_args.logfile is not None else get_default_logfile()
    baseconf = {'handlers': ['console', 'file'],
                'setsighandler': True,
                'console': {'module': bxilog_consolehandler.__name__,
                            'filters': console_filters,
                            'stderr_level': 'WARNING',
                            'colors': '216_dark',
                            },
                'file': {'module': bxilog_filehandler.__name__,
                         'filters': file_filters,
                         'file': logfile,
                         'append': True,
                         }
                }

    if known_args.output_default_logcfg:
        config = configobj.ConfigObj(infile=baseconf)
        for l in config.write():
            print(l)
    # If at least one of other options has been set, it overrides the 
    # logging configuration file
    if known_args.logcfgfile is None or None in [known_args.console_filters,
                                                 known_args.file_filters,
                                                 known_args.logfile]:

        config = configobj.ConfigObj(infile=baseconf)
    else:
        config = configobj.ConfigObj(infile=known_args.logcfgfile)

    logging.set_config(config)

    parser.add_argument('--help-logs',
                        action=_LoggedHelpAction,
                        default=posless.SUPPRESS,
                        help=_('show detailed logging options and exit'))


def addargs(parser,
            config_dirname=DEFAULT_CONFIG_DIRNAME,
            config_filename=DEFAULT_CONFIG_FILENAME,
            filename_suffix=DEFAULT_CONFIG_SUFFIX,
            setsighandler=True):
    _add_config(parser, config_dirname, config_filename, filename_suffix)
    _configure_log(parser)

    parser.add_argument('--help-full',
                        action=_FullHelpAction, default=posless.SUPPRESS,
                        help=_('show all options and exit'))


def getdefaultvalue(parser, Sections, value, _LOGGER, default=None, config=None):
    def _return_dict_value(config, Sections):
        try:
            if len(Sections) > 1:
                return _return_dict_value(config.get(Sections[0]), Sections[1:])
            else:
                return config.get(Sections[0])
        except bxierr.BXIError as e:
            raise bxierr.BXIError("[%s]%s" % (Sections[0], e.msg))
        except:
            if _LOGGER is not None:
                _LOGGER.exception("Initial error:", level=logging.DEBUG)
            raise bxierr.BXIError("[%s]" % Sections[0])

    try:
        if config is None:
            config = parser.config
    except AttributeError:
        if _LOGGER is not None:
            _LOGGER.exception("No configuration provided."
                              " Using %s as default for value %s from section %s",
                              default, value, Sections, level=logging.DEBUG)
        return default

    try:
        dictonary = _return_dict_value(config, Sections)
    except bxierr.BXIError as e:
        if _LOGGER is not None:
            _LOGGER.debug("Provided configuration (config) doesn't include the (sub)section config%s."
                          " using %s as default for value %s from (sub)section [%s]", e.msg,
                          default, value, ']['.join(Sections))
        return default

    try:
        if _LOGGER is not None:
            _LOGGER.debug("Return %s for value %s in section %s",
                          dictonary[value], value, Sections)
        return dictonary[value]
    except (KeyError, TypeError, AttributeError):
        if _LOGGER is not None:
            _LOGGER.exception("No value %s in the (sub)sections [%s]."
                              " using %s as default for value %s from (sub)section [%s]",
                              value, ']['.join(Sections),
                              default, value, ']['.join(Sections), level=logging.DEBUG)
        return default
