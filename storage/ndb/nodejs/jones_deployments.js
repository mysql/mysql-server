/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

var deployments = {};

/* The test deployment is used in automated testing of the Jones projects
*/
deployments.test = function(properties) {
  if(process.env["PORT_SQL1"]) {
    properties.mysql_host = process.env["HOSTNAME"];
    properties.mysql_port = process.env["PORT_SQL1"];
    properties.ndb_connectstring =
      process.env["HOSTNAME"] + ":" + process.env["PORT_MGMD"];
  }
};

/* This deployment uses an environment set up by mysql-test-run:
     ./mtr --debug --start ndb.ndb_basic
*/
deployments.mtr = function(properties) {
  properties.mysql_host = "127.0.0.1";
  properties.mysql_port = 13001;
  properties.ndb_connectstring = "localhost:13000";
};

/* Example of a deployment function defining a production environment.
   This function would be invoked from the code:
     var properties = new jones.ConnectionProperties("mysql", "production");
*/
deployments.production = function(properties) {
  properties.mysql_host = "rw.db.prod.mysite.com";
  properties.mysql_user = "prod_web_user";
  properties.mysql_password = "secretPassword";
  properties.database = "my_app_db";
};


/* Another example of a deployment function; this one might define a 
   connection to a read-only slave server.
*/
deployments.prod_slave = function(properties) {
  deployments.production(properties);
  properties.mysql_host = "ro.db.prod.mysite.com";   // read-only host
};

module.exports = deployments;
