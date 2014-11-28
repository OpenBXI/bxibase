#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
define([BXIMODULE], bxibase)
AC_PACKAGE([BXIMODULE])
define([BXIMODULE_VERSION],[esyscmd([awk '/^%define version/ {print }' $1.spec | sed 's/.* //' | tr -d '\n'])])
AC_INIT([BXIMODULE], [BXIMODULE_VERSION(BXIMODULE)])
AC_CONFIG_AUX_DIR([.autotools_cache])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIR([.autotools_cache/m4])
AC_CONFIG_HEADERS([template_config.h])

# Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
    AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
fi
AC_CHECK_PROGS([DOT], [dot])
if test -z "$DOT"; then
    AC_MSG_WARN([dot (graphviz) not found - continuing without dot support])
fi
AC_CHECK_PROGS([SVNVERSION], [svnversion])
AC_SUBST([SVNVERSION])

#check the POSIX conformity
AC_EGREP_CPP(posix_200809L_supported,
             [#define _POSIX_C_SOURCE 200809L
              #include <unistd.h>
              #ifdef _POSIX_VERSION
              #if _POSIX_VERSION == 200809L
              posix_200809L_supported
              #endif
              #endif
              ],
              [],
              [AC_MSG_FAILURE([*** Implementation must conform to the POSIX.1-2008 standard.])]
              )


AC_CHECK_HEADER([stdbool.h],
                [],
                [AC_MSG_ERROR([The bxi divio header has not been found, the bxi header package should be installed])]
                )


# Checks for typedefs, structures, and compiler characteristics.


#don't use #AC_CHECK_HEADER_STDBOOL it's 2.69 autoconf
#AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_PROG_MKDIR_P
AC_PROG_INSTALL

###########################
# debug
###########################
AC_ARG_ENABLE([debug], [AS_HELP_STRING([--disable-debug], [enable debugging, default: yes])])
if test x"$enable_debug" != "xno"; then
    CFLAGS="-g3 -O0 $CFLAGS";
fi

###########################
# gcov coverage reporting
###########################
AC_ARG_ENABLE([gcov], [AS_HELP_STRING([--enable-gcov], [use Gcov to test the test suite , default: no])],)
if test x"$enable_gcov" == "xyes"; then
    CFLAGS+=" --coverage"
    LDFLAGS+=" --coverage"
else
    enable_gcov=no
fi



###########################
# valgrind
###########################
run_valgrind="no"
AC_ARG_ENABLE([valgrind], [AS_HELP_STRING([--enable-valgrind[=args]], 
                                            [use Valgrind to test the test suite])])
if test x"$enable_valgrind" != "xno"; then
    AC_CHECK_PROG(VALGRIND_CHECK,valgrind,yes)
    if test x"$VALGRIND_CHECK" == "xyes"; then
        if test x"$enable_valgrind" == "xyes"; then
            enable_valgrind=""
        fi
    else
        AC_MSG_ERROR([Please install valgrind before installing.])
    fi

    run_valgrind="yes"
    VALGRIND="libtool --mode=execute valgrind --quiet"
    AC_SUBST(VALGRIND)

fi
if test x"$enable_valgrind" == "xno"; then
    enable_valgrind=""
fi

VALGRIND_ARGS=" $enable_valgrind  --track-origins=yes --leak-check=full --show-reachable=yes --undef-value-errors=yes --suppressions=valgrind.supp "
AC_SUBST(VALGRIND_ARGS)


################################ Internal dependencies ###########################
## Checks for libraries.
#AC_CHECK_LIB([bxibb], [bxibb_server_new],[],
#             [AC_MSG_ERROR([Could not find bxibb library])])
#AC_CHECK_LIB([bxi], [bximisc_get_time],[],
#             [AC_MSG_ERROR([Could not find bxi library])])


################################ link dependences ################################

#done below with PKG
#AC_CHECK_LIB([zmq], [zmq_init], [],
#             [AC_MSG_ERROR([Could not find zmq library])])
#AC_CHECK_LIB([czmq], [zsocket_new], [],
#             [AC_MSG_ERROR([Could not find czmq library])])

