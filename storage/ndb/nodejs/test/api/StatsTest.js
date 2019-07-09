/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

/** This is the smoke test for the spi suite.
    We go just as far as getDBServiceProvider().
    This tests the loading of required compiled code in shared library files.
 */

"use strict";

var http = require("http");

var stats_module = require(mynode.api.stats);

var test = new harness.SerialTest("statsServer");

var stats_server_port = 15301;

test.run = function() {
  var t = this;

  function onResult(response) {
    if(response.statusCode === 200) {
      t.pass();
    }
    else {
      t.fail(response.statusCode);
    }
    /* To go an extra step here, use response.on('data') to validate the 
       response body */
  }
  
  function statsQuery() {
    var requestParams = {
      host: 'localhost',
      port: stats_server_port,
      path: '/'
    };

    var req = http.get(requestParams, onResult);
    req.on('error', function() { t.fail("connect error"); });
  }

  stats_module.startStatsServer(stats_server_port, "localhost", statsQuery);
};

test.cleanup = function() {
  stats_module.stopStatsServers();
};

module.exports.tests = [ test ] ;

