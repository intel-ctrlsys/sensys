#
# Copyright (c) 2015      Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
#
# For make V=1 verbosity
#

include $(top_srcdir)/Makefile.ompi-rules

#
# Test scripts.  
# They return "make check" standard 0 on PASS, 99 on FAIL, 77 on SKIP.
#

TESTS = test_init.sh test_finalize.sh test_analyze_null_input.sh test_analyze_average.sh \
	test_analyze_max.sh test_analyze_min.sh test_analyze_incorrect_operation.sh \
	test_analyze_incorrect_attribute.sh test_analyze_null_data.sh

#
# How to run test scripts 
#

#TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = source

#
# Executables to be built for "make check"
#

check_PROGRAMS = test_analytics_aggregate

test_analytics_aggregate_SOURCES = test_analytics_aggregate.c 

#
# Libraries we depend on
#

ANALYTICS_AGGREGATE_BUILD_DIR=$(top_builddir)/orcm/mca/analytics/aggregate

if MCA_BUILD_orcm_analytics_aggregate_DSO

analytics_aggregate_LIB=$(ANALYTICS_AGGREGATE_BUILD_DIR)/mca_analytics_aggregate.la 

else

analytics_aggregate_LIB=$(ANALYTICS_AGGREGATE_BUILD_DIR)/libmca_analytics_aggregate.la 

endif

test_analytics_aggregate_LDADD = \
        $(analytics_aggregate_LIB) \
        $(top_builddir)/orcm/liborcm.la \
        $(top_builddir)/orte/lib@ORTE_LIB_PREFIX@open-rte.la \
        $(top_builddir)/opal/lib@OPAL_LIB_PREFIX@open-pal.la


#
# Preprocessor flags
#

ANALYTICS_AGGREGATE_DIR=$(top_srcdir)/orcm/mca/analytics/aggregate

test_analytics_aggregate_CPPFLAGS=-I$(ANALYTICS_AGGREGATE_DIR)
test_analytics_aggregate_LDFLAGS=-lstdc++
