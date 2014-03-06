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

var DataSource = require("./DataSource.js"),
    Loader     = require("./MysqljsLoader.js"),
    BadRecordLogger = require("./BadRecordLogger.js").BadRecordLogger,
    RandomDataSource = DataSource.RandomDataSource,
    FileDataSource = DataSource.FileDataSource,
    MysqljsLoader = Loader.MysqljsLoader,
    AtomicMysqljsLoader = Loader.AtomicMysqljsLoader;

var theController;   // File-scope singleton

function Controller(job, session) {
  var tableHandler;

  this.session         = session;
  this.options         = job.controller;
  this.destination     = job.destination;
  this.plugin          = job.plugin;
  this.dataSource      = null;
  this.loader          = null;
  this.badRecordLogger = null;
  this.rowsProcessed   = 0; // all rows processed by data source
  this.rowsSkipped     = 0; // rows procesed by data source but skipped
  this.rowsComplete    = 0; // all rows completed by loader (success or failure)
  this.rowsError       = 0; // rows failed by loader
  this.shutdown        = 0;
  this.ticker          = null;  // interval timer
  this.tickNumber      = 0;
  this.firstRow        = true;

  /* If the data source is maxLead rows ahead of the loader, pause it.
     When the difference shrinks to minLead, resume it.  
  */
  this.maxLead = 2000;
  this.minLead = 1000;

  // note that this uses undocumented path to access the dbTableHandler:
  tableHandler = this.destination.rowConstructor.prototype.mynode.tableHandler;

  /* Data Source */
  if(this.options.randomData) {
    this.dataSource = new RandomDataSource(job.dataSource, tableHandler, this);
  } else {
    this.dataSource = new FileDataSource(job, this);
  }

  /* Data Loader */
  if(this.options.inOneTransaction) {
    this.loader = new AtomicMysqljsLoader(job.dataLoader, session, this);
  } else {
    this.loader = new MysqljsLoader(job.dataLoader, session, this);
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
    console.log("This job would attempt to build a single transaction"
                + " of infinite size.");
    process.exit();
  }
}

Controller.prototype.run = function() {
  if(this.options.workerId || this.options.skipRows) {
    this.dataSource.skip(true);
  }
  this.dataSource.start();
};

Controller.prototype.dsNewItem = function(record) {
  var handlerReturnCode;

  /* Get column names from first row of data */
  if(this.firstRow) {
    if(this.options.columnsInHeader) {
      this.destination.setColumnsFromObject(record.row);
    }
    this.firstRow = false;
  }

  this.rowsProcessed++;

  /* Set the default Domain Object Constructor */
  record.class = this.destination.rowConstructor;

  /* Pass the item to the plugin */
  handlerReturnCode = this.plugin.onReadRecord(record);

  /* Send the item to the loader */
  if(handlerReturnCode !== false) {
    this.loader.loadItem(record);
  }

  /* Stop if we have hit the limit */
  if(this.rowsProcessed === this.options.maxRows) {
    this.dataSource.end();
  }
  /* Skip the next record if this is one worker of many */
  if(this.options.nWorkers > 1) {
    this.dataSource.skip(true);
  }
  /* If the data source is too far ahead of the loader, pause it */
  if(! this.options.inOneTransaction) {
    if(this.rowsProcessed - this.rowsComplete > this.maxLead) {
      this.dataSource.pause();
      this.loader.dataSourceIsPaused();
    }
  }
};

Controller.prototype.dsDiscardedItem = function() {
  this.rowsProcessed++;
  this.rowsSkipped++;

  /* Stop if we have hit the limit */
  if(this.rowsProcessed === this.options.maxRows) {
    this.dataSource.end();
  }
  /* Turn off skipping */
  if((this.rowsSkipped >= this.options.skipRows)
    && (this.rowsProcessed % this.options.nWorkers === this.options.workerId)) {
      this.dataSource.skip(false);
  }
};

/* SHUTDOWN SEQUENCE:
   DataSource sends dsFinished() to controller.
   Controller sends end() to loader.
   Loader sends final record to controller in loaderRecordComplete().
   Controller sends end() to BadRecordLogger.
   BadRecordLogger sends loggerFinished() to controller.
   Controller calls plugin.onFinished(controller).
   Plugin calls controller.pluginFinished().
*/
Controller.prototype.dsFinished = function(err) {
  if(err) {
    console.log(err);
  }
  this.shutdown = 1;
  this.loader.end();
};

Controller.prototype.loggerFinished = function() {
  this.plugin.onFinished(this);
};

Controller.prototype.pluginFinished = function() {
  this.finished();
};

Controller.prototype.loaderRecordComplete = function(record) {
  this.rowsComplete++;

  if(record.error) {
    this.rowsError++;
    this.badRecordLogger.logRecord(record);
    this.plugin.onRecordError(record);
  } else {
    this.plugin.onRecordStored(record);
  }

  /* Resume if paused */
  if(this.dataSource.isPaused() &&
     (this.rowsProcessed - this.rowsComplete < this.minLead)) {
     this.dataSource.resume();
  }

  /* Shut down if the last row was complete */
  if(this.shutdown &&
    (this.rowsComplete + this.rowsSkipped === this.rowsProcessed)) {
      this.badRecordLogger.end();
  }
  /* Set the interval timer for 50 msec. if using the non-atomic loader 
     and the interval is not yet set
  */
  else if(! (this.ticker || this.options.inOneTransaction)) {
    this.ticker = setInterval(function() { theController.onTick(); }, 50);
  }
};

Controller.prototype.loaderAborted = function(err) {
  if(err) {
    console.log(err);
  }
  this.dataSource.end();
};

Controller.prototype.finished = function() {
  if(this.ticker) {
    clearInterval(this.ticker);
  }
  this.session.close().then(function() {
    console.log("Rows processed:", theController.rowsProcessed,
                "Skipped", theController.rowsSkipped,
                "Loaded:", theController.rowsComplete - theController.rowsError,
                "Failed:", theController.rowsError);
    return theController.rowsError;
  });
};

Controller.prototype.onTick = function() {
  var avg;
  this.tickNumber++;

  /* Report status after 5 seconds */
  if(this.tickNumber === 100) {
    avg = Math.floor(this.rowsComplete / 5);
    console.log("Loading", avg,"records per second.");
  }

  /* Loader has a tick handler */
  this.loader.onTick();

  /* Plugin has a tick handler */
  this.plugin.onTick();
};


exports.Controller = Controller;

