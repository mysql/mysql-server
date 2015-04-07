/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

var assert     = require("assert"),
    udebug     = require(mynode.api.unified_debug).getLogger("LoaderJob.js"),
    machine    = require("./control_file.js"),
    Controller = require("./Controller.js").Controller;


/* Specification for a Loader Job */


// Define a single column of a destination table
function ColumnDefinition(columnName) {
  this.name              = columnName;
  this.startPos          = null;  // For fixed-width input
  this.endPos            = null;
};

// Define a destination:  Database, Table, columns, and mapped class
function LoaderJobDestination() {
  this.database          = null;
  this.table             = "";
  this.columnDefinitions = [];
  this.rowConstructor    = null;
};

LoaderJobDestination.prototype.addColumnDefinition = function(name) {
  assert(typeof name === 'string');
  var defn = new ColumnDefinition(name);
  this.columnDefinitions.push(defn);
  return defn;
};

LoaderJobDestination.prototype.createTableMapping = function() {
  if(this.table.length === 0) {
    throw new Error("No table specified in loader job.");
  }

  var literalMapping, mapping;
  literalMapping = {
    table         : this.table,
    database      : this.database,
    mapAllColumns : (this.columnDefinitions.length === 0)
  };
  mapping = new mynode.TableMapping(literalMapping);

  /* A ``function() {}'' for constructing mapped objects */
  this.rowConstructor = new Function();

  this.columnDefinitions.forEach(function(column) {
    mapping.mapField(column.name);
  });

  mapping.applyToClass(this.rowConstructor);
  return this.rowConstructor;
};

LoaderJobDestination.prototype.setColumnsFromArray = function(columnArray) {
  udebug.log("setColumnsFromArray length:", columnArray.length);
  var i;
  for(i = 0 ; i < columnArray.length ; i++) {
    this.columnDefinitions.push(new ColumnDefinition(columnArray[i]));
  }
  this.createTableMapping();  // Causes the default TableHandler to be discarded
};

LoaderJobDestination.prototype.getTableHandler = function() {
  // FIXME this uses undocumented path to access the dbTableHandler:
  return this.rowConstructor.prototype.mynode.tableHandler;
};

// LoaderJob
function LoaderJob() {
  this.plugin            = null;
  this.destination       = new LoaderJobDestination();
  this.ctlFileDescriptor = null;
  this.controller = {
    randomData           : false,
    badfile              : "",
    maxRows              : null,
    speedMeasure         : null,
    speedFast            : true,
    workerId             : 0,
    nWorkers             : 1,
    skipRows             : 0,
    inOneTransaction     : false
  };
  this.dataSource = {
    file                 : "",
    useControlFile       : null,
    isJSON               : false,
    commentStart         : null,
    fieldSep             : "\t",
    fieldSepOnWhitespace : false,
    fieldQuoteStart      : "\"",
    fieldQuoteEnd        : "\"",
    fieldQuoteEsc        : "\\",
    fieldQuoteOptional   : false,
    lineStartString      : "",
    lineEndString        : "\n",
    columnsInHeader      : false
  };
  this.dataLoader = {
    replaceMode          : false,
    requireEmpty         : false,
    doTruncate           : false
  };
  this.setInsertMode("APPEND");
}

LoaderJob.prototype.setPlugin = function(plugin) {
  this.plugin = plugin;
};

LoaderJob.prototype.initializeFromFile = function(filename) {
  // Get the SQL text.  The control file may also contain data.
  var ctlMaxReadLen, ctlReadBuffer, size, sqlText;
  ctlMaxReadLen = 16 * 1024;
  ctlReadBuffer = new Buffer(ctlMaxReadLen);
  this.ctlFileDescriptor = fs.openSync(filename, 'r');
  size = fs.readSync(this.ctlFileDescriptor, ctlReadBuffer, 0, ctlMaxReadLen);
  sqlText = ctlReadBuffer.toString('utf8', 0, size);
  this.initializeFromSQL(sqlText);

  if(this.dataSource.useControlFile) {
    this.dataSource.useControlFile.text = sqlText;
  } else {
    fs.closeSync(this.ctlFileDescriptor);
  }
};

LoaderJob.prototype.BeginDataAtControlFileLine = function(lineNo) {
  this.dataSource.useControlFile =
  {
    openFd     : this.ctlFileDescriptor,
    text       : null,
    inlineSkip : lineNo
  };
};

LoaderJob.prototype.initializeFromSQL = function(text) {
  var tokens, tree, error;

  if(text) {
    error = null;
    try {
      tokens = machine.scan(text);
    } catch(e) {
      error = e;
    }
    this.plugin.onSqlScan(error, tokens);

    error = null;
    // try {
      tree = machine.parse(tokens);
    // } catch(e) {
    //   error = e;
    // }
    this.plugin.onSqlParse(error, tree);
  }

  error = null;
  try {
    machine.analyze(tree, this);
  } catch(e) {
    error = e;
  }
  this.plugin.onLoaderJob(error, this);
};

