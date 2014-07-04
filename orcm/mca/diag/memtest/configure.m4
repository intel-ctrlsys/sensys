dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_diag_memtest_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_diag_memtest_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/diag/memtest/Makefile])

    AC_ARG_WITH([memtest],
                [AC_HELP_STRING([--with-memtest],
                                [Build memtest support (default: no)])],
	                        [], with_memtest=no)

    # do not build if support not requested
    AS_IF([test "$with_memtest" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([sysinfo support in memtest is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
