# Committing a large transaction through `ordered_commit()`

This document is temporarily a separate file, and will be merged into `low-level-design.md`, where it replaces "Section 2, Commit code path". The fallback subsections of that section stay.

## 1. Idea

A BOLT transaction is committed through the standard binary log group commit (`ordered_commit()`), and reuses almost all of it. The BOLT transaction goes through the same stages as any transaction (flush, sync, commit), together with the other transactions of its commit group. The only requirement is that the BOLT transaction must be the leader of its group.

Only one step differs, in the flush stage. A standard transaction copies its cache into the active binary log file. A BOLT transaction instead rotates the binary log, and its spilled temp file becomes the new binary log file. The rotation creates the new file as usual, and before the new file is published, the temp file takes its place, with the new file's header events copied in front of the transaction. The other members of the group are then written into the promoted file, after the BOLT transaction, and the whole group proceeds to the sync and commit stages as in any group commit.

Everything else comes from the standard code, including commit order on replicas, the semi-sync and Group Replication hooks, the rotation itself (the crash safety of the new file, the index update and the GTID persistence), and the `max_binlog_size` rotation after the commit.

Two terms are used below. The **active file** is the binary log file before the rotation. The **new file** is the next file in the binary log sequence, which the rotation creates and the promoted temp file replaces.

## 2. `ordered_commit()` step by step

A BOLT transaction runs every step of `ordered_commit()`. The "Difference" column says whether a step is identical for a BOLT transaction, or how the step differs. *New* means that BOLT adds work to the step, and *replaced* means that BOLT does different work in its place. *Identical* means that the same code runs with the same effect.

