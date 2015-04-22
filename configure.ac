AC_PREREQ([2.63])
AC_CONFIG_MACRO_DIR([.autotools_cache/m4])
AC_CONFIG_AUX_DIR([.autotools_cache])
m4_include([.autotools_cache/m4/common.m4])

define([BXIMODULE], bxibase)

AC_PACKAGE([BXIMODULE])
AC_INIT(BXIMODULE, [BXIMODULE_VERSION(BXIMODULE)])
REPLACE_BRIEF(BXIMODULE)
INIT_BXIPRODUCT(BXIMODULE)


BXI_CHECK_C_COMPILER
AC_PROG_LIBTOOL

AC_CHECK_LIB([m], [floor], [],
             [AC_MSG_ERROR([Could not find math library])])
AC_CHECK_LIB([pthread], [pthread_create], [],
             [AC_MSG_ERROR([Could not find pthread library])])
AC_CHECK_LIB([rt], [clock_gettime], [],
             [AC_MSG_ERROR([Could not find rt library])])
AC_CHECK_LIB([backtrace], [backtrace_full], [], 
             [AC_MSG_ERROR([Could not find backtrace library])])


PKG_CHECK_MODULES([ZMQ], [libzmq >= 3.0.0], [],
                  [
                   AC_CHECK_LIB([zmq], [zmq_msg_init], [], [AC_MSG_ERROR([Could not find zmq library])])
                  ])

LDFLAGS+=" $ZMQ_LIBS $BACKTRACE_LIBS "

DEFAULT_BXIOPTION

FLAGS_BXITEST


# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([gethostname memset mkdir pow strdup strtol])

#TODO add the pc.in file
AC_CONFIG_FILES([Makefile packaged/Makefile packaged/lib/Makefile packaged/include/Makefile packaged/doc/Makefile tests/Makefile packaged/src/version.c packaged/include/bxi/base/version.h])

AM_PATH_PYTHON
PC_INIT([2.7], [2.99])
#PC_PYTHON_CHECK_MODULE([pyparsing], [], [AC_MSG_ERROR(Module not found)])
PC_PYTHON_CHECK_MODULE([cffi], [], [AC_MSG_ERROR(Module not found)])

AC_OUTPUT


DISPLAY_CONF(BXIMODULE)
