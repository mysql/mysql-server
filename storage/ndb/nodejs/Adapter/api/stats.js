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

"use strict";

var assert    = require("assert"),
    util      = require("util"),
    http      = require("http");

var global_stats;
var running_servers = {};
var unified_debug = require("./unified_debug");
var udebug = unified_debug.getLogger("STATS");
var DETAIL = udebug.is_detail();

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


/* registerStats(statsObject, keyPart, ...)
*/
exports.register = function(userStatsContainer) {
	var statParts, statsDomain, globalStatsNode, i;
	statParts = [];
	for(i = 1 ; i < arguments.length - 1; i++) {
		statParts.push(arguments[i]);
	}
	statsDomain = arguments[i];  // the final part of the domain
	
	assert(typeof userStatsContainer === 'object');
	globalStatsNode = getStatsDomain(global_stats, statParts, statParts.length);
	globalStatsNode[statsDomain] = userStatsContainer;
	return this;
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


exports.peek = function(query) {
  var parts;
  var tree = global_stats;  
  if(query) {
    parts = parseStatsUrl(query);
    tree = getStatsDomain(global_stats, parts, parts.length);
  }
  console.log(JSON.stringify(tree));
};


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



