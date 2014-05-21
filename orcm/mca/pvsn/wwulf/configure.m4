dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_pvsn_wwulf_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_pvsn_wwulf_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/pvsn/wwulf/Makefile])

    AC_ARG_WITH([warewulf],
                [AC_HELP_STRING([--with-warewulf],
                                [Build Warewulf support (default: no)])],
	                        [], with_warewulf=no)

    # do not build if support not requested
    AS_IF([test "$with_warewulf" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([Warewulf support was requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
