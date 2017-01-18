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


DEFAULT_CONFIG_DIRNAME = 'bxi'
DEFAULT_CONFIG_SUFFIX = '.conf'
DEFAULT_CONFIG_FILENAME = 'default' + DEFAULT_CONFIG_SUFFIX

DEFAULT_CONSOLE_COLORS = '216_dark'
DEFAULT_CONSOLE_FILTERS = ':output'

BXILOG_DEFAULT_CONFIGFILE_KEY = 'bxilog.default.configfile'

_LOGGER = logging.get_logger(logging.LIB_PREFIX + 'bxi.base.parserconf')


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


def find_logconfigs(module_name, config):
    result = []
    if 'handlers' not in config:
        return result

    if isinstance(config['handlers'], basestring):
        handlers = [config['handlers']]
    else:
        handlers = config['handlers']

    for handler in handlers:
        section = config[handler]
        if 'module' not in section:
            continue
        if section['module'] == module_name:
            result.append(handler)
    return result


class _FullHelpAction(HelpActionFormatted):
    def __call__(self, parser, namespace, values, option_string=None):
        super(_FullHelpAction, self).__call__(parser, namespace, values,
                                              option_string,
                                              filter_function=_fullfiltering)


def _get_config_from_file(filename):
    with open(filename, 'r') as f:
        config = ConfigObj(f, interpolation=False)

        if 'include' not in config or config['include'] is None:
            return config

        if isinstance(config['include'], basestring):
            includes = [config['include']]
        else:
            includes = config['include']
        new_conf = ConfigObj()
        for include_filename in includes:
            if not os.path.isabs(include_filename):
                dir_ = os.path.dirname(filename)
                include_filename = os.path.join(dir_, include_filename)
            included = _get_config_from_file(include_filename)
            new_conf.merge(included)

        new_conf.merge(config)
        return new_conf


def _add_config(parser,
                default_config_dirname,
                default_config_filename,
                domain_name,
                cmd_config_file_suffix):

    if os.getuid() == 0:
        config_dir_prefix = '/etc/'
    else:
        config_dir_prefix = os.path.join(os.path.expanduser('~'), '.config')

    full_config_dir = os.path.join(config_dir_prefix, default_config_dirname)
    full_config_dir = os.getenv('BXICONFIGDIR', full_config_dir)

    # First, we try to fetch a configuration for the command line
    cmd_config = os.path.join(full_config_dir,
                              parser.prog + cmd_config_file_suffix)
    if not os.path.exists(cmd_config):
        # Second we try to fetch a configuration for the domain name
        if domain_name is not None:
            cmd_config = os.path.join(full_config_dir,
                                      domain_name + cmd_config_file_suffix)
        if not os.path.exists(cmd_config):
            # Third we try the default path
            cmd_config = os.path.join(full_config_dir, default_config_filename)

    def _add_config_dir_arg(target_parser, known_args=None):
        group = target_parser.add_argument_group('File Based Configuration'
                                                 ' (domain: %s)' % domain_name)

        if known_args is None:
            default_config_dir = None
            default_config_file = None
        else:
            # Check if the --config-file option has been given
            if known_args.config_file is not None:
                default_config_dir = None
                default_config_file = known_args.config_file
            else:
                if known_args.config_dir is None:
                    default_config_dir = full_config_dir
                    default_config_file = cmd_config
                else:
                    default_config_dir = known_args.config_dir
                    candidate = os.path.join(default_config_dir,
                                             os.path.basename(target_parser.prog) +
                                             cmd_config_file_suffix)
                    if not os.path.exists(candidate):
                        # Second we try to fetch a configuration for the domain name
                        if domain_name is not None:
                            candidate = os.path.join(default_config_dir,
                                                     domain_name +
                                                     cmd_config_file_suffix)
                        if not os.path.exists(candidate):
                            # Third we try the default path
                            candidate = os.path.join(default_config_dir,
                                                     default_config_filename)
                    default_config_file = candidate

        group.add_argument("--config-dir",
                           help="The directory where the configuration file "
                                "must be looked for."
                                " Value: %(default)s. "
                                "Environment variable: %(envvar)s",
                           default=default_config_dir,
                           envvar="BXICONFIGDIR",
                           metavar="DIR")

        group.add_argument("-C", "--config-file",
                           mustbeprinted=False,
                           help="The configuration file to use. If None, "
                           "the file is taken from the configuration directory. "
                           "Value: %(default)s. "
                           "Environment variable: %(envvar)s",
                           default=default_config_file,
                           envvar="BXICONFIGFILE",
                           metavar="FILE")
        target_parser.known_config_file = default_config_file

    dummy = posless.ArgumentParser(prog=parser.prog, add_help=False)
    _add_config_dir_arg(dummy)

    known_args = dummy.parse_known_args()[0]

    _add_config_dir_arg(parser, known_args)

    if os.path.exists(parser.known_config_file):
        try:
            parser.config = _get_config_from_file(parser.known_config_file)
        except IOError as err:
            parser.error('Configuration file error: %s' % err)
            logging.cleanup()
        except ConfigObjError as err:
            parser.error('Configuration file parsing error: %s' % err)
            logging.cleanup()
    else:
        parser.config = ConfigObj()


