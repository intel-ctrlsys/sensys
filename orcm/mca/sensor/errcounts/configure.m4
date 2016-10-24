dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2015      Intel Corporation. All rights reserved.
dnl Copyright (c) 2016      Intel Corporation. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_errcounts_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_errcounts_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/errcounts/Makefile])

    AC_ARG_WITH([errcounts],
                [AC_HELP_STRING([--with-errcounts],
                                [Build errcounts support (default: no)])],
	                        [], with_errcounts=no)

    # do not build if support not requested
    AS_IF([test "$with_errcounts" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([ECC correctable and uncorrectable error counts were requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
