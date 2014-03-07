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

// configure.js:
//    Try to find installed mysql that matches architecture of node
//    Ask user for mysql pathname
//    Write mysql pathname into config.gypi and config.waf

// TODO: Auto-detect mysql layout here, and write about it in config.gypi

"use strict";

var fs = require("fs"),
    path = require("path"),
    readline = require("readline");
    
var archbits, archname, archmysql;

switch(process.arch) {
  case 'ia32':
    archbits = 32;
    archmysql = 'i686';
    break;
  case 'x64':
    archbits = 64;
    archmysql = 'x86_64';
    break;
  default:
    throw "Architecture " + process.arch + " unsupported.";
}

var path_sep = ( typeof path.sep == 'undefined' ? '/' : path.sep);
 
archname = String(archbits) + '-bit';

var lf = '\n';

var greeting = 
'# '                                                                        +lf+
'#                 MySQL Cluster NoSQL API for Node.JS'                     +lf+
'#  April, 2013'                                                            +lf+
'# '                                                                        +lf+
'#  The NoSQL API for Node.JS provides lightweight object mapping for '     +lf+
'#  JavaScript.  The API can be used with two separate backend adapters:'   +lf+
'#    - The "ndb" adapter, which uses the C++ NDB API to provide'           +lf+
'#      high-performance native access to MySQL Cluster. '                  +lf+
'#    - The "mysql" adapter, which uses the node-mysql driver '             +lf+ 
'#      available from http://github.com/felixge/node-mysql'                +lf+
'# '                                                                        +lf+
'#  The mysql backend translates API calls into SQL statements and sends '  +lf+
'#  them to a MySQL server.  The ndb backend communicates directly with '   +lf+ 
'#  NDB data nodes, without translating to SQL or making any use of a '     +lf+ 
'#  MySQL Server.'                                                          +lf+
'# '                                                                        +lf+
'#  In order to build and run the ndb adapter, you must have: '             +lf+
'#    - An installation of MySQL Cluster 7.x or MySQL 5.6 '                 +lf+
'#      including headers and shared library files [' +archname +']'        +lf+
'#    - A working C++ compiler '                                            +lf+
'# ' +lf;

function verify(dir) {
  var stats;
  try {
    stats = fs.statSync(dir);
    return stats.isDirectory();
  }
  catch(e) {
    return false;
  }
}

function get_candidates_windows() {
  var candidates = [];
  var c1 = "C:\\Program Files\\MySQL Server 5.6";
  if(verify(c1)) {
    candidates.push(c1);
  }
  return candidates;
}


function get_candidates() { 
  var candidates = [];
  var link, verified;

  if(verify('/usr/share/mysql/java')) {   // RPM
    candidates.push('/usr');
  }

  if(verify('/usr/local/mysql/share/java'))  {  // Mac or generic TAR
    /* if /usr/local/mysql is a symlink, the real directory name must match
       the target architecture */
    try {
      link = fs.readlinkSync('/usr/local/mysql');
      verified = (link.indexOf(archmysql) > 0);
    }
    catch(e) { 
      verified = null;   // not a symlink
    }

    if(verified !== false) {
      candidates.push('/usr/local/mysql');
    }
  }

  if(verify('/opt/mysql/server-5.6/share/java'))  {   // Debian
    candidates.push('/opt/mysql/server-5.6');
  }
  
  return candidates;
}


function build_prompt(candidates) {
  var i = 0, found = '';
  
  if(candidates.length) {
    found = '# ' +lf+
            '# ' +lf+
            '#  Choose your preferred mysql install location: ' +lf+
            '# ' +lf;
    
    for(i ; i < candidates.length ; i++) {
      found += ' [' + String(i+1) + ']  ' + candidates[i] + lf;
    }
  }
  else {
    found = '# ' +lf+
            '#  ~~~~~~~~ No '+archname+'  MySQL Cluster installations found.' +lf+
            '# ' +lf;
  }
  found += ' [' + String(++i) + ']  Choose custom mysql directory' +lf;

  return found;
}

function finish() {
  console.log("");
  console.log("Now run this command:\n\tnode-gyp configure build -d");
  process.exit(0);
}

function testPath(mysqlPath) {
  // We assert that a path is a valid mysql install true if and only if 
  // it contains a mysql-test directory
  var testPath = path.join(mysqlPath.trim(), "mysql-test");
  return verify(testPath);
}

function configure(mysql, layout) {
  if(mysql) {
    layout = "";  // fixme
    var gyp = { "variables" : {"mysql_path":mysql, "mysql_layout":layout}};
    fs.writeFileSync("config.gypi", JSON.stringify(gyp) + "\n", "ascii");
    fs.writeFileSync("config.waf", mysql + "\n", 'ascii');
    finish();
  }
  else {
    process.exit(-1);
  }
}


// Filename completion
// Returns [ [array of matches], original substring ]
function completion(line) {
  var matches = [];
  var files = [];
  var dir, base, stat;

  function readCurrentDir(dir) {
    files = [];  // parent scope
    try {
      files = fs.readdirSync(dir);
    }
    catch(e) {}
  }
 
 if(line.slice(-1) == path_sep) {
    dir = line;
    readCurrentDir(dir);
    base = "";
  }
  else {
    dir = path.dirname(line);   // returns "." if path is unrooted
    base = path.basename(line);
    readCurrentDir(dir);
  }
 
  for(var i = 0; i < files.length ; i++) {
    if(files[i].substring(0,1) !== "." && files[i].match("^" + base)) {
      matches.push(path.join(dir, files[i]));
    }
  }
  
  if(matches.length == 1) {
    try {
      stat = fs.statSync(matches[0]);
      if(stat.isDirectory()) matches[0] += path_sep;
    }
    catch(e) {}
  }
  
  return [matches, line];
}


///// ****** MAIN ROUTINE STARTS HERE ****** /////
function main() {
  var candidates;
  if(process.platform == 'win32')
    candidates = get_candidates_windows();
  else
    candidates = get_candidates();
  var text = build_prompt(candidates);
  var rl = readline.createInterface(process.stdin, process.stdout, completion);
  
  function hangup() {
    rl.close();
    process.exit(-1);
  }

  function onEntry(choice) {
    var range = candidates.length + 1;
    var num = Number(choice);

    if(num == NaN) {  // user skipped straight to pathname entry
      onPath(choice); 
    }
    else if(num < 1 || num > range) {
      rl.write("Please enter a number between 1 and " + range + "." + lf);
      rl.write("Hit CTRL-C to exit." +lf);
      rl.prompt(true);
    }
    else if(num === (range - 1)) {
      rl.removeListener('line', onEntry);
      customMode();
    }
    else {
      rl.close();
      configure(candidates[num - 1]);
    }
  }

  function onPath(mysqlPath) {
    if(testPath(mysqlPath)) {
      rl.close();
      configure(mysqlPath);
    }
    else {
      console.log("ERROR: not a MySQL install tree" + lf);
      rl.prompt(true);
    }
  }

  function mainMode() {
    rl.setPrompt('Your choice> ', 13);
    rl.on('line', onEntry);
    rl.prompt(true);
  }

  function customMode() {
    rl.setPrompt('MySQL Install Path> ', 20);
    rl.on('line', onPath);
    rl.prompt(true);
  }

  /* Start here: */
  rl.write(greeting);
  rl.on('SIGINT', hangup);
  if(candidates.length) {
    rl.write(text);
    mainMode();  
  }
  else {
    customMode();
  }
}
 
 
main();
