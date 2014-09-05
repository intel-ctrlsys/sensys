dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_componentpower_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_componentpower_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/componentpower/Makefile])

    AC_ARG_WITH([componentpower],
                [AC_HELP_STRING([--with-componentpower],
                                [Build componentpower support (default: no)])],
	                        [], with_componentpower=no)

    # do not build if support not requested
    AS_IF([test "$with_componentpower" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([only for Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
