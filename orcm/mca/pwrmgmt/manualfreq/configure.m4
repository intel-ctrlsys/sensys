dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_pwrmgmt_manualfreq_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_pwrmgmt_manualfreq_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/pwrmgmt/manualfreq/Makefile])

    AC_ARG_WITH([manualfreq],
                [AC_HELP_STRING([--with-manualfreq],
                                [Build manualfreq support (default: yes)])],
	                        [], with_manualfreq=yes)

    # do not build if support not requested
    AS_IF([test "$with_manualfreq" != "no"],
          [AS_IF([test "$opal_found_linux" = "yes"],
                 [$1],
                 [AC_MSG_WARN([Manual Frequency was requested but is only supported on Linux systems])
                  AC_MSG_ERROR([Cannot continue])
                  $2])
          ],
          [$2])
])dnl
