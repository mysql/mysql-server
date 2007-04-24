# Copyright (C) 2007 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 59
# Temple Place, Suite 330, Boston, MA  02111-1307  USA

AC_DEFUN([_MYSQL_CONFIG],[
  AC_ARG_WITH([mysql-config],
  AS_HELP_STRING([--with-mysql-config=PATH], [A path to mysql_config script]),
                 [mysql_config="$withval"], [mysql_config=mysql_config])
])

dnl
dnl Usage:
dnl
dnl  MYSQL_CLIENT([version], [client|thread-safe|embedded])
dnl
dnl Two optional arguments:
dnl   first: The minimal version of the MySQL to use
dnl           if not specified, any version will be accepted.
dnl           The version should be specified as three numbers,
dnl           without suffixes. E.g. 4.10.15 or 5.0.3
dnl   second: force the specified library flavor to be selected,
dnl           if not specified, a user will be able to choose
dnl           between client (non-thread-safe) and embedded
dnl
dnl On successful execution sets MYSQL_CLIENT_CFLAGS and
dnl MYSQL_CLIENT_LIBS shell variables and makes substitutions
dnl out of them (calls AC_SUBST)
dnl

AC_DEFUN([MYSQL_CLIENT],[
  AC_REQUIRE([_MYSQL_CONFIG])
  AC_MSG_CHECKING([for MySQL])
  ifelse([$2], [client],
               [mysql_libs=--libs mysql_cflags=--cflags],
         [$2], [thread-safe],
               [mysql_libs=--libs_r mysql_cflags=--cflags],
         [$2], [embedded],
               [mysql_libs=--libmysqld-libs mysql_cflags=--cflags],
         [$2], [], [
    AC_ARG_WITH([mysql-library],
    AS_HELP_STRING([--with-mysql-library], ['client' or 'embedded']),
                   [mysql_lib="$withval"], [mysql_lib=client])
[                   
    case "$mysql_lib" in
      client) mysql_libs=--libs mysql_cflags=--cflags ;;
      embedded) mysql_libs=--libmysqld-libs mysql_cflags=--cflags ;;
      *) ]AC_MSG_ERROR([Bad value for --with-mysql-library])[
    esac
]
                   ],
          [AC_FATAL([Bad second (library flavor) argument to MYSQL_CLIENT])])
[
    mysql_version=`$mysql_config --version`
    if test -z "$mysql_version" ; then
      ]AC_MSG_ERROR([Cannot execute $mysql_config])[
    fi
]
    ifelse([$1], [], [], [
      ifelse(regexp([$1], [^[0-9][0-9]?\.[0-9][0-9]?\.[0-9][0-9]?$]), -1,
      [AC_FATAL([Bad first (version) argument to MYSQL_CLIENT])], [
dnl
dnl Transformation below works as follows:
dnl   assume, we have a number 1.2.3-beta
dnl   *a* line removes the suffix and adds first and last dot to the version:
dnl             .1.2.3.
dnl   *b* line adds a 0 to a "single digit surrounded by dots"
dnl             .01.2.03.
dnl       note that the pattern that matched .1. has eaten the dot for .2.
dnl       and 2 still has no 0
dnl   *c* we repeat the same replacement as in *b*, matching .2. this time
dnl             .01.02.03.
dnl   the last replacement removes all dots
dnl             010203
dnl   giving us a number we can compare with
dnl
    mysql_ver=`echo ${mysql_version}|dnl
      sed 's/[[-a-z]].*//; s/.*/.&./;dnl   *a*
           s/\.\([[0-9]]\)\./.0\1./g;dnl   *b*
           s/\.\([[0-9]]\)\./.0\1./g;dnl   *c*
           s/\.//g'`
    if test "$mysql_ver" -lt]dnl
dnl the same as sed transformation above, without suffix-stripping, in m4
    patsubst(patsubst(patsubst(.[$1]., [\.\([0-9]\)\.], [.0\1.]), [\.\([0-9]\)\.], [.0\1.]), [\.], [])[ ; then
      AC_MSG_ERROR([MySQL version $mysql_version is too low, minimum of $1 is required])
    fi
    ])])

    MYSQL_CLIENT_CFLAGS=`$mysql_config $mysql_cflags`
    MYSQL_CLIENT_LIBS=`$mysql_config $mysql_libs`
    AC_SUBST(MYSQL_CLIENT_CFLAGS)
    AC_SUBST(MYSQL_CLIENT_LIBS)

    # should we try to build a test program ?

    AC_MSG_RESULT([$mysql_version])
])

