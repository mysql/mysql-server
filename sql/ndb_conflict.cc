/*
   Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <my_global.h> /* For config defines */

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
/* distcheck does not compile from here... */

#include "ha_ndbcluster_glue.h"
#include "ndb_conflict.h"

#ifdef HAVE_NDB_BINLOG

#define NDBTAB NdbDictionary::Table
#define NDBCOL NdbDictionary::Column

int
ExceptionsTableWriter::init(const NdbDictionary::Table* mainTable,
                            const NdbDictionary::Table* exceptionsTable,
                            char* msg_buf,
                            uint msg_buf_len,
                            const char** msg)
{
  const char* ex_tab_name = exceptionsTable->getName();
  const int fixed_cols= 4;
  bool ok=
    exceptionsTable->getNoOfColumns() >= fixed_cols &&
    exceptionsTable->getNoOfPrimaryKeys() == 4 &&
    /* server id */
    exceptionsTable->getColumn(0)->getType() == NDBCOL::Unsigned &&
    exceptionsTable->getColumn(0)->getPrimaryKey() &&
    /* master_server_id */
    exceptionsTable->getColumn(1)->getType() == NDBCOL::Unsigned &&
    exceptionsTable->getColumn(1)->getPrimaryKey() &&
    /* master_epoch */
    exceptionsTable->getColumn(2)->getType() == NDBCOL::Bigunsigned &&
    exceptionsTable->getColumn(2)->getPrimaryKey() &&
    /* count */
    exceptionsTable->getColumn(3)->getType() == NDBCOL::Unsigned &&
    exceptionsTable->getColumn(3)->getPrimaryKey();
  if (ok)
  {
    int ncol= mainTable->getNoOfColumns();
    int nkey= mainTable->getNoOfPrimaryKeys();
    int i, k;
    for (i= k= 0; i < ncol && k < nkey; i++)
    {
      const NdbDictionary::Column* col= mainTable->getColumn(i);
      if (col->getPrimaryKey())
      {
        const NdbDictionary::Column* ex_col=
          exceptionsTable->getColumn(fixed_cols + k);
        ok=
          ex_col != NULL &&
          col->getType() == ex_col->getType() &&
          col->getLength() == ex_col->getLength() &&
          col->getNullable() == ex_col->getNullable();
        if (!ok)
          break;
        /*
          Store mapping of Exception table key# to
          orig table attrid
        */
        m_key_attrids[k]= i;
        k++;
      }
    }
    if (ok)
    {
      m_ex_tab= exceptionsTable;
      m_pk_cols= nkey;
      return 0;
    }
    else
      my_snprintf(msg_buf, msg_buf_len,
                  "NDB Slave: exceptions table %s has wrong "
                  "definition (column %d)",
                  ex_tab_name, fixed_cols + k);
  }
  else
    my_snprintf(msg_buf, msg_buf_len,
                "NDB Slave: exceptions table %s has wrong "
                "definition (initial %d columns)",
                ex_tab_name, fixed_cols);

  *msg = msg_buf;
  return -1;
}

void
ExceptionsTableWriter::free(Ndb* ndb)
{
  if (m_ex_tab)
  {
    NdbDictionary::Dictionary* dict = ndb->getDictionary();
    dict->removeTableGlobal(*m_ex_tab, 0);
    m_ex_tab= 0;
  }
}

