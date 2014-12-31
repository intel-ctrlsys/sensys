# -*- shell-script -*-
#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
# --------------------------------------------------------


# OPAL_CHECK_SIGARLIB()
# -----------------------------------------------------------

AC_DEFUN([OPAL_CHECK_SIGARLIB], [
    AC_MSG_CHECKING([for SIGAR LIB support])
    AC_ARG_WITH([sigarlib],
                [AC_HELP_STRING([--with-sigarlib(=DIR)],
                                [Build SIGAR support, optionally adding DIR to the search path])],
                [# with_sigarlib=yes|no|path
                 AS_IF([test "$with_sigarlib" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([SIGAR Lib support not requested])
                        sigarlib_check_happy=no],
                       [# Support explicitly requested (with_sigarlib=yes|path)
                        AC_MSG_RESULT([SIGAR Lib support explicitly requested])
                        AS_IF([test "$with_sigarlib" != "yes"],
                            [sigarlib_prefix_dir=$with_sigarlib])
                        AS_IF([test "$opal_found_apple" = "yes"], 
                            [libname="sigar-universal-macosx"],
                            [libname="sigar"
                            AC_DEFINE_UNQUOTED(ORCM_SIGAR_LINUX, [test "$opal_found_linux" = "yes"],
                                [Which name to use for the sigar library on this OS])])
                              AC_MSG_RESULT([with_sigarlib Directory is $sigarlib_prefix_dir])
                               OPAL_CHECK_PACKAGE([sigar],
                                     [sigar.h],
                                     [$libname],
                                     [sigar_proc_cpu_get],
                                     [],
                                     [$sigarlib_prefix_dir],
                                     [],
                                     [AC_MSG_RESULT([found libs at $sigarlib_prefix_dir])
                                     sigarlib_check_happy=yes],
                                     [AC_MSG_WARN([SIGAR lib sensor support requested])
                                      AC_MSG_ERROR([But the required dependent Library or Header files weren't found])
                                      sigarlib_check_happy=no])])],
                [# Support not explicitly requested, try to build if possible
                AS_IF([test "$opal_found_apple" = "yes"], 
                            [libname="sigar-universal-macosx"],
                            [libname="sigar"
                            AC_DEFINE_UNQUOTED(ORCM_SIGAR_LINUX, [test "$opal_found_linux" = "yes"],
                                [Which name to use for the sigar library on this OS])])
                OPAL_CHECK_PACKAGE([sigar],
                             [sigar.h],
                             [sigar],
                             [sigar_file_system_usage_get],
                             [],
                             [$sigarlib_prefix_dir],
                             [],
                             [sigarlib_check_happy=yes],
                             [AC_MSG_RESULT([SIGAR lib library not found])
                              AC_MSG_RESULT([building without SIGAR support])
                              sigarlib_check_happy=no])])
    AC_SUBST(sigarlib_CPPFLAGS)
    AC_SUBST(sigarlib_LDFLAGS)
    AC_SUBST(sigarlib_LIBS)

])dnl
