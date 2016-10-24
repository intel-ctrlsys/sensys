dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel Corporation. All rights reserved.
dnl Copyright (c) 2016      Intel Corporation. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_mcedata_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_mcedata_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/mcedata/Makefile])

    AC_ARG_WITH([mcedata],
                [AC_HELP_STRING([--with-mcedata],
                                [Build mcedata support (default: no)])],
	                        [], with_mcedata=no)

    # do not build if support not requested
    AS_IF([test "$with_mcedata" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([MCE error/event collection requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
