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


var path = require("path"),
    util = require("util"),

    UDEB_OFF      = 0,
    UDEB_URGENT   = 1,
    UDEB_NOTICE   = 2,
    UDEB_INFO     = 3,
    UDEB_DEBUG    = 4,
    UDEB_DETAIL   = 5,
    UDEB_META     = 6, 

    udeb_on           = 1,                 // initially on/off
    udeb_level        = UDEB_NOTICE,       // initial level
    destinationStream = process.stderr,    // initial message destination
    nativeCodeClients = [],
    logListeners      = [];


/* This is a common internal run-time debugging package for C and JavaScript. 
 * In the spirit of Fred Fish's dbug package, which is widely used in the 
 * MySQL server, this package manages "printf()-style" debugging of source 
 * code.  
 *
 * It allows a JavaScript user to control debugging output both from 
 * JavaScript code and from compiled C code. The user can direct the debugging
 * output to a particular file, and can enable or disable debugging output
 * from indiviual source code files.
 * 
 * SUMMARY:
 * 
 * var debug = require("unified_debug.js");
 *
 * debug.on()                       // turn debugging on
 * debug.off()                      // turn debugging off
 * debug.level_urgent()             // set output level to URGENT
 * debug.level_notice()             // set output level to NOTICE
 * debug.level_info()               // set output level to INFO
 * debug.level_debug()              // set output level to DEBUG
 * debug.level_detail()             // set output level to DETAIL
 * debug.level_meta()               // also debug the debug system
 *
 * debug.close()                    // close log file
 *
 * debug.log(<message>)             // write debugging message (at DEBUG level)
 * debug.log_urgent(<message>)      // write message at URGENT level
 * debug.log_notice(<message>)      // write message at NOTICE level
 * debug.log_info(<message>)        // write message at INFO level
 * debug.log_debug(<message>)       // write message at DEBUG level
 * debug.log_detail(<message>)      // write message at DETAIL level
 *
 * debug.all_files()                // enable messages from all source files
 * debug.all_but_selected()         //  ... from all files except those selected
 * debug.none_but_selected()        //  ... from selected source files only
 * debug.add_file(<source_file>)    // add source_file to selected list         
 * debug.drop_file(<source_file)    // drop source_file from selected list
 * 
 */


/* close the destination stream
*/
exports.close = function() {
  if(destinationStream !== process.stderr && destinationStream !== process.stdout) {
    destinationStream.end();
  }
};

/* Tell native code logging clients about the level 
*/
function clientSetLevel(l) {
  var i, client;
  for(i = 0 ; i < nativeCodeClients.length ; i++) {
    client = nativeCodeClients[i];
    client.setLevel(l);
  }
}

/* Turn debugging output on.
*/
exports.on = function() {
  udeb_on = 1;
  clientSetLevel(udeb_level);
  exports.log("unified debug enabled");
};

/* Turn debugging output off.
*/
exports.off = function() {
  udeb_on = 0;
  clientSetLevel(UDEB_OFF);
  exports.log("unified debug disabled");
};


/* Set the logging level
*/
function udeb_set_level(lvl) {
  udeb_level = lvl;
  clientSetLevel(udeb_level);
}

exports.level_urgent = function() {
  udeb_set_level(UDEB_URGENT);
};

exports.level_notice = function() {
  udeb_set_level(UDEB_NOTICE);
};

exports.level_info = function() {
  udeb_set_level(UDEB_INFO);
};

exports.level_debug = function() {
  udeb_set_level(UDEB_DEBUG);
};

exports.level_detail = function() {
  udeb_set_level(UDEB_DETAIL);
};

exports.level_meta = function() {
  udeb_set_level(UDEB_META);
};


/**********************************************************/
function write_log_message(level, file, message) {
  if(udeb_level >= level) {
    message += "\n"
    destinationStream.write(message, 'ascii');
  }
}

// TO DO, THESE ARE ACTUALLY FUNCTIONS OF THE PER-FILE LOGGER CLASS AND ARE GENERATED.
exports.log_urgent = function() {
  var message;
  if(udeb_level >= UDEB_URGENT) {
    message = util.format.apply(null, arguments);
    write_log_message(UDEB_URGENT, path.basename(module.parent.filename), 
                      message);
  }
};

exports.log_notice = function() {
  var message;
  if(udeb_level >= UDEB_NOTICE) {
    message = util.format.apply(null, arguments);
    write_log_message(UDEB_NOTICE, path.basename(module.parent.filename), 
                      message);
  }
};

exports.log_debug = function() {
  var message;
  if(udeb_level >= UDEB_DEBUG) {
    message = util.format.apply(null, arguments);
    write_log_message(UDEB_DEBUG, path.basename(module.parent.filename), 
                      message);
  }
};

/* By default, log at DEBUG level
*/
exports.log = exports.log_debug;

/* Write a message at INFO level
*/
exports.log_info = function() {
  var message;
  if(udeb_level >= UDEB_INFO) {
    message = util.format.apply(null, arguments);
    write_log_message(UDEB_INFO, path.basename(module.parent.filename), 
                      message);
  }
};

/* Write a message at DETAIL level
*/
exports.log_detail = function() {
  var message;
  if(udeb_level >= UDEB_DETAIL) {
    message = util.format.apply(null, arguments);
    write_log_message(UDEB_DETAIL, path.basename(module.parent.filename), 
                      message);
  }
};


/* Enable debugging output from all source files.
   This is the default.
*/
exports.all_files = function() {
};

/* Enable debugging output only from specifically selected source files.
   Files from which output is desired must be added to the target list.
*/
exports.none_but_selected = function() {
};

/* Enable debugging from all source files -except- those on the target list.
*/
exports.all_but_selected = function() {
};

/* Add a source file to the target list.
*/
exports.add_file = function(source_file_name) {
};

/* Remove a source file from the target list.
*/
exports.drop_file = function(source_file_name) {
};
