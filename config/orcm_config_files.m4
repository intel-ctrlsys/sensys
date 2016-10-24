# -*- shell-script -*-
#
# Copyright (c) 2013-2014 Intel Corporation. All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

AC_DEFUN([ORCM_CONFIG_FILES],[
    AC_CONFIG_FILES([
    orcm/Makefile
    orcm/etc/Makefile
    orcm/include/Makefile
    orcm/common/Makefile

    orcm/tools/octl/Makefile
    orcm/tools/orcm-info/Makefile
    orcm/tools/orcmd/Makefile
    orcm/tools/orcmsched/Makefile
    orcm/tools/wrappers/Makefile
    orcm/tools/wrappers/orcm.pc
    orcm/tools/wrappers/orcmcc-wrapper-data.txt
    ])
])
