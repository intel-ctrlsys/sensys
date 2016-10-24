
Copyright (c) 2013-2015 Intel Corporation. All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
$COPYRIGHT$

Additional copyrights may follow

$HEADER$

These are two tests that link with Google Test.

You must configure ORCM/GREI with these two options in order to run them:

--with-gtest-incdir=
--with-gtest-libdir=

The first is the path to the directory containing the "gtest" directory
that contains the Google Test headers.

The second is that path to "libgtest_main.a".  If your Google Test library
has a different name, you need to change it to "libgtest_main.a".

To add these example tests to the "make check" build and run, uncomment
the reference to this directory in the Makefile.am in the directory above.
Rerun autogen.pl (because you changed a Makefile.am).  Then "configure", 
"make", "make install" and "make check".

The "make install" step can be skipped if you are building with static
libraries.

One test will pass and one will fail.  The failure is expected, so
we define XFAIL_TESTS to the name of the failing test so that the
test system doesn't consider it a failed test.

