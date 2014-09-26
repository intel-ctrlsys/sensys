dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_diag_cputest_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_diag_cputest_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/diag/cputest/Makefile])

    AC_ARG_WITH([cputest],
                [AC_HELP_STRING([--with-cputest],
                                [Build cputest support (default: no)])],
	                        [], with_cputest=no)

])dnl