LoaderJob.prototype.run = function(session, finalCallback) {
  var controller = new Controller(this, session, finalCallback);
  controller.run();
};

LoaderJob.prototype.setWorkerId = function(id, nWorkers) {
  assert(typeof id === 'number' &&
         typeof nWorkers === 'number' &&
         id > 0 &&
         id <= nWorkers);
  this.controller.workerId = id - 1;
  this.controller.nWorkers = nWorkers;
};

LoaderJob.prototype.setDataFile = function(fileName) {
  assert(typeof fileName === 'string');
  this.dataSource.file = fileName;
};

LoaderJob.prototype.generateRandomData = function() {
  this.controller.randomData = true;
};

LoaderJob.prototype.dataSourceIsJSON = function() {
  this.dataSource.isJSON = true;
};

LoaderJob.prototype.dataSourceIsCSV = function() {
  this.setFieldSeparator(",");
  this.setColumnsInHeader();
};

/*     [ INSERT | REPLACE | APPEND | TRUNCATE | IGNORE ]
   This is the union of keywords supported by Oracle SQL*Loader and by MySQL.
   
   APPEND allows the table to have existing data.  This is the default behavior.

   INSERT requires the table to empty before loading.

   TRUNCATE instructs the loader to delete all rows before loading data.

   REPLACE has the meaning, as in MySQL, that existing rows will be updated 
   with values from the data file.  (This is quite different from the semantics
   of REPLACE in SQL*Loader).  With REPLACE, all rows are written with the 
   semantics of save() ("update or insert") rather than persist().
   
   IGNORE is present for compatibility with MySQL, but is ignored.
*/
LoaderJob.prototype.setInsertMode = function(mode) {
  mode = mode.toUpperCase();
  switch(mode) {
    case "INSERT":
      this.dataLoader.requireEmpty = true;
      this.dataLoader.replaceMode  = false;
      this.dataLoader.doTruncate   = false;
      throw new Error("INSERT mode is not yet implemented");
      break;
    case "REPLACE":
      this.dataLoader.requireEmpty = false;
      this.dataLoader.replaceMode  = true;
      this.dataLoader.doTruncate   = false;
      break;
    case "APPEND":
      this.dataLoader.requireEmpty = false;
      this.dataLoader.replaceMode  = false;
      this.dataLoader.doTruncate   = false;
      break;
    case "TRUNCATE":
      this.dataLoader.requireEmpty = false;
      this.dataLoader.replaceMode  = false;
      this.dataLoader.doTruncate   = true;
      throw new Error("TRUNCATE mode is not yet implemented");
      break;
    case "IGNORE":
      // log a warning that IGNORE is ignored?
      break;
    default:
      throw new Error("Illegal insert mode:" + mode);
  }
};

LoaderJob.prototype.setFieldQuoteOptional = function() {
  this.dataSource.fieldQuoteOptional = true;
};

LoaderJob.prototype.setFieldQuoteStartAndEnd = function(start, end) {
  assert(typeof start === 'string' &&
         typeof end   === 'string' &&
         start.length === 1        &&
         end.length   === 1);
  this.dataSource.fieldQuoteStart = start;
  this.dataSource.fieldQuoteEnd = end;
};

LoaderJob.prototype.setFieldSeparator = function(sep) {
  assert(typeof sep === 'string' &&
         sep.length === 1);
  this.dataSource.fieldSep = sep;
};

LoaderJob.prototype.setFieldSeparatorToWhitespace = function() {
  this.dataSource.fieldSepOnWhitespace = true;
};

LoaderJob.prototype.setFieldQuoteEsc = function(escChar) {
  assert(typeof escChar === 'string' &&
         escChar.length === 1);
  this.dataSource.fieldQuoteEsc = escChar;
};

LoaderJob.prototype.setTable = function(name) {
  this.destination.table = name;
};

LoaderJob.prototype.setDatabase = function(db) {
  this.destination.database = db;
};

LoaderJob.prototype.setBadFile = function(file) {
  assert(typeof file === 'string');
  this.controller.badfile = file;
};

LoaderJob.prototype.inOneTransaction = function() {
  this.controller.inOneTransaction = true;
};

LoaderJob.prototype.setSkipRows = function(n) {
  assert(typeof n === 'number' && n >= 0);
  this.controller.skipRows = n;
};

LoaderJob.prototype.setMaxRows = function(n) {
  assert(typeof n === 'number' && n >= 0);
  this.controller.maxRows = n;
};

LoaderJob.prototype.setCommentStart = function(str) {
  assert(typeof str === 'string' && str.length > 0);
  this.dataSource.commentStart = str;
};

LoaderJob.prototype.setLineStart = function(str) {
  assert(typeof str === 'string' && str.length > 0);
  this.dataSource.lineStartString = str;
};

LoaderJob.prototype.setLineEnd = function(str) {
  assert(typeof str === 'string' && str.length > 0);
  this.dataSource.lineEndString = str;
};

LoaderJob.prototype.setColumnsInHeader = function() {
  this.dataSource.columnsInHeader = true;
};

exports.LoaderJob = LoaderJob;
