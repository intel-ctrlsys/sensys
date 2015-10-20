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

Initially there is only one test - a test of the ft_tester sensor.  

To run the test:
===============

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

To add a test:
=============

Create a directory for your test in the appropriate location under
orcm/test if it does not already exist.

In Makefile.am in the directory just above, add your new directory name to 
the SUBDIRS variable list so make will recurse into your directory.

Create a Makefile.am in your directory, which will be used to generate
the Makefile that builds and runs your test.  See the comments in
orcm/test/mca/sensor/ft_tester/Makefile.am for an explanation of
the special targets interpreted by automake.

In config/orcm_config_files.m4, add your Makefile to the list of makefiles
to be processed by the "configure" command.

Details on building test suites with automake can be found here:

https://www.gnu.org/software/automake/manual/html_node/Tests.html

Note that tests should return 0 on success, 99 on a test error not
related to the code under test, 77 if the test is skipped, and any
other value (like 1) if the test fails..
