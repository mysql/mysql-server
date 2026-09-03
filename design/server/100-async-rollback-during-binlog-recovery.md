# Roll Back Prepared Transactions Asynchronously During Binlog Recovery

Primary GitHub Issue: 100
Design and implementation PR: 711

## Description

Binlog recovery resolves internal transactions that were prepared in
a storage engine before the server stopped. If the binlog does not contain
the transaction's commit decision (`Xid_log_event`), the transaction must be
rolled back. Until now, the server has invoked `rollback_by_xid()` in the startup
thread and waited for the rollback to complete. Undoing a large transaction
row by row can therefore keep the server unavailable for hours.

This change adds an optional recovery-specific storage-engine callback to
perform a fast rollback handoff. InnoDB uses it to durably change the state of
a prepared DML transaction back to active. The existing InnoDB recovery
rollback thread then does the expensive row-by-row undo work in the background
while server startup continues. The final transaction outcome does not change.

## Functional Requirements

FR1. The time binlog recovery spends rolling back a recovered prepared DML
transaction SHOULD be independent of the transaction's size. Rolling back a
large transaction SHOULD delay startup by approximately the same amount as
rolling back a small transaction.

## Non-Functional Requirements

NFR1. Binlog recovery MUST preserve the transaction decision: an internal
prepared transaction absent from the recovered binlog commit set must
eventually be rolled back.

## High Level Architecture

### Asynchronous rollback model

During binlog recovery, an internal transaction that is prepared in InnoDB but
has no commit decision in the binlog must be rolled back. A prepared
transaction cannot be handled by the existing InnoDB recovery rollback thread,
so synchronous recovery performs the complete rollback in the startup thread.

The key idea is to change the recovered transaction from the prepared state
back to the active state after binlog recovery makes the rollback decision.
The existing InnoDB recovery rollback thread already rolls back recovered
active transactions. It can therefore perform the expensive row-by-row undo in
the background, allowing startup to continue after the fast state transition.

```text
PREPARED
   |
   | binlog recovery decides ROLLBACK
   v
ACTIVE (persisted)
   |
   | InnoDB recovery rollback thread
   v
ROLLED BACK
```

This changes where and when the undo work runs, but does not change the
transaction coordinator's rollback decision or the final transaction outcome.

### Persisting the active state

Making this transition only in memory is not crash-safe. If the server stops
again after binlog recovery advances, the next startup could find the
transaction prepared in InnoDB, but the binlog information needed to resolve
it would no longer be available.

The prepared-to-active transition must therefore be persisted before binlog
recovery considers the rollback handoff complete. InnoDB records the active
undo-log state in redo and flushes that redo before changing the in-memory
transaction state. If the server crashes before the state has been persisted,
binlog recovery can retry the rollback decision. If the server crashes after
the state has been persisted, the transaction is recovered as active and
the background rollback thread continues the rollback.

### Storage-engine integration

The server introduces an optional recovery-specific rollback callback to the
storage-engine interface. Transaction-coordinator recovery invokes it after
deciding that an internal prepared transaction must be rolled back. InnoDB
implements the callback by performing the crash-safe prepared-to-active
transition described above and returning without doing row-by-row undo.

An engine without this callback continues to use the existing synchronous
`rollback_by_xid()` path. InnoDB also keeps DDL transaction rollback synchronous
because later DDL recovery depends on its dictionary and physical-file effects
being settled before startup continues.

## Low Level Design

### Storage-engine interface

`handlerton` gains the following optional callback type and member:

```cpp
typedef xa_status_code (*recover_rollback_by_xid_t)(handlerton *hton,
                                                    XID *xid);

recover_rollback_by_xid_t recover_rollback_by_xid;
```
The callback has the same parameters and return values as the
`rollback_by_xid` callback.

The server calls this callback only during transaction-coordinator recovery,
after deciding that an internal prepared XID must be rolled back. A storage
engine may complete the rollback in the callback or make the rollback durable
and delegate its execution to engine recovery. If the callback is null,
the server falls back to rollback_by_xid().

The callback returns the existing `xa_status_code`. InnoDB returns `XAER_NOTA`
when it cannot find the XID and `XA_OK` after a successful handoff. Errors are
handled by the existing XA recovery error path.

### InnoDB callback

`innobase_recover_rollback_by_xid()` performs these steps:

1. Transactions that performed DDL continue to be rolled back synchronously.
2. For DML transactions, it changes each existing undo log from prepared to
   active.
3. It flushes the redo log records for these changes to disk.
4. It adds every table recorded in the transaction's `mod_tables` set to the
   recovery rollback thread's MDL acquisition list. The rollback thread
   acquires shared metadata locks on these tables before server startup
   continues, preventing concurrent DDL while undo is running.
5. Under `trx_sys_mutex`, it changes the in-memory transaction state from
   `TRX_STATE_PREPARED` to `TRX_STATE_ACTIVE` and decrements
   `trx_sys->n_prepared_trx`.

The existing recovery rollback thread finds the active recovered transaction
and performs normal undo. No new rollback algorithm or worker pool is added.

### Recovery-thread startup ordering

The recovery rollback thread is created by
`srv_start_threads_after_ddl_recovery()`, which is called from InnoDB's
`post_recover` callback. The server invokes storage-engine `post_recover`
callbacks only after `tc_log->open()` has completed transaction-coordinator and
binlog recovery. Therefore every prepared DML transaction selected for
asynchronous rollback has already been changed to active before the recovery
rollback thread starts its first scan.

### Forced-recovery behavior

