# -*- shell-script -*-
#
# Copyright (c) 2014-2015  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dnl PGSQL_FIND_DB_USING_PG_CONFIG
dnl Use pg_config program to locate Postgres headers and libraries.
dnl
dnl If successful, writes LIBS, CPPFLAGS and LDFLAGS for opal_db_postgres
dnl and sets opal_db_postgres_found to "yes".
dnl
dnl We just check that header and library exist, we don't try to compile
dnl them and verify a symbol is in the library.


AC_DEFUN([PGSQL_FIND_DB_USING_PG_CONFIG],

 [ AC_CHECK_PROG([PGSQL_CONFIG], [pg_config], [pg_config], [not_found])

   pgsql_config_extra_libdir=""
   pgsql_config_libdir=""
   pgsql_config_incdir=""

   if test "x$PGSQL_CONFIG" != xnot_found  ; then

     temp_dir=`$PGSQL_CONFIG --includedir`

     if test -e $temp_dir/libpq-fe.h ; then
       pgsql_config_incdir=$temp_dir
     fi

     # On most systems libpq.so is in "pg_config --libdir".  On one, there are
     # some libraries there, but libpq itself is in /usr/lib64.
     # If libpq.so is not in pg_config_library_dir, we'll still include the
     # directory on the link line in case those libs are needed.

     temp_dir=`$PGSQL_CONFIG --libdir`

     for pgsql_dir in $temp_dir /usr/lib /usr/lib64 ; do
       if test -e $pgsql_dir/libpq.so || test -e $pgsql_dir/libpq.a ; then
         pgsql_config_libdir=$pgsql_dir
         if test "$pgsql_dir" != "$temp_dir" ; then
           pgsql_config_extra_libdir=$temp_dir
         fi
         break
       fi
     done
   fi

   if test "xpgsql_config_incdir" != "x" && test "xpgsql_config_libdir" != "x" ; then
     opal_db_postgres_LIBS="-lpq"
     opal_db_postgres_CPPFLAGS="-I$pgsql_config_incdir"
     if test "xpgsql_config_extra_libdir" != "x"; then
       opal_db_postgres_LDFLAGS="-L$pgsql_config_extra_libdir -L$pgsql_config_libdir"
     else
       opal_db_postgres_LDFLAGS="-L$pgsql_config_libdir"
     fi
     opal_db_postgres_found="yes"
   fi
 ])


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
                   [test "$with_postgres" = "yes"],
                   [PGSQL_FIND_DB_USING_PG_CONFIG
                   if test $opal_db_postgres_found = yes; then
                     AC_MSG_RESULT([found])
                   else
                     AC_MSG_ERROR(["PostgreSQL not found"])
                   fi],
                   [# Support explicitly requested (with_postgres=path)
                    # Headers must be at path/include, library must be at path/lib or path/lib64
                    opal_db_postgres_prefix_dir=$with_postgres
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
                                        opal_db_postgres_failed="yes"])])],

            [# Support not explicitly requested, try to build if possible
             PGSQL_FIND_DB_USING_PG_CONFIG
             if test $opal_db_postgres_found = yes; then
               AC_MSG_RESULT([found])
             else
               AC_MSG_WARN([headers and/or library not found])
             fi])
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
