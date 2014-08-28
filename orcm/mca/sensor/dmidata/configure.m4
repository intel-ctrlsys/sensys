dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_dmidata_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_dmidata_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/dmidata/Makefile])

    AC_ARG_WITH([dmidata],
                [AC_HELP_STRING([--with-dmidata],
                                [Build dmidata support (default: no)])],
	                        [], with_dmidata=no)

    # do not build if support not requested
    AS_IF([test "$with_dmidata" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([DMI Data Read was requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
