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

var LineScanner = require("./Scanner.js").LineScanner;

/*
    Data Source API
    
   Data Source Methods (for controller)
    ------------------------------------
    ALL METHODS ARE IMMEDIATE and return undefined unless otherwise noted.
    start()    : controller asks data source to begin sending records.
            Returns an Error, or undefined on success.
    pause()    : controller asks data source to pause.
    resume()   : controller asks paused data source to resume sending.
    end()      : controller asks data source to clean up and shut down.
    bool isPaused() : data source reports whether it is currently paused.
    skip(bool) : if true, data source should read lines but discard them.

    Data Source callbacks into controller
    -------------------------------------
    dsNewItem(record)  : Data Source supplies record to controller for loading.

    dsDiscardedItem() : Data Source informs controller that a row was read
                        and discarded (skipped).

    dsFinished(err)   : Data Source has finished.
                        If the controller has called end(), err is undefined.
                        Otherwise err holds an Error explaining why the 
                        DataSource has stopped.
*/


var theDataSource;
var theController;


/* Record represents a single item of data from this data source.
   There is a similar class Record in RandomDataSource.js.
*/
function Record(lineScannerSpec) {
  this.row    = null;
  this.error  = null;
  this.source = lineScannerSpec.source;
  this.start  = lineScannerSpec.start;
  this.end    = lineScannerSpec.end;
}

Record.prototype.logger = function(fd, callback) {
  var message = "";
  if(theDataSource.options.commentStart) {
    message = theDataSource.options.commentStart +
              this.error.message +
              theDataSource.options.lineEndString;
  }
  message += this.source.substring(this.start, this.end);
  var buffer = new Buffer(message);
  fs.write(fd, buffer, 0, buffer.length, null, callback);
};



function FileDataSource(job, controller) {
  this.options     = job.dataSource;
  this.columns     = job.columnDefinitions;
  this.started     = 0;
  this.running     = 0;
  this.shutdown    = 0;
  this.parser      = null;   // Parser (TextParser or JsonParser)
  this.fd          = null;
  this.bufferSize  = 16384;
  this.scanSpec    = null;
  this.lineScanner = new LineScanner(this.options);

  theDataSource    = this;
  theController    = controller;

  if(this.options.file === "-") {
    this.fd = 0;  // STDIN.  (Make buffer size smaller?)
    this.read();
  } else {
    fs.open(this.options.file, 'r', function(err, fd) {
      theDataSource.onFileOpen(err, fd);
    });
  }
}

FileDataSource.prototype.onFileOpen = function(err, fd) {
  if(err) {
    theController.dsFinished(err);
  } else {
    this.fd = fd;
  }
  this.read();
}

FileDataSource.prototype.pause = function() {
  this.running = 0;
};

FileDataSource.prototype.resume = function() {
  this.running = 1;
  this.runIfReady();
};

FileDataSource.prototype.isPaused = function() {
  return (this.running || this.shutdown) ? false : true;
};

FileDataSource.prototype.end = function() {
  this.running = 0;
  this.shutdown = 1;
};

FileDataSource.prototype.start = function() {
  this.started = 1;
  this.running = 1;
  this.runIfReady();
};

FileDataSource.prototype.read = function() {
  var startPos = 0;
  if(this.scanSpec) {   // TODO: Rebuffer partial line at end of last read

  }
  this.scanSpec = null;
  var buffer = new Buffer(this.bufferSize);
  fs.read(this.fd, buffer, startPos, this.bufferSize, null, function(err, sz, buf) {
    theDataSource.onRead(err, sz, buf);
  });
};

FileDataSource.prototype.runIfReady = function() {
  if( this.started &&
      this.running &&
      this.scanSpec)     { this.run(); }
}

FileDataSource.prototype.onRead = function(err, size, buffer) {
  if(err) {
    theController.dsFinished(err);
  } else if(size === 0) {
    theController.dsFinished();
  } else {
    this.scanSpec = this.lineScanner.newSpec(buffer.toString('utf8', 0, size));
    this.runIfReady();
  }
};

FileDataSource.prototype.run = function() {
  var spec = this.scanSpec;
  do {
    this.lineScanner.scan(spec);
    if(this.skipping) {
      theController.dsDiscardedItem();
    } else {
      // TODO: run the FieldScanner
    }
    spec.start = spec.end + 1;
  } while(this.running && ! spec.eof);

  if(this.shutdown) {
    theController.dsFinished();
  } else if(this.scanSpec.eof) {
    this.read();
  }
};

exports.FileDataSource = FileDataSource;
