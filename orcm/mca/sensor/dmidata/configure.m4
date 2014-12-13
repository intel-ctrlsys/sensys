dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_dmidata_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------

AC_DEFUN([MCA_orcm_sensor_dmidata_CONFIG], [
    # Check if hwloc support is available or not.
    AC_CONFIG_FILES([orcm/mca/sensor/dmidata/Makefile])
    AC_MSG_CHECKING([for dmidata support])
    AC_REQUIRE([OPAL_HAVE_HWLOC])

   AC_ARG_WITH([dmidata],
                [AC_HELP_STRING([--with-dmidata(=yes/no)],
                                [Build dmidata inventory support])],
                [AC_MSG_RESULT([with_dmidata selected with paramater "$with_dmidata"!])]
                [AS_IF([test $OPAL_HAVE_HWLOC = 0 -a "$with-dmidata" = "yes"],
                        AC_MSG_RESULT([HWLOC Check failed and we need to skip building dmidata component here])
                        AC_MSG_WARN([HWLOC not enabled])
                        AC_MSG_ERROR([dmidata requested but dependency not met])
                        $2)]
                [AS_IF([test $OPAL_HAVE_HWLOC = 1 -a "$with-dmidata" = "yes"],
                        AC_MSG_RESULT([HWLOC Check passed and dmidata explicitly requested])                        
                        AC_MSG_RESULT([dmidata requested and dependency met and dmidata will be built])
                        $1)]
                [AS_IF([test $OPAL_HAVE_HWLOC = 1 -a "$with-dmidata" = "no"],
                        AC_MSG_RESULT([HWLOC Check passed and but dmidta explicitly not requested])                        
                        AC_MSG_RESULT([dmidata not requested and will not be built])
                        $2)]
                [AS_IF([test $OPAL_HAVE_HWLOC = 0 -a "$with-dmidata" = "no"],
                        AC_MSG_RESULT([HWLOC Check failed and dmidata explicitly not requested])                        
                        AC_MSG_RESULT([dmidata not requested and dependency not met so dmidata will not be built])
                        $2)],
                [AC_MSG_RESULT([with_ipmi_sensor not specified!])]
                [AS_IF([test $OPAL_HAVE_HWLOC = 0],
                        AC_MSG_RESULT([HWLOC Check failed and dmidata not requested])                        
                        AC_MSG_RESULT([dmidata not requested and dependency not met so dmidata will not be built])
                        $2)]
                [AS_IF([test $OPAL_HAVE_HWLOC = 1],
                        AC_MSG_RESULT([HWLOC Check passed and dmidata not requested])                        
                        AC_MSG_RESULT([dmidata not explicitly requested but dependency was met so dmidata will be built])
                        $1)])
    
    AC_SUBST(sensor_dmidata_CPPFLAGS)
    AC_SUBST(sensor_dmidata_LDFLAGS)
    AC_SUBST(sensor_dmidata_LIBS)

])dnl
