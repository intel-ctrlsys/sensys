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
    AC_CONFIG_FILES([orcm/mca/sensor/ipmi/Makefile])

    AC_ARG_WITH([ipmi],
                [AC_HELP_STRING([--with-ipmi],
                                [Build ipmi support (default: no)])],
	                        [], with_ipmi=no)

    # do not build if support not requested
    AS_IF([test "1" = "1"],
          [$1],
          [$2])

    AC_SUBST(sensor_ipmi_CPPFLAGS)
    AC_SUBST(sensor_ipmi_LDFLAGS)
    AC_SUBST(sensor_ipmi_LIBS)
])dnl