def _configure_log(parser):
    def _add_common(target_parser):
        # Warning: do not introduce --log-STUFF unless STUFF is actually the name
        # of a bxilog handler.
        group = target_parser.add_argument_group('BXI Log options')
        group.add_argument("--loglevels",
                           mustbeprinted=False,
                           action=LogLevelsAction,
                           help="Displays all log levels and exit.")

        group.add_argument("--logoutput-default-config",
                           dest='output_default_logcfg',
                           action='store_true',
                           mustbeprinted=False,
                           help="Output the default logging configuration and exit.")

        default_logcfgfile = getdefaultvalue(target_parser,
                                             ['Defaults'],
                                             BXILOG_DEFAULT_CONFIGFILE_KEY,
                                             None)
        group.add_argument("--logcfgfile",
                           mustbeprinted=False,
                           metavar='logcfgfile',
                           default=default_logcfgfile,
                           help="Logging configuration file. "
                           " Use '--logoutput-default-config' to customize one to your "
                           "own needs. Value: %(default)s.")
        return group

    def _add_others(target_parser, args, group, config):
        sections = find_logconfigs(bxilog_consolehandler.__name__, config)
        if len(sections) > 1:
            target_parser.error("Multiple instances of module %s is "
                                "currently unsupported. Configuration: %s " %
                                (bxilog_consolehandler.__name__, config))
        if len(sections) == 0:
            console_handler = None
        else:
            console_handler = sections[0]
            console_section = config[console_handler]
            if 'filters' not in console_section:
                target_parser.error("Bad logging configuration: 'filters' is missing "
                                    "in section '%s' of config %s" % (console_handler,
                                                                      config))
            default = console_section['filters']
            group.add_argument("-l", "--log-%s-filters" % console_handler,
                               mustbeprinted=False,
                               metavar='log-%s-filters' % console_handler,
                               envvar='BXILOG_%s_FILTERS' % console_handler.upper(),
                               default=default,
                               help="Define the logging filters for the %s handler " %
                                    console_handler +
                                    "Value: '%(default)s'. "
                                    "Logging filters are defined by the following format: "
                                    "logger_name_prefix:level[,prefix:level]*")

            if 'colors' not in console_section:
                default = DEFAULT_CONSOLE_COLORS
            else:
                default = console_section['colors']

            group.add_argument("--log-%s-colors" % console_handler,
                               mustbeprinted=False,
                               metavar='log-%s-colors' % console_handler,
                               envvar='BXILOG_%s_COLORS' % console_handler.upper(),
                               default=default,
                               choices=bxilog_consolehandler.COLORS.keys(),
                               help="Define the logging colors for the %s handler " %
                                    console_handler +
                                    "Value: '%(default)s'. " +
                                    "choices=%s. " % bxilog_consolehandler.COLORS.keys())

        sections = find_logconfigs(bxilog_filehandler.__name__, config)
        for section in sections:
            conf = config[section]
            if 'filters' not in conf:
                target_parser.error("Bad logging configuration: 'filters' is missing "
                                    "in section '%s' of config %s" % (section, config))
            default = conf['filters']
            if console_handler is not None:
                auto_help_msg = "If set to '%s', " % bxilog_filehandler.FILTERS_AUTO + \
                                "filters are automatically computed to " + \
                                "provide two levels more details than handler '%s' " % \
                                console_handler + \
                                "and at least error levels and above. "
            else:
                auto_help_msg = ""
            group.add_argument("--log-%s-filters" % section,
                               metavar='log-%s-filters' % section,
                               mustbeprinted=False,
                               envvar='BXILOG_%s_FILTERS' % section.upper(),
                               default=default,
                               help="Define the logging filters for the "
                                    "%s handler " % section +
                                    "of the default logging configuration. " +
                                    auto_help_msg +
                                    "The format is the one defined by "
                                    "console_filters option. "
                                    "Value: '%(default)s'. ")
            if 'path' not in conf:
                target_parser.error("Bad logging configuration: 'path' is missing "
                                    "in section '%s' of config %s" % (section, config))
            default = conf['path']
            group.add_argument("--log-%s-path" % section,
                               metavar='PATH',
                               envvar='BXILOGPATH',
                               mustbeprinted=False,
                               default=default,
                               help="Define the destination file for the %s handler " %
                                    section + "Value: %(default)s")

    def _override_logconfig(config, known_args):
        def _override_kv(option, key, config, args):
            # option has the following format: --log-handler-key=value
            if option.endswith(key):
                handler_name = option[len('log_'):option.index(key) - 1]
                assert handler_name in config
                # replace in config
