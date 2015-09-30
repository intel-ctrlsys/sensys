# -*- shell-script -*-
#
# Copyright (c) 2014-2015  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
# --------------------------------------------------------
dnl SPECIAL_POSTGRES_SERVER_HEADER_CHECK(headerlist, prefixdir,
dnl                                      [action if found], [action if not found])
AC_DEFUN([POSTGRES_SERVER_HEADER_CHECK],[
special_postgres_save_CPPFLAGS=$CPPFLAGS

AC_MSG_CHECKING([for PostgreSQL Server support])
AS_IF([test "$2" != "" ], [ CPPFLAGS="$CPPFLAGS -I$2/include/server" ], [])

# The C lines below are not intented on purpose.
AC_CHECK_HEADERS([$1],
                 [$3],
                 [$4],
                 [
#ifdef HAVE_POSTGRES_FE_H
#include <postgres_fe.h>
#endif
#ifdef HAVE_PG_TYPE_H
#include <pg_type.h>
#endif
                 ])
CPPFLAGS=$special_postgres_save_CPPFLAGS
])

# --------------------------------------------------------
AC_DEFUN([OPAL_CHECK_POSTGRES],[

opal_db_postgres_prefix_dir=""
opal_db_postgres_found="no"
opal_db_postgres_failed="no"

AC_MSG_CHECKING([for PostgreSQL support])

AC_ARG_WITH([postgres],
            [AC_HELP_STRING([--with-postgres(=DIR)],
                            [Build PostgreSQL support, optionally adding DIR to the search path])],
            [# with_postgres=yes|no|path
             AS_IF([test "$with_postgres" = "no"],
                   [# Support explicitly not requested
                    AC_MSG_RESULT([support not requested])],
                   [# Support explicitly requested (with_postgres=yes|path)
                    AS_IF([test "$with_postgres" != "yes"],
                          [opal_db_postgres_prefix_dir=$with_postgres])
                    OPAL_CHECK_PACKAGE([opal_db_postgres],
                                       [libpq-fe.h],
                                       [pq],
                                       [PQsetdbLogin],
                                       [],
                                       [$opal_db_postgres_prefix_dir],
                                       [],
                                       [opal_db_postgres_found="yes"],
                                       [AC_MSG_WARN([PostgreSQL database support requested])
                                        AC_MSG_WARN([but required library not found or link test failed])
                                        opal_db_postgres_failed="yes"])
                    AS_IF([test "$opal_db_postgres_found" = "yes"],
                          [# If found check to see if the required Postgres Server headers are installed
                           POSTGRES_SERVER_HEADER_CHECK([postgres_fe.h catalog/pg_type.h],
                                                        [$with_postgres],
                                                        [],
                                                        [ AC_MSG_WARN([PostgreSQL database support requested])
                                                          AC_MSG_WARN([but required server headers were not found])
                                                          opal_db_postgres_found="no"
                                                          opal_db_postgres_failed="yes"])
                          AS_IF([test "$opal_db_postgres_failed" != "yes"],
                                [opal_db_postgres_CPPFLAGS="$opal_db_postgres_CPPFLAGS -I$with_postgres/include/server"],
                                [])
                          ],
                          [])
                    ])],
            [# Support not explicitly requested, try to build if possible
             OPAL_CHECK_PACKAGE([opal_db_postgres],
                                [libpq-fe.h],
                                [pq],
                                [PQsetdbLogin],
                                [],
                                [],
                                [],
                                [opal_db_postgres_found="yes"
                                 CPPFLAGS],
                                [AC_MSG_WARN([PostgreSQL library not found or link test failed])
                                 AC_MSG_WARN([building without PostgreSQL support])])
             AS_IF([test "$opal_db_postgres_found" = "yes"],
                   [# If found check to see if the required Postgres Server headers are installed
                    POSTGRES_SERVER_HEADER_CHECK([postgres_fe.h catalog/pg_type.h],
                                                 [],
                                                 [CPPFLAGS],
                                                 [ AC_MSG_WARN([PostgreSQL database support requested])
                                                   AC_MSG_WARN([but required server headers were not found])
                                                   opal_db_postgres_found="no"])
                   ],
                   [])
            ])
])

# --------------------------------------------------------
AC_DEFUN([OPAL_CHECK_MYSQL],[
    AC_ARG_WITH([mysql],
                [AC_HELP_STRING([--with-mysql(=DIR)],
                                [Build Mysql support, optionally adding DIR to the search path (default: no)])],
	                        [], with_mysql=no)

    AC_MSG_CHECKING([if Mysql support requested])
    AS_IF([test "$with_mysql" = "no" -o "x$with_mysql" = "x"],
      [opal_check_mysql_happy=no
       AC_MSG_RESULT([no])],
      [AS_IF([test "$with_mysql" = "yes"],
             [AS_IF([test "x`ls /usr/include/mysql/mysql.h 2> /dev/null`" = "x"],
                    [AC_MSG_RESULT([not found in standard location])
                     AC_MSG_WARN([Expected file /usr/include/mysql/mysql.h not found])
                     AC_MSG_ERROR([Cannot continue])],
                    [AC_MSG_RESULT([found])
                     opal_check_mysql_happy=yes
                     opal_mysql_incdir="/usr/include/mysql"])],
             [AS_IF([test ! -d "$with_mysql"],
                    [AC_MSG_RESULT([not found])
                     AC_MSG_WARN([Directory $with_mysql not found])
                     AC_MSG_ERROR([Cannot continue])],
                    [AS_IF([test "x`ls $with_mysql/include/mysql.h 2> /dev/null`" = "x"],
                           [AS_IF([test "x`ls $with_mysql/mysql.h 2> /dev/null`" = "x"],
                                  [AC_MSG_RESULT([not found])
                                   AC_MSG_WARN([Could not find mysql.h in $with_mysql/include or $with_mysql])
                                   AC_MSG_ERROR([Cannot continue])],
                                  [opal_check_mysql_happy=yes
                                   opal_mysql_incdir=$with_mysql
                                   AC_MSG_RESULT([found ($with_mysql/mysql.h)])])],
                           [opal_check_mysql_happy=yes
                            opal_mysql_incdir="$with_mysql/include"
                            AC_MSG_RESULT([found ($opal_mysql_incdir/mysql.h)])])])])])
])
