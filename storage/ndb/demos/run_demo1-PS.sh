#!/bin/sh

# Copyright (c) 2005, 2020, Oracle and/or its affiliates.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if [ -z "$MYSQLCLUSTER_TOP" ]; then
  echo "MYSQLCLUSTER_TOP not set"
  exit 1
fi
if [ -d "$MYSQLCLUSTER_TOP/ndb" ]; then :; else
  echo "$MYSQLCLUSTER_TOP/ndb directory does not exist"
  exit 1
fi
NDB_CONNECTSTRING=
NDB_HOME=
NDB_DEMO=$MYSQLCLUSTER_TOP/ndb/demos/1-node-PS

NDB_PORT_BASE="102"
NDB_REP_ID="5"
NDB_EXTREP_ID="4"

NDB_DEMO_NAME="Demo 1-PS MySQL Cluster"
NDB_HOST1=$1
NDB_HOST2=$2
if [ -z "$NDB_HOST1" ]; then
  NDB_HOST1=localhost
fi
if [ -z "$NDB_HOST2" ]; then
  NDB_HOST2=localhost
fi
NDB_HOST=$NDB_HOST1
NDB_EXTHOST=$NDB_HOST2

source $MYSQLCLUSTER_TOP/ndb/demos/run_demo1-PS-SS_common.sh
