# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

if HAVE_GTEST
gtestSubdirs=ipmi errcounts snmp base coretemp freq sigar componentpower \
             mcedata nodepower resusage syslog file heartbeat
endif

# Removed ft_tester from production runs.
SUBDIRS=$(gtestSubdirs) #ft_tester