#                 print("Overriding: %s -> %s[%s]=%s" %
#                       (option, handler_name, key, args[option]))
                config[handler_name][key] = args[option]

        args = vars(known_args)
        for option in args:
            _override_kv(option, 'filters', config, args)
            _override_kv(option, 'colors', config, args)
            _override_kv(option, 'path', config, args)

    parser.add_argument('--help-logs',
                        action=_LoggedHelpAction,
                        default=posless.SUPPRESS,
                        help=_('Show detailed logging options and exit'))

    dummy = posless.ArgumentParser(prog=parser.prog, add_help=False)
    group1 = _add_common(dummy)
    group = _add_common(parser)
    
    known_args = dummy.parse_known_args()[0]

    baseconf = {'handlers': ['console', 'file'],
                'setsighandler': True,
                'console': {'module': bxilog_consolehandler.__name__,
                            'filters': ':output',
                            'stderr_level': 'WARNING',
                            'colors': '216_dark',
                            },
                'file': {'module': bxilog_filehandler.__name__,
                         'filters': 'auto',
                          'path': os.path.join(tempfile.gettempdir(),
                                               '%(prog)s') + '.bxilog',
                         'append': True,
                         }
                }

    if known_args.output_default_logcfg:
        config = configobj.ConfigObj(infile=baseconf, interpolation=False)
        for l in config.write():
            print(l)
        sys.exit(0)

    if known_args.logcfgfile is None:
        infile = parser.get_default('logcfgfile')
        if infile is None:
            # Default case: no logging configuration given and no file found by default
            infile = baseconf
            logcfg_msg = "No logging configuration file given, using default."
        else:
            if not os.path.isabs(infile):
                # Find the absolute path from config-file...
                config_dir = os.path.dirname(parser.known_config_file)
                infile = os.path.join(config_dir, infile)

            if not os.path.exists(infile):
                parser.error("Logging configuration file "
                             "not found: %s. " % infile +
                             "Check your configuration "
                             "from file: %s" % parser.known_config_file)
            logcfg_msg = "Using logging configuration file '%s' specified by '%s'" %\
                         (infile, parser.known_config_file)

        config = configobj.ConfigObj(infile=infile, interpolation=False)
    elif not os.path.exists(known_args.logcfgfile):
        dummy.error("File not found: %s" % known_args.logcfgfile)
    else:
        config = configobj.ConfigObj(infile=known_args.logcfgfile, interpolation=False)
        logcfg_msg = "Using logging configuration file '%s' specified by command line" %\
                     known_args.logcfgfile

    _add_others(dummy, known_args, group1, config)
    _add_others(parser, known_args, group, config)

    known_args = dummy.parse_known_args()[0]

