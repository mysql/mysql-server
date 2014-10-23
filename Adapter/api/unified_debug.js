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
    assert = require("assert"),

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
    presetPerFileLevel= {},
    myOwnLogger       = null;


/* This is a common internal run-time debugging package for C and JavaScript. 
 * In the spirit of Fred Fish's dbug package, which is widely used in the 
 * MySQL server, this package manages "printf()-style" debugging of source 
 * code.  
 *
 * It allows a JavaScript user to control debugging output both from 
 * JavaScript code and from compiled C code. The user can direct the debugging
 * output to a particular file, and can enable output from indiviual source code 
 * files.
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
  if(fileLoggers[filename]) {
    fileLoggers[filename].set_file_level(level);
  }
  else {
    /* Maybe a  file not yet registered */
    presetPerFileLevel[filename] = level;

    /* Maybe a C++ file */
    for(i = 0 ; i < nativeCodeClients.length ; i++) {
      var client = nativeCodeClients[i];
      client.setFileLevel(filename,level);
    }
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
  var fileName;
  assert(typeof client.setLogger === 'function');
  assert(typeof client.setLevel  === 'function');
  
  client.setLogger(handle_log_event);
  client.setLevel(udeb_level);
  
  nativeCodeClients.push(client);
  
  /* Register per-file logging levels */
  for(fileName in presetPerFileLevel) {
    if(presetPerFileLevel.hasOwnProperty(fileName)) {
      client.setFileLevel(fileName, presetPerFileLevel[fileName]);
    }
  }
};


function dispatch_log_message(level, filename, msg_array) {
  var message = util.format.apply(null, msg_array);
  if(level > UDEB_NOTICE) {            
    message = filename + " " + message;
  }
  handle_log_event(level, filename, message);  
}


function Logger() {}
Logger.prototype = {
  URGENT         : UDEB_URGENT,
  NOTICE         : UDEB_NOTICE,
  INFO           : UDEB_INFO,
  DEBUG          : UDEB_DEBUG,
  DETAIL         : UDEB_DETAIL,
  file_level     : 0,
  set_file_level : function(x) { this.file_level = x; }
};

/***********************************************************
 * get a custom logger class for a source file
 *
 ***********************************************************/
exports.getLogger = function(filename) {  
  assert(! fileLoggers[filename]);  // A filename cannot be registered twice 
  
    
  function makeLogFunction(level) {
    return function() {      
      if((udeb_level >= level) || (this.file_level >= level)) 
      {
        dispatch_log_message(level, filename, arguments);
      }
    };
  }

  function makeIsFunction(level) {
    return function() {      
      return (udeb_level >= level) || (this.file_level >= level);
    };
  }

  var theLogger = new Logger();
  var perFileLevel; 
  if(presetPerFileLevel[filename]) {
    theLogger.file_level = presetPerFileLevel[filename];
    delete presetPerFileLevel[filename];
  } else {
    theLogger.file_level = UDEB_URGENT;
  }

  theLogger.log_urgent     = makeLogFunction(1);
  theLogger.log_notice     = makeLogFunction(2);
  theLogger.log_info       = makeLogFunction(3);
  theLogger.log_debug      = makeLogFunction(4);
  theLogger.log_detail     = makeLogFunction(5);
  theLogger.log            = theLogger.log_debug;
  
  theLogger.is_urgent     = makeIsFunction(1);
  theLogger.is_notice     = makeIsFunction(2);
  theLogger.is_info       = makeIsFunction(3);
  theLogger.is_debug      = makeIsFunction(4);
  theLogger.is_detail     = makeIsFunction(5);

  fileLoggers[filename] = theLogger;
  
  return theLogger;
};

myOwnLogger = exports.getLogger("unified_debug.js");

