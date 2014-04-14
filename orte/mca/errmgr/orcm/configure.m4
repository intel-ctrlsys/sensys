dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_errmgr_orcm_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orte_errmgr_orcm_CONFIG], [
    AC_CONFIG_FILES([orte/mca/errmgr/orcm/Makefile])

    # do not build if orcm not requested
    AS_IF([test "$orcm_is_present" = "yes" -a "$enable_orcm" != "no"],
          [$1], [$2])

])dnl
