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
 along with this program; if not, write to the  Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

var udebug = unified_debug.getLogger("Controller.js"),
    DbWriter = require("./DbWriter.js").DbWriter,
    BadRecordLogger = require("./BadRecordLogger.js").BadRecordLogger,
    DataSource = require("./DataSource.js"),
    RandomDataSource = DataSource.RandomDataSource,
    FileDataSource = DataSource.FileDataSource;


var theController;   // File-scope singleton

function Controller(job, session, finalCallback) {
  this.session         = session;
  this.options         = job.controller;
  this.destination     = job.destination;
  this.plugin          = job.plugin;
  this.finalCallback   = finalCallback;
  this.dataSource      = null;
  this.writer          = null;
  this.badRecordLogger = null;
  this.stats = {
    rowsProcessed      : 0, // all rows processed by data source
    rowsSkipped        : 0, // rows procesed by data source but skipped
    rowsComplete       : 0, // all rows completed by loader (success or failure)
    rowsError          : 0, // rows failed by loader
    tickNumber         : 0
  };
  this.shutdown        = 0;
  this.ticker          = null;  // interval timer
  this.fatalError      = null;

  /* If the data source is maxLead rows ahead of the loader, pause it.
     When the difference shrinks to minLead, resume it.  
  */
  this.maxLead = 2000;
  this.minLead = 1000;

  /* Data Source */
  if(this.options.randomData) {
    this.dataSource = new RandomDataSource(job, this);
  } else {
    this.dataSource = new FileDataSource(job, this);
  }

  /* Data Writer */
  this.writer = new DbWriter(job.dataLoader, session, this);
  if(this.options.inOneTransaction) {
    this.writer.beginAtomic();
  }

  /* Bad Record Logger */
  this.badRecordLogger = new BadRecordLogger(this);

  /* Set singleton */
  theController = this;

  // Adjustments
  if(this.options.skipRows && this.options.maxRows) {
    this.options.maxRows += this.options.skipRows;
  }

  // Sanity Checks
  if(this.options.inOneTransaction && this.options.randomData &&
     (! this.options.maxRows)) {
    this.fatalError = new Error("This job would attempt to build a single " +
                                "transaction of infinite size.");
    this.finalCallback(this.fatalError, null);
  }

  process.on('exit', function() { udebug.log(theController.stats); });
}

Controller.prototype.run = function() {
  if(this.options.workerId || this.options.skipRows) {
    this.dataSource.skip(true);
  }
  this.dataSource.start();
};

Controller.prototype.dsNewItem = function(record) {
  var handlerReturnCode;

  /* Count all rows processed */
  this.stats.rowsProcessed++;

  /* Set the default Domain Object Constructor */
  record.class = this.destination.rowConstructor;

  /* Pass the item to the plugin */
  handlerReturnCode = this.plugin.onReadRecord(record);

  /* Send the item to the loader */
  if(handlerReturnCode === false) {
    this.stats.rowsSkipped++;
  } else {
    this.writer.loadItem(record);
  }

  /* Stop if we have hit the limit */
  if(this.stats.rowsProcessed === this.options.maxRows) {
    this.dataSource.end();
  }
  /* Skip the next record if this is one worker of many */
  if(this.options.nWorkers > 1) {
    this.dataSource.skip(true);
  }
  /* If the data source is too far ahead of the loader, pause it */
  if(this.stats.rowsProcessed - this.stats.rowsComplete > this.maxLead) {
    this.dataSource.pause();
    this.writer.dataSourceIsPaused();
  }
};

Controller.prototype.dsDiscardedItem = function() {
  this.stats.rowsProcessed++;
  this.stats.rowsSkipped++;

  /* Stop if we have hit the limit */
  if(this.stats.rowsProcessed === this.options.maxRows) {
    this.dataSource.end();
  }
  /* Turn off skipping */
  if((this.stats.rowsSkipped >= this.options.skipRows)
    && (this.stats.rowsProcessed % this.options.nWorkers === this.options.workerId)) {
      this.dataSource.skip(false);
  }
};

/* SHUTDOWN SEQUENCE:
   DataSource sends dsFinished() to controller.
   Controller sends end() to dbWriter.
   DbWriter sends final record to controller in loaderRecordComplete().
   Controller calls writer.endAtomic() [even if not in atomic mode].
   DbWriter calls loaderTransactionDidCommit() or loaderTransactionDidRollback().
   Controller sends end() to BadRecordLogger.
   BadRecordLogger sends loggerFinished() to controller.
   Controller calls plugin.onFinished(controllerCallback).
   Plugin calls its provided callback.
   Controller shuts down.
*/
Controller.prototype.dsFinished = function(err) {
  udebug.log("dsFinished");
  if(err) {
    this.fatalError = err;
  }
  this.shutdown = 1;
  this.writer.end();
  this.commitIfComplete();
};

Controller.prototype.commitIfComplete = function() {
  var rowsLeft = this.stats.rowsProcessed -
                 (this.stats.rowsComplete + this.stats.rowsSkipped);
  if(rowsLeft === 0) {
    this.writer.endAtomic();
  } else {
    udebug.log("No shutdown yet - ", rowsLeft, "pending");
  }
};

Controller.prototype.loaderTransactionDidCommit = function() {
  this.badRecordLogger.end();
}

Controller.prototype.loaderTransactionDidRollback = function() {
  this.fatalError = new Error("Transaction rolled back.  No records loaded.");
  this.badRecordLogger.end();
};

Controller.prototype.loggerFinished = function() {
  this.plugin.onFinished(function() {
    theController.finished();
  });
};

Controller.prototype.loaderRecordComplete = function(record) {
  this.stats.rowsComplete++;

  if(record.error) {
    this.stats.rowsError++;
    this.badRecordLogger.logRecord(record);
    this.plugin.onRecordError(record);
  } else {
    this.plugin.onRecordStored(record);
  }

  /* Resume if paused */
  if(this.dataSource.isPaused() &&
     (this.stats.rowsProcessed - this.stats.rowsComplete < this.minLead)) {
     this.dataSource.resume();
  }

  /* Shut down if the last row was complete */
  if(this.shutdown) {
    this.commitIfComplete();
  }

  /* Set the interval timer for 50 msec. if not yet set */
  else if(! (this.ticker)) {
    this.ticker = setInterval(function() { theController.onTick(); }, 50);
  }
};

Controller.prototype.loaderAborted = function(err) {
  if(err) {
    this.fatalError = err;
  }
  this.dataSource.end();
};

Controller.prototype.finished = function() {
  if(this.ticker) {
    clearInterval(this.ticker);
  }
  this.finalCallback(this.fatalError, this.stats);
};


Controller.prototype.onTick = function() {
  var avg;
  this.stats.tickNumber++;

  /* Report status after 5 seconds */
  if(this.stats.tickNumber === 100) {
    avg = Math.floor(this.stats.rowsComplete / 5);
    console.log("Loading", avg,"records per second.");
  }

  /* Loader has a tick handler */
  this.writer.onTick();

  /* Plugin has a tick handler */
  this.plugin.onTick(this.stats);
};


exports.Controller = Controller;

