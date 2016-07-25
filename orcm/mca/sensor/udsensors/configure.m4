dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2016      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_udsensors_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_udsensors_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/udsensors/Makefile])

    AC_ARG_WITH([udsensors],
                [AC_HELP_STRING([--with-udsensors],
                                [Build udsensors support (default: no)])],
                                [], with_udsensors=no)

    # do not build if support not requested
    AS_IF([test "$with_udsensors" != "no"],
          [$1],
          [$2])
])dnl
