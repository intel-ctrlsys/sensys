# -*- shell-script -*-
#
# Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

AC_DEFUN([GREI_CONFIG_FILES],[
    AC_CONFIG_FILES([
    orcm/test/Makefile
    orcm/test/gtest_example/Makefile
    orcm/test/mca/Makefile
    orcm/test/mca/evgen/Makefile
    orcm/test/mca/evgen/saeg/Makefile
    orcm/test/mca/sensor/Makefile
    orcm/test/mca/sensor/ipmi/Makefile
    orcm/test/mca/sensor/base/Makefile
    orcm/test/mca/sensor/coretemp/Makefile
    orcm/test/mca/sensor/freq/Makefile
    orcm/test/mca/sensor/sigar/Makefile
    orcm/test/mca/sensor/componentpower/Makefile
    orcm/test/mca/sensor/evinj/Makefile
    orcm/test/mca/sensor/mcedata/Makefile
    orcm/test/mca/sensor/nodepower/Makefile
    orcm/test/mca/sensor/resusage/Makefile
    orcm/test/mca/sensor/syslog/Makefile
    orcm/test/mca/sensor/snmp/Makefile
    orcm/test/mca/sensor/file/Makefile
    orcm/test/mca/sensor/errcounts/Makefile
    orcm/test/mca/analytics/Makefile
    orcm/test/mca/analytics/window/Makefile
    orcm/test/mca/analytics/aggregate/Makefile
    orcm/test/mca/analytics/cott/Makefile
    ])
])
