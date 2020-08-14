/*
   Copyright (c) 2014, 2020, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  MySQL Connection Properties

*/ 

var MysqlDefaultConnectionProperties = {
  "implementation" : "mysql",
  "engine"         : "ndb",
  "database"       : "test",
  
  "mysql_host"     : "localhost",
  "mysql_port"     : 3306,
  "mysql_user"     : "root",
  "mysql_password" : "",
  "mysql_charset"  : "UTF8MB4",
  "mysql_sql_mode" : "STRICT_ALL_TABLES",
  "mysql_socket"   : null,
  "debug"          : true,
  "mysql_trace"    : false,
  "mysql_debug"    : false,
  "mysql_pool_size": 10
};


/* This file is valid JavaScript 
*/
module.exports = MysqlDefaultConnectionProperties;
