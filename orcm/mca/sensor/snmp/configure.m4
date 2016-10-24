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

# MCA_sensor_snmp_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_snmp_CONFIG], [
    # Check if net-snmp support is available or not
    AC_CONFIG_FILES([orcm/mca/sensor/snmp/Makefile])
    AC_MSG_CHECKING([for SNMP Sensor support])
    AC_REQUIRE([OPAL_CHECK_NETSNMP])

   AC_ARG_WITH([snmp],
                [AC_HELP_STRING([--with-snmp(=yes/no)],
                                [Build SNMP sensor support])],
                [AC_MSG_RESULT([with_snmp selected with paramater "$with_snmp"!])]
                [AS_IF([test "$snmp_check_happy" = "no" -a "$with_snmp" = "yes"],
                        AC_MSG_RESULT([NET-SNMP Check failed and we need to skip building SNMP Sensor Component here])
                        AC_MSG_WARN([NET-SNMP Libs not present])
                        AC_MSG_ERROR([SNMP Sensor requested but dependency not met])
                        $2)]
                [AS_IF([test "$snmp_check_happy" = "yes" -a "$with_snmp" = "yes"],
                        AC_MSG_RESULT([NET-SNMP Check passed and SNMP sensor explicitly requested])
                        AC_MSG_RESULT([SNMP Sensor requested and dependency met and snmp sensor will be built])
                        $1)]
                [AS_IF([test "$snmp_check_happy" = "yes" -a "$with_snmp" = "no"],
                        AC_MSG_RESULT([NET-SNMP Check passed and but SNMP sensor explicitly not requested])
                        AC_MSG_RESULT([NET-SNMP Sensor not requested and snmp sensor will not be built])
                        $2)]
                [AS_IF([test "$snmp_check_happy" = "no" -a "$with_snmp" = "no"],
                        AC_MSG_RESULT([NET-SNMP Check failed and SNMP sensor explicitly not requested])
                        AC_MSG_RESULT([SNMP Sensor not requested and dependency not met and snmp sensor will not be built])
                        $2)],
                [AC_MSG_RESULT([with_snmp not specified!])]
                [AS_IF([test "$snmp_check_happy" = "no"],
                        AC_MSG_RESULT([NET-SNMP Check failed and SNMP sensor not requested])
                        AC_MSG_RESULT([NET-SNMP Sensor not requested and dependency not met and snmp sensor will not be built])
                        $2)]
                [AS_IF([test "$snmp_check_happy" = "yes"],
                        AC_MSG_RESULT([NET-SNMP Check passed and SNMP sensor not requested])
                        AC_MSG_RESULT([SNMP Sensor not explicitly requested but dependency was met and snmp sensor will be built])
                        $1)])

    AC_SUBST(sensor_snmp_CPPFLAGS)
    AC_SUBST(sensor_snmp_LDFLAGS)
    AC_SUBST(sensor_snmp_LIBS)

])dnl
