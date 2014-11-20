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

var udebug = unified_debug.getLogger("BadRecordLogger.js");

var theController, theBadRecordLogger;

/* QuietLogger is a null logger */
function QuietLogger(controller) {
  udebug.log("QuietLogger Constructor");
  theController = controller;
}

QuietLogger.prototype.logRecord = function(record) {
};

QuietLogger.prototype.end = function() {
  udebug.log("end");
  theController.loggerFinished();
};


/* BadRecordLogger maintains the BADFILE */
function BadRecordLogger(controller) {
  udebug.log("BadRecordLogger Constructor");
  this.fileName   = controller.options.badfile;
  this.fd         = null;
  this.queue      = [];

  theController = controller;
  theBadRecordLogger = this;
}

function runQueue() {
  var record = theBadRecordLogger.queue[0];
  if(record === "CLOSE") {
    fs.close(theBadRecordLogger.fd, function() {
      theController.loggerFinished();
    });
  } else {
    record.logger(theBadRecordLogger.fd, function(err) {
      theBadRecordLogger.queue.shift();
      if(theBadRecordLogger.queue.length) {
        runQueue();
      }
    });
  }
}

BadRecordLogger.prototype.runQueue = function() {
  if(this.queue.length === 1) {
    runQueue();
  }
};

BadRecordLogger.prototype.logRecord = function(record) {
  this.queue.push(record);

  if(this.fd > 0) {
    this.runQueue();
  }
  else if(this.fileName === "") {  // No badfile specified
    console.log("No BADFILE specifed.  Writing rejected lines to stderr.");
    this.fd = 2;
    runQueue();
  }
  else if(this.fd === null) {   // Open file now
    fs.open(this.fileName, 'w', function(err, fd) {
      if(err) {
        console.log("Error opening BADFILE:", err);
        console.log("Rejected lines will be written to stderr.");
        theBadRecordLogger.fd = 2;
      } else {
        theBadRecordLogger.fd = fd;
      }
      runQueue();
    });
  }
}

BadRecordLogger.prototype.end = function() {
  udebug.log("end", this.fd);
  if(this.fd > 2) {
    this.queue.push("CLOSE");
    this.runQueue();
  }
  else {
    /* You can't close STDERR, so just tell the controller we're done */
    theController.loggerFinished();
  }
}

exports.BadRecordLogger = BadRecordLogger;

