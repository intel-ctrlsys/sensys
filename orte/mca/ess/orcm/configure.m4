# -*- shell-script -*-
#
# Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# MCA_ess_orcm_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orte_ess_orcm_CONFIG],[
    AC_CONFIG_FILES([orte/mca/ess/orcm/Makefile])

    # Evaluate succeed / fail
    AS_IF([test "$orcm_is_present" = "yes" -a "$enable_orcm" != "no"],
          [$1],
          [$2])
])dnl
