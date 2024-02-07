#!/bin/sh

# Copyright (c) 2011, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

set -e

: ${load:=1}
: ${loops:=10}
: ${queries:=500}
: ${host:=127.0.0.1}
: ${port:=3306}
: ${RQG_HOME:=~/randgen-2.2.0}
: ${user:=root}
: ${data:=simple.zz}
: ${grammar:=spj_test.yy}
: ${charset:="latin1 collate latin1_bin"}
: ${EXE_MYSQL:=}
: ${EXE_MYSQLTEST:=}
: ${runtime:=}
: ${seed:=}
: ${mode:=m,nv,np}
: ${oj=}

while getopts ":nom:r:l:h:p:q:d:c:g:u:t:M:s:R:" opt; do
  case $opt in
    M)
      MYSQL_BASE_DIR="${OPTARG}"
      ;;
    R)
      RQG_HOME="${OPTARG}"
      ;;
    s)
      seed="${OPTARG}"
      ;;
    l)
      loops="${OPTARG}"
      ;;
    q)
      queries="${OPTARG}"
      ;;
    d)
      data="${OPTARG}"
      ;;
    c)
      charset="${OPTARG}"
      ;;
    g)
      grammar="${OPTARG}"
      ;;
    h)
      host="${OPTARG}"
      ;;
    p)
      port="${OPTARG}"
      ;;
    u)
      user="${OPTARG}"
      ;;
    t)
      runtime="${OPTARG}"
      ;;
    n)
      load="";
      ;;
    m)
      mode="${OPTARG}"
      ;;
    o)
      oj=1
      ;;
    \?)
      echo "Usage: `basename $0` [options]                        " >&2
      echo "-R <RQH_HOME>         :                               " >&2
      echo "-M <mysql install>    :                               " >&2
      echo "-l <no of loops>      :                               " >&2
      echo "-q <queries per loop> :                               " >&2
      echo "-t <runtime>          : in seconds, overrides loops   " >&2
      echo "-h <host>             :                               " >&2
      echo "-p <port>             :                               " >&2
      echo "-u <user>             :                               " >&2
      echo "-d <data-spec>        :                               " >&2
      echo "-g <grammar-spec>     :                               " >&2
      echo "-n                    : skip creating/loading database" >&2
      echo "-c <charsets>         : charsets                      " >&2
      echo "-m <mode>             :                               " >&2
      echo "-s <seed>             :                               " >&2
      echo "-o                    : oj schema extentions          " >&2

      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

if [ ! -x "${RQG_HOME}/gensql.pl" ]
then
    echo "Failed to locate \"${RQG_HOME}/gensql.pl\", check RQG_HOME"
    exit 1
fi

if [ ! -x "${RQG_HOME}/gendata.pl" ]
then
    echo "Failed to locate \"${RQG_HOME}/gendata.pl\", check RQG_HOME"
    exit 1
fi

if [ -z "$EXE_MYSQL" ]
then
    if [ -z "$MYSQL_BASE_DIR" ]
    then
	EXE_MYSQL=`which mysql`
    else
	EXE_MYSQL="$MYSQL_BASE_DIR/bin/mysql"
    fi
fi

if [ -z "$EXE_MYSQLTEST" ]
then
    if [ -z "$MYSQL_BASE_DIR" ]
    then
	EXE_MYSQLTEST=`which mysqltest`
    else
	EXE_MYSQLTEST="$MYSQL_BASE_DIR/bin/mysqltest"
    fi
fi


if [ ! -x "$EXE_MYSQL" ]
then
    echo "Failed to locate mysql binary"
    exit 1
fi

if [ ! -x "$EXE_MYSQLTEST" ]
then
    echo "Failed to locate mysqltest binary"
    exit 1
fi

if [ ! -f "${data}" ]
then
    if [ -f "`dirname $0`/${data}" ]
    then
	data="`dirname $0`/${data}"
    fi
fi

if [ ! -f "${grammar}" ]
then
    if [ -f "`dirname $0`/${grammar}" ]
    then
	grammar="`dirname $0`/${grammar}"
    fi
fi


dsn="dbi:mysql:host=${host}:port=${port}:user=root"
gensql=${RQG_HOME}/gensql.pl
gendata=${RQG_HOME}/gendata.pl
mysql_exe="$EXE_MYSQL --show-warnings --user=${user} --host=${host} --port=${port}"
mysqltest_exe="$EXE_MYSQLTEST --user=${user} --host=${host} --port=${port}"
export RQG_HOME

getepochtime="date +%s"
if [ `uname` = "SunOS" ]
then
    getepochtime="nawk 'BEGIN{print srand();}'"
fi

md5sum="md5sum"
if [ `uname` = "SunOS" ]
then
    md5sum="digest -a md5"
fi

awk_exe=awk
if [ `uname` = "SunOS" ]
then
    awk_exe=gawk
fi

pre="spj"
opre="$pre.$$"
innodb_db="${pre}_innodb"
ndb_db="${pre}_ndb"
