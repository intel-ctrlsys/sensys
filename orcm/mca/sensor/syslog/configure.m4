dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2015 Intel Corporation. All rights reserved.
dnl Copyright (c) 2016      Intel Corporation. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_syslog_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_syslog_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/syslog/Makefile])

    AC_ARG_WITH([syslog],
                [AC_HELP_STRING([--with-syslog],
                                [Build syslog support (default: no)])],
                                [], with_syslog=no)

    # do not build if support not requested
    AS_IF([test "$with_syslog" != "no"],
          [AS_IF([test ! -z "$with_syslog" -a "$with_syslog" != "yes"],
                 [orcm_check_syslog_dir="$with_syslog"])

           OPAL_CHECK_PACKAGE([sensor_syslog],
                              [ipmicmd.h],
                              [ipmiutil],
                              [-lcrypto],
                              [$orcm_check_syslog_dir],
                              [],
                              [$1],
                              [AC_MSG_WARN([SYSLOG SENSOR SUPPORT REQUESTED])
                               AC_MSG_WARN([BUT REQUIRED LIBRARY OR HEADER NOT FOUND])
                               AC_MSG_ERROR([CANNOT CONTINUE])
                               $2])],
          [$2])
])dnl
