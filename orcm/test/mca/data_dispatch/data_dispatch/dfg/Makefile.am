
#
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

TESTS = dfg_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = dfg_tests

dfg_tests_SOURCES = \
       data_dispatch_dfg_tests.cpp \
       data_dispatch_dfg_tests.h

DFG_BUILD_DIR=$(top_builddir)/orcm/mca/data_dispatch/dfg

if MCA_BUILD_orcm_data_dispatch_dfg_DSO
DFG_LIB=$(DFG_BUILD_DIR)/mca_data_dispatch_dfg.la 
else
DFG_LIB=$(DFG_BUILD_DIR)/libmca_data_dispatch_dfg.la 
endif	

#
# Libraries we depend on
#

LDADD = \
        @GTEST_LIBRARY_DIR@/libgtest_main.a \
        $(DFG_LIB)

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto

#
# Preprocessor flags
#
DFG_DIR=$(top_srcdir)/orcm/mca/data_dispatch/dfg
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(DFG_DIR)