With `innodb_force_recovery` at
`SRV_FORCE_NO_BACKGROUND` (level 2) or higher, the callback still durably
changes an internal prepared DML transaction to active during TC recovery.
However, `innobase_post_recover()` returns without creating the recovery
rollback thread, so the transaction is not rolled back during that server
run. A later restart with `innodb_force_recovery` below level 2 recovers the
active transaction and completes its rollback in the background. This matches
the existing behavior of transactions that were already active at the time of
the original crash.

The adjacent `srv_read_only_mode` early-return branch does not create another
supported deferred-rollback case: InnoDB rejects read-only startup when crash
recovery is required.

### Persisted data and compatibility

The design reuses existing undo-log states and redo operations. It adds no data
dictionary objects, file-format fields, binlog events, or redo record types.
Existing databases need no upgrade step, and older servers do not encounter a
new on-disk representation.

There is no change to external XA recovery: the new callback is selected for
internal transactions by the existing binlog recovery decision path. There is
also no change to security checks, SQL interfaces, replication protocols, or
user-visible configuration. The interaction with the existing
`innodb_force_recovery` setting is described above.

The asynchronous handoff changes only when InnoDB performs the rollback; it
does not change the transaction coordinator's decision or the recovered binlog
contents and position. No GTID is added to or removed from the recovered GTID
set, and point-in-time recovery replays the same binlog events. GTID recovery,
recovery to the selected binlog position, and point-in-time recovery therefore
preserve the same final committed transaction outcome as synchronous rollback.

Asynchronous replication and Group Replication are not explicitly gated on
completion of the InnoDB recovery rollback thread, so their receiver and
applier threads may start while rollback is still running. The recovered
transaction retains its InnoDB row and table locks, as well as shared metadata
locks for modified tables, until rollback releases them. An asynchronous
replication or Group Replication applier transaction that conflicts with those
locks follows normal InnoDB lock-wait handling and resumes after rollback
releases the locks; nonconflicting transactions can continue. Consequently,
rollback can temporarily increase replication lag, but it does not change
which recovered transaction is committed or rolled back.

### Performance and resource use

The startup thread still pays for the undo-state mini-transaction
and a synchronous redo flush for each handed-off DML transaction. It no longer
waits for row-by-row undo. The single recovery rollback thread is unchanged,
so rollback execution and resource use keep their current characteristics.


### Observability and diagnostics

No new status variable or Performance Schema instrument is introduced. While
undo is in progress, the recovered transaction remains visible in
`INFORMATION_SCHEMA.INNODB_TRX` with the state `ROLLING BACK`. Existing
InnoDB rollback progress messages and XA recovery error reporting remain in
use.

### Affected source areas

- `sql/handler.h`: optional `handlerton` callback.
- `sql/xa/recovery.cc`: callback selection for rollback decisions.
- `storage/innobase/handler/ha_innodb.cc`: InnoDB registration and durable
  handoff implementation.
- `storage/innobase/trx/trx0roll.cc`: recovered-transaction handling and test
  synchronization.
- `storage/innobase/trx/trx0trx.cc`: scheduling tables for metadata-lock
  acquisition before background rollback.

## Alternatives Considered

Changing `rollback_by_xid()` to return before rollback completes was not
selected because that callback is also used outside crash recovery and its
callers expect synchronous rollback semantics. A recovery-specific callback
keeps the existing contract unchanged and limits asynchronous handoff to the
transaction-coordinator recovery path, where the required startup ordering and
crash-safety guarantees are known.

## Testing

The debug-only MTR test
`binlog.binlog_recover_async_rollback_trx` exercises the following scenarios:

1. **Prepared DML rollback and DDL exclusion (FR1, NFR1):** crash after InnoDB
   flushes the prepared record but before the XID reaches the binlog. Pause the
   background rollback after restart and verify that a concurrent `DROP TABLE`
   waits for metadata lock. Crash again before the DDL can execute, restart
   normally, and verify that the uncommitted row disappears. Reusing the
   affected key verifies that no transaction or lock remains.
2. **Crash after a durable handoff (NFR1):** create a prepared DML transaction
   whose XID is absent from the binlog, then restart and pause its background
   rollback after binlog recovery has persisted the prepared-to-active
   transition. Verify through `INNODB_TRX` that it is `ROLLING BACK`, crash
   again, and verify that a normal restart recovers it as active and completes
   the rollback.
3. **Crash before the handoff begins (NFR1):** use the
   `crash_before_recover_rollback_undo_state_change` injection point to crash
   before either undo log is changed. The next restart must recover the
   transaction as prepared, retry the rollback decision, roll back the update,
   and release the transaction's row lock.
4. **Synchronous DDL fallback:** crash after an atomic `CREATE TABLE` is
   prepared in InnoDB but before its XID reaches the binlog. Pause background
   recovery rollback before it applies undo and verify that binlog recovery
   has already rolled back the DDL synchronously, leaving no recovered
   transaction or table. Then verify that the same table name can be created
   again.

Existing XA and binlog recovery tests continue to cover the null-callback
fallback and synchronous recovery behavior, and existing InnoDB DDL recovery
tests still validate that DDL is settled during startup.
The test uses debug synchronization points and is excluded from Valgrind and
crash-reporter runs.

## References

- Primary GitHub Issue: #100
- Design and implementation PR: #711
- [MySQL Bug #114053](https://bugs.mysql.com/bug.php?id=114053)
- [MariaDB: Rollback Prepared Transactions Asynchronously During Binlog Crash Recovery](https://mariadb.com/resources/blog/rollback-prepared-transactions-asynchronously-during-binlog-crash-recovery/)
