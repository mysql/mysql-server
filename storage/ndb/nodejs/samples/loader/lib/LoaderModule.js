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


/* LoaderModule is constructed with an optionHandler so that a module
   plugged into dbLoader can handle command-line options.
   A LoaderModule with optionHandler = null can be used, 
   but will throw an exception if addOption() is called.
*/
function LoaderModule(optionHandler) {
  this.optionHandler = optionHandler;
};


/* addOption(shortForm, longForm, helpText, callback).
   Callback will receive(args, index), where args is the array of command
   line arguments, and index is the index in that array of either the short
   or long form of this option.
   The callback should RETURN the number of parameters consumed 
   starting at index, e.g.:
       0: this plugin did not recognize the option at options[index]
       1: this plugin consumed a flag, e.g. "-x"
       2: this plugin consumed an option and its value, e.g. "-expect pass"
*/

// dataLoader.addOptionHandler("-x","--extra","do extra work",
//     function(args,index) { return 1; });

LoaderModule.prototype.addOption = function() {
  this.optionHandler.addOption(Option.apply({}, arguments));
};


 /* onReadRecord(record) 
    Gives the plugin access to a Domain Object that has been created by 
    the FieldScanner.  The user can modify it before it is handed to the 
    loader.

    record.row holds an object containing the data parsed into fields.  If
    you modify record.row, different values will be stored.

    record.class holds the Domain Object Constructor that maps the object
    to the database.  If you change record.class, you cause the record to be 
    stored in a different table.
    
    record.source holds a reference to the disk buffer containing this record
    in the input file.  The text source of the record is the substring of
    record.source from record.start to record.end.

    If onReadRecord returns false, THE RECORD WILL NOT BE PERSISTED. 
    It will also not be logged.  Logging of discarded records is left up
    to the plugin. 
    
    On any other return value, the record will be stored in the database.
 */
LoaderModule.prototype.onReadRecord       = function(record) {
};


/* onScanLine()
   Gives the plugin access to a line scanned from the data file.
   Only lines that will be evaluated for data are delivered; not blank
   lines, comment lines, or skipped lines.
   lineNo is the physical line number in the source file.

   The line of data is the substring from source[startPos] to source[endPos].
*/
LoaderModule.prototype.onScanLine         = function(lineNo, source, startPos, endPos) {
};


/* onTick()
   Gives the plugin access to the internal timer interval.
   This function will be called approximately once every 20 milliseconds.
   stats is an object containing these counters:
     rowsProcessed   // all rows processed by data source
     rowsSkipped     // rows procesed by data source but skipped
     rowsComplete    // all rows completed by writer (success or failure)
     rowsError       // rows failed by writer
     tickNumber      // starts at zero
*/
LoaderModule.prototype.onTick             = function(stats) {
};


/* onRecordError(record)
   Called after a record has been received by the database.
   If an error has occured, record.error will be set.
*/
LoaderModule.prototype.onRecordError      = function(record) {
};


/* onRecordStored(record) 
   Called after a record has been succesfully stored in the database.
*/
LoaderModule.prototype.onRecordStored     = function(record) {
};


/* onFinished(controller)
   This will be called at the end of processing.  It gives the plugin a chance
   to asynchronously close any resources.  When processing is finished, the 
   plugin must call the provided callback function.
  */
LoaderModule.prototype.onFinished         = function(callbackOnClosed) {
  callbackOnClosed();
};


/* onSqlScan(scannerError, tokenList)
   Gives the plugin access to the control file text ("LOAD DATA INFILE...") 
   as an array of tokens from the SQL scanner, and to any error produced by
   the scanner. 
*/
//  dataLoader.onSqlScan = function(scannerError, tokenList) {
//  };
LoaderModule.prototype.onSqlScan          = function(scannerError, tokenList) {
  if(scannerError) {
    throw scannerError;
  }
};


/* onSqlParse(parserError, parseTree)
   Gives the plugin access to the control file text ("LOAD DATA INFILE...")
   as a tree of parse nodes from the SQL parser, and to any error produced by
   the parser.
*/
//  dataLoader.onSqlParse = function(parserError, parseTree) {
//  };
LoaderModule.prototype.onSqlParse         = function(parserError, parseTree) {
  if(parserError) {
    throw parserError;
  }
};


/* onLoaderJob(semanticError, loaderJob)
   Gives the plugin access to the LoaderJob that will control the loader's 
   behavior.  If a control file text was supplied (either on the command line
   or from a file), the LoaderJob will have been generated from the parse 
   tree for that text.  Otherwise, the LoaderJob will be instantiated with
   all default values, and the user must fill in at least the data source and
   destination table (see LoaderJob documentation).
*/
//  dataLoader.onLoaderJob = function(semanticError, loaderJob) {
//  };
LoaderModule.prototype.onLoaderJob        = function(semanticError, loaderJob) {
  if(semanticError) {
    throw semanticError;
  }
};


/* createMappings()
   Allows the plugin to create TableMappings before connecting to the database.
   Every table used by the loader must be mapped.
   By default, a mapping is created for any table mentioned in the control file.

   RETURN an array of Domain Object Constructors with applied TableMappings.
 */
//   dataLoader.createMappings = function() {
//   };
LoaderModule.prototype.createMappings     = function() {
};


exports.LoaderModule = LoaderModule;

