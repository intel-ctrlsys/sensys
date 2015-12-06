
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

TESTS = saeg_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = saeg_tests

saeg_tests_SOURCES = \
       evgen_saeg_tests.cpp \
       evgen_saeg_tests.h   
	
SAEG_BUILD_DIR=$(top_builddir)/orcm/mca/evgen/saeg

if MCA_BUILD_orcm_evgen_saeg_DSO

SAEG_LIB=$(SAEG_BUILD_DIR)/mca_evgen_saeg.la 

else

SAEG_LIB=$(SAEG_BUILD_DIR)/libmca_evgen_saeg.la 

endif	

#
# Libraries we depend on
#

LDADD = \
        @GTEST_LIBRARY_DIR@/libgtest_main.a \
        $(SAEG_LIB)

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto

#
# Preprocessor flags
#
SAEG_DIR=$(top_srcdir)/orcm/mca/evgen/saeg
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(SAEG_DIR)