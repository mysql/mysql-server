/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

'use strict';

var sys = require('util'), 
    http = require('http'),
    mynode = require(".."),
    stats = require("../Adapter/api/stats");

function startWebServer(err, sessionFactory) {
  function runServer(req, res) {

    function onSession(err, session) {
      function onClose() {};

      function respond(err, data) {
        if(err || ! data) {
          res.writeHead(500);
          console.log("find error!");
        }
        else {
          res.writeHead(200, {'Content-Type': 'text/html'});
          res.write(data.id +"\t" + data.cint + "\n");
        }
        res.end();
        session.close(onClose);
      }

      if(err || !session) {
        res.writeHead(500);
        console.log("openSession error!");
        res.end();
      }
      else {
        session.find("a", Math.floor(Math.random()*4000), respond);
      }
    }

    /* runServer() starts here */  
    sessionFactory.openSession(null, onSession);
  }

  /* startWebServer() starts here */
  http.createServer(runServer).listen(8080);
  console.log("Server started");
}

function showStatsOnExit() {
  stats.peek();
  process.exit(0);
}

function main() {
  var properties = new mynode.ConnectionProperties(process.argv[2]);
  properties.database = "jscrund";
  properties.mysql_user = "root";
  process.on('SIGINT', showStatsOnExit); 
  process.on('SIGUSR1', stats.peek);
  mynode.connect(properties, null, startWebServer);
}

main();
