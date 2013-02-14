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

// On Windows, we write config.gypi and build with gyp.
//
// On non-Windows, we write a shell script (pathname supplied on the command
// line) and build with node-waf.

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

  if(verify('opt/mysql/server-5.6/share/java'))  {   // Debian
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
  found += ' [' + String(++i) + ']  Skip (Do not build the ndb backend)' +lf+lf;

  return found;
}


function configure(mysql) {
  var envCmd = "";

  // Write the tempfile, used with node-waf builds
  var tempfile = process.argv[2];
  if(mysql) {
    envCmd = 'PREFERRED_MYSQL=' + mysql + '\n';
  }
  fs.writeFileSync(tempfile, envCmd, 'ascii');

  // Write config.gypi, used with node-gyp
  var gyp = { "variables" : { "mysql_path" : mysql }};
  fs.writeFileSync("config.gypi", JSON.stringify(gyp) + "\n", "ascii");

  process.exit();
}


// Filename completion
// Returns [ [array of matches], original substring ]
function completion(line) {
  var matches = [];
  var dir, base, files, stat;
  try {  // input is a directory?
    dir = line;
    files = fs.readdirSync(dir);
    base = "";
  }
  catch(e) {
    dir = path.dirname(line);   // returns "." if path is unrooted
    base = path.basename(line);
    files = fs.readdirSync(dir);
  }
 
  for(var i = 0; i < files.length ; i++) {
    if(files[i].match("^" + base)) {
      matches.push(path.join(dir, files[i]));
    }
  }
  
 
  
  if(matches.length == 1) {
    try {
      stat = fs.statSync(matches[0]);
      if(stat.isDirectory()) matches[0] += path.sep;
    }
    catch(e) {
    };
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
    var range = candidates.length + 2;
    var num = Number(choice);
    
    if(num < 1 || num > range) {
      rl.write("Please enter a number between 1 and " + range + "." + lf);
      rl.write("Hit CTRL-C to exit." +lf);
      rl.prompt();
    }
    else if(num === range) {  // skip
      hangup();
    }
    else if(num === (range - 1)) {
      customMode();
    }
    else {
      rl.close();
      configure(candidates[num - 1]);
    }
  }

  function onPath(path) {
    rl.close();
    configure(path);
  }

  function mainMode() {
    rl.setPrompt('Your choice> ', 13);
    rl.on('line', onEntry).on('SIGINT', hangup);
    rl.prompt(true);
  }

  function customMode() {
    rl.setPrompt('MySQL Install Path> ', 20);
    rl.removeListener('line', onEntry);
    rl.on('line', onPath);
    rl.prompt(true);
  }

  /* Start here: */
  rl.write(greeting);
  rl.write(text);
  mainMode();  
}
 
 
main();