| # | Step | Standard transaction | BOLT transaction (spilled file promoted) | Difference |
|---|---|---|---|---|
| 0a | `MYSQL_BIN_LOG::commit()`, before `ordered_commit()` | finalizes its caches, which writes the end event and records where the terminating event is (its offset, type and body, which only BOLT uses), then calls `ordered_commit()` | finalizes its caches as a standard transaction does. Then `get_cache_for_large_trx_commit()` checks that it is a candidate and that no commit-time condition blocks it, `flush_and_sync_spilled_file()` makes the complete spilled file durable outside `LOCK_log`, `request_spilled_file_promotion()` marks its transaction cache for promotion, and `set_must_lead_flush_group(true)` makes it the leader of its flush group. Then it calls `ordered_commit()` | new, the candidate decision, the sync and the two marks |
| 0b | `assign_ticket()`, `init_thd_variables()` | takes its binlog group commit ticket and resets its commit state | identical | none |
| 0c | Stage #0 (commit order), `Commit_order_manager::wait()` | on a replica with `replica_preserve_commit_order`, waits for its turn | identical | none |
| 1a | Stage #1 (flush), enrollment, `change_stage(BINLOG_FLUSH_STAGE)` → `append_to()` | waits for its GR ticket turn, then appends to the flush queue. The session that finds the queue empty is the leader and waits for `LOCK_log`, and the others wait for the leader | waits for its GR ticket turn. If the flush queue is empty, it appends to the queue and is the leader. Otherwise it waits in the leader-candidate list until the leader that fetches the current queue appends it to the emptied queue. Either way it is the leader of its group and waits for `LOCK_log` | replaced, it is always the leader of its group |
| 1b | Binary log closed? | skips the flush and sync stages and goes to the commit stage | not reached, because with the binary log closed, `MYSQL_BIN_LOG::commit()` does not request the promotion, and records the fallback with reason `kLogClosed` | none in this step |
| 1c | The leader fetches the queue, `fetch_and_process_flush_stage_queue()` | takes every session enrolled so far as its group, which closes the group. The flush queue is then empty again, so a session that arrives now enrolls into a new queue and becomes the leader of the next group, which flushes once this group leaves the flush stage. `ha_flush_logs()` makes the engines' prepare records durable, and the commit-order waiters are released | the same three actions. In addition, still holding the flush queue lock, the leader appends the first waiting BOLT candidate, if any, to the emptied queue as the next group's leader, and wakes it | new, the leader handoff, which is a check for every group and a handoff only when a BOLT candidate waits |
| 1d | `assign_automatic_gtids_to_flush_group()` | assigns the group's automatic GTIDs | identical | none |
| 1e | Write each member, `flush_thread_caches()` → `binlog_cache_data::flush()` | generates its logical timestamps, writes its Gtid event and copies its cache into the active file | only for a transaction that `MYSQL_BIN_LOG::commit()` marked for promotion and that leads its flush group, so it is the first member written. Other transactions are copied as usual. The BOLT transaction calls `rotate_and_promote_spilled_file()`, which first checks, before rotating, that the binary log is open, that encryption is OFF and that the checksum algorithm matches. If one check fails, there is no rotation and the transaction is copied into the active file as usual. Otherwise the standard rotation `new_file_impl()` runs. It waits for prepared XIDs, persists GTIDs, writes the Rotate event into the active file and closes it, and `open_binlog()` writes the new file's header and calls `promote_spilled_file()`, which turns the spilled file into the new active binary log file. Nothing is copied. The cache is reset, keeping the file | replaced, a rotation with the spilled file instead of the copy |
| 1f | After each member, `set_trans_pos()`, `inc_prep_xids()` | records its position (its end in the active file) and counts it as prepared in that file | the same two actions. The position is the end of the new file, and the transaction counts as prepared in the new file | none, applied to the new file |
| 1g | Other members of the group | written into the active file | written into the new active binary log file, after the BOLT transaction | none |
| 1h | `set_max_size_exceeded()` | marks the group when the active file is over `max_binlog_size` | identical, on the new file, which is usually over the limit | none |
| 1i | `flush_cache_to_file()`, `after_flush` hook, `update_binlog_end_pos()` | flushes the binary log IO_CACHE. Semi-sync registers the position, and dump threads may read up to it | the same three actions, on the new active binary log file. Dump threads can read the BOLT transaction only from this step, because when the rotation made the promoted file the active file, it let dump threads read up to the end of the file's header events and no further | none in this step, because the rotation limits what dump threads can read |
| 1j | Flush error, `handle_binlog_flush_or_sync_error()` | applies `binlog_error_action` | identical. A failure after the rename reaches this step as a rotation error | none |
| 2 | Stage #2 (sync), sync delay, `sync_binlog_file()`, end position when `sync_binlog=1` | fsyncs the active file | identical, on the new active binary log file. The BOLT transaction's part is already durable, because its body was synced in `MYSQL_BIN_LOG::commit()`, and its header block was synced while the temp file was promoted into a binary log file, before the rename | none |
| 3 | Stage #3 (commit), `call_after_sync_hook()`, `process_commit_stage_queue()` | runs the `after_sync` hook, commits in the engines in order and adds the GTIDs to `gtid_executed` | identical | none |
| 4 | After-commit stage, `process_after_commit_stage_queue()` | runs the `after_commit` hook | identical | none |
| 5 | `signal_done()`, `finish_commit()` | wakes the followers and cleans up the session | identical | none |
| 6 | Size rotation, `rotate()` | when the group exceeded `max_binlog_size`, rotates to the next file | identical. If the new active binary log file is over `max_binlog_size`, the binary log rotates to the next file, after the whole group has committed | none |

If the BOLT transaction falls back when it is written, it is written as a standard transaction, into the active file, or into the new file that the rotation created when the rotation already ran. Section 5 lists these cases.

The code of `ordered_commit()` itself is not changed, apart from debug points for tests. The BOLT logic sits in three places around it. Before `ordered_commit()`, `MYSQL_BIN_LOG::commit()` decides on the promotion and marks the transaction. At flush stage enrollment, the transaction is made the leader of its flush group. In the flush stage, the copy of the group's first member is replaced by a rotation, and the rotation reuses the standard `new_file_impl()` and `open_binlog()` with a few additions for the promotion. All other steps run the standard code unchanged.

## 3. Rotation in the flush stage, led by the BOLT transaction

