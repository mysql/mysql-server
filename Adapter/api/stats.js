/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

var global_stats;
var running_servers = {};

var http = require("http");

/* Because modules are cached, this initialization should happen only once. 
   If you try to do it twice the assert will fail.
*/ 
assert(typeof(global_stats) === 'undefined');
global_stats = {};


function getStatsDomain(root, keys, nparts) {
  var i, key;
  var stat = root;

  for(i = 0 ; i < nparts; i++) {
    key = keys[i];
    if(typeof(stat[key]) === 'undefined') {
      stat[key] = {};
    }
    stat = stat[key];
  }
  return stat;
}


exports.getWriter = function() {
  var statWriter = {};
  var thisDomain = getStatsDomain(global_stats, arguments, arguments.length);
  
  statWriter.incr = function() {
    var len = arguments.length - 1;
    var domain = getStatsDomain(thisDomain, arguments, len);
    var key = arguments[(len)];

    if(domain[key]) {
      domain[key] += 1;
    }
    else { 
      domain[key] = 1;
    }
  };

  statWriter.set = function() {
    var len = arguments.length - 2;
    var domain = getStatsDomain(thisDomain, arguments, len);
    var key = arguments[len];
    var value = arguments[len + 1];
    
    domain[key] = value;
  };
  
  statWriter.push = function() {
    var len = arguments.length - 2;
    var domain = getStatsDomain(thisDomain, arguments, len);
    var key = arguments[len];
    var value = arguments[len + 1];
    
    if(! Array.isArray(domain[key])) {
      domain[key] = [];
    }
    
    domain[key].push(value);
  };
  
  return statWriter;
};


exports.peek = function() {
  console.log(JSON.stringify(global_stats));
};


exports.query = function() {
  return getStatsDomain(global_stats, arguments, arguments.length);
};


/* Translate a URL like "/a/b/" into an array ["a","b"] 
*/
function parseStatsUrl(url) {
  var parts = url.split("/");
  if(parts[0].length == 0) {
    parts.shift();
  }
  if(parts[parts.length-1].length == 0) {
    parts.pop();
  }
  return parts;
}


exports.startStatsServer = function(port, host, callback) {
  var key = host + ":" + port;
  var server;

  function onStatsRequest(req, res) {
    var parts, stats, response;
    parts = parseStatsUrl(req.url);
    
    stats = getStatsDomain(global_stats, parts, parts.length);    
    res.writeHead(200, {'Content-Type': 'text/plain'});
    response = util.inspect(stats, true, null, false) + "\n";
    res.end(response);
  }

  if(running_servers[key]) {
    server = running_servers[key];
  }
  else {
    server = http.createServer(onStatsRequest);
    running_servers[key] = server;
    server.listen(port, host, callback);
  }
  
  return server;
};


exports.stopStatsServers = function() {
  var key;
  for(key in running_servers) {
    running_servers[key].close();
  }
}