AC_CHECK_LIB([m], [floor], [],
             [AC_MSG_ERROR([Could not find math library])])
AC_CHECK_LIB([pthread], [pthread_create], [],
             [AC_MSG_ERROR([Could not find pthread library])])
AC_CHECK_LIB([rt], [clock_gettime], [],
             [AC_MSG_ERROR([Could not find rt library])])

PKG_CHECK_MODULES([ZMQ], [libzmq >= 3.0.0], [],
                  [
                   AC_CHECK_LIB([zmq], [zmq_msg_init], [], [AC_MSG_ERROR([Could not find zmq library])])
                  ])
PKG_CHECK_MODULES([CZMQ], [libczmq >= 1.0.0], [],
                  [AC_CHECK_LIB([czmq], [zsocket_new], [], [AC_MSG_ERROR([Could not find czmq library])])])

                  #AC_SUBST([ZMQ_CFLAGS])
                  #AC_SUBST([ZMQ_LDFLAGS])
                  #AC_SUBST([CZMQ_CFLAGS])
                  #AC_SUBST([CZMQ_LDFLAGS])
                  #AC_SUBST([UUID_CFLAGS])
                  #AC_SUBST([UUID_LDFLAGS])



LDFLAGS+=" $ZMQ_LIBS "


#
#
##-----------------
## Switch Simulator
##-----------------
#OpenMPI>=1.4.3
#Net-SNMP==5.7
#
#

###########################
# warnings
###########################
AC_ARG_ENABLE([doc], [AS_HELP_STRING([--disable-doc], [disable the generation of the documentation])])
if test x"$enable_doc" != "xno" ; then
    if ! test -z "$DOXYGEN";
    then
        if ! test -z "$DOT";
        then
            ENABLE_DOC="yes"
            AC_CONFIG_FILES([packaged/doc/Doxyfile])
        fi
    fi
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test x"$ENABLE_DOC" == "xyes"])
if test x"$ENABLE_DOC" != "xyes"
then
    ENABLE_DOC = no
fi

AC_ARG_ENABLE([mode-maintaners], [AS_HELP_STRING([--enable-mode-maintaners])])
 if test x"$enable_mode_maintaners" != "xno" ; then
     CFLAGS+=" -Wall -Werror  -Wextra -Wconversion"
 fi


################################ testing dependencies ############################
OLD_CLFAGS=${CFLAGS}
OLD_LDLFAGS=${LDFLAGS}
OLD_LIBS=${LIBS}
#build flags specifical for test

AC_SEARCH_LIBS([CU_register_suites], [cunit])

TST_CFLAGS=${CFLAGS}
TST_LDFLAGS=${LDFLAGS}
TST_LIBS=${LIBS}

#does the substitution inside the makefile
AC_SUBST(TST_CFLAGS)
AC_SUBST(TST_LDFLAGS)
AC_SUBST(TST_LIBS)

CFLAGS=${OLD_CLFAGS}
LDFLAGS=${OLD_LDLFAGS}
LIBS=${OLD_LIBS}


# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([gethostname memset mkdir pow strdup strtol])

AM_PATH_PYTHON


#TODO add the pc.in file
AC_CONFIG_FILES([Makefile packaged/Makefile packaged/lib/Makefile packaged/include/Makefile packaged/doc/Makefile tests/Makefile])
AC_OUTPUT

echo "summary   :"
echo "          MODULE          : "BXIMODULE
echo "          VERSION         : "BXIMODULE_VERSION(BXIMODULE)
echo "          CC              : ${CC}"
echo "          CFLAGS          : ${CFLAGS}"
echo "          LDFLAGS         : ${LDFLAGS} ${LIBS}"
echo
echo "          ENABLE_DOC      : ${ENABLE_DOC}"
echo "          enable valgrind : $run_valgrind"
echo "          VALGRIND        : $VALGRIND"
echo "          GCOV            : ${enable_gcov}"
echo
echo
echo "          python site dir : "${pythondir}