int
ExceptionsTableWriter::writeRow(NdbTransaction* trans,
                                const NdbRecord* keyRecord,
                                uint32 server_id,
                                uint32 master_server_id,
                                uint64 master_epoch,
                                const uchar* rowPtr,
                                NdbError& err)
{
  DBUG_ENTER("ExceptionsTableWriter::writeRow");
  assert(err.code == 0);
  do
  {
    /* Have exceptions table, add row to it */
    const NDBTAB *ex_tab= m_ex_tab;

    /* get insert op */
    NdbOperation *ex_op= trans->getNdbOperation(ex_tab);
    if (ex_op == NULL)
    {
      err= trans->getNdbError();
      break;
    }
    if (ex_op->insertTuple() == -1)
    {
      err= ex_op->getNdbError();
      break;
    }
    {
      uint32 count= (uint32)++m_count;
      if (ex_op->setValue((Uint32)0, (const char *)&(server_id)) ||
          ex_op->setValue((Uint32)1, (const char *)&(master_server_id)) ||
          ex_op->setValue((Uint32)2, (const char *)&(master_epoch)) ||
          ex_op->setValue((Uint32)3, (const char *)&(count)))
      {
        err= ex_op->getNdbError();
        break;
      }
    }
    /* copy primary keys */
    {
      const int fixed_cols= 4;
      int nkey= m_pk_cols;
      int k;
      for (k= 0; k < nkey; k++)
      {
        DBUG_ASSERT(rowPtr != NULL);
        const uchar* data=
          (const uchar*) NdbDictionary::getValuePtr(keyRecord,
                                                    (const char*) rowPtr,
                                                    m_key_attrids[k]);
        if (ex_op->setValue((Uint32)(fixed_cols + k), (const char*)data) == -1)
        {
          err= ex_op->getNdbError();
          break;
        }
      }
    }
  } while (0);

  if (err.code != 0)
  {
    if (err.classification == NdbError::SchemaError)
    {
      /* Something up with Exceptions table schema, forget it.
       * No further exceptions will be recorded.
       * TODO : Log this somehow
       */
      NdbDictionary::Dictionary* dict= trans->getNdb()->getDictionary();
      dict->removeTableGlobal(*m_ex_tab, false);
      m_ex_tab= NULL;
      DBUG_RETURN(0);
    }
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/* HAVE_NDB_BINLOG */
#endif

/**
   st_ndb_slave_state constructor

   Initialise Ndb Slave state object
*/
st_ndb_slave_state::st_ndb_slave_state()
  : current_master_server_epoch(0),
    current_max_rep_epoch(0),
    conflict_flags(0),
    retry_trans_count(0),
    current_trans_row_conflict_count(0),
    current_trans_row_reject_count(0),
    current_trans_in_conflict_count(0),
    max_rep_epoch(0),
    sql_run_id(~Uint32(0)),
    trans_row_conflict_count(0),
    trans_row_reject_count(0),
    trans_detect_iter_count(0),
    trans_in_conflict_count(0),
    trans_conflict_commit_count(0),
    trans_conflict_apply_state(SAS_NORMAL),
    trans_dependency_tracker(NULL)
{
  memset(current_violation_count, 0, sizeof(current_violation_count));
  memset(total_violation_count, 0, sizeof(total_violation_count));

  /* Init conflict handling state memroot */
  const size_t CONFLICT_MEMROOT_BLOCK_SIZE = 32768;
  init_alloc_root(&conflict_mem_root, CONFLICT_MEMROOT_BLOCK_SIZE, 0);
};

/**
   resetPerAttemptCounters

   Reset the per-epoch-transaction-application-attempt counters
*/
void
st_ndb_slave_state::resetPerAttemptCounters()
{
  memset(current_violation_count, 0, sizeof(current_violation_count));
  current_trans_row_conflict_count = 0;
  current_trans_row_reject_count = 0;
  current_trans_in_conflict_count = 0;

  conflict_flags = 0;
  current_max_rep_epoch = 0;
}

/**
   atTransactionAbort()

   Called by Slave SQL thread during transaction abort.
*/
void
st_ndb_slave_state::atTransactionAbort()
{
#ifdef HAVE_NDB_BINLOG
  /* Reset any gathered transaction dependency information */
  atEndTransConflictHandling();
  trans_conflict_apply_state = SAS_NORMAL;
#endif

  /* Reset current-transaction counters + state */
  resetPerAttemptCounters();
}



/**
   atTransactionCommit()

   Called by Slave SQL thread after transaction commit
*/
void
st_ndb_slave_state::atTransactionCommit()
{
  assert( ((trans_dependency_tracker == NULL) &&
           (trans_conflict_apply_state == SAS_NORMAL)) ||
          ((trans_dependency_tracker != NULL) &&
           (trans_conflict_apply_state == SAS_TRACK_TRANS_DEPENDENCIES)) );
  assert( trans_conflict_apply_state != SAS_APPLY_TRANS_DEPENDENCIES );

  /* Merge committed transaction counters into total state
   * Then reset current transaction counters
   */
  for (int i=0; i < CFT_NUMBER_OF_CFTS; i++)
  {
    total_violation_count[i]+= current_violation_count[i];
  }
  trans_row_conflict_count+= current_trans_row_conflict_count;
  trans_row_reject_count+= current_trans_row_reject_count;
  trans_in_conflict_count+= current_trans_in_conflict_count;

  if (current_trans_in_conflict_count)
    trans_conflict_commit_count++;

  if (current_max_rep_epoch > max_rep_epoch)
  {
    DBUG_PRINT("info", ("Max replicated epoch increases from %llu to %llu",
                        max_rep_epoch,
                        current_max_rep_epoch));
    max_rep_epoch = current_max_rep_epoch;
  }

  resetPerAttemptCounters();

  /* Clear per-epoch-transaction retry_trans_count */
  retry_trans_count = 0;
}

/**
   atApplyStatusWrite

   Called by Slave SQL thread when applying an event to the
   ndb_apply_status table
*/
void
st_ndb_slave_state::atApplyStatusWrite(Uint32 master_server_id,
                                       Uint32 row_server_id,
                                       Uint64 row_epoch,
                                       bool is_row_server_id_local)
{
  if (row_server_id == master_server_id)
  {
    /*
       WRITE_ROW to ndb_apply_status injected by MySQLD
       immediately upstream of us.
       Record epoch
    */
    current_master_server_epoch = row_epoch;
    assert(! is_row_server_id_local);
  }
  else if (is_row_server_id_local)
  {
    DBUG_PRINT("info", ("Recording application of local server %u epoch %llu "
                        " which is %s.",
                        row_server_id, row_epoch,
                        (row_epoch > current_max_rep_epoch)?
                        " new highest." : " older than previously applied"));
    if (row_epoch > current_max_rep_epoch)
    {
      /*
        Store new highest epoch in thdvar.  If we commit successfully
        then this can become the new global max
      */
      current_max_rep_epoch = row_epoch;
    }
  }
}

/**
   atResetSlave()

   Called when RESET SLAVE command issued - in context of command client.
*/
void
st_ndb_slave_state::atResetSlave()
{
  /* Reset the Maximum replicated epoch vars
   * on slave reset
   * No need to touch the sql_run_id as that
   * will increment if the slave is started
   * again.
   */
  resetPerAttemptCounters();

  retry_trans_count = 0;
  max_rep_epoch = 0;
}


/**
   atStartSlave()

   Called by Slave SQL thread when first applying a row to Ndb after
   a START SLAVE command.
*/
void
st_ndb_slave_state::atStartSlave()
{
#ifdef HAVE_NDB_BINLOG
  if (trans_conflict_apply_state != SAS_NORMAL)
  {
    /*
      Remove conflict handling state on a SQL thread
      restart
    */
    atEndTransConflictHandling();
    trans_conflict_apply_state = SAS_NORMAL;
  }
#endif
};

#ifdef HAVE_NDB_BINLOG

/**
   atEndTransConflictHandling

   Called when transactional conflict handling has completed.
*/
void
st_ndb_slave_state::atEndTransConflictHandling()
{
  DBUG_ENTER("atEndTransConflictHandling");
  /* Release any conflict handling state */
  if (trans_dependency_tracker)
  {
    current_trans_in_conflict_count =
      trans_dependency_tracker->get_conflict_count();
    trans_dependency_tracker = NULL;
    free_root(&conflict_mem_root, MY_MARK_BLOCKS_FREE);
  }
  DBUG_VOID_RETURN;
};

/**
   atBeginTransConflictHandling()

   Called by Slave SQL thread when it determines that Transactional
   Conflict handling is required
*/
void
st_ndb_slave_state::atBeginTransConflictHandling()
{
  DBUG_ENTER("atBeginTransConflictHandling");
  /*
     Allocate and initialise Transactional Conflict
     Resolution Handling Structures
  */
  assert(trans_dependency_tracker == NULL);
  trans_dependency_tracker = DependencyTracker::newDependencyTracker(&conflict_mem_root);
  DBUG_VOID_RETURN;
};

/**
   atPrepareConflictDetection

   Called by Slave SQL thread prior to defining an operation on
   a table with conflict detection defined.
*/
int
st_ndb_slave_state::atPrepareConflictDetection(const NdbDictionary::Table* table,
                                               const NdbRecord* key_rec,
                                               const uchar* row_data,
                                               Uint64 transaction_id,
                                               bool& handle_conflict_now)
{
  DBUG_ENTER("atPrepareConflictDetection");
  /*
    Slave is preparing to apply an operation with conflict detection.
    If we're performing Transactional Conflict Resolution, take
    extra steps
  */
  switch( trans_conflict_apply_state )
  {
  case SAS_NORMAL:
    DBUG_PRINT("info", ("SAS_NORMAL : No special handling"));
    /* No special handling */
    break;
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES : Tracking operation"));
    /*
      Track this operation and its transaction id, to determine
      inter-transaction dependencies by {table, primary key}
    */
    assert( trans_dependency_tracker );

    int res = trans_dependency_tracker
      ->track_operation(table,
                        key_rec,
                        row_data,
                        transaction_id);
    if (res != 0)
    {
      sql_print_error("%s", trans_dependency_tracker->get_error_text());
      DBUG_RETURN(res);
    }
    /* Proceed as normal */
    break;
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES : Deciding whether to apply"));
    /*
       Check if this operation's transaction id is marked in-conflict.
       If it is, we tell the caller to perform conflict resolution now instead
       of attempting to apply the operation.
    */
    assert( trans_dependency_tracker );

    if (trans_dependency_tracker->in_conflict(transaction_id))
    {
      DBUG_PRINT("info", ("Event for transaction %llu is conflicting.  Handling.",
                          transaction_id));
      current_trans_row_reject_count++;
      handle_conflict_now = true;
      DBUG_RETURN(0);
    }

    /*
       This transaction is not marked in-conflict, so continue with normal
       processing.
       Note that normal processing may subsequently detect a conflict which
       didn't exist at the time of the previous TRACK_DEPENDENCIES pass.
       In this case, we will rollback and repeat the TRACK_DEPENDENCIES
       stage.
    */
    DBUG_PRINT("info", ("Event for transaction %llu is OK, applying",
                        transaction_id));
    break;
  }
  }
  DBUG_RETURN(0);
}

/**
   atTransConflictDetected

   Called by the Slave SQL thread when a conflict is detected on
   an executed operation.
*/
int
st_ndb_slave_state::atTransConflictDetected(Uint64 transaction_id)
{
  DBUG_ENTER("atTransConflictDetected");

  /*
     The Slave has detected a conflict on an operation applied
     to a table with Transactional Conflict Resolution defined.
     Handle according to current state.
  */
  conflict_flags |= SCS_TRANS_CONFLICT_DETECTED_THIS_PASS;
  current_trans_row_conflict_count++;

  switch (trans_conflict_apply_state)
  {
  case SAS_NORMAL:
  {
    DBUG_PRINT("info", ("SAS_NORMAL : Conflict on op on table with trans detection."
                        "Requires multi-pass resolution.  Will transition to "
                        "SAS_TRACK_TRANS_DEPENDENCIES at Commit."));
    /*
      Conflict on table with transactional conflict resolution
      defined.
      This is the trigger that we will do transactional conflict
      resolution.
      Record that we need to do multiple passes to correctly
      perform resolution.
      TODO : Early exit from applying epoch?
    */
    break;
  }
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES : Operation in transaction %llu "
                        "had conflict",
                        transaction_id));
    /*
       Conflict on table with transactional conflict resolution
       defined.
       We will mark the operation's transaction_id as in-conflict,
       so that any other operations on the transaction are also
       considered in-conflict, and any dependent transactions are also
       considered in-conflict.
    */
    assert(trans_dependency_tracker != NULL);
    int res = trans_dependency_tracker
      ->mark_conflict(transaction_id);

    if (res != 0)
    {
      sql_print_error("%s", trans_dependency_tracker->get_error_text());
      DBUG_RETURN(res);
    }
    break;
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    /*
       This must be a new conflict, not noticed on the previous
       pass.
    */
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES : Conflict detected.  "
                        "Must be further conflict.  Will return to "
                        "SAS_TRACK_TRANS_DEPENDENCIES state at commit."));
    // TODO : Early exit from applying epoch
    break;
  }
  default:
    break;
  }

  DBUG_RETURN(0);
}

