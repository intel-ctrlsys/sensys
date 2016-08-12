# -*- shell-script -*-
#
# Copyright (c) 2016      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
# --------------------------------------------------------


# OPAL_CHECK_ZEROMQ()
# -----------------------------------------------------------

AC_DEFUN([OPAL_CHECK_ZEROMQ], [
    AC_MSG_CHECKING([for ZeroMQ support])
    AC_ARG_WITH([zeromq],
                [AC_HELP_STRING([--with-zeromq(=DIR)],
                                [Build ZeroMQ support, optionally adding DIR to the search path])],
                [# with_zeromq=yes|no|path
                 AS_IF([test "$with_zeromq" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([ZeroMQ support not requested])
                        zeromq_check_happy=no],
                       [# Support explicitly requested (with_zeromq=yes|path)
                        AC_MSG_RESULT([ZeroMQ support explicitly requested])
                        AS_IF([test "$with_zeromq" != "yes"],
                              [zeromq_prefix_dir=$with_zeromq])
                              AC_MSG_RESULT([with_zeromq Directory is $zeromq_prefix_dir])
                               OPAL_CHECK_PACKAGE([zeromq],
                                     [zmq.h],
                                     [zmq],
                                     [zmq_init],
                                     [],
                                     [$zeromq_prefix_dir],
                                     [],
                                     [AC_MSG_RESULT([found libs at $zeromq_prefix_dir])
                                     zeromq_check_happy=yes],
                                     [AC_MSG_WARN([ZeroMQ support requested])
                                      zeromq_check_happy=no])])],
               [# Support not explicitly requested, try to build if possible
                OPAL_CHECK_PACKAGE([zeromq],
                             [zmq.h],
                             [zmq],
                             [zmq_init],
                             [],
                             [$zeromq_prefix_dir],
                             [],
                             [zeromq_check_happy=yes],
                             [AC_MSG_RESULT([ZeroMQ library not found])
                              AC_MSG_RESULT([building without ZeroMQ support])
                              zeromq_check_happy=no])])
    AC_SUBST(zeromq_CPPFLAGS)
    AC_SUBST(zeromq_LDFLAGS)
    AC_SUBST(zeromq_LIBS)

])dnl
