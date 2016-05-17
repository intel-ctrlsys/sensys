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

TESTS = print_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = print_tests

print_tests_SOURCES = \
    db_print_test.cpp \
    db_print_test.h

PRINT_BUILD_DIR=$(top_builddir)/orcm/mca/db/print

if MCA_BUILD_orcm_db_print_DSO

PRINT_LIB=$(PRINT_BUILD_DIR)/mca_db_print.la

else

PRINT_LIB=$(PRINT_BUILD_DIR)/libmca_db_print.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(PRINT_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#

PRINT_DIR=$(top_srcdir)/orcm/mca/db/print
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(PRINIT_DIR) -I$(ANALYTICS_UTIL_DIR)
