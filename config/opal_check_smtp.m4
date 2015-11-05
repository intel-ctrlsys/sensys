# -*- shell-script -*-
#
# Copyright (c) 2015      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
# --------------------------------------------------------


# OPAL_CHECK_SMTP()
# -----------------------------------------------------------

AC_DEFUN([OPAL_CHECK_SMTP], [
    AC_MSG_CHECKING([for SMTP support])
    AC_ARG_WITH([smtp],
                [AC_HELP_STRING([--with-smtp(=DIR)],
                                [Build SMTP support, optionally adding DIR to the search path])],
                [# with_smtp=yes|no|path
                 AS_IF([test "$with_smtp" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([SMTP support not requested])
                        smtp_check_happy=no],
                       [# Support explicitly requested (with_smtp=yes|path)
                        AC_MSG_RESULT([SMTP support explicitly requested])
                        AS_IF([test "$with_smtp" != "yes"],
                              [smtp_prefix_dir=$with_smtp])
                              AC_MSG_RESULT([with_smtp Directory is $smtp_prefix_dir])
                               OPAL_CHECK_PACKAGE([smtp],
                                     [libesmtp.h],
                                     [esmtp],
                                     [smtp_create_session],
                                     [],
                                     [$smtl_prefix_dir],
                                     [],
                                     [AC_MSG_RESULT([found libs at $smtp_prefix_dir])
                                     smtp_check_happy=yes],
                                     [AC_MSG_WARN([SMTP notifier support requested])
                                      smtp_check_happy=no])])],
               [# Support not explicitly requested, try to build if possible
                OPAL_CHECK_PACKAGE([smtp],
                             [libesmtp.h],
                             [esmtp],
                             [smtp_create_session],
                             [],
                             [$smtp_prefix_dir],
                             [],
                             [smtp_check_happy=yes],
                             [AC_MSG_RESULT([SMTP library not found])
                              AC_MSG_RESULT([building without SMTP support])
                              smtp_check_happy=no])])
    AC_SUBST(smtp_CPPFLAGS)
    AC_SUBST(smtp_LDFLAGS)
    AC_SUBST(smtp_LIBS)

])dnl
