# -*- shell-script -*-
#
# Copyright (c) 2014-2015  Intel Corporation. All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dnl GREI_FIND_GTEST processes these options:
dnl
dnl  --with-gtest-incdir=DIR   
dnl     path to directory containing "gtest" header file directory
dnl
dnl  --with-gtest-libdir=DIR   
dnl     path to directory containing libgtest_main.a
dnl
dnl If the headers and library are found, we set:
dnl
dnl  #define HAVE_GTEST 1     in the generated opal/include/opal_config.h
dnl  @GTEST_INCLUDE_DIR@      variable in Makefile.am
dnl  @GTEST_LIBRARY_DIR@      variable in Makefile.am
dnl  HAVE_GTEST               conditional in Makefile.am to true
dnl
dnl If not found, we set:
dnl
dnl  HAVE_GTEST               conditional in Makefile.am to false
dnl

AC_DEFUN([GREI_FIND_GTEST], [

  grei_find_gtest_incdir=unset
  grei_find_gtest_libdir=unset
  grei_find_gtest_found=no

  AC_MSG_CHECKING([Google test headers and library])

  AC_ARG_WITH([gtest-incdir],

    [AS_HELP_STRING([--with-gtest-incdir],
      [path to directory containing Google test "gtest" header file directory])],

    [ if test -f "$with_gtest_incdir/gtest/gtest.h" ; then
        grei_find_gtest_incdir=$with_gtest_incdir
      else
        AC_MSG_ERROR([cannot find Google Test headers at $with_gtest_incdir/gtest])
      fi
    ]

    [])

  AC_ARG_WITH([gtest-libdir],

    [AS_HELP_STRING([--with-gtest-libdir],
      [path to directory containing Google test libgtest_main.a])],

    [ if test -f "$with_gtest_libdir/libgtest_main.a" ; then
        grei_find_gtest_libdir=$with_gtest_libdir
      else
        AC_MSG_ERROR([cannot find libgtest_main.a at $with_gtest_libdir])
      fi
    ]

    [])

   if test "$grei_find_gtest_incdir" != "unset" && test "$grei_find_gtest_libdir" != "unset" ; then

     grei_find_gtest_found=yes

     AC_MSG_RESULT([found])

   else

     AC_MSG_RESULT([not found])

   fi

   if test $grei_find_gtest_found = yes ; then

     AC_DEFINE([HAVE_GTEST],[1], [We have Google Test])
     AC_SUBST([GTEST_INCLUDE_DIR], [$grei_find_gtest_incdir])
     AC_SUBST([GTEST_LIBRARY_DIR], [$grei_find_gtest_libdir])

   fi

   AM_CONDITIONAL([HAVE_GTEST], [test $grei_find_gtest_found = yes])
    
 ])
