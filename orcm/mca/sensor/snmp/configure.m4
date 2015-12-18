dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2015      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_snmp_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_snmp_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/snmp/Makefile])

    AC_ARG_WITH([snmp],
                [AC_HELP_STRING([--with-snmp],
                                [Build snmp support (default: no)])],
                                                         [], with_snmp=no)

    # do not build if support not requested
    AS_IF([test "$with_snmp" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([SNMP device sensor data were requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
