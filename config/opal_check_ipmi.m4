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


# OPAL_CHECK_IPMIUTIL()
# -----------------------------------------------------------

AC_DEFUN([OPAL_CHECK_IPMIUTIL], [
    AC_MSG_CHECKING([for IPMIUTIL support])
    AC_ARG_WITH([ipmiutil],
                [AC_HELP_STRING([--with-ipmiutil(=DIR)],
                                [Build IPMI support, optionally adding DIR to the search path])],
                [# with_ipmiutil=yes|no|path
                 AS_IF([test "$with_ipmiutil" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([IPMIUTIL support not requested])
                        ipmiutil_check_happy=no],
                       [# Support explicitly requested (with_ipmiutil=yes|path)
                        AC_MSG_RESULT([IPMI support explicitly requested])
                        AS_IF([test "$with_ipmiutil" != "yes"],
                              [ipmiutil_prefix_dir=$with_ipmiutil])
                              AC_MSG_RESULT([with_ipmiutil Directory is $ipmiutil_prefix_dir])
                               OPAL_CHECK_PACKAGE([ipmiutil],
                                     [ipmicmd.h],
                                     [ipmiutil],
                                     [ipmi_cmd_mc],
                                     [-lcrypto],
                                     [$ipmiutil_prefix_dir],
                                     [],
                                     [AC_MSG_RESULT([found libs at $ipmiutil_prefix_dir])
                                     ipmiutil_check_happy=yes],
                                     [AC_MSG_WARN([IPMI sensor support requested])
                                      AC_MSG_ERROR([But the required dependent Library or Header files weren't found])
                                      ipmiutil_check_happy=no])])],
               [# Support not explicitly requested, try to build if possible
                OPAL_CHECK_PACKAGE([ipmiutil],
                             [ipmicmd.h],
                             [ipmiutil],
                             [ipmi_cmd_mc],
                             [-lcrypto],
                             [$ipmiutil_prefix_dir],
                             [],
                             [ipmiutil_check_happy=yes],
                             [AC_MSG_RESULT([IPMI library not found])
                              AC_MSG_RESULT([building without IPMI support])
                              ipmiutil_check_happy=no])])
    AC_SUBST(ipmiutil_CPPFLAGS)
    AC_SUBST(ipmiutil_LDFLAGS)
    AC_SUBST(ipmiutil_LIBS)

])dnl
