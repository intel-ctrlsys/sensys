
#
# For make V=1 verbosity
#

include $(top_srcdir)/Makefile.ompi-rules

#
# Tests.  "make check" expects these return values:
#
# 0:              pass
# 77:             skipped test
# 99:             error
# other non-zero: fail
#

#
# List of tests
#

TESTS = passingTest failingTest

#
# XFAIL_TESTS means this test is supposed to fail.
#

XFAIL_TESTS = failingTest

#
# Executables to be built for "make check"
#

check_PROGRAMS = passingTest failingTest

passingTest_SOURCES = passingTest.cpp

failingTest_SOURCES = failingTest.cpp

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -pthread

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@
