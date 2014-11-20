/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

global.debug = 0 ;  // FIXME, currently used by parser & scanner

var assert = require("assert"),
    mynode = require("../.."),
    LoaderJob = require("./lib/LoaderJob.js").LoaderJob,
    LoaderModule = require("./lib/LoaderModule.js").LoaderModule
;

function usage(flagHandler) {
  var i, msg, option;
  msg = "Usage:  node dbloader [options]\n" +
        "  Options:\n" + flagHandler.helpText;
  console.log(msg);
  process.exit(1);
}


/* Define a command-line option.
   If takesValue is false, then container[valueName] will hold valueIfSet if
   option is set; if takesValue is true, container[valueName] will hold
   the next command-line parameter after option.
*/
function Option(shortForm, longForm, helpText, callback) {
  this.shortForm  = shortForm;
  this.longForm   = longForm;
  this.helpText   = helpText;
  this.callback   = callback;
}

Option.prototype.isOption = true;

function FlagHandler() {
  this.flags = {};
  this.helpText = "";
}

FlagHandler.prototype.addOption = function(option) {
  assert(option.isOption);
  var shortFlag, longFlag, helpText;
  helpText = "";
  if(option.shortForm) {
    shortFlag = option.shortForm;
    this.flags[shortFlag] = option;
  }
  if(option.longForm) {
    longFlag = option.longForm;
    this.flags[longFlag] = option;
  }
  if(shortFlag && longFlag) {
    helpText = "  " + longFlag + "\n" +
               "  " + shortFlag + "     " + option.helpText + "\n";
  } else if(shortFlag) {
    helpText = "  " + shortFlag + "     " + option.helpText + "\n";
  } else if (longFlag) {
    helpText = "  " + longFlag + "  " +     option.helpText + "\n";
  }

  this.helpText += helpText;
};

FlagHandler.prototype.processArguments = function() {
  var i, len, opt, nextArg, flag, consumed;
  i = 2;
  len = process.argv.length;
  while(i < len) {
    opt = process.argv[i];
    nextArg = (i + 1 < len) ? process.argv[i+1] : null;
    flag = this.flags[opt];
    if(flag) {
      consumed = flag.callback(nextArg);
      if(consumed > 0) {
        i += consumed;
      } else {
        usage(this);
      }
    } else {
      usage(this);
    }
  }
};


function parse_command_line() {
  var i, len, handler, options;
  handler = new FlagHandler();
  options = {
    "adapter" : "ndb",
    "plugin"  : new LoaderModule(handler)
  };
  handler.addOption(new Option(
    "-n", "--ndb", "Use NDB adapter", function(nextArg) {
      options.adapter = "ndb";
      return 1;
    }));
  handler.addOption(new Option(
   "-m", "--mysql", "Use MySQL adapter", function(nextArg) {
      options.adapter = "mysql";
      return 1;
    }));
  handler.addOption(new Option(
    null, "--stats", "collect statistics", function(nextArg) {
      options.stats = true;
      return 1;
    }));
  handler.addOption(new Option(
    "-d", "--debug", "enable debugging output", function(nextArg) {
      unified_debug.on();
      unified_debug.level_debug();
      return 1;
      }));
  handler.addOption(new Option(
    null, "-df", "enable debugging output from <source_file>", function(nextArg) {
      unified_debug.on();
      unified_debug.set_file_level(nextArg, 5);
      return 2;
    }));
  handler.addOption(new Option(
    "-c", null, "<ndb_connect_string>", function(nextArg) {
      options.connect_string = nextArg;
      return 2;
    }));
  handler.addOption(new Option(
    "-f", null, "<control_file>", function(nextArg) {
      options.control_file = nextArg;
      return 2;
    }));
  handler.addOption(new Option(
    "-e", null, "<load_data_command_text>", function(nextArg) {
      options.control_text = nextArg;
      return 2;
    }));
  handler.addOption(new Option(
    "-j", null, "<javascript_plugin_file>", function(nextArg) {
      /* Here is the code that actually loads the user's module: */
      var modulePath;
      options.plugin_file = nextArg;
      modulePath = options.plugin_file;
      // Assume the module path is relative to PWD unless it begins with / or .
      if(modulePath[0] !== "." && modulePath[0] !== "/") {
        modulePath = "./" + modulePath;
      }
      require(modulePath).init(options.plugin);
      return 2;
    }));
  handler.addOption(new Option(
    "-h", "--help", "show usage", function(nextArg) {
      usage(handler);
      process.exit(1);
    }));

  handler.processArguments();

  if(! (options.control_file || options.control_text || options.plugin)) {
    usage(handler);
  }
  return options;
}


function main() {
  // Parse command-line options
  var cmdOptions = parse_command_line();

  // The User's custom plugin
  var plugin = cmdOptions.plugin;

  // Generate Loader Job
  var job = new LoaderJob();
  job.setPlugin(plugin);

  if(cmdOptions.control_file) {
    job.initializeFromFile(cmdOptions.control_file);
  } else {
    job.initializeFromSQL(cmdOptions.control_text);
  }

  // Create a TableMapping for the destination
  var mappedConstructors = [ job.destination.createTableMapping() ];

  // Add other mappings if defined by the plugin
  var extMappings = job.plugin.createMappings();
  if(Array.isArray(extMappings)) {
    mappedConstructors = mappedConstructors.concat(extMappings);
  }

  // Set connection properties
  var connectionProperties = new mynode.ConnectionProperties(cmdOptions.adapter);

  if(cmdOptions.connect_string) {
    connectionProperties.ndb_connectstring = cmdOptions.connect_string;
  }
  connectionProperties.database = job.destination.database;

  // Connect to the database and start the controller
  mynode.openSession(connectionProperties, mappedConstructors, function(err, session) {
    if(err) throw err; /*JSON.stringify(err); process.exit(1);*/
    job.run(session, function onComplete(error, stats) {
      session.close().then(function() {
        var exitCode = 0;  // OK
        if(error) {
          console.log(error);
          exitCode = 1;  // Failure
        } else {
          console.log("Rows processed:", stats.rowsProcessed,
                      "Skipped", stats.rowsSkipped,
                      "Loaded:", stats.rowsComplete - stats.rowsError,
                      "Failed:", stats.rowsError);
          if(stats.rowsError > 0) {  exitCode = 2;  }    // Rejected Rows
        }
        process.exit(exitCode);
      });
    });
  });
}

main();

