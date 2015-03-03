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
    util = require("util"),
    udebug = unified_debug.getLogger("DataSource.js");

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
  this.started    = 0;       // "has been started"
  this.running    = 0;       // "is not currently paused"
  this.shutdown   = 0;       // "has been told to shutdown"
  this.skipping   = false;   // "should skip the next record", i.e.
                             // call dsDiscardedItem() rather than dsNewItem()
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
  udebug.log("end");
  this.shutdown = 1;
  if(this.running) {
    this.running = 0;
  } else {
    this.controller.dsFinished();
  }
};


////////////////////////////// RandomDataSource    ///////////

function RandomDataSource(job, controller) {
  this.options    = job.options;
  this.controller = controller;
  this.generator  = new RandomRowGenerator(job.destination.getTableHandler());
}

util.inherits(RandomDataSource, AbstractDataSource);

/* Record Constructor */
RandomDataSource.prototype.Record = function(row) {
  this.row    = row;
  this.error  = null;
};

RandomDataSource.prototype.Record.prototype.logger = function(fd, callback) {
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
  udebug.log("run");
  var newRow, record;

  while(this.running === 1) {
    newRow = this.generator.newRow();
    // Note there is no onScanLine plugin for random data
    if(this.skipping) {
      this.controller.dsDiscardedItem();
    } else {
      record = new this.Record(newRow);
      this.controller.dsNewItem(record);
    }
  }

  if(this.shutdown) {
    this.controller.dsFinished();
  }
};


//////////////////////////////  BufferDescriptor     ///////////

function BufferDescriptor(source) {
  this.source         = source;
  this.lineStart      = 0;
  this.lineEnd        = 0;
  this.lineHasFields  = false;
  this.atEnd          = false;
}

BufferDescriptor.prototype.println = function() {
  udebug.log(this.source.substring(this.lineStart, this.lineEnd));
};

//////////////////////////////  FileDataSource     ///////////

function FileDataSource(job, controller) {
  udebug.log("FileDataSource()");
  this.options      = job.dataSource;
  this.columns      = job.destination;
  this.controller   = controller;
  this.fd           = null;
  this.bufferSize   = 16384;
  this.bufferDesc   = null;
  this.lineScanner  = new LineScanner(this.options);
  this.recopied     = 0;  // no. of characters left from previous read buffer
  this.physLineNo   = 0;  // actual lines (not records) read from data file

  theDataSource    = this;
  theController    = controller;

  if(this.options.isJSON) {
    this.fieldScanner = new JSONFieldScanner(this.columns);
  } else {
    this.fieldScanner = new TextFieldScanner(this.options);
  }

  if(this.options.useControlFile) {
    this.skipToBEGINDATA();
  } else if(this.options.file === "-") {
    this.fd = 0;  // STDIN.  (Make buffer size smaller?)
    this.read();
  } else {
    fs.open(this.options.file, 'r', function(err, fd) {
      theDataSource.onFileOpen(err, fd);
    });
  }
}

util.inherits(FileDataSource, AbstractDataSource);

// Record Constructor
FileDataSource.prototype.Record = function(bufferDescriptor, fields) {
  this.row    = fields;
  this.error  = null;
  this.source = bufferDescriptor.source;
  this.start  = bufferDescriptor.lineStart;
  this.end    = bufferDescriptor.lineEnd;
};

FileDataSource.prototype.Record.prototype.logger = function(fd, callback) {
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

FileDataSource.prototype.onFileOpen = function(err, fd) {
  udebug.log("onFileOpen fd:", fd);
  if(err) {
    theController.dsFinished(err);
  } else {
    this.fd = fd;
    this.read();
  }
}

// TODO: Handle the case of a single record that is larger than the buffer
//       OR document this limitation and let the user select the buffer size
FileDataSource.prototype.read = function() {
  udebug.log("read");
  var buffer, readLen;
  buffer = new Buffer(this.bufferSize);

  /* Rewrite partial last record into new buffer */
  if(this.bufferDesc && ! this.bufferDesc.lineEnd < this.bufferDesc.lineStart) {
    this.recopied = buffer.write(this.bufferDesc.source.substring(this.bufferDesc.lineStart));
  } else {
    this.recopied = 0;
  }

  readLen = this.bufferSize - this.recopied;
  fs.read(this.fd, buffer, this.recopied, readLen, null, function(err, sz, buf) {
    theDataSource.onRead(err, sz, buf);
  });
};

FileDataSource.prototype.onRead = function(err, size, buffer) {
  udebug.log("onRead size:", size);
  size += this.recopied;
  if(err) {
    theController.dsFinished(err);
  } else if(size === 0) {
    udebug.log("onRead: EOF");
    this.running = 0;
    this.end();
  } else {
    this.bufferDesc = new BufferDescriptor(buffer.toString('utf8', 0, size));
    this.runIfReady();
  }
};

FileDataSource.prototype.skipToBEGINDATA = function() {
  var ctl, desc;
  ctl = this.options.useControlFile;
  desc = new BufferDescriptor(ctl.text, 0, ctl.text.length);
  this.fd = ctl.openFd;
  this.lineScanner.skipPhysicalLines(desc, ctl.inlineSkip);
  this.physLineNo = ctl.inlineSkip;
  udebug.log("skipToBEGINDATA skip to line:", this.physLineNo);
  desc.lineStart = desc.lineEnd;  // Advance to the newline after BEGINDATA
  this.bufferDesc = desc;
  this.options.useControlFile.buffer = null; // discard reference
  this.runIfReady();
};

FileDataSource.prototype.runIfReady = function() {
  if( this.started &&
      this.running &&
      this.bufferDesc)     { this.run(); }
};

FileDataSource.prototype.run = function() {
  udebug.log("run");
  var desc, extraLines, fields, row, record;
  desc = this.bufferDesc;
  extraLines = 0;

  while(this.running && ! desc.atEnd) {
    extraLines = this.lineScanner.scan(desc);  // Scan the line
    this.physLineNo += (extraLines + 1);
    if(this.options.columnsInHeader) {
      if(desc.lineHasFields) {
        this.options.columnsInHeader = null;  // reset from true to null
        fields = this.fieldScanner.scan(desc);  // Extract the data fields
        this.columns.setColumnsFromArray(fields);  // Creates a new tableHandler on next write
      }
    } else if (this.skipping) {
      theController.dsDiscardedItem();
    } else {
      theController.plugin.onScanLine(this.physLineNo, desc.source,
                                      desc.lineStart, desc.lineEnd);
      if(desc.lineHasFields) {
        fields = this.fieldScanner.scan(desc);    // Extract the data fields
        record = new this.Record(desc, row);
        theController.dsNewItem(record);
      }
    }
    desc.lineStart = desc.lineEnd + 1;   // Advance to first char. of next line
  }

  if(this.shutdown) {
    /* Controller has shut us down, e.g. because of DO N ROWS */
    theController.dsFinished();
  } else if(this.fd === null) {
    /* We have been reading data from a string passed on the command line */
    theController.dsFinished();
  } else if(desc.atEnd) {
    /* Read to the end of a read buffer; read again. */
    this.read();
  } else {
    /* Reaching this point is probably a bug */
    console.log("why are we here?", this);
  }
};


exports.RandomDataSource = RandomDataSource;
exports.FileDataSource = FileDataSource;

