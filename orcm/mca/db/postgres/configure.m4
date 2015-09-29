dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
dnl Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

dnl PGSQL_FIND_INCLUDE_SERVER_USING_PG_CONFIG(location of PostgreSQL servers)
dnl Use pg_config program to locate Postgres server headers
dnl
dnl Writes pg_config_include_server dir 

AC_DEFUN([PGSQL_FIND_INCLUDE_SERVER_USING_PG_CONFIG],

 [ if test "x[$1]" != "x" ; then      # case where --with-postgres={directory}
     temp_dir=[$1]/include/server 
     if test -e $temp_dir/postgres_fe.h  && test -e $temp_dir/catalog/pg_type.h; then
       opal_db_postgres_include_server_dir=$temp_dir
     fi
   fi

   if test x"$opal_db_postgres_include_server_dir" = xunset ; then

     AC_CHECK_PROG([PGSQL_CONFIG], [pg_config], [pg_config], [not_found])

     if test "x$PGSQL_CONFIG" != xnot_found  ; then

       temp_dir=`$PGSQL_CONFIG --includedir-server`

       if test -e $temp_dir/postgres_fe.h && test -e $temp_dir/catalog/pg_type.h; then
           opal_db_postgres_include_server_dir=$temp_dir
       fi
     fi
   fi
 ])


# MCA_db_postgres_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_postgres_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/postgres/Makefile])

    AC_REQUIRE([OPAL_CHECK_POSTGRES])
    # This will set:
    # opal_db_postgres_prefix_dir
    # opal_db_postgres_found, opal_db_postgres_failed,
    # opal_db_postgres_CPPFLAGS, opal_db_postgres_LDFLAGS, and
    # opal_db_postgres_LIBS

    if test "$opal_db_postgres_found" = "yes" ; then

      AC_MSG_CHECKING([for PostgreSQL server header files])
      opal_db_postgres_include_server_dir=unset
      PGSQL_FIND_INCLUDE_SERVER_USING_PG_CONFIG($opal_db_postgres_prefix_dir)

      if test "x$opal_db_postgres_include_server_dir" = xunset ; then
        opal_db_postgres_found=no
        AC_MSG_RESULT([not found])
      else
        opal_db_postgres_CPPFLAGS="$opal_db_postgres_CPPFLAGS -I$opal_db_postgres_include_server_dir"
        AC_MSG_RESULT([found])
      fi
    fi

    AS_IF([test "$opal_db_postgres_found" = "yes"],
          [$1],
          [$2
           AS_IF([test "$opal_db_postgres_failed" = "yes"],
                 [AC_MSG_ERROR([Cannot continue])])])
    
    AC_SUBST(opal_db_postgres_CPPFLAGS)
    AC_SUBST(opal_db_postgres_LDFLAGS)
    AC_SUBST(opal_db_postgres_LIBS)

])dnl
