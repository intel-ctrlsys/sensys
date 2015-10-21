
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

TESTS = passingTest failingTest

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
