dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_diag_ethtest_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_diag_ethtest_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/diag/ethtest/Makefile])

    AC_ARG_WITH([ethtest],
                [AC_HELP_STRING([--with-ethtest],
                                [Build ethtest support (default: no)])],
	                        [], with_ethtest=no)

    # do not build if support not requested
    AS_IF([test "$with_ethtest" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([ethtool used in memtest is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
