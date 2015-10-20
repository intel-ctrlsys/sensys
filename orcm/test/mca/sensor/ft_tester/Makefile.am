
#
# For make V=1 verbosity
#

include $(top_srcdir)/Makefile.ompi-rules

#
# Test scripts.  
# They return "make check" standard 0 on PASS, 99 on FAIL, 77 on SKIP.
#

TESTS = aggKill.sh childKill.sh daemonKill.sh \
        aggNoKill.sh childNoKill.sh daemonNoKill.sh

#
# How to run test scripts 
#

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = source

#
# Executables to be built for "make check"
#

check_PROGRAMS = test_ft_tester 

test_ft_tester_SOURCES = test_ft_tester.c

#
# Libraries we depend on
#

FT_TESTER_BUILD_DIR=$(top_builddir)/orcm/mca/sensor/ft_tester

if MCA_BUILD_orcm_sensor_ft_tester_DSO

FT_TESTER_LIB=$(FT_TESTER_BUILD_DIR)/mca_sensor_ft_tester.la 

else

FT_TESTER_LIB=$(FT_TESTER_BUILD_DIR)/libmca_sensor_ft_tester.la 

endif

test_ft_tester_LDADD = \
        $(FT_TESTER_LIB) \
        $(top_builddir)/orcm/liborcm.la \
        $(top_builddir)/orte/lib@ORTE_LIB_PREFIX@open-rte.la \
        $(top_builddir)/opal/lib@OPAL_LIB_PREFIX@open-pal.la


#
# Preprocessor flags
#

FT_TESTER_DIR=$(top_srcdir)/orcm/mca/sensor/ft_tester

test_ft_tester_CPPFLAGS=-I$(FT_TESTER_DIR)
