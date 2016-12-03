dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2016      Intel Corporation. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_ipmi_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_ipmi_ts_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/ipmi_ts/Makefile])

    AC_ARG_WITH([ipmi],
                [AC_HELP_STRING([--with-ipmi],
                                [Build ipmi support (default: no)])],
                                [], with_ipmi=no)

    # do not build if support not requested
    AS_IF([test "$with_ipmi" != "no"],
          [$1],
          [$1])

    AC_SUBST(sensor_ipmi_ts_CPPFLAGS)
    AC_SUBST(sensor_ipmi_ts_LDFLAGS)
    AC_SUBST(sensor_ipmi_ts_LIBS)
])dnl
