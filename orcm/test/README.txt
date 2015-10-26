#
#
# Copyright (c) 2015      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

Notes on test directory.
=======================

There are many ways to organize tests within a source code repository.
This is just one way.  Feel free to organize it differently if this one 
isn't optimal.

"orcm/test" is designed to contain tests of components in the
orcm subdirectories "mca" and "tools".  These tests will be built and 
run when you type "make check".  The directory structure of "orcm/test"
mirrors that of the orcm source tree.

test
├── gtest_example
│   ├── failingTest.cpp
│   ├── Makefile.am
│   └── passingTest.cpp
├── Makefile.am
├── mca
│   ├── analytics
│   ├── cfgi
│   ├── db
│   ├── Makefile.am
│   └── sensor
│       ├── ft_tester
│       │   ├── Makefile.am
│       │   └── test_ft_tester.c
│       ├── Makefile.am
├── README.txt
└── tools
    ├── ocli
    └── octl


There was formerly a directory named "orcm/test" that was moved to
"orcm/old_test".  The tests in that directory do not appear to be in use.

Initially there are two tests, to serve as examples.  The test in 
mca/sensor/ft_tester is a test of the ft_tester sensor.  It is a standalone
test that links with the ft_tester object file and with the orcm, orte and
opal libraries.  It calls each of the functions in the sensor framework
interface and validates that their behavior is correct.

The second example is the two simple tests in gtest_example.  These are linked
with the Google Test framework.  They are not linked with any of the
ORCM code.  To build and run tests that use this framework, you must supply
two configuration options when configuring ORCM:

--with-gtest-incdir=
--with-gtest-libdir=

The first option is the path to the directory containing the "gtest" directory
under which are the gtest header files.

The second option is the path to the directory containing libgtest_main.a.  
There are many ways to build Google Test, which result in different library
names.  If your library does not have this name, you must rename it to
"libgtest_main.a".  Obviously this is not portable to systems like Windows
that have a different naming scheme for libraries.

To run the tests:
================

configure
make
make install   (required only on shared library builds)
make check

The "make install" step is not required for static builds.  For shared
library builds, libtool requires a "make install" before the test executable
can locate the libraries.

As "make check" is running you will see a display of tests that are passing
or failing.  A total is listed at the end.  More information may be found
in each test's directory in log files.

Test results can be uploaded to a test server.  Details are at the
gnu.org link below.

If a test fails, then "make check" will immediately stop running tests.
Until you fix the problem, you can list your test in the Makefile.am after
XFAIL_TESTS in addition to TESTS.  This tells the test system that the
test is supposed to fail.  (See gtest_example/Makefile.am.)

To add a test:
=============

Create a directory for your test in the appropriate location under
orcm/test if it does not already exist.

In Makefile.am in the directory just above, add your new directory name to 
the SUBDIRS variable list so make will recurse into your directory.

If your test should be run conditionally only if Google Test was
found, then include its Makefile conditionally in SUBDIRS using the example 
in orcm/test/Makefile.am.

Create a Makefile.am in your directory, which will be used to generate
the Makefile that builds and runs your test.  See the Makefile.am in
orcm/test/mca/sensor/ft_tester or orcm/test/gtets_example for help.

The automakeVariables.pdf file in this directory describes the predefined
variables that you may want to use in Makefile.am.

In config/grei_config_files.m4, add your Makefile to the list of makefiles
to be processed by the "configure" command.

If you have added or changed a Makefile.am, then you have to rerun
the autogen.pl script and re-configure.

Details on building test suites with automake can be found here:

https://www.gnu.org/software/automake/manual/html_node/Tests.html

Note that tests should return 0 on success, 99 on a test error not
related to the code under test, 77 if the test is skipped, and any
other value (like 1) if the test fails.

More about using Google Test:
============================
The gtest configure macro (config/grei_check_gtest.m4) that runs at
configure time does the following if you supplied the --with-gtest-incdir and 
--with-gtest-libdir options and the headers and library were found.

In the generated opal/include/opal_config.h we get:

#define HAVE_GTEST 1

As variables that can be used in Makefile.am we get:

@GTEST_INCLUDE_DIR@  
@GTEST_LIBRARY_DIR@ 

As a boolean in the Makefile.am we get

HAVE_GTEST

which is true if we have found the Google Test headers and library and
false otherwise.

Problems:
========
If a test fails or reports an error (return code 99), the testing
stops.  This is not supposed to happen.  "make check" is supposed to
run all tests in spite of failures and report the result.  