### 3.1 Why the rotation runs in the flush stage

Transactions get their place in the binary log in the flush stage, where the leader writes them one after another into the active binary log file, under `LOCK_log`. In the flush stage, the BOLT transaction's spilled file gets its binary log file number (the next one in the sequence), and the transaction gets its GTID and its logical timestamps. So the rotation must run in the flush stage, before the other members of the group are written, and those members then get their places after the BOLT transaction, in the new file. If the rotation ran in a later stage, `LOCK_log` would already belong to the next group, which would be writing into the active file.

Two parts of the rotation work differently for a BOLT transaction.

- The logical clock (the dependency tracker) restarts with the new active binary log file before the BOLT transaction's timestamps are generated, so the BOLT transaction is a parallelization barrier for the replica's parallel applier.
- The rotation saves the previous file's GTIDs to `mysql.gtid_executed` in a dedicated session, not the BOLT transaction's session, because the rotation runs inside the BOLT transaction's commit, while that transaction is still open.

### 3.2 Why the BOLT transaction must lead its commit group

A rotation waits until every transaction prepared in the active file has committed in the storage engines. Crash recovery scans only the last binary log file, so a transaction still prepared in the old file after the switch would be rolled back after a crash.

Suppose the BOLT transaction were not first in its group. The members written before it are prepared in the active file, and commit only in the group's commit stage, which comes after the flush stage. The rotation during the flush stage would wait for those members to commit, and the members cannot commit until the flush stage, and the rotation in it, completes. That is a deadlock, so the BOLT transaction must be written first in its group, as its leader.

- The transactions in the group cannot be reordered to move the BOLT transaction first, because on a replica with `replica_preserve_commit_order` the order of the queue is the commit order.
- Earlier groups are not a problem, although their transactions may be prepared in the active file but not yet committed. Those groups are in their commit stage, which needs `LOCK_commit`, not `LOCK_log`, so those groups can finish their commit without needing anything from the BOLT transaction's group.
- Members that join after the BOLT transaction are written into the new active binary log file, after the rotation, so the rotation never waits for them.

### 3.3 How the BOLT transaction becomes the leader

A BOLT transaction enrolls in the flush stage as the leader if the flush queue is empty. Otherwise the transaction waits in a first-in, first-out candidate list. The group commit leader that fetches the flush queue appends the first transaction from the candidate list to the emptied flush queue while still holding the flush queue lock, which makes that candidate the leader of the next group, and wakes it. No other session can enroll between the fetch and the leader assignment. Transactions that arrive after the candidate join the group behind the BOLT transaction.

- A BOLT transaction with k candidates ahead of it in the candidate list becomes a leader at the (k+1)th fetch after it starts waiting, regardless of how many standard sessions arrive.
- Waiting in the candidate list cannot deadlock with replica commit order or with Group Replication tickets. The BOLT transaction waits for its commit-order turn and its ticket turn before it joins the candidate list, so it waits only for the current group to be fetched, which does not depend on any later transaction. Transactions after it in replica commit order wait until it has been made the leader and enrolled in the flush stage.
- The flush loop allows the promotion only while it writes the group's first member. A transaction marked for promotion that is not first, which is not expected, is copied as usual and counted as `kPromotionFailed`.

### 3.4 How the spilled file is promoted in the flush stage

The promotion is one extra step inside a standard binary log rotation. The rotation closes the active file and creates the new file as usual. The promotion then replaces the new file with the temp file, and the rotation finishes as usual by adding the new file to the binary log index.

