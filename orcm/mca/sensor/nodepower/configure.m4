dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_sensor_nodepower_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_sensor_nodepower_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/sensor/nodepower/Makefile])

    AC_ARG_WITH([nodepower],
                [AC_HELP_STRING([--with-nodepower],
                                [Build nodepower support (default: no)])],
	                        [], with_nodepower=no)

    # do not build if support not requested
    AS_IF([test "$with_nodepower" != "no"],
          [AS_IF([test ! -z "$with_nodepower" -a "$with_nodepower" != "yes"],
                 [orcm_check_nodepower_dir="$with_nodepower"])

           OPAL_CHECK_PACKAGE([sensor_nodepower],
                              [ipmicmd.h],
                              [ipmiutil],
                              [-lcrypto],
                              [$orcm_check_nodepower_dir],
                              [],
                              [$1],
                              [AC_MSG_WARN([NODEPOWER SENSOR SUPPORT REQUESTED])
                               AC_MSG_WARN([BUT REQUIRED LIBRARY OR HEADER NOT FOUND])
                               AC_MSG_ERROR([CANNOT CONTINUE])
                               $2])],
          [$2])
])dnl
