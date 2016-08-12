dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2016      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl
dnl Additional copyrights may follow
dnl
dnl $HEADER$
dnl

# MCA_db_zeromq_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_zeromq_CONFIG], [
    # Check if zeromq support is available or not.
    AC_CONFIG_FILES([orcm/mca/db/zeromq/Makefile])
    AC_MSG_CHECKING([for ZeromMQ db support])
    AC_REQUIRE([OPAL_CHECK_ZEROMQ])

   AC_ARG_WITH([zeromq],
                [AC_HELP_STRING([--with-zeromq(=yes/no)],
                                [Build ZeroMQ db support])],
                [AC_MSG_RESULT([with_zeromq selected with parameter "$with_zeromq"!])]
                [AS_IF([test "$zeromq_check_happy" = "no" -a "$with_zeromq" = "yes"],
                        AC_MSG_RESULT([ZeroMQ Check failed and we need to skip building zeromq db Component here])
                        AC_MSG_WARN([ZeroMQ Libs not present])
                        AC_MSG_ERROR([ZeroMQ DB requested but dependency not met])
                        $2)]
                [AS_IF([test "$zeromq_check_happy" = "yes" -a "$with_zeromq" = "yes"],
                        AC_MSG_RESULT([ZeroMQ Check passed and ZeromMQ db explicitly requested])
                        AC_MSG_RESULT([ZeroMQ DB requested and dependency met and zeromq db will be built])
                        $1)]
                [AS_IF([test "$zeromq_check_happy" = "yes" -a "$with_zeromq" = "no"],
                        AC_MSG_RESULT([ZeroMQ Check passed and but ZeroMQ db explicitly not requested])
                        AC_MSG_RESULT([ZeroMQ DB not requested and zeromq db will not be built])
                        $2)]
                [AS_IF([test "$zeromq_check_happy" = "no" -a "$with_zeromq" = "no"],
                        AC_MSG_RESULT([ZeroMQ Check failed and ZeroMQ db explicitly not requested])
                        AC_MSG_RESULT([ZeroMQ DB not requested and dependency not met and zeromq db will not be built])
                        $2)],
                [AC_MSG_RESULT([with_zeromq not specified!])]
                [AS_IF([test "$zeromq_check_happy" = "no"],
                        AC_MSG_RESULT([ZeroMQ Check failed and ZeroMQ db not requested])
                        AC_MSG_RESULT([ZeroMQ DB not requested and dependency not met and zeromq db will not be built])
                        $2)]
                [AS_IF([test "$zeromq_check_happy" = "yes"],
                        AC_MSG_RESULT([ZeroMQ Check passed and ZeroMQ db not requested])
                        AC_MSG_RESULT([ZeroMQ DB not explicitly requested but dependency was met and zeromq db will be built])
                        $1)])
])dnl
