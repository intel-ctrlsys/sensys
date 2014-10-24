dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_ipmi_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_ipmi_CONFIG], [
    # Check if ipmiutil support is available or not.
    AC_CONFIG_FILES([orcm/mca/sensor/ipmi/Makefile])
    AC_MSG_CHECKING([for IPMI Sensor support])
    AC_REQUIRE([OPAL_CHECK_IPMIUTIL])

   AC_ARG_WITH([ipmi_sensor],
                [AC_HELP_STRING([--with-ipmi_sensor(=yes/no)],
                                [Build IPMI sensor support])],
                [AC_MSG_RESULT([with_ipmi_sensor selected with paramater "$with_ipmi_sensor"!])]
                [AS_IF([test "$ipmiutil_check_happy" = "no" -a "$with_ipmi_sensor" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and we need to skip building IPMI Sensor Component here])
                        AC_MSG_WARN([IPMIUTIL Libs not present])
                        AC_MSG_ERROR([IPMI Sensor requested but dependency not met])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes" -a "$with_ipmi_sensor" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and IPMI sensor explicitly requested])                        
                        AC_MSG_RESULT([IPMI Sensor requested and dependency met and ipmi sensor will be built])
                        $1)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes" -a "$with_ipmi_sensor" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and but IPMI sensor explicitly not requested])                        
                        AC_MSG_RESULT([IPMI Sensor not requested and ipmi sensor will not be built])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "no" -a "$with_ipmi_sensor" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and IPMI sensor explicitly not requested])                        
                        AC_MSG_RESULT([IPMI Sensor not requested and dependency not met and ipmi sensor will not be built])
                        $2)],
                [AC_MSG_RESULT([with_ipmi_sensor not specified!])]
                [AS_IF([test "$ipmiutil_check_happy" = "no"],
                        AC_MSG_RESULT([IPMIUTIL Check failed and IPMI sensor not requested])                        
                        AC_MSG_RESULT([IPMI Sensor not requested and dependency not met and ipmi sensor will not be built])
                        $2)]
                [AS_IF([test "$ipmiutil_check_happy" = "yes"],
                        AC_MSG_RESULT([IPMIUTIL Check passed and IPMI sensor not requested])                        
                        AC_MSG_RESULT([IPMI Sensor not explicitly requested but dependency was met and ipmi sensor will be built])
                        $1)])
    
    AC_SUBST(sensor_ipmi_CPPFLAGS)
    AC_SUBST(sensor_ipmi_LDFLAGS)
    AC_SUBST(sensor_ipmi_LIBS)

])dnl
