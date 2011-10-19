#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ([2.63b])
AC_INIT([handlersocket-plugin], [1.0.6], [https://github.com/ahiguti/HandlerSocket-Plugin-for-MySQL/issues])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([libhsclient/fatal.cpp])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL

ac_mysql_debug=
AC_ARG_ENABLE(mysql-debug,
  [AS_HELP_STRING([--enable-mysql-debug], [specify whether MySQL is build with DBUG_ON])],[ac_mysql_debug="$enableval"],[ac_mysql_debug=no])
AC_MSG_CHECKING([if --enable-mysql-debug is specified])
AC_MSG_RESULT($ac_mysql_debug)

AC_DEFUN([CONFIG_OPTION_MYSQL],[
  AC_MSG_CHECKING([mysql source])

  MYSQL_SOURCE_VERSION=
  MYSQL_INC=
  ac_mysql_source_dir=
  AC_ARG_WITH([mysql-source],
    [AS_HELP_STRING([--with-mysql-source=PATH], [MySQL source directory PATH])],
    [
      ac_mysql_source_dir=`cd $withval && pwd`
      if test -f "$ac_mysql_source_dir/sql/handler.h" ; then
        MYSQL_INC="-I$ac_mysql_source_dir/sql"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir/include"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir/regex"
        MYSQL_INC="$MYSQL_INC -I$ac_mysql_source_dir"
        AC_SUBST(MYSQL_INC)
        if test -f "$ac_mysql_source_dir/VERSION"; then
          source "$ac_mysql_source_dir/VERSION"
          MYSQL_SOURCE_VERSION="$MYSQL_VERSION_MAJOR.$MYSQL_VERSION_MINOR.$MYSQL_VERSION_PATCH"
        else
          if test -f "$ac_mysql_source_dir/configure.in"; then
            MYSQL_SOURCE_VERSION=`cat $ac_mysql_source_dir/configure.in | grep "\[[MySQL Server\]]" | sed -e "s|.*\([[0-9]]\+\.[[0-9]]\+\.[[0-9]]\+[[0-9a-zA-Z\_\-]]*\).*|\1|"`
          else
            AC_MSG_ERROR([invalid MySQL source directory: $ac_mysql_source_dir])
          fi
        fi
        AC_MSG_RESULT([yes: Using $ac_mysql_source_dir, version $MYSQL_SOURCE_VERSION])
      else
        AC_MSG_ERROR([invalid MySQL source directory: $ac_mysql_source_dir])
      fi
    ],
    [AC_MSG_ERROR([--with-mysql-source=PATH is required for standalone build])]
  )

  MYSQL_BIN_VERSION=
  ac_mysql_config=
  AC_ARG_WITH([mysql-bindir],
    [AS_HELP_STRING([--with-mysql-bindir=PATH], [MySQL binary directory PATH. This should be the directory where mysql_config is located.])],
    [
      mysql_bin_dir=`cd $withval 2> /dev/null && pwd || echo ""`
      ac_mysql_config="$mysql_bin_dir/mysql_config"
    ],
    [
      AC_PATH_PROG([ac_mysql_config], [mysql_config])
    ]
  )

  AC_MSG_CHECKING([mysql binary])
  if test ! -x "$ac_mysql_config" ; then
    AC_MSG_ERROR([mysql_config not found! You have to specify the directory where mysql_config resides to --with-mysql-bindir=PATH.])
  fi

  MYSQL_CFLAGS_ADD=`"$ac_mysql_config" --cflags`
  MYSQL_CFLAGS="$MYSQL_CFLAGS $MYSQL_CFLAGS_ADD"
  if test "$ac_mysql_debug" = "yes"; then
    MYSQL_CFLAGS="$MYSQL_CFLAGS -DDBUG_ON -DENABLED_DEBUG_SYNC"
  else
    MYSQL_CFLAGS="$MYSQL_CFLAGS -DDBUG_OFF"
  fi
  AC_SUBST(MYSQL_CFLAGS)

  MYSQL_BIN_VERSION=`"$ac_mysql_config" --version`
  AC_MSG_RESULT([yes: Using $ac_mysql_config, version $MYSQL_BIN_VERSION])

  MYSQL_LIB=`"$ac_mysql_config" --libs_r`
  LIB_DIR=`echo $MYSQL_LIB | sed -e "s|.*-L/|/|" | sed -e "s| .*||"`
    # FIXME
  if test a`basename "$LIB_DIR"` = amysql ; then
    MYSQL_LIB="-L`dirname $LIB_DIR` $MYSQL_LIB" 
      # FIXME
  fi
  AC_SUBST(MYSQL_LIB)

  if test a$MYSQL_SOURCE_VERSION != a$MYSQL_BIN_VERSION ; then
    AC_MSG_ERROR([MySQL source version does not match MySQL binary version])
  fi

  AC_MSG_CHECKING([mysql plugin dir])
  ac_mysql_plugin_dir=
  AC_ARG_WITH([mysql-plugindir],
    [AS_HELP_STRING([--with-mysql-plugindir=PATH], [MySQL plugin directory where handlersocket.so to be copied])],
    [
      ac_mysql_plugin_dir=`cd $withval && pwd`
      if test -d "$ac_mysql_plugin_dir/" ; then
        PLUGIN_DIR="$ac_mysql_plugin_dir"
        AC_SUBST(PLUGIN_DIR)
        AC_MSG_RESULT([yes: Using $ac_mysql_plugin_dir])
      else
        AC_MSG_ERROR([invalid MySQL plugin directory : $ac_mysql_plugin_dir])
      fi
    ],
    [
      LIB_DIR_TMP=`"$ac_mysql_config" --plugindir`
      if test ! -d "$LIB_DIR_TMP"; then
        LIB_DIR_TMP=`"$ac_mysql_config" --libs_r | sed -e "s|.*-L/|/|" | sed -e "s| .*||"`/plugin
          # FIXME
      fi
      ac_mysql_plugin_dir=$LIB_DIR_TMP
      PLUGIN_DIR="$ac_mysql_plugin_dir"
      AC_SUBST(PLUGIN_DIR)
      AC_MSG_RESULT([--with-mysql-plugindir was not set. Using $ac_mysql_plugin_dir])
    ]
  )
])

HANDLERSOCKET_SUBDIRS="libhsclient"
AC_ARG_ENABLE(handlersocket_server,
  [  --enable-handlersocket-server    build HandlerSocket plugin (defalut=yes)])
if test "$enable_handlersocket_server" != "no"; then
  CONFIG_OPTION_MYSQL
  HANDLERSOCKET_SUBDIRS="libhsclient handlersocket client"
fi
AC_SUBST(HANDLERSOCKET_SUBDIRS)

CFLAGS="$CFLAGS -Werror"
CXXFLAGS="$CXXFLAGS -Wall -g -fno-rtti -fno-exceptions -fPIC -DPIC"

AC_CONFIG_FILES([Makefile
                 handlersocket/Makefile
                 libhsclient/Makefile
                 client/Makefile])

AC_OUTPUT
