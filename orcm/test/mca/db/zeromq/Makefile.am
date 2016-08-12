#
# Copyright (c) 2016      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$

# For make V=1 verbosity
#

include $(top_srcdir)/Makefile.ompi-rules

#
# Tests.  "make check" return values:
#
# 0:              pass
# 77:             skipped test
# 99:             hard error, stop testing
# other non-zero: fail
#

TESTS = zeromq_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = zeromq_tests

zeromq_tests_SOURCES = \
    zeromq_tests.cpp


ZEROMQ_BUILD_DIR=$(top_builddir)/orcm/mca/db/zeromq

if MCA_BUILD_orcm_db_zeromq_DSO

ZEROMQ_LIB=$(ZEROMQ_BUILD_DIR)/mca_db_zeromq.la

else

ZEROMQ_LIB=$(ZEROMQ_BUILD_DIR)/libmca_db_zeromq.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(ZEROMQ_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lzmq

#
# Preprocessor flags
#

ZEROMQ_DIR=$(top_srcdir)/orcm/mca/db/zermoq
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-std=c++11 -I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(ZEROMQ_DIR) -I$(ANALYTICS_UTIL_DIR)
