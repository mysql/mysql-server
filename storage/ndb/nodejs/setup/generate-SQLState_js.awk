# Copyright (c) 2012, Oracle and/or its affiliates. All rights
# reserved.
# 
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA


# 
# The input to this script is {MYSQL TREE}/sql/share/errmsg-utf8.txt
# The output is {NODEJS ADAPTER TREE}/Adapter/impl/common/MysqlErrToSQLStateMap.js
#

 BEGIN  { errno = 1000 
          print "/* Automatically generated from sql/share/errmsg-utf8.txt "
          print "   by setup/generate-SQLState_js.awk"
          print "   Do not edit by hand. "
          print "*/ "
          print ""
          print "module.exports = {"
        }
 /^ER/  { if($2) {
            mysqlerr = "\"" $1 "\""
            sqlstate = "\"" $2 "\""
            printf("\t%45s : %s,\n", mysqlerr, errno);
            printf("\t%45d : %s,\n", errno, sqlstate);
          }
          errno++
        }
 END    { print "};" }
 
