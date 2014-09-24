dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_scd_mfile_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_scd_mfile_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/scd/mfile/Makefile])

    AC_CHECK_HEADERS(sys/inotify.h, [$1], [$2])
])dnl
