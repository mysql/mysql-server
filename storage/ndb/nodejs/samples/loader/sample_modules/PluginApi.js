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


/* 
   dbloader gives user-created JavaScript modules extensive access to the
   process of loading data.  Some of the things modules can do include:
     * add new command-line options to dbloader
     * customize the specification of the loader job
     * access a raw line of data read from the data file
     * access a parsed record and modify its fields, route it to a
       different destination table, or reject it.
     * receive callbacks for each row successfully loaded to the database
       or for each error.
     
   A module must export an init() function which receives an instance of a
   DataLoader object.

   All DataLoader methods are shown briefly here; full documentation is 
   available in lib/DataLoader.js
*/

exports.init = function(dataLoader) {

 /* dbloader can accept command-line options that are defined by the plugin.
     For each option, use dataLoader.addOptionHandler here.
  */

  /* addOptionHandler(shortForm, longForm, helpText, callback).
     Callback will receive (nextArg) and should return:
       0 if no arguments were consumed (i.e. not even the option flag)
       1 if the option flag was consumed
       2 if both the option flag and next argument were consumed
  */
  // dataLoader.addOption("-x","--extra","do extra work",
  //     function(args,index) { return 1; });


   /* onReadRecord(record)
      Gives the plugin access to a Domain Object that has been created by 
      the FieldScanner.  The user can modify it before it is handed to the 
      loader.

      record.row holds an object containing the data parsed into fields.  If
      you modify record.row, different values will be stored.

      record.class holds the Domain Object Constructor that maps the object
      to the database.  If you change record.class, you cause the record to be 
      stored in a different table.

      If onReadRecord returns false, the record will not be persisted.
   */
   // dataLoader.onReadRecord = function(record) {
   // };


  /* onScanLine() 
     Gives the plugin access to a line scanned from the data file.
     Only lines that will be evaluated for data are delivered; not blank
     lines, comment lines, or skipped lines.
     lineNo is the physical line number in the source file.

     The line of data is the string from source[startPos] to source[endPos].

     Note that in the random data generator (LOAD RANDOM DATA...) there is 
     no call to onScanLine()
  */
  // dataLoader.onScanLine = function(lineNo, source, startPos, endPos) {
  // };


  /* onTick(stats)
     Gives the plugin access to the internal timer interval.
     This function will be called approximately once every 20 milliseconds.
  */
  // dataLoader.onTick = function(stats) {
  // };


  /* onRecordStored(record): 
     Called when record has been successfully stored.
  */
  // dataLoader.onRecordStored = function(record) {
  // };


  /* onRecordError(record) 
     Record has been sent to the database and caused an error;
     record.error will be set.
   */
  // dataLoader.onRecordError = function(record) {
  // };


  /* onFinished(controller)
     This will be called at the end of processing.  It gives the plugin a chance
     to asynchronously close any resources.  When processing is finished, the 
     plugin must call the provided callback function.
  */
// dataLoader.onFinished = function(callbackOnClosed) {
//   callbackOnClosed();
// };


  /* onSqlScan(scannerError, tokenList)
     Access the control file as an array of tokens from the SQL scanner.
  */
//  dataLoader.onSqlScan = function(scannerError, tokenList) {
//  };


  /* onSqlParse(parserError, parseTree)
     Access the control file as a tree of parse nodes from the SQL parser.
  */
//  dataLoader.onSqlParse = function(parserError, parseTree) {
//  };


  /* onLoaderJob(semanticError, loaderJob)
     Access the LoaderJob generated after parsing.
     If no SQL statement was given, then this is a blank default LoaderJob;
     see API in lib/LoaderJob.js.  The user must fill in at least the data 
     source and destination table.
  */
//  dataLoader.onLoaderJob = function(semanticError, loaderJob) {
//  };


  /* createMappings()
     Create additional TableMappings before connecting.
  */
//   dataLoader.createMappings = function() {
//   };
}