/**
   atConflictPreCommit

   Called by the Slave SQL thread prior to committing a Slave transaction.
   This method can request that the Slave transaction is retried.


   State transitions :

                       START SLAVE /
                       RESET SLAVE /
                        STARTUP
                            |
                            |
                            v
                    ****************
                    *  SAS_NORMAL  *
                    ****************
                       ^       |
    No transactional   |       | Conflict on transactional table
       conflicts       |       | (Rollback)
       (Commit)        |       |
                       |       v
            **********************************
            *  SAS_TRACK_TRANS_DEPENDENCIES  *
            **********************************
               ^          I              ^
     More      I          I Dependencies |
    conflicts  I          I determined   | No new conflicts
     found     I          I (Rollback)   | (Commit)
    (Rollback) I          I              |
               I          v              |
           **********************************
           *  SAS_APPLY_TRANS_DEPENDENCIES  *
           **********************************


   Operation
     The initial state is SAS_NORMAL.

     On detecting a conflict on a transactional conflict detetecing table,
     SAS_TRACK_TRANS_DEPENDENCIES is entered, and the epoch transaction is
     rolled back and reapplied.

     In SAS_TRACK_TRANS_DEPENDENCIES state, transaction dependencies and
     conflicts are tracked as the epoch transaction is applied.

     Then the Slave transitions to SAS_APPLY_TRANS_DEPENDENCIES state, and
     the epoch transaction is rolled back and reapplied.

     In the SAS_APPLY_TRANS_DEPENDENCIES state, operations for transactions
     marked as in-conflict are not applied.

     If this results in no new conflicts, the epoch transaction is committed,
     and the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered for processing
     the next replicated epch transaction.
     If it results in new conflicts, the epoch transactions is rolled back, and
     the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered again, to determine
     the new set of dependencies.

     If no conflicts are found in the SAS_TRACK_TRANS_DEPENDENCIES state, then
     the epoch transaction is committed, and the Slave transitions to SAS_NORMAL
     state.


   Properties
     1) Normally, there is no transaction dependency tracking overhead paid by
        the slave.

     2) On first detecting a transactional conflict, the epoch transaction must be
        applied at least three times, with two rollbacks.

     3) Transactional conflicts detected in subsequent epochs require the epoch
        transaction to be applied two times, with one rollback.

     4) A loop between states SAS_TRACK_TRANS_DEPENDENCIES and SAS_APPLY_TRANS_
        DEPENDENCIES occurs when further transactional conflicts are discovered
        in SAS_APPLY_TRANS_DEPENDENCIES state.  This implies that the  conflicts
        discovered in the SAS_TRACK_TRANS_DEPENDENCIES state must not be complete,
        so we revisit that state to get a more complete picture.

     5) The number of iterations of this loop is fixed to a hard coded limit, after
        which the Slave will stop with an error.  This should be an unlikely
        occurrence, as it requires not just n conflicts, but at least 1 new conflict
        appearing between the transactions in the epoch transaction and the
        database between the two states, n times in a row.

     6) Where conflicts are occasional, as expected, the post-commit transition to
        SAS_TRACK_TRANS_DEPENDENCIES rather than SAS_NORMAL results in one epoch
        transaction having its transaction dependencies needlessly tracked.

*/
int
st_ndb_slave_state::atConflictPreCommit(bool& retry_slave_trans)
{
  DBUG_ENTER("atConflictPreCommit");

  /*
    Prior to committing a Slave transaction, we check whether
    Transactional conflicts have been detected which require
    us to retry the slave transaction
  */
  retry_slave_trans = false;
  switch(trans_conflict_apply_state)
  {
  case SAS_NORMAL:
  {
    DBUG_PRINT("info", ("SAS_NORMAL"));
    /*
       Normal case.  Only if we defined conflict detection on a table
       with transactional conflict detection, and saw conflicts (on any table)
       do we go to another state
     */
    if (conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS)
    {
      DBUG_PRINT("info", ("Conflict(s) detected this pass, transitioning to "
                          "SAS_TRACK_TRANS_DEPENDENCIES."));
      assert(conflict_flags & SCS_OPS_DEFINED);
      /* Transactional conflict resolution required, switch state */
      atBeginTransConflictHandling();
      resetPerAttemptCounters();
      trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;
      retry_slave_trans = true;
    }
    break;
  }
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES"));

    if (conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS)
    {
      /*
         Conflict on table with transactional detection
         this pass, we have collected the details and
         dependencies, now transition to
         SAS_APPLY_TRANS_DEPENDENCIES and
         reapply the epoch transaction without the
         conflicting transactions.
      */
      assert(conflict_flags & SCS_OPS_DEFINED);
      DBUG_PRINT("info", ("Transactional conflicts, transitioning to "
                          "SAS_APPLY_TRANS_DEPENDENCIES"));

      trans_conflict_apply_state = SAS_APPLY_TRANS_DEPENDENCIES;
      trans_detect_iter_count++;
      retry_slave_trans = true;
      break;
    }
    else
    {
      /*
         No transactional conflicts detected this pass, lets
         return to SAS_NORMAL state after commit for more efficient
         application of epoch transactions
      */
      DBUG_PRINT("info", ("No transactional conflicts, transitioning to "
                          "SAS_NORMAL"));
      atEndTransConflictHandling();
      trans_conflict_apply_state = SAS_NORMAL;
      break;
    }
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES"));
    assert(conflict_flags & SCS_OPS_DEFINED);
    /*
       We've applied the Slave epoch transaction subject to the
       conflict detection.  If any further transactional
       conflicts have been observed, then we must repeat the
       process.
    */
    atEndTransConflictHandling();
    atBeginTransConflictHandling();
    trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;

    if (unlikely(conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS))
    {
      DBUG_PRINT("info", ("Further conflict(s) detected, repeating the "
                          "TRACK_TRANS_DEPENDENCIES pass"));
      /*
         Further conflict observed when applying, need
         to re-determine dependencies
      */
      resetPerAttemptCounters();
      retry_slave_trans = true;
      break;
    }


    DBUG_PRINT("info", ("No further conflicts detected, committing and "
                        "returning to SAS_TRACK_TRANS_DEPENDENCIES state"));
    /*
       With dependencies taken into account, no further
       conflicts detected, can now proceed to commit
    */
    break;
  }
  }

  /*
    Clear conflict flags, to ensure that we detect any new conflicts
  */
  conflict_flags = 0;

  if (retry_slave_trans)
  {
    DBUG_PRINT("info", ("Requesting transaction restart"));
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("Allowing commit to proceed"));
  DBUG_RETURN(0);
}

/* HAVE_NDB_BINLOG */
#endif

/* WITH_NDBCLUSTER_STORAGE_ENGINE */
#endif