1. The rotation waits until every transaction prepared in the active file has committed in the storage engines, saves the active file's GTIDs to `mysql.gtid_executed` in a dedicated session, writes the Rotate event into the active file, and closes the active file.
2. `open_binlog()` records the new file's name in the purge index, creates the new file and writes its header events (the Format_description and Previous_gtids events), as for any rotation.
3. `promote_spilled_file()` checks again that encryption is OFF and that the checksum algorithm matches, checks that the new file's header events fit in the reserved region at the front of the temp file, and reads them back.
4. The promotion writes a copy of the header events into the reserved region, followed by the Large_transaction_header and Gtid events, and syncs the temp file.
5. The promotion renames the temp file to replace the new file, and syncs the binary log directory and the temp files directory, so the rename is durable.
6. The promotion reopens the server's handle of the active binary log file on the renamed file, at its end.
7. `open_binlog()` finishes as for any rotation. It adds the new file to the binary log index, removes it from the purge index, and lets dump threads read up to the end of the header events. Only the header end is published here, although the file already holds the whole transaction, so that dump threads cannot send the transaction before the `after_flush` hook, or, with `sync_binlog=1`, before the sync stage. The transaction is published later at the standard step, as for any transaction.

The steps run inside the standard rotation code, for the leader of a flush group whose cache is marked for promotion. The call chain below shows which function runs each step.

```
MYSQL_BIN_LOG::ordered_commit()
└─ Binlog_tc_log::process_flush_stage_queue()        # flush stage, LOCK_log held
   └─ MYSQL_BIN_LOG::flush_thread_caches()           # once per group member
      └─ binlog_cache_mngr::flush()
         └─ binlog_cache_data::flush()               # transaction cache, group leader only
            └─ MYSQL_BIN_LOG::rotate_and_promote_spilled_file()
               │  # before rotating: binary log open, encryption OFF,
               │  # checksum matches; otherwise no rotation, fall back
               └─ MYSQL_BIN_LOG::new_file_impl()     # runs step 1
                  └─ MYSQL_BIN_LOG::open_binlog()    # runs step 2, the call below, then step 7
                     └─ MYSQL_BIN_LOG::promote_spilled_file()             # runs steps 3 to 5
                        └─ MYSQL_BIN_LOG::reopen_promoted_binlog_file()   # runs step 6
```

Other callers of `open_binlog()`, such as server startup, the size rotation and FLUSH LOGS, do not promote a file.

The header events are always written by the standard rotation code, and the promotion only copies them. So a promoted file starts with exactly the same header as any other binary log file, and a change to the file header applies to promoted files without BOLT code. The cost is creating one small file and reading back its header, a few kilobytes for each BOLT transaction.

If a step from 3 to 5 fails before the rename is complete, nothing has been replaced. The rotation then finishes with the new file it created in step 2, which holds only its header events, and the BOLT transaction is copied into that file as a standard transaction. A failure after the rename, in the directory sync, the reopen or the index update, is a rotation error, as section 5 describes.

When the rotation starts, the BOLT transaction is prepared in the storage engines, and its temp file is complete and synced. The table lists what a crash leaves on disk at each point, and what happens at restart. Each row is covered by `binlog.binlog_bolt_2pc_recovery`.

| Crash | State on disk | At restart |
|---|---|---|
| Before step 1 | The active file is the last binary log file and holds no event of the BOLT transaction. | Recovery finds no XID for the BOLT transaction, so the storage engine rolls it back. The startup cleanup of the temp files directory removes the temp file. |
| In step 1, after the GTIDs are saved | `mysql.gtid_executed` already holds the active file's GTIDs. | The same as before step 1. Saving these GTIDs twice is harmless, because the executed GTID set is rebuilt from the table and the binary log together. |
| In step 1, after the Rotate event is written and the active file is closed | The active file ends with a Rotate event to a file that is not in the binary log index. | The same as before step 1, as for any rotation interrupted at this point. |
| In steps 2 to 4 | The new file exists and holds only its header events. Its name is in the purge index, and not in the binary log index. The temp file may already hold its header block. | Startup removes the new file, because its name is in the purge index, and the temp files cleanup removes the temp file. Recovery scans the active file, and the BOLT transaction is rolled back. |
| In steps 5 and 6 | The temp file has replaced the new file, or, if the rename was not durable yet, the new file or the temp file is still where it was. Either way, the new file's name is only in the purge index. | Startup removes the file at the new file's name through the purge index, and the temp files cleanup removes a temp file that was not renamed. The BOLT transaction is rolled back. |
| After step 7 | The new file is in the binary log index and holds the whole BOLT transaction, even if its name is still in the purge index. | The new file is the last binary log file. Recovery skips the transaction body using the Large_transaction_header event, finds the XID, and commits the BOLT transaction. A transaction ended by XA PREPARE stays prepared. |
| After the engine commit, or around the size rotation after the commit stage | The BOLT transaction is committed in the storage engines and in the binary log. | If the new file is still the last binary log file, recovery scans it as after step 7, skipping the transaction body using the Large_transaction_header event, and finds the BOLT transaction already committed. An interrupted size rotation is handled as for any rotation. |

