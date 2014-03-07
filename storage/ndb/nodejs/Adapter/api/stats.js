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

"use strict";

/*global assert, util */
var global_stats;
var running_servers = {};
var unified_debug = require("./unified_debug");
var udebug = unified_debug.getLogger("STATS");
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

function dot(argsList, length) {
  var i, r = "";
  for (i = 0 ; i < length ; i++) {
    if(typeof argsList[i] !== 'undefined') {
      if(i) r += ".";
      r += argsList[i];
    }
  }
  return r;
}

function dotted_name(base, keyPath) { 
  return base + "." + dot(keyPath, keyPath.length);
}

function stats_incr(baseDomain, basePrefix, keyPath) {
  var len = keyPath.length - 1;
  var domain = getStatsDomain(baseDomain, keyPath, len);
  var key = keyPath[len];

  if(domain[key]) {
    domain[key] += 1;
  }
  else { 
    domain[key] = 1;
  }

  udebug.log(dotted_name(basePrefix, keyPath), "INCR", "(" + domain[key] + ")");
}

function stats_set(baseDomain, basePrefix, keyPath, value) {
  var len = keyPath.length - 1;
  var domain = getStatsDomain(baseDomain, keyPath, len);
  var key = keyPath[(len)];
   
  domain[key] = value;
  udebug.log(dotted_name(basePrefix, keyPath), "SET", value);
}

function stats_push(baseDomain, basePrefix, keyPath, value) {
  var len = keyPath.length - 1;
  var domain = getStatsDomain(baseDomain, keyPath, len);
  var key = keyPath[(len)];

  if(! Array.isArray(domain[key])) {
    domain[key] = [];
  }  
  domain[key].push(value);

  udebug.log(dotted_name(basePrefix, keyPath), "PUSH", value);
}

function testPathIsArray(keyOrPath) {
  var r = true;
  if(typeof keyOrPath === 'string') {
    r = false;
  }
  else if(! Array.isArray(keyOrPath)) {
    throw new Error("stats.incr() takes string key or array key path");
  }    
  return r;
}


exports.getWriter = function(domainPath) {
  var statWriter = {};
  var thisDomain = getStatsDomain(global_stats, domainPath, domainPath.length);
  var prefix = dot(domainPath, domainPath.length);
  
  statWriter.incr = function(keyOrPath) {
    return( testPathIsArray(keyOrPath) ? 
      stats_incr(thisDomain, prefix, keyOrPath) :
      stats_incr(thisDomain, prefix, [ keyOrPath ] )
    );
  };

  statWriter.set = function(keyOrPath, value) {
    return( testPathIsArray(keyOrPath) ? 
      stats_set(thisDomain, prefix, keyOrPath,      value) :
      stats_set(thisDomain, prefix, [ keyOrPath ] , value )
    );
  };
  
  statWriter.push = function(keyOrPath, value) {
    return( testPathIsArray(keyOrPath) ? 
      stats_push(thisDomain, prefix, keyOrPath,      value) :
      stats_push(thisDomain, prefix, [ keyOrPath ] , value )
    );      
  };
  
  return statWriter;
};


exports.peek = function() {
  console.log(JSON.stringify(global_stats));
};


exports.query = function(path) {
  return getStatsDomain(global_stats, path, path.length);
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
    if(running_servers.hasOwnProperty(key)) {
      running_servers[key].close();
    }
  }
};



