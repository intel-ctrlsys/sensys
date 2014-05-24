dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_db_odbc_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_odbc_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/odbc/Makefile])

    AC_ARG_WITH([odbc],
                [AC_HELP_STRING([--with-odbc],
                                [Build odbc support (default: no)])],
	                        [], with_odbc=no)

    # do not build if support not requested
    AS_IF([test "$with_odbc" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([ODBC was requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