## 4. Effect on standard transactions

BOLT transactions go through the same three stages of `ordered_commit()` as standard transactions. This section lists what a BOLT transaction costs them.

### 4.1 Cost of joining a BOLT-led group

A standard transaction that arrives while a BOLT transaction leads the next group joins that group, behind the BOLT transaction, as it would join any group, so it does not starve even when BOLT transactions keep arriving.

The cost for the standard transactions in a BOLT-led group is the time the rotation adds to the group's flush stage, which they wait for. The rotation adds time in two ways. First, it does the work of any rotation, plus a few small file operations for the promotion. That work does not depend on the BOLT transaction's size, because the temp file was synced before the transaction entered `ordered_commit()`. Second, the rotation waits until the transactions of the previous group have committed. A standard group's flush stage can run while the previous group is still in its commit stage, but a BOLT-led group's flush stage has to wait for that commit stage to finish.

### 4.2 Cost after the flush stage

After the flush stage, a BOLT-led group costs its standard members little. The sync stage takes no longer than for a standard group, because the BOLT transaction's body was already synced, so the fsync only has the other members left to write. After the commit stage, the leader rotates the binary log again, because the promoted file is usually over `max_binlog_size`. The next group waits for that rotation, because it needs `LOCK_log`, as after any group that fills a binary log file.

With semi-sync, the members wait until a replica has received the BOLT transaction, which is the cost of any large transaction, also on the standard path. On the replica, however, the BOLT transaction is a parallelization barrier, because it starts a new binary log file, so transactions before and after it are not applied in parallel with it. On the standard path, a large transaction is a barrier only if a rotation happens to fall next to it. After a BOLT transaction over 4 GiB, the members written behind it in the same file have wrapped 4-byte `end_log_pos` values, as after any transaction over 4 GiB on the standard path.

## 5. Failure handling

| Where | What happens |
|---|---|
| In `MYSQL_BIN_LOG::commit()`, at the promotion decision (a commit-time blocker, see the fallback subsections) or when the spilled file's sync fails | no promotion. The transaction is written into the active file as usual, and the fallback is counted |
| Before the rotation (the log is closed, encryption is ON, the checksum changed, the forced reserved-header fallback, or the transaction got no GTID) | no promotion. The transaction is written into the active file as usual, and the fallback is counted |
| In `binlog_cache_data::flush()`, when the marked transaction is not its flush group's leader (not expected) | the transaction is copied as usual, with reason `kPromotionFailed` |
| In the rotation, when the GTID table is read-only and the active file is below `max_binlog_size` | the rotation keeps the active file (existing behaviour). The transaction is written into the active file, with reason `kGtidPersistence` |
| In `promote_spilled_file()`, up to the rename (the recheck, the fit check, the header read, the header write and sync, or the rename) | the rotation finishes with the new file it created, which holds only its header events, and the BOLT transaction is copied into that file as a standard transaction, with reason `kEncryption`, `kChecksumMismatch`, `kReservedHeaderSpace` or `kPromotionFailed`. The transaction's Gtid event keeps the session's original commit timestamp and server version |
| Any other failure of the rotation, before the promotion (GTID save, purge index, new file, header) or after the rename (directory sync, reopen, sync, index update) | a rotation error. `binlog_error_action` applies (abort, or disable binary logging), as for any rotation |

The flush stage handles a rotation error itself, through `binlog_error_action`, so the client error that the rotation raised is cleared from the session.