#     print(config)
    _override_logconfig(config, known_args)
#     print(config)
    logging.captureWarnings(True)
    if logging.is_configured():
        logging.info("Reconfiguration of the logging system")
        logging.cleanup()
    logging.set_config(config)
    if parser.known_config_file is not None:
        _LOGGER.info("Configuration based on '%s': %s", parser.known_config_file, parser.config)
    else:
        _LOGGER.info("No configuration file found, using default values: %s", parser.config)
    _LOGGER.debug(logcfg_msg)


def addargs(parser,
            config_dirname=DEFAULT_CONFIG_DIRNAME,              # /etc/*bxi*
            config_filename=DEFAULT_CONFIG_FILENAME,            # /etc/bxi/*default.conf*
            domain_name=None,                                   # /etc/bxi/*domain*.conf
            filename_suffix=DEFAULT_CONFIG_SUFFIX,              # /etc/bxi/cmd*.conf*
            setsighandler=True):
    _add_config(parser, config_dirname, config_filename, domain_name, filename_suffix)
    _configure_log(parser)

    parser.add_argument('--help-full',
                        action=_FullHelpAction, default=posless.SUPPRESS,
                        help=_('Show all options and exit'))


def getdefaultvalue(parser, Sections, value, LOGGER, default=None, config=None):
    """return default value for a given cli parameter.

    value is searched for in the "config" configuration file,
    otherwise the parser configuration file,
    otherwise the default argument.
    The value searched for is identified by the couple (Sections, value)
    @param[in] parser to search conf from (used only if no config provided)
    @param[in] Sections sections where value is searched
    @param[in] value name under which value is stored in config
    @param[in] LOGGER where messages are logged
    @param[in] default default value in case search in configs fails
    @param[in] config provided config to search value from.


    """
    def _return_dict_value(config, Sections):
        try:
            if len(Sections) > 1:
                return _return_dict_value(config.get(Sections[0]), Sections[1:])
            else:
                return config.get(Sections[0])
        except bxierr.BXIError as e:
            raise bxierr.BXIError("[%s]%s" % (Sections[0], e.msg))
        except:
            if LOGGER is not None:
                LOGGER.exception("Initial error:", level=logging.DEBUG)
            raise bxierr.BXIError("[%s]" % Sections[0])

    try:
        if config is None:
            config = parser.config
    except AttributeError:
        if LOGGER is not None:
            LOGGER.exception("No configuration provided."
                              " Using %s as default for value %s from section %s",
                              default, value, Sections, level=logging.DEBUG)
#         else:
#             print("No configuration provided."
#                   " Using %s as default for value %s from section %s" %
#                   (default, value, Sections))
        return default

    try:
        dictonary = _return_dict_value(config, Sections)
    except bxierr.BXIError as e:
        if LOGGER is not None:
            LOGGER.debug("Provided configuration (config) doesn't include the (sub)section config%s."
                          " using %s as default for value %s from (sub)section [%s]", e.msg,
                          default, value, ']['.join(Sections))
        return default

    try:
        if LOGGER is not None:
            LOGGER.debug("Return %s for value %s in section %s",
                          dictonary[value], value, Sections)
        return dictonary[value]
    except (KeyError, TypeError, AttributeError):
        if LOGGER is not None:
            _LOGGER.exception("No value %s in the (sub)sections [%s]."
                              " using %s as default for value %s from (sub)section [%s]",
                              value, ']['.join(Sections),
                              default, value, ']['.join(Sections), level=logging.DEBUG)
        return default
