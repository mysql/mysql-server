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

var udebug = unified_debug.getLogger("DbWriter.js");

/* File Scope singletons */
var theController;
var theWriter;


//////////////////////////////  BatchManager  ///////////
//
// BatchManager is in charge of managing the batch size for best performance.
// We start at batch size = 1 and batch size direction = up.
function BatchManager(dbWriter) {
  this.writer         = dbWriter;
  this.fibA           = 0;  // Previous two Fibonacci numbers
  this.fibB           = 0;
  this.fibC           = 1;  // Current Fibonacci number & current batch size
  this.goingUp        = true;  // is batch size increasing?
}

// If we processed fewer rows in this interval than in the previous one,
// reverse the direction.
BatchManager.prototype.onTick = function(currentOpCount, priorOpCount) {
  if(currentOpCount < priorOpCount) {
    this.goingUp = (! this.goingUp);
  }

  if(this.goingUp) {
    this.batchSizeUp();
  } else {
    this.batchSizeDown();
  }
};

// Batch sizes go up and down following the Fibonacci Sequence 1,1,2,3,5,8,...
BatchManager.prototype.batchSizeUp = function() {
  this.fibA = this.fibB;
  this.fibB = this.fibC;
  this.fibC = (this.fibA + this.fibB);
};

BatchManager.prototype.batchSizeDown = function() {
  if(this.fibC > 1) {
    this.fibC = this.fibB;
    this.fibB = this.fibA;
    this.fibA = (this.fibC - this.fibB);
  }
};

BatchManager.prototype.getTargetSize = function() {
  return this.fibC;
};


//////////////////////////////  DbWriter         ///////////

function DbWriter(options, session, controller) {
  theController       = controller;
  this.options        = options;
  this.session        = session;
  this.batchManager   = new BatchManager(this);
  this.batch          = this.session.createBatch();
  this.currentOpCount = 0;
  this.priorOpCount   = 0;
  this.aborted        = 0;
  this.atomic         = false;
  this.txStatus       = null;

  theWriter = this;
}

/* In atomic mode, the batch is preceded by begin() and followed, if there 
   were no hard errors, by commit() -- and otherwise by rollback().
   We do run the whole load, so that the log file will show all rows that 
   cause errors and the user can fix them before the next attempt.
*/
DbWriter.prototype.beginAtomic = function() {
  this.atomic = true;
  this.session.currentTransaction().begin();
};

DbWriter.prototype.loadItem = function(record) {
  this._store(record);
  if(this.batch.getOperationCount() >= this.batchManager.getTargetSize()) {
    this.executeBatch();
  }
};

DbWriter.prototype._store = function(record) {
  // Here we create a closure over { record } for each row inserted

  // Assess errors:
  //    On temporary error, retry the row.   [DONE]
  //    On table full, abort the loader   [DONE]
  //    On cluster shutdown, abort the loader
  //    TODO: Also handle errors originating from the mysql adapter
  //          Base all error handling on SQL State
  function rowCallback(error) {
    theWriter.currentOpCount++;

    if(error) {
      if(error.ndb_error) {
        if(error.ndb_error.classification === "InsufficientSpace") {
          theWriter.abort(error);
          return;
        }
        else if(error.ndb_error.status === "TemporaryError") {
          this.batchManager.batchSizeDown();
          theWriter.loadItem(record);  // retry
          return;  // Do not set record.error or register as complete
          // TODO: Maintain a retry count & eventually give up
        } // else {
         // Cluster shutdown case
        // }
      }
      /* All other errors: */
      if(theWriter.atomic) {
        theWriter.session.currentTransaction().setRollbackOnly();
      }
    }
    record.error = error;
    theController.loaderRecordComplete(record);
  }

  if(this.options.replaceMode) {
    this.batch.save(record.class, record.row, rowCallback);
  } else {
    this.batch.persist(record.class, record.row, rowCallback);
  }
}

DbWriter.prototype.executeBatch = function() {
  this.batch.execute(function(err) {
    // This is the batch callback
    udebug.log("batch complete");
  });
};

DbWriter.prototype.dataSourceIsPaused = function() {
  udebug.log("dataSourceIsPaused");
  // The data source has paused, so we're not going to get any more rows,
  // so we'd better execute the current batch.
  this.executeBatch();
};

DbWriter.prototype.onTick = function() {
  this.batchManager.onTick(this.currentOpCount, this.priorOpCount);
  this.priorOpCount = this.currentOpCount;
  this.currentOpCount = 0;
};

DbWriter.prototype.abort = function(error) {
  if(! this.aborted) {
    this.aborted = 1;
    this.session.currentTransaction().setRollbackOnly();
    theController.loaderAborted(error);
  }
};

DbWriter.prototype.end = function() {
  udebug.log("end");
  this.executeBatch();
};

DbWriter.prototype.endAtomic = function() {
  var tx;
  if(this.atomic) {
    tx = this.session.currentTransaction();
    if(tx.getRollbackOnly()) {
      this.rollback(tx);
    } else {
      this.commit(tx);
    }
  } else {
    theController.loaderTransactionDidCommit();
  }
};

DbWriter.prototype.commit = function(tx) {
  tx.commit(function(error) {
    if(error) {
      theController.loaderAborted(error);
    } else {
      theController.loaderTransactionDidCommit();
    }
  });
};

DbWriter.prototype.rollback = function(tx) {
  tx.rollback(function() {
    theController.loaderTransactionDidRollback();
  });
};

exports.DbWriter = DbWriter;

