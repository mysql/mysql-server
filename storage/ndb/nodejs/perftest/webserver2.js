/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
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
