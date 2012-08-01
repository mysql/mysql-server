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

var path = require("path"),
    impl = require("../impl/build/Release/common/unified_debug_impl");

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
 * debug.level_info()               // set output level to INFO
 * debug.level_debug()              // set output level to DEBUG
 * debug.level_detail()             // set output level to DETAIL
 * debug.level_meta()               // also debug the debug system
 *
 * debug.destination(<log_file>)    // direct output to log_file
 *
 * debug.log(<message>)             // write debugging message (at DEBUG level)
 * debug.log_info(<message>)        // write message at INFO level
 * debug.log_debug(<message>)       // same as debug.log
 * debug.log_detail(<message>)      // write message at DETAIL level
 *
 * debug.all_files()                // enable messages from all source files
 * debug.all_but_selected()         //  ... from all files except those selected
 * debug.none_but_selected()        //  ... from selected source files only
 * debug.add_file(<source_file>)    // add source_file to selected list         
 * debug.drop_file(<source_file)    // drop source_file from selected list
 * 
 */

/** Set the path for debugging output. 
    By default, debugging output goes to standard error.
    This function will append debugging output to an existing file,
    creating it if necessary.
*/
exports.destination = function(filename) {
  impl.udeb_destination(filename);
}

/* Turn debugging output on.
*/
exports.on = function() {
  impl.udeb_switch(1);
  exports.log("unified debug enabled");
}

/* Turn debugging output off.
*/
exports.off = function() {
  exports.log("unified debug disabled");
  impl.udeb_switch(0);
}

/* Set the logging level
*/
exports.level_info = function() {
  impl.udeb_switch(impl.UDEB_INFO);
}

exports.level_debug = function() {
  impl.udeb_switch(impl.UDEB_DEBUG);
}

exports.level_detail = function() {
  impl.udeb_switch(impl.UDEB_DETAIL);
}

exports.level_meta = function() {
  impl.udeb_switch(impl.UDEB_META);
}


/* Write a message to the debugging destination.
   By default, if debugging is on, all messages are logged.
   However, it is also possible to enable logging only from specific 
   source code files (see below).
*/
exports.log_debug = function(message) {
  impl.udeb_print(path.basename(module.parent.filename), impl.UDEB_DEBUG, message);
}

/* By default, log at DEBUG level
*/
exports.log = exports.log_debug;

/* Write a message at INFO level
*/
exports.log_info = function(message) {
  impl.udeb_print(path.basename(module.parent.filename), impl.UDEB_INFO, message);
}

/* Write a message at DETAIL level
*/
exports.log_detail = function(message) {
  impl.udeb_print(path.basename(module.parent.filename), impl.UDEB_DETAIL, message);
}

/* Enable debugging output from all source files.
   This is the default.
*/
exports.all_files = function() {
  impl.udeb_select(0);
}

/* Enable debugging output only from specifically selected source files.
   Files from which output is desired must be added to the target list.
*/
exports.none_but_selected = function() {
  impl.udeb_select(5);
}

/* Enable debugging from all source files -except- those on the target list.
*/
exports.all_but_selected = function() {
  impl.udeb_select(4);
}

/* Add a source file to the target list.
*/
exports.add_file = function(source_file_name) {
  impl.udeb_add_drop(source_file_name, 1);
}

/* Remove a source file from the target list.
*/
exports.drop_file = function(source_file_name) {
  impl.udeb_add_drop(source_file_name, 2);
}
