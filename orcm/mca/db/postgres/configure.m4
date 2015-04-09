dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
dnl Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_db_postgres_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_postgres_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/postgres/Makefile])

    orcm_db_postgres_check_prefix_dir=""

    AC_MSG_CHECKING([for PostgreSQL support])

    AC_ARG_WITH([postgres],
                [AC_HELP_STRING([--with-postgres(=DIR)],
                                [Build PostgreSQL support, optionally adding DIR to the search path])],
                [# with_postgres=yes|no|path
                 AS_IF([test "$with_postgres" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([support not requested])
                        [$2]],
                       [# Support explicitly requested (with_postgres=yes|path)
                        AS_IF([test "$with_postgres" != "yes"],
                              [orcm_db_postgres_check_prefix_dir=$with_postgres])
                        OPAL_CHECK_PACKAGE([db_postgres],
                                           [libpq-fe.h],
                                           [postgres],
                                           [PQsetdbLogin],
                                           [],
                                           [$orcm_db_postgres_check_prefix_dir],
                                           [],
                                           [$1],
                                           [AC_MSG_WARN([PostgreSQL database support requested])
                                            AC_MSG_WARN([but required library not found or link test failed])
                                            AC_MSG_ERROR([Cannot continue])
                                            $2])])],
                [# Support not explicitly requested, try to build if possible
                 OPAL_CHECK_PACKAGE([db_postgres],
                                    [libpq-fe.h],
                                    [postgres],
                                    [PQsetdbLogin],
                                    [],
                                    [],
                                    [],
                                    [$1],
                                    [AC_MSG_WARN([PostgreSQL library not found or link test failed])
                                     AC_MSG_WARN([building without PostgreSQL support])
                                     $2])])

    AC_SUBST(db_postgres_CPPFLAGS)
    AC_SUBST(db_postgres_LDFLAGS)
    AC_SUBST(db_postgres_LIBS)

])dnl
