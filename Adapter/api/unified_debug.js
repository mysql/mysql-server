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

/*global assert */

"use strict";


var path = require("path"),
    util = require("util"),

    UDEB_OFF      = 0,
    UDEB_URGENT   = 1,
    UDEB_NOTICE   = 2,
    UDEB_INFO     = 3,
    UDEB_DEBUG    = 4,
    UDEB_DETAIL   = 5,

    udeb_on           = 1,                 // initially on/off
    udeb_level        = UDEB_NOTICE,       // initial level
    destinationStream = process.stderr,    // initial message destination
    nativeCodeClients = [],
    logListeners      = [],
    fileLoggers       = {},
    perFileLevel      = {},
    myOwnLogger       = null;


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
 * var unified_debug = require("unified_debug.js");
 *
 * var udebug = unified_debug.getLogger("myFilename.js");
 * 
 * udebug.log(<message>)             // write message (at DEBUG level)
 * udebug.log_urgent(<message>)      // write message at URGENT level
 * udebug.log_notice(<message>)      // write message at NOTICE level
 * udebug.log_info(<message>)        // write message at INFO level
 * udebug.log_debug(<message>)       // write message at DEBUG level
 * udebug.log_detail(<message>)      // write message at DETAIL level
 * udebug.set_file_level(level)      // override output level for this file
 *
 *
 * unified_debug.on()                // turn debugging on
 * unified_debug.off()               // turn debugging off
 * unified_debug.level_urgent()      // set output level to URGENT
 * unified_debug.level_notice()      // set output level to NOTICE
 * unified_debug.level_info()        // set output level to INFO
 * unified_debug.level_debug()       // set output level to DEBUG
 * unified_debug.level_detail()      // set output level to DETAIL
 * unified_debug.level_meta()        // also debug the debug system
 * unified_debug.close()             // close the destination stream
 * 
 *   // Set the destination stream for debugging messages:
 * unified_debug.set_destination(writeableStream) 
 *
 *   // Register a native code module, which must export 
 *   // setLogger() and setLevel() functions to JavaScript:
 * unified_debug.register_client(module)    
 *
 *   // Set a per-filename output level:
 * unifed_debug.set_file_level(filename, level) 
 *
 *   // Register a receiver function which will be called with 
 *   // arguments (level, filename, message) whenever a message is logged.
 * unified_debug.register_receiver(receiverFunction);
 *
 */


/* Customize the destination stream
*/
exports.set_destination = function(writableStream) {
  destinationStream = writableStream;
};


/* close the destination stream
*/
exports.close = function() {
  if(destinationStream !== process.stderr && destinationStream !== process.stdout) {
    destinationStream.end();
  }
};

/* Register a log receiver 
*/
exports.register_receiver = function(rFunc) {
  logListeners.push(rFunc);
};

/* Set per-file debugging level
*/
exports.set_file_level = function(filename, level) {
  var i;
  perFileLevel[filename] = level;
  
  for(i = 0 ; i < nativeCodeClients.length ; i++) {
    var client = nativeCodeClients[i];
    client.setFileLevel(filename,level);
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
  myOwnLogger.log("unified debug enabled");
};

/* Turn debugging output off.
*/
exports.off = function() {
  udeb_on = 0;
  clientSetLevel(UDEB_OFF);
  myOwnLogger.log("unified debug disabled");
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


/**********************************************************/
function handle_log_event(level, file, message) {
  var i;
  for(i = 0 ; i < logListeners.length ; i++) {
    logListeners[i](level, file, message);
  }
}

function write_log_message(level, file, message) {
  message += "\n";
  destinationStream.write(message, 'ascii');
}

/// INITIALIZATION TIME: REGISTER write_log_message as a listener: ///
logListeners.push(write_log_message);


/* Register a C client so that it can send debugging output up to JavaScript
*/
exports.register_client = function(client) {
  assert(typeof client.setLogger === 'function');
  assert(typeof client.setLevel  === 'function');
  
  client.setLogger(write_log_message);
  client.setLevel(udeb_level);
  
  nativeCodeClients.push(client);  
};


/***********************************************************
 * get a custom logger class for a source file
 *
 ***********************************************************/
exports.getLogger = function(filename) {
  /* The same filename cannot be registered twice */
  assert(! fileLoggers[filename]);
    
  function makeLogFunction(level) {
    var message;
    var f = function() {
      
      var activeLevel = perFileLevel[filename] > udeb_level ?
        perFileLevel[filename] : udeb_level;
            
      if(activeLevel >= level) {
        message = util.format.apply(null, arguments);
        if(level > UDEB_NOTICE) {
          message = filename + " " + message;
        }
        handle_log_event(level, filename, message);
      }
    };
    return f;
  }

  function Logger() {}
  Logger.prototype = {
    URGENT : UDEB_URGENT,
    NOTICE : UDEB_NOTICE,
    INFO   : UDEB_INFO,
    DEBUG  : UDEB_DEBUG,
    DETAIL : UDEB_DETAIL,
    set_file_level : function(x) { perFileLevel[filename] = x; }
  };

  var theLogger = new Logger();

  theLogger.log_urgent     = makeLogFunction(UDEB_URGENT);
  theLogger.log_notice     = makeLogFunction(UDEB_NOTICE);
  theLogger.log_info       = makeLogFunction(UDEB_INFO);
  theLogger.log_debug      = makeLogFunction(UDEB_DEBUG);
  theLogger.log_detail     = makeLogFunction(UDEB_DETAIL);
  theLogger.log            = theLogger.log_debug;

  if(typeof perFileLevel[filename] === 'undefined') {
    perFileLevel[filename] = UDEB_URGENT;
  }
  
  fileLoggers[filename] = theLogger;
  
  return theLogger;
};

myOwnLogger = exports.getLogger("unified_debug.js");

