dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_db_odbc_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_odbc_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/odbc/Makefile])

    orcm_db_odbc_check_save_CPPFLAGS=$CPPFLAGS
    orcm_db_odbc_check_save_LDFLAGS=$LDFLAGS
    orcm_db_odbc_check_save_LIBS=$LIBS

    orcm_db_odbc_check_prefix_dir=""

    AC_MSG_CHECKING([for ODBC support])

    AC_ARG_WITH([odbc],
                [AC_HELP_STRING([--with-odbc(=DIR)],
                                [Build ODBC support, optionally adding DIR to the search path])],
                [# with_odbc=yes|no|path
                 AS_IF([test "$with_odbc" = "no"],
                       [# Support explicitly not requested
                        AC_MSG_RESULT([support not requested])
                        [$2]],
                       [# Support explicitly requested (with_odbc=yes|path)
                        AS_IF([test "$with_odbc" != "yes"],
                              [orcm_db_odbc_check_prefix_dir=$with_odbc])
                        OPAL_CHECK_PACKAGE([db_odbc],
                                           [sql.h],
                                           [odbc],
                                           [SQLConnect],
                                           [],
                                           [$orcm_db_odbc_check_prefix_dir],
                                           [],
                                           [$1],
                                           [AC_MSG_WARN([ODBC database support requested])
                                            AC_MSG_WARN([but required library not found or link test failed])
                                            AC_MSG_ERROR([Cannot continue])
                                            $2])])],
                [# Support not explicitly requested, try to build if possible
                 OPAL_CHECK_PACKAGE([db_odbc],
                                    [sql.h],
                                    [odbc],
                                    [SQLConnect],
                                    [],
                                    [],
                                    [],
                                    [$1],
                                    [AC_MSG_WARN([ODBC library not found or link test failed])
                                     AC_MSG_WARN([building without ODBC support])
                                     $2])])

    CPPFLAGS=$orcm_db_odbc_check_save_CPPFLAGS
    LDFLAGS=$orcm_db_odbc_check_save_LDFLAGS
    LIBS=$orcm_db_odbc_check_save_LIBS

    AC_SUBST(db_odbc_CPPFLAGS)
    AC_SUBST(db_odbc_LDFLAGS)
    AC_SUBST(db_odbc_LIBS)

])dnl
