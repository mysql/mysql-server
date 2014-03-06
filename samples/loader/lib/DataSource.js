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

var RandomRowGenerator = require("./RandomData.js").RandomRowGenerator,
    LineScanner = require("./Scanner.js").LineScanner,
    TextFieldScanner = require("./Scanner.js").TextFieldScanner,
    util = require("util");

var theDataSource;
var theController;


/*
    Data Source API
    
    Data Source Methods (for controller)
    ------------------------------------
    ALL METHODS ARE IMMEDIATE and return undefined unless otherwise noted.
    start()    : controller asks data source to begin sending records.
            Returns an Error, or undefined on success.
    pause()    : controller asks data source to pause.
    resume()   : controller asks paused data source to resume sending.
    bool isPaused() : data source reports whether it is currently paused.
    end()      : controller asks data source to clean up and shut down.
    skip(bool) : if true, data source should read lines but discard them.

    Data Source callbacks into controller
    -------------------------------------
    dsNewItem(obj)    : Data Source supplies obj to controller for loading.
                        obj is an object with property names corresponding
                        to column names.
    dsDiscardedItem() : Data Source informs controller that a row was read
                        and discarded (skipped).
    dsFinished(err)   : Data Source has finished.
                        If the controller has called end(), err is undefined.
                        Otherwise err holds an Error explaining why the 
                        DataSource has stopped.
*/



function AbstractDataSource() {
  this.started    = 0;
  this.running    = 0;
  this.shutdown   = 0;
  this.skipping   = false;
}

AbstractDataSource.prototype.start = function() {
  this.started = 1;
  this.running = 1;
  this.runIfReady();
};

AbstractDataSource.prototype.pause = function() {
  this.running = 0;
};

AbstractDataSource.prototype.resume = function() {
  this.running = 1;
  this.runIfReady();
};

AbstractDataSource.prototype.isPaused = function() {
  return (this.running || this.shutdown) ? false : true;
};

AbstractDataSource.prototype.skip = function(doSkip) {
  this.skipping = doSkip;
};

AbstractDataSource.prototype.end = function() {
  this.shutdown = 1;
  if(this.running) {
    this.running = 0;
  } else {
    this.controller.dsFinished();
  }
};


////////////////////////////// RandomDataSource    ///////////

function RandomDataSource(options, tableHandler, controller) {
  this.options    = options;
  this.controller = controller;
  this.generator  = new RandomRowGenerator(tableHandler);
}

util.inherits(RandomDataSource, AbstractDataSource);


/* Record represents a single item of data from a DataSource.
   RdsRecord is the variety of Record created by RandomDataSource.
*/
function RdsRecord(row) {
  this.row    = row;
  this.error  = null;
}

RdsRecord.prototype.logger = function(fd, callback) {
  var message =
     "/* " + this.error.message + " */\n" +
     JSON.stringify(this.row) + "\n";
  var buffer = new Buffer(message);
  fs.write(fd, buffer, 0, buffer.length, null, callback);
};

RandomDataSource.prototype.runIfReady = function() {
  if( this.started &&
      this.running)        { this.run(); }
};

RandomDataSource.prototype.run = function() {
  var newRow, record;

  while(this.running === 1) {
    newRow = this.generator.newRow();
    if(this.skipping) {
      this.controller.dsDiscardedItem();
    } else {
      record = new RdsRecord(newRow);
      this.controller.dsNewItem(record);
    }
  }

  if(this.shutdown) {
    this.controller.dsFinished();
  }
};


//////////////////////////////  FileDataSource     ///////////

/* Version of Record created by FileDataSource
*/

function FdsRecord(lineScannerSpec) {
  this.row    = null;
  this.error  = null;
  this.source = lineScannerSpec.source;
  this.start  = lineScannerSpec.start;
  this.end    = lineScannerSpec.end;
}

FdsRecord.prototype.logger = function(fd, callback) {
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
  this.options      = job.dataSource;
  this.columns      = job.columnDefinitions;
  this.fd           = null;
  this.bufferSize   = 16384;
  this.scanSpec     = null;
  this.lineScanner  = new LineScanner(this.options);
  this.recopied    = 0;  // no. of characters left from previous read buffer

  theDataSource    = this;
  theController    = controller;

  if(this.options.isJSON) {
    this.fieldScanner = new JSONFieldScanner(this.columns);
  } else {
    this.fieldScanner = new TextFieldScanner(this.columns, this.options);
  }

  if(this.options.file === "-") {
    this.fd = 0;  // STDIN.  (Make buffer size smaller?)
    this.read();
  } else {
    fs.open(this.options.file, 'r', function(err, fd) {
      theDataSource.onFileOpen(err, fd);
    });
  }
}

util.inherits(FileDataSource, AbstractDataSource);

FileDataSource.prototype.onFileOpen = function(err, fd) {
  if(err) {
    theController.dsFinished(err);
  } else {
    this.fd = fd;
  }
  this.read();
}

// TODO: Handle the case of a single record that is larger than the buffer
FileDataSource.prototype.read = function() {
  var buffer, readLen;
  buffer = new Buffer(this.bufferSize);
  if(this.scanSpec && ! this.scanSpec.complete) {
    /* Rewrite partial last record into new buffer */
    this.recopied = buffer.write(this.scanSpec.source.substring(this.scanSpec.start));
  } else {
    this.recopied = 0;
  }
  readLen = this.bufferSize - this.recopied;
  fs.read(this.fd, buffer, this.recopied, readLen, null, function(err, sz, buf) {
    theDataSource.onRead(err, sz, buf);
  });
};

FileDataSource.prototype.onRead = function(err, size, buffer) {
  size += this.recopied;
  if(err) {
    theController.dsFinished(err);
  } else if(size === 0) {
    theController.dsFinished();
  } else {
    this.scanSpec = this.lineScanner.newSpec(buffer.toString('utf8', 0, size));
    this.runIfReady();
  }
};

FileDataSource.prototype.runIfReady = function() {
  if( this.started &&
      this.running &&
      this.scanSpec)     { this.run(); }
};

FileDataSource.prototype.run = function() {
  var spec = this.scanSpec;
  while(this.running && ! spec.eof) {
    this.lineScanner.scan(spec);
    if(! spec.eof) {
      theController.plugin.onScanLine(spec.source, spec.start, spec.end);
      if(this.skipping) {
        theController.dsDiscardedItem();
      } else {
        // TODO: run the FieldScanner
      }
      spec.start = spec.end + 1;
    }
  }

  if(this.shutdown) {
    theController.dsFinished();
  } else if(this.scanSpec.eof) {
    this.read();
  }
};


exports.RandomDataSource = RandomDataSource;
exports.FileDataSource = FileDataSource;

