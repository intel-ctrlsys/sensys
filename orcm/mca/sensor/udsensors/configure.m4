dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2016      Intel Corporation. All rights reserved.
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
    AC_ARG_WITH([rand_generator],
                [AC_HELP_STRING([--with-rand-generator],
                                [Build rand_generator sensor plugin support (default: no)])],
                                [], with_rand_generator=no)

    AM_CONDITIONAL([WITH_RAND_GENERATOR], [test "$with_rand_generator" = "yes"])

    # do not build if support not requested
    AS_IF([test "$with_udsensors" != "no"],
          [$1],
          [$2])
])dnl
