#!/bin/bash

# Copyright (c) 2013, 2014 Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if [ "$MYSQL_HOME" = "" ] ; then
  source ../env.properties
  echo "MYSQL_HOME=$MYSQL_HOME"
  PATH="$MYSQL_LIBEXEC:$MYSQL_BIN:$PATH"
fi

#set -x

cwd="$(pwd)"
mylogdir="$cwd/ndblog"
mkdir -p "$mylogdir"
myini="$cwd/../config.ini"

echo
echo start mgmd...
# options:
# --print-defaults         Print the program argument list and exit.
# -P, --print-full-config  Print full config and exit
# -c, --ndb-connectstring=name  Set connect string for connecting to ndb_mgmd
#                          "[nodeid=<id>;][host=]<hostname>[:<port>]"
#                          Overrides entries in NDB_CONNECTSTRING and my.cnf
# --bind-address=name      Local bind address
# -f, --config-file=name   Specify cluster configuration file
# --mycnf                  Read cluster config from my.cnf
# --initial                Delete all binary config files and start from
#                          config.ini or my.cnf.  Only in > 6.3
( cd "$mylogdir" ; "ndb_mgmd" --initial -f "$myini" )
#( cd "$mylogdir" ; "ndb_mgmd" --debug --initial -f "$myini" )
#
# XXX no effect: -c "localhost:1187"
#( cd "$mylogdir" ; "ndb_mgmd" --initial -c "localhost:1187" -f "$myini" )
# see ndb_1_cluster.log
#     [MgmtSrvr] INFO     -- Got initial configuration from '../../config.ini', will try to set it when all ndb_mgmd(s) started
#     [MgmtSrvr] INFO     -- Id: 1, Command port: *:1186
# MgmtSrvr.cpp sets default port:
#   ndb_mgm_set_connectstring(mgm_handle, buf.c_str());
# ConfigManager.cpp likely not called:
#   m_connect_string(connect_string),
#
# port number seems not supported as part of --bind-address="localhost:1187"
#( cd "$mylogdir" ; "ndb_mgmd" --bind-address="localhost:1187" -f "$myini" )

#echo
#ps -efa | grep ndb

#set +x
