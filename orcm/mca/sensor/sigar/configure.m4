dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_sigar_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_sigar_CONFIG], [
    # Check if sigarlib support is available or not.
    AC_CONFIG_FILES([orcm/mca/sensor/sigar/Makefile])
    AC_MSG_CHECKING([for sigar Sensor support])
    AC_REQUIRE([OPAL_CHECK_SIGARLIB])

   AC_ARG_WITH([sigar_sensor],
                [AC_HELP_STRING([--with-sigar_sensor(=yes/no)],
                                [Build Sigar sensor support])],
                [AC_MSG_RESULT([with_sigar_sensor selected with paramater "$with_sigar_sensor"!])]
                [AS_IF([test "$sigarlib_check_happy" = "no" -a "$with_sigar_sensor" = "yes"],
                        AC_MSG_RESULT([SIGAR LIB Check failed and we need to skip building Sigar Sensor Component here])
                        AC_MSG_WARN([SIGAR  Libs not present])
                        AC_MSG_ERROR([Sigar Sensor requested but dependency not met])
                        $2)]
                [AS_IF([test "$sigarlib_check_happy" = "yes" -a "$with_sigar_sensor" = "yes"],
                        AC_MSG_RESULT([SIGAR LIB Check passed and Sigar sensor explicitly requested])                        
                        AC_MSG_RESULT([SIGAR Sensor requested and dependency met and sigar sensor will be built])
                        $1)]
                [AS_IF([test "$sigarlib_check_happy" = "yes" -a "$with_sigar_sensor" = "no"],
                        AC_MSG_RESULT([SIGAR LIB Check passed and but SIGAR sensor explicitly not requested])                        
                        AC_MSG_RESULT([SIGAR Sensor not requested and sigar sensor will not be built])
                        $2)]
                [AS_IF([test "$sigarlib_check_happy" = "no" -a "$with_sigar_sensor" = "no"],
                        AC_MSG_RESULT([SIGAR LIB Check failed and SIGAR sensor explicitly not requested])                        
                        AC_MSG_RESULT([SIGAR Sensor not requested and dependency not met and sigar sensor will not be built])
                        $2)],
                [AC_MSG_RESULT([with_sigar_sensor not specified!])]
                [AS_IF([test "$sigarlib_check_happy" = "no"],
                        AC_MSG_RESULT([SIGAR LIB Check failed and SIGAR sensor not requested])                        
                        AC_MSG_RESULT([SIGAR Sensor not requested and dependency not met and sigar sensor will not be built])
                        $2)]
                [AS_IF([test "$sigarlib_check_happy" = "yes"],
                        AC_MSG_RESULT([SIGAR LIB Check passed and sigar sensor not requested])                        
                        AC_MSG_RESULT([SIGAR Sensor not explicitly requested but dependency was met and sigar sensor will be built])
                        $1)])
    
    AC_SUBST(sensor_sigar_CPPFLAGS)
    AC_SUBST(sensor_sigar_LDFLAGS)
    AC_SUBST(sensor_sigar_LIBS)

])dnl

