dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2017      Intel Corporation. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_sensor_ipmi_ts_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_ipmi_ts_CONFIG], [
    # Check if ipmiutil support is available or not.
    AC_CONFIG_FILES([orcm/mca/sensor/ipmi_ts/Makefile])
    AC_MSG_CHECKING([for IPMI Thread Safe Sensor support])
    AC_REQUIRE([OPAL_CHECK_IPMIUTIL])

   AC_ARG_WITH([ipmi_ts],
                [AC_HELP_STRING([--with-ipmi-ts(=yes/no)],
                                [Build IPMI Thread Safe sensor support])],
                [AC_MSG_RESULT([with_ipmi_ts selected with paramater "$with_ipmi_ts"!])]
                [AS_IF([test "$ipmiutil_check_happy" = "no" -a "$with_ipmi_ts" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and we need to skip building IPMI Thread Safe Sensor Component here])
                        AC_MSG_WARN([IPMIUTIL Libs not present])
                        AC_MSG_ERROR([IPMI Thread Safe Sensor requested but dependency not met])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes" -a "$with_ipmi_ts" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and IPMI Thread Safe sensor explicitly requested])
                        AC_MSG_RESULT([IPMI Thread Safe Sensor requested and dependency met and ipmi_ts sensor will be built])
                        $1)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes" -a "$with_ipmi_ts" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and but IPMI Thread Safe sensor explicitly not requested])
                        AC_MSG_RESULT([IPMI Thread Safe Sensor not requested and ipmi_ts sensor will not be built])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "no" -a "$with_ipmi_ts" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and IPMI Thread Safe sensor explicitly not requested])
                        AC_MSG_RESULT([IPMI Thread Safe Sensor not requested and dependency not met and ipmi_ts sensor will not be built])
                        $2)],
                [AC_MSG_RESULT([with_ipmi_ts not specified!])]
                [AS_IF([test "$ipmiutil_check_happy" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and IPMI Thread Safe sensor not requested])
                        AC_MSG_RESULT([IPMI Thread Safe Sensor not requested and dependency not met and ipmi_ts sensor will not be built])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and IPMI Thread Safe sensor not requested])
                        AC_MSG_RESULT([IPMI Thread Safe Sensor not explicitly requested but dependency was met and ipmi_ts sensor will be built])
                        $1)])

    AC_SUBST(sensor_ipmi_ts_CPPFLAGS)
    AC_SUBST(sensor_ipmi_ts_LDFLAGS)
    AC_SUBST(sensor_ipmi_ts_LIBS)

])dnl
