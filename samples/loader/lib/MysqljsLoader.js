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

/* File Scope singletons */
var theController;
var theLoader;


//////////////////////////////  MysqljsLoader     ///////////


function MysqljsLoader(options, session, controller) {
  theController       = controller;
  this.options        = options;
  this.session        = session;
  this.fibA           = 0;  // Previous two Fibonacci numbers
  this.fibB           = 0;
  this.fibC           = 1;  // Current Fibonacci number & current batch size
  this.goingUp        = true;  // is batch size increasing?
  this.batch          = this.session.createBatch();
  this.batchSize      = 0;  // actual current size of this.batch
  this.currentOpCount = 0;
  this.priorOpCount   = 0;
  this.aborted        = 0;

  theLoader = this;
};


MysqljsLoader.prototype.loadItem = function(record) {
  this._store(this.batch, record);
  if(++this.batchSize >= this.fibC) {
    this.executeBatch();
  }
};


MysqljsLoader.prototype._store = function(context, record) {
  // Here we create a closure over { record } for each row inserted

  // Assess errors:
  //    On temporary error, retry the row.   [DONE]
  //    On table full, abort the loader   [DONE]
  //    On cluster shutdown, abort the loader
  //    TODO: Also handle errors originating from the mysql adapter
  function rowCallback(error) {
    theLoader.currentOpCount++;

    if(error) {
      if(error.ndb_error) {
        if(error.ndb_error.classification === "InsufficientSpace") {
          theLoader.abort(error);
        }
        else if(error.ndb_error.status === "TemporaryError") {
          this.batchSizeDown();
          theLoader.loadItem(record);  // retry
          return;  // Do not set record.error or register as complete
          // TODO: Maintain a retry count & eventually give up
        }
      }
    }
    record.error = error;
    theController.loaderRecordComplete(record);
  }

  if(this.options.replaceMode) {
    context.save(record.class, record.row, rowCallback);
  } else {
    context.persist(record.class, record.row, rowCallback);
  }
}


MysqljsLoader.prototype.executeBatch = function() {
  this.batchSize = 0;
  this.batch.execute(function(err) {
    // This is the batch callback
  });
};


MysqljsLoader.prototype.dataSourceIsPaused = function() {
  // The data source has paused, so we're not going to get any more rows,
  // so we'd better execute the current batch.
  this.executeBatch();
};


// FLOW CONTROL ALGORITHM
// We start at batch size = 1 and batch size direction = up.
// If we processed fewer rows in this interval than in the previous one,
// reverse the direction.
// Batch sizes go up and down following the Fibonacci Sequence 1,1,2,3,5,8,...
MysqljsLoader.prototype.onTick = function() {
  if(this.currentOpCount < this.priorOpCount) {
    this.goingUp = (! this.goingUp);
  }

  if(this.goingUp) {
    this.batchSizeUp();
  } else {
    this.batchSizeDown();
  }

  this.priorOpCount = this.currentOpCount;
  this.currentOpCount = 0;
};

MysqljsLoader.prototype.batchSizeUp = function() {
  this.fibA = this.fibB;
  this.fibB = this.fibC;
  this.fibC = (this.fibA + this.fibB);
};

MysqljsLoader.prototype.batchSizeDown = function() {
  if(this.fibC > 1) {
    this.fibC = this.fibB;
    this.fibB = this.fibA;
    this.fibA = (this.fibC - this.fibB);
  }
};


MysqljsLoader.prototype.abort = function(error) {
  if(! this.aborted) {
    theController.loaderAborted(error);
  }
  this.aborted = 1;
};

MysqljsLoader.prototype.end = function() {
  this.executeBatch();
}


//////////////////////////////  AtomicMysqljsLoader     ///////////

/* The "atomic" variant of the loader handles the IN ONE TRANSACTION version
   of a loader job.  Any single failure causes transaction rollback.
   This version does a single execute(NoCommit) followed by commit or rollback.
   Alternative implementation: 
     Factor out common methods into AbstractMysqljsLoader.
     Let AtomicMysqljsLoader use the batch-size algorithm (execute nocommit).
     This has the advantages of reusing code & behavior.
     It could have the disadvantage of longer total time holding locks.
*/

function AtomicMysqljsLoader(options, session, controller) {
  this.session        = session;
  this.batch          = null;
  this.pending        = 0;
  this.recordQueue    = [];

  theController = controller;
  theLoader = this;

  this.session.currentTransaction().begin();
  this.batch = this.session.createBatch();
};


AtomicMysqljsLoader.prototype.loadItem = function(record) {

  /* Any error causes the transaction to be rolled back */
  function rowCallback(error) {
    if(error) {
      console.log("Aborting IN ONE TRANSACTION load due to error.");
      console.log(error);
      console.log(record.row);
      theLoader.rollback();
    } else {
      theLoader.pending--;
    }
    if(theLoader.pending === 0) {
      theLoader.commit();
    }
  }

  this.recordQueue.push(record);
  if(this.options.replaceMode) {
    this.batch.save(record.class, record.row, rowCallback);
  } else {
    this.batch.persist(record.class, record.row, rowCallback);
  }
  this.pending++;
};

AtomicMysqljsLoader.prototype.end = function() {
  this.batch.execute(function(err) {
    // This is the batch callback
  });
};

AtomicMysqljsLoader.prototype.commit = function() {
  this.session.currentTransaction().commit(function(error) {
    var record;

    if(error) {
      theController.loaderAborted(error);
    } else {
      record = theLoader.recordQueue.pop();
      while(record) {
        theController.loaderRecordComplete(record);
        record = theLoader.recordQueue.pop();
      }
    }
  })
};

AtomicMysqljsLoader.prototype.rollback = function() {
  this.session.currentTransaction().commit(function(error) {
    theController.loaderAborted();
  });
};


exports.MysqljsLoader       = MysqljsLoader;
exports.AtomicMysqljsLoader = AtomicMysqljsLoader;
