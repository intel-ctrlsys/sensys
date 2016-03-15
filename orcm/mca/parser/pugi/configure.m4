dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2016      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_parser_pugi_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_parser_pugi_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/parser/pugi/Makefile])

    AC_ARG_WITH([pugi],
                [AC_HELP_STRING([--with-pugi],
                                [Build pugi support (default: yes)])],
                                [], with_pugi=yes)

])dnl
