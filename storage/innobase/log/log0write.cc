/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2009, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0write.cc

 Redo log writing and flushing, including functions for:
         1. Waiting for the log written / flushed up to provided lsn.
         2. Redo log background threads (except the log checkpointer).

 @author Paweł Olchawa

 *******************************************************/

#ifndef UNIV_HOTBACKUP

#include <cstring>
#include "ha_prototypes.h"

#include <debug_sync.h>

#include "arch0arch.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "dict0boot.h"
#include "dict0stats_bg.h"
#include "fil0fil.h"
#include "log0log.h"
#include "log0recv.h"
#include "mem0mem.h"
#include "mysqld.h" /* server_uuid */
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "sync0sync.h"
#include "trx0roll.h"
#include "trx0sys.h"
#include "trx0trx.h"

/**************************************************/ /**
 @page PAGE_INNODB_REDO_LOG_THREADS Background redo log threads

 Three background log threads are responsible for writes of new data to disk:

 -# [Log writer](@ref sect_redo_log_writer) - writes from the log buffer or
 write-ahead buffer to OS buffers.

 -# [Log flusher](@ref sect_redo_log_flusher) - writes from OS buffers to disk
 (fsyncs).

 -# [Log write_notifier](@ref sect_redo_log_write_notifier) - notifies user
 threads about completed writes to disk (when write_lsn is advanced).

 -# [Log flush_notifier](@ref sect_redo_log_flush_notifier) - notifies user
 threads about completed fsyncs (when flushed_to_disk_lsn is advanced).

 Two background log threads are responsible for checkpoints (reclaiming space
 in log files):

 -# [Log closer](@ref sect_redo_log_closer) - tracks up to which lsn all
 dirty pages have been added to flush lists (wrt. oldest_modification).

 -# [Log checkpointer](@ref sect_redo_log_checkpointer) - determines
 @ref subsect_redo_log_available_for_checkpoint_lsn and writes checkpoints.


 @section sect_redo_log_writer Thread: log writer

 This thread is responsible for writing data from the log buffer to disk
 (to the log files). However, it's not responsible for doing fsync() calls.
 It copies data to system buffers. It is the log flusher thread, which is
 responsible for doing fsync().

 There are following points that need to be addressed by the log writer thread:

 -# %Find out how much data is ready in the log buffer, which is concurrently
    filled in by multiple user threads.

   In the log recent written buffer, user threads set links for every finished
   write to the log buffer. Each such link is represented as a number of bytes
   written, starting from a _start_lsn_. The link is stored in the slot assigned
   to the _start_lsn_ of the write.

   The log writer thread tracks links in the recent written buffer, traversing
   a connected path created by the links. It stops when it encounters a missing
   outgoing link. In such case the next fragment of the log buffer is still
   being written (or the maximum assigned lsn was reached).

   It also stops as soon as it has traversed by more than 4kB, in which case
   it is enough for a next write (unless we decided again to do fsyncs from
   inside the log writer thread). After traversing links and clearing slots
   occupied by the links (in the recent written buffer), the log writer thread
   updates @ref subsect_redo_log_buf_ready_for_write_lsn.

   @diafile storage/innobase/log/recent_written_buffer.dia "Example of links in
 the recent written buffer"

   @note The log buffer has no holes up to the _log.buf_ready_for_write_lsn_
   (all concurrent writes for smaller lsn have been finished).

   If there were no links to traverse, _log.buf_ready_for_write_lsn_ was not
   advanced and the log writer thread needs to wait. In such case it first
   uses spin delay and afterwards switches to wait on the _writer_event_.


 -# Prepare log blocks for writing - update their headers and footers.

   The log writer thread detects completed log blocks in the log buffer.
   Such log blocks will not receive any more writes. Hence their headers
   and footers could be easily updated (e.g. checksum is calculated).

   @diafile storage/innobase/log/log_writer_complete_blocks.dia "Complete blocks
 are detected and written"

   If any complete blocks were detected, they are written directly from
   the log buffer (after updating headers and footers). Afterwards the
   log writer thread retries the previous step before making next decisions.
   For each write consisting of one or more complete blocks, the
   _MONITOR_LOG_FULL_BLOCK_WRITES_ is incremented by one.

   @note There is a special case - when write-ahead is required, data needs
   to be copied to the write-ahead buffer and the last incomplete block could
   also be copied and written. For details read below and check the next point.

   The special case is also for the last, incomplete log block. Note that
   @ref subsect_redo_log_buf_ready_for_write_lsn could be in the middle of
   such block. In such case, next writes are likely incoming to the log block.

   @diafile storage/innobase/log/log_writer_incomplete_block.dia "Incomplete
 block is copied"

   For performance reasons we often need to write the last incomplete block.
   That's because it turned out, that we should try to reclaim user threads
   as soon as possible, allowing them to handle next transactions and provide
   next data.

   In such case:
     - the last log block is first copied to the dedicated buffer, up to the
       @ref subsect_redo_log_buf_ready_for_write_lsn,
     - the remaining part of the block in the dedicated buffer is filled in
       with _0x00_ bytes,
     - header fields are updated,
     - checksum is calculated and stored in the block's footer,
     - the block is written from the dedicated buffer,
     - the _MONITOR_LOG_PARTIAL_BLOCK_WRITES_ is incremented by one.

       @note The write-ahead buffer is used as the dedicated buffer for writes
       of the last incomplete block. That's because, whenever we needed a next
       write-ahead (even for complete blocks), we possibly can also write the
       last incomplete block during the write-ahead. The monitor counters for
       full/partial block writes are incremented before the logic related to
       writing ahead is applied. Hence the counter of partial block writes is
       not incremented if a full block write was possible (in which case only
       requirement for write-ahead could be the reason of writing the incomplete
       block).

   @remarks The log writer thread never updates
   [first_rec_group](@ref a_redo_log_block_first_rec_group) field.
   It has to be set by user threads before the block is allowed to be written.
   That's because only user threads know where are the boundaries between
   groups of log records. The user thread which has written data ending at
   lsn which needs to be pointed as _first_rec_group_, is the one responsible
   for setting the field. User thread which has written exactly up to the end
   of log block, is considered ending at lsn after the header of the next log
   block. That's because after such write, the log writer is allowed to write
   the next empty log block (_buf_ready_for_write_lsn_ points then to such lsn).
   The _first_rec_group_ field is updated before the link is added to the log
   recent written buffer.


 -# Avoid read-on-write issue.

   The log writer thread is also responsible for writing ahead to avoid
   the read-on-write problem. It tracks up to which point the write ahead
   has been done. When a write would go further:

     - If we were trying to write more than size of single write-ahead
       region, we limit the write to completed write-ahead sized regions,
       and postpone writing the last fragment for later (retrying with the
       first step and updating the _buf_ready_for_write_lsn_).

       @note If we needed to write complete regions of write-ahead bytes,
       they are ready in the log buffer and could be written directly from
       there. Such writes would not cause read-on-write problem, because
       size of the writes is divisible by write-ahead region.

     - Else, we copy data to special write-ahead buffer, from which
       we could safely write the whole single write-ahead sized region.
       After copying the data, the write-ahead buffer is completed with
       _0x00_ bytes.

       @note The write-ahead buffer is also used for copying the last
       incomplete log block, which was described in the previous point.


 -# Update write_lsn.

   After doing single write (single fil_io()), the log writer thread updates
   @ref subsect_redo_log_write_lsn and fallbacks to its main loop. That's
   because a lot more data could be prepared in meantime, as the write operation
   could take significant time.

   That's why the general rule is that after doing fil_io(), we need to update
   @ref subsect_redo_log_buf_ready_for_write_lsn before making next decisions
   on how much to write within next fil_io() call.


 -# Notify [log writer_notifier thread](@ref sect_redo_log_write_notifier)
   using os_event_set on the _write_notifier_event_.

   @see @ref sect_redo_log_waiting_for_writer


 -# Notify [log flusher thread](@ref sect_redo_log_flusher) using os_event_set()
   on the _flusher_event_.


 @section sect_redo_log_flusher Thread: log flusher

 The log flusher thread is responsible for doing fsync() of the log files.

 When the fsync() calls are finished, the log flusher thread updates the
 @ref subsect_redo_log_flushed_to_disk_lsn and notifies the
 [log flush_notifier thread](@ref sect_redo_log_flush_notifier) using
 os_event_set() on the _flush_notifier_event_.

 @remarks
 Small optimization has been applied - if there was only a single log block
 flushed since the previous flush, then the log flusher thread notifies user
 threads directly (instead of notifying the log flush_notifier thread).
 Impact of the optimization turned out to be positive for some scenarios and
 negative for other, so further investigation is required. However, because
 the change seems to make sense from logical point of view, it has been
 preserved.

 If the log flusher thread detects that none of the conditions is satisfied,
 it simply waits and retries the checks. After initial spin delay, it waits
 on the _flusher_event_.


 @section sect_redo_log_flush_notifier Thread: log flush_notifier

 The log flush_notifier thread is responsible for notifying all user threads
 that are waiting for @ref subsect_redo_log_flushed_to_disk_lsn >= lsn, when
 the condition is satisfied.

 @remarks
 It also notifies when it is very likely to be satisfied (lsn values are
 within the same log block). It is allowed to make mistakes and it is
 responsibility of the notified user threads to ensure, that
 the _flushed_to_disk_lsn_ is advanced sufficiently.

 The log flush_notifier thread waits for the advanced _flushed_to_disk_lsn_
 in loop, using os_event_wait_time_low() on the _flush_notifier_event_.
 When it gets notified by the [log flusher](@ref sect_redo_log_flusher),
 it ensures that the _flushed_to_disk_lsn_ has been advanced (single new
 byte is enough though).

 It notifies user threads waiting on all events between (inclusive):
   - event for a block with the previous value of _flushed_to_disk_lsn_,
   - event for a block containing the new value of _flushed_to_disk_lsn_.

 Events are assigned per blocks in the circular array of events using mapping:

         event_slot = (lsn-1) / OS_FILE_LOG_BLOCK_SIZE % S

 where S is size of the array (number of slots with events). Each slot has
 single event, which groups all user threads waiting for flush up to any lsn
 within the same log block (or log block with number greater by S*i).

 @diafile storage/innobase/log/log_notifier_notifications.dia "Notifications
 executed on slots"

 Internal mutex in event is used, to avoid missed notifications (these would
 be worse than the false notifications).

 However, there is also maximum timeout defined for the waiting on the event.
 After the timeout was reached (default: 1ms), the _flushed_to_disk_lsn_ is
 re-checked in the user thread (just in case).

 @note Because flushes are possible for @ref subsect_redo_log_write_lsn set in
 the middle of log block, it is likely that the same slot for the same block
 will be notified multiple times in a row. We tried delaying notifications for
 the last block, but the results were only worse then. It turned out that
 latency is extremely important here.

 @see @ref sect_redo_log_waiting_for_flusher


 @section sect_redo_log_write_notifier Thread: log write_notifier

 The log write_notifier thread is responsible for notifying all user threads
 that are waiting for @ref subsect_redo_log_write_lsn >= lsn, when the condition
 is satisfied.

 @remarks
 It also notifies when it is very likely to be satisfied (lsn values are
 within the same log block). It is allowed to make mistakes and it is
 responsibility of the notified user threads to ensure, that the _write_lsn_
 is advanced sufficiently.

 The log write_notifier thread waits for the advanced _write_lsn_ in loop,
 using os_event_wait_time_low() on the _write_notifier_event_.
 When it gets notified (by the [log writer](@ref sect_redo_log_writer)),
 it ensures that the _write_lsn_ has been advanced (single new byte is enough).
 Then it notifies user threads waiting on all events between (inclusive):
   - event for a block with the previous value of _write_lsn_,
   - event for a block containing the new value of _write_lsn_.

 Events are assigned per blocks in the circular array of events using mapping:

         event_slot = (lsn-1) / OS_FILE_LOG_BLOCK_SIZE % S

 where S is size of the array (number of slots with events). Each slot has
 single event, which groups all user threads waiting for write up to any lsn
 within the same log block (or log block with number greater by S*i).

 Internal mutex in event is used, to avoid missed notifications (these would
 be worse than the false notifications).

 However, there is also maximum timeout defined for the waiting on the event.
 After the timeout was reached (default: 1ms), the _write_lsn_ is re-checked
 in the user thread (just in case).

 @note Because writes are possible for @ref subsect_redo_log_write_lsn set in
 the middle of log block, it is likely that the same slot for the same block
 will be notified multiple times in a row.

 @see @ref sect_redo_log_waiting_for_writer


 @section sect_redo_log_closer Thread: log closer

 The log closer thread is responsible for tracking up to which lsn, all
 dirty pages have already been added to flush lists. It traverses links
 in the log recent closed buffer, following a connected path, which is
 created by the links. The traversed links are removed and afterwards
 the @ref subsect_redo_log_buf_dirty_pages_added_up_to_lsn is updated.

 Links are stored inside slots in a ring buffer. When link is removed,
 the related slot becomes empty. Later it is reused for link pointing
 from larger lsn value.

 The log checkpointer thread must not write a checkpoint for lsn larger
 than _buf_dirty_pages_added_up_to_lsn_. That is because some user thread
 might be in state where it is just after writing to the log buffer, but
 before adding its dirty pages to flush lists. The dirty pages could have
 modifications protected by log records, which start at lsn, which would
 be logically deleted by such checkpoint.


 @section sect_redo_log_checkpointer Thread: log checkpointer

 The log checkpointer thread is responsible for:

 -# Checking if a checkpoint write is required (to decrease checkpoint age
 before it gets too big).

 -# Checking if synchronous flush of dirty pages should be forced on page
 cleaner threads, because of space in redo log or age of the oldest page.

 -# Writing checkpoints (it's the only thread allowed to do it!).

 This thread has been introduced at the very end. It was not required for
 the performance, but it makes the design more consistent after we have
 introduced other log threads. That's because user threads are not doing
 any writes to the log files themselves then. Previously they were writing
 checkpoints when needed, which required synchronization between them.

 The log checkpointer thread updates log.available_for_checkpoint_lsn,
 which is calculated as:

         min(log.buf_dirty_pages_added_up_to_lsn, max(0, oldest_lsn - L))

 where:
   - oldest_lsn = min(oldest modification of the earliest page from each
                      flush list),
   - L is a number of slots in the log recent closed buffer.

 The special case is when there is no dirty page in flush lists - then it's
 basically set to the _log.buf_dirty_pages_added_up_to_lsn_.

 @note Note that previously, all user threads were trying to calculate this
 lsn concurrently, causing contention on flush_list mutex, which is required
 to read the _oldest_modification_ of the earliest added page. Now the lsn
 is updated in single thread.


 @section sect_redo_log_waiting_for_writer Waiting until log has been written to
 disk

 User has to wait until the [log writer thread](@ref sect_redo_log_writer)
 has written data from the log buffer to disk for lsn >= _end_lsn_ of log range
 used by the user, which is true when:

         write_lsn >= end_lsn

 The @ref subsect_redo_log_write_lsn is updated by the log writer thread.

 The waiting is solved using array of events. The user thread waiting for
 a given lsn, waits using the event at position:

         slot = (end_lsn - 1) / OS_FILE_LOG_BLOCK_SIZE % S

 where _S_ is number of entries in the array. Therefore the event corresponds
 to log block which contains the _end_lsn_.

 The [log write_notifier thread](@ref sect_redo_log_write_notifier) tracks how
 the @ref subsect_redo_log_write_lsn is advanced and notifies user threads for
 consecutive slots.

 @remarks
 When the _write_lsn_ is in the middle of log block, all user threads waiting
 for lsn values within the whole block are notified. When user thread is
 notified, it checks if the current value of the _write_lsn_ is sufficient and
 retries waiting if not. To avoid missed notifications, event's internal mutex
 is used.


 @section sect_redo_log_waiting_for_flusher Waiting until log has been flushed
 to disk

 If a user need to assure the log persistence in case of crash (e.g. on COMMIT
 of a transaction), he has to wait until [log flusher](@ref
 sect_redo_log_flusher) has flushed log files to disk for lsn >= _end_lsn_ of
 log range used by the user, which is true when:

         flushed_to_disk_lsn >= end_lsn

 The @ref subsect_redo_log_flushed_to_disk_lsn is updated by the log flusher
 thread.

 The waiting is solved using array of events. The user thread waiting for
 a given lsn, waits using the event at position:

         slot = (end_lsn - 1) / OS_FILE_LOG_BLOCK_SIZE % S

 where _S_ is number of entries in the array. Therefore the event corresponds
 to log block which contains the _end_lsn_.

 The [log flush_notifier thread](@ref sect_redo_log_flush_notifier) tracks how
 the
 @ref subsect_redo_log_flushed_to_disk_lsn is advanced and notifies user
 threads for consecutive slots.

 @remarks
 When the _flushed_to_disk_lsn_ is in the middle of log block, all
 user threads waiting for lsn values within the whole block are notified.
 When user thread is notified, it checks if the current value of the
 _flushed_to_disk_lsn_ is sufficient and retries waiting if not.
 To avoid missed notifications, event's internal mutex is used.


 @page PAGE_INNODB_REDO_LOG_FORMAT Format of redo log

 @section sect_redo_log_format_overview Overview

 Redo log contains multiple log files, each has the same format. Consecutive
 files have data for consecutive ranges of lsn values. When a file ends at
 _end_lsn_, the next log file begins at the _end_lsn_. There is a fixed number
 of log files, they are re-used in circular manner. That is, for the last
 log file, the first log file is a successor.

 @note A single big file would remain fully cached for some of file systems,
 even if only a small fragment of the file is being modified. Hence multiple
 log files are used to make evictions always possible. Keep in mind though
 that log files are used in circular manner (lsn modulo size of log files,
 when size is calculated except the log file headers).

 The default log file names are: _ib_logfile0_, _ib_logfile1_, ... The maximum
 allowed number of log files is 100. The special file name _ib_logfile101_ is
 used when new log files are created and it is used instead of _ib_logfile0_
 until all the files are ready. Afterwards the _ib_logfile101_ is atomically
 renamed to _ib_logfile0_ and files are considered successfully created then.

 @section sect_redo_log_format_file Log file format

 @subsection subsect_redo_log_format_header Header of log file

 %Log file starts with a header of _LOG_FILE_HDR_SIZE_ bytes. It contains:

   - Initial block of _OS_FILE_LOG_BLOCK_SIZE_ (512) bytes, which has:

     - Binding of an offset within the file to the lsn value.

       This binding allows to map any lsn value which is represented
       within the file to corresponding lsn value.

   - Format of redo log - remains the same as before the patch.

   - Checksum of the block.

   - Two checkpoint blocks - _LOG_CHECKPOINT_1_ and _LOG_CHECKPOINT_2_.

     Each checkpoint block contains _OS_FILE_LOG_BLOCK_SIZE_ bytes:

       - _checkpoint_lsn_ - lsn to start recovery at.

         @note In earlier versions than 8.0, checkpoint_lsn pointed
         directly to the beginning of the first log record group,
         which should be recovered (but still the related page could
         have been flushed). However since 8.0 this value might point
         to some byte inside a log record. In such case, recovery is
         supposed to skip the group of log records which contains
         the checkpoint lsn (and start at the beginning of the next).
         We cannot easily determine beginning of the next group.
         There are two cases:

         - block with _checkpoint_lsn_ has no beginning of group at all
           (first_rec_group = 0) - then we search forward for the first
           block that has non-zero first_rec_group and there we have
           the next group's start,

         - block with _checkpoint_lsn_ has one or more groups of records
           starting inside the block - then we start parsing at the first
           group that starts in the block and keep parsing consecutive
           groups until we passed checkpoint_lsn; we don't apply these
           groups of records (we must not because of fil renames); after
           we passed checkpoint_lsn, the next group that starts is the
           one we were looking for to start recovery at; it is possible
           that the next group begins in the next block (if there was no
           more groups starting after checkpoint_lsn within the block)

       - _checkpoint_no_ - checkpoint number - when checkpoint is
         being written, a next checkpoint number is assigned.

       - _log.buf_size_ - size of the log buffer when the checkpoint
         write was started.

         It remains a mystery, why do we need that. It's neither used
         by the recovery, nor required for MEB. Some rumours say that
         maybe it could be useful for auto-config external tools to
         detect what configuration of MySQL should be used.

         @note
         Note that size of the log buffer could be decreased in runtime,
         after writing the checkpoint (which was not the case, when this
         field was being introduced).

     There are two checkpoint headers, because they are updated alternately.
     In case of crash in the middle of any such update, the alternate header
     would remain valid (so it's the same reason for which double write buffer
     is used for pages).

     @remarks
     Each log file has its own header. However checkpoints are read only from
     the first log file (_ib_logfile0_) during recovery.

 @subsection subsect_redo_log_format_blocks Log blocks

 After the header, there are consecutive log blocks. Each log block has the same
 format and consists of _OS_FILE_LOG_BLOCK_SIZE_ bytes (512). These bytes are
 enumerated by lsn values.

 @note Bytes used by [headers of log files](@ref subsect_redo_log_format_header)
 are NOT included in lsn sequence.

 Each log block contains:
   - header - _LOG_BLOCK_HDR_SIZE_ bytes (12):

     - @anchor a_redo_log_block_hdr_no hdr_no

       This is a block number. Consecutive blocks have consecutive numbers.
       Hence this is basically lsn divided by _OS_FILE_LOG_BLOCK_SIZE_.
       However it is also wrapped at 1G (due to limited size of the field).
       It should be possible to wrap it at 2G (only the single flush bit is
       reserved as the highest bit) but for historical reasons it is 1G.

     - @anchor a_redo_log_block_flush_bit flush_bit

       This is a single bit stored as the highest bit of hdr_no. The bit is
       skipped when calculating block number.

       It is set for the first block of multiple blocks written in a single
       call to fil_io().

       It was supposed to help to filter out writes which were not atomic.
       When the flush bit is read from disk, it means that up to this lsn,
       all previous log records have been fully written from the log buffer
       to OS buffers. That's because previous calls to fil_io() had to be
       finished, before a fil_io() call for current block was started.

       The wrong assumption was that we can trust those log records then.
       Note, we have no guarantee that order of writes is preserved by disk
       controller. That's why only after fsync() call is finished, one could
       be sure, that data is fully written (up to the write_lsn at which
       fsync() was started).

       During recovery, when the flush bit is encountered, *contiguous_lsn
       is updated, but then the updated lsn seems unused...

       It seems that there is no real benefit from the flush bit at all,
       and even in 5.7 it was completely ignored during the recovery.

     - @anchor a_redo_log_block_data_len data_len

       Number of bytes within the log block. Possible values:

         - _0_ - this is an empty block (end the recovery).

         - _OS_FILE_LOG_BLOCK_SIZE_ - this is a full block.

         - value within [_LOG_BLOCK_HDR_SIZE_,
           _OS_FILE_LOG_BLOCK_SIZE_ - _LOG_BLOCK_TRL_SIZE_),
           which means that this is the last block and it is an
           incomplete block.

           This could be then considered an offset, which points
           to the end of the data within the block. This value
           includes _LOG_BLOCK_HDR_SIZE_ bytes of the header.

     - @anchor a_redo_log_block_first_rec_group first_rec_group

       Offset within the log block to the beginning of the first group
       of log records that starts within the block or 0 if none starts.
       This offset includes _LOG_BLOCK_HDR_SIZE_ bytes of the header.

     - @anchor a_redo_log_block_checkpoint_no checkpoint_no

       Checkpoint number of a next checkpoint write. Set by the log
       writer thread just before a write starts for the block.

       It could be used during recovery to detect that we have read
       old block of redo log (tail) because of the wrapped log files.

   - data part - bytes up to [data_len](@ref a_redo_log_block_data_len) byte.

     Actual data bytes are followed by _0x00_ if the block is incomplete.

     @note Bytes within this fragment of the block, are enumerated by _sn_
     sequence (whereas bytes of header and trailer are NOT). This is the
     only difference between _sn_ and _lsn_ sequences (_lsn_ enumerates
     also bytes of header and trailer).

   - trailer - _LOG_BLOCK_TRL_SIZE_ bytes (4):

     - checksum

       Algorithm used for the checksum depends on the configuration.
       Note that there is a potential problem if a crash happened just
       after switching to "checksums enabled". During recovery some log
       blocks would have checksum = LOG_NO_CHECKSUM_MAGIC and some would
       have a valid checksum. Then recovery with enabled checksums would
       point problems for the blocks without valid checksum. User would
       have to disable checksums for the recovery then.



 @remarks
 All fields except [first_rec_group](@ref a_redo_log_block_first_rec_group)
 are updated by the [log writer thread](@ref sect_redo_log_writer) just before
 writing the block.

 *******************************************************/

/** Writes fragment of log buffer to log files. The first write to the first
log block in a new log file, flushes header of the file. It stops after doing
single fil_io operation. The reason is that it might make sense to advance
lsn up to which we have ready data in log buffer for write, after time
consuming operation, such as fil_io. The log.write_lsn is advanced.
@param[in]	log		redo log
@param[in]	buffer		the beginning of first log block to write
@param[in]	buffer_size	number of bytes to write since 'buffer'
@param[in]	start_lsn	lsn corresponding to first block start */
static void log_files_write_buffer(log_t &log, byte *buffer, size_t buffer_size,
                                   lsn_t start_lsn);

/** Writes fragment of the log buffer up to provided lsn (not further).
Stops after the first call to fil_io() (possibly at smaller lsn).
Main side-effect: log.write_lsn is advanced.
@param[in]	log		redo log
@param[in]	next_write_lsn	write up to this lsn value */
static void log_writer_write_buffer(log_t &log, lsn_t next_write_lsn);

/** Executes a synchronous flush of the log files (doing fsyncs).
Advances log.flushed_to_disk_lsn and notifies log flush_notifier thread.
Note: if only a single log block was flushed to disk, user threads
waiting for lsns within the block are notified directly from here,
and log flush_notifier thread is not notified! (optimization)
@param[in,out]	log	redo log */
static void log_flush_low(log_t &log);

/** Writes encryption information to log header.
@param[in,out]	buf	log file header
@param[in]	key	encryption key
@param[in]	iv	encryption iv
@param[in]	is_boot	if it's for bootstrap */
static bool log_file_header_fill_encryption(byte *buf, byte *key, byte *iv,
                                            bool is_boot);

/**************************************************/ /**

 @name Waiting for redo log written or flushed up to lsn

 *******************************************************/

/* @{ */

/** Waits until redo log is written up to provided lsn (or greater).
We do not care if it's flushed or not.
@param[in]	log	redo log
@param[in]	lsn	wait until log.write_lsn >= lsn
@return		statistics related to waiting inside */
static Wait_stats log_wait_for_write(const log_t &log, lsn_t lsn) {
  if (log.write_lsn.load() >= lsn) {
    return (Wait_stats{0});
  }

  os_event_try_set(log.writer_event);

  auto max_spins = srv_log_wait_for_write_spin_delay;

  if (srv_flush_log_at_trx_commit == 1 ||
      srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm ||
      srv_cpu_usage.utime_pct >= srv_log_spin_cpu_pct_hwm) {
    max_spins = 0;
  }

  auto stop_condition = [&log, lsn](bool) {
    if (log.write_lsn.load() >= lsn) {
      return (true);
    }

    ut_d(log_background_write_threads_active_validate(log));
    return (false);
  };

  size_t slot =
      (lsn - 1) / OS_FILE_LOG_BLOCK_SIZE & (log.write_events_size - 1);

  const auto wait_stats =
      os_event_wait_for(log.write_events[slot], max_spins,
                        srv_log_wait_for_write_timeout, stop_condition);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_WRITE_, wait_stats);

  return (wait_stats);
}

/** Waits until redo log is flushed up to provided lsn (or greater).
@param[in]	log	redo log
@param[in]	lsn	wait until log.flushed_to_disk_lsn >= lsn
@return		statistics related to waiting inside */
static Wait_stats log_wait_for_flush(const log_t &log, lsn_t lsn) {
  os_event_try_set(log.flusher_event);

  /* Optional spinning - useful for usr1-4 case, disabled by default. */
  auto max_spins = srv_log_wait_for_flush_spin_delay;

  if (log.flush_avg_time >= srv_log_wait_for_flush_spin_hwm ||
      srv_flush_log_at_trx_commit != 1 ||
      srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm ||
      srv_cpu_usage.utime_pct >= srv_log_spin_cpu_pct_hwm) {
    /* Average flush time is too big, don't spin,
    also don't spin when trx != 1. */
    max_spins = 0;
  }

  auto stop_condition = [&log, lsn](bool) {
    LOG_SYNC_POINT("log_wait_for_flush_before_flushed_to_disk_lsn");

    if (log.flushed_to_disk_lsn.load() >= lsn) {
      return (true);
    }

    if (srv_flush_log_at_trx_commit != 1) {
      os_event_set(log.flusher_event);
    }

    LOG_SYNC_POINT("log_wait_for_flush_before_wait");
    return (false);
  };

  size_t slot =
      (lsn - 1) / OS_FILE_LOG_BLOCK_SIZE & (log.flush_events_size - 1);

  const auto wait_stats =
      os_event_wait_for(log.flush_events[slot], max_spins,
                        srv_log_wait_for_flush_timeout, stop_condition);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_FLUSH_, wait_stats);

  return (wait_stats);
}

Wait_stats log_write_up_to(log_t &log, lsn_t end_lsn, bool flush_to_disk) {
  ut_a(!srv_read_only_mode);

  /* If we were updating log.flushed_to_disk_lsn while parsing redo log
  during recovery, we would have valid value here and we would not need
  to explicitly exit because of the recovery. However we do not update
  the log.flushed_to_disk during recovery (it is zero).

  On the other hand, when we apply log records during recovery, we modify
  pages and update their oldest/newest_modification. The modified pages
  become dirty. When size of the buffer pool is too small, some pages
  have to be flushed from LRU, to reclaim a free page for a next read.

  When flushing such dirty pages, we notice that newest_modification != 0,
  so the redo log has to be flushed up to the newest_modification, before
  flushing the page. In such case we end up here during recovery.

  Note that redo log is actually flushed, because changes to the page
  are caused by applying the redo. */

  if (recv_no_ibuf_operations) {
    /* Recovery is running and no operations on the log files are
    allowed yet, which is implicitly deduced from the fact, that
    still ibuf merges are disallowed. */
    return (Wait_stats{0});
  }

  ut_a(end_lsn != LSN_MAX);

  ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE == 0 ||
       end_lsn % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

  ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE <=
       OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

  ut_a(end_lsn <= log_get_lsn(log));

  if (flush_to_disk) {
    if (log.flushed_to_disk_lsn.load() >= end_lsn) {
      return (Wait_stats{0});
    }

    Wait_stats wait_stats{0};

    /* The flusher might be sleeping for some time when flush is not
    forced, if we want to wait for flush in such cases (e.g. for DDL
    operations) we need to wake flusher up. */
    if (srv_flush_log_at_trx_commit != 1) {
      /* First we need to assure the data is written and ready
      for flusher to continue with. */
      wait_stats += log_wait_for_write(log, end_lsn);
      os_event_set(log.flusher_event);
    }

    /* Wait until log gets flushed up to end_lsn. */
    return (wait_stats + log_wait_for_flush(log, end_lsn));

  } else {
    if (log.write_lsn.load() >= end_lsn) {
      return (Wait_stats{0});
    }

    /* Wait until log gets written up to end_lsn. */
    return (log_wait_for_write(log, end_lsn));
  }
}

/* @} */

/**************************************************/ /**

 @name Log writer thread

 *******************************************************/

/* @{ */

uint64_t log_files_size_offset(const log_t &log, uint64_t offset) {
  ut_ad(log_writer_mutex_own(log));

  return (offset - LOG_FILE_HDR_SIZE * (1 + offset / log.file_size));
}

uint64_t log_files_real_offset(const log_t &log, uint64_t offset) {
  ut_ad(log_writer_mutex_own(log));

  return (offset + LOG_FILE_HDR_SIZE *
                       (1 + offset / (log.file_size - LOG_FILE_HDR_SIZE)));
}

uint64_t log_files_real_offset_for_lsn(const log_t &log, lsn_t lsn) {
  uint64_t size_offset;
  uint64_t size_capacity;
  uint64_t delta;

  ut_ad(log_writer_mutex_own(log));

  size_capacity = log.n_files * (log.file_size - LOG_FILE_HDR_SIZE);

  if (lsn >= log.current_file_lsn) {
    delta = lsn - log.current_file_lsn;

    delta = delta % size_capacity;

  } else {
    /* Special case because lsn and offset are unsigned. */

    delta = log.current_file_lsn - lsn;

    delta = size_capacity - delta % size_capacity;
  }

  size_offset = log_files_size_offset(log, log.current_file_real_offset);

  size_offset = (size_offset + delta) % size_capacity;

  return (log_files_real_offset(log, size_offset));
}

void log_files_update_offsets(log_t &log, lsn_t lsn) {
  ut_ad(log_writer_mutex_own(log));
  ut_a(log.file_size > 0);
  ut_a(log.n_files > 0);

  lsn = ut_uint64_align_down(lsn, OS_FILE_LOG_BLOCK_SIZE);

  log.current_file_real_offset = log_files_real_offset_for_lsn(log, lsn);

  /* Real offsets never enter headers of files when calculated
  for some LSN / size offset. */
  ut_a(log.current_file_real_offset % log.file_size >= LOG_FILE_HDR_SIZE);

  log.current_file_lsn = lsn;

  log.current_file_end_offset = log.current_file_real_offset -
                                log.current_file_real_offset % log.file_size +
                                log.file_size;

  ut_a(log.current_file_end_offset % log.file_size == 0);
}

namespace Log_files_write_impl {

static inline void validate_buffer(const log_t &log, const byte *buffer,
                                   size_t buffer_size) {
  ut_a(buffer >= log.buf);
  ut_a(buffer_size > 0);
  ut_a(buffer + buffer_size <= log.buf + log.buf_size);
}

static inline void validate_start_lsn(const log_t &log, lsn_t start_lsn,
                                      size_t buffer_size) {
  /* start_lsn corresponds to block, it must be aligned to 512 */
  ut_a(start_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  /* Either full log block writes are possible or partial writes,
  which have to cover full header of log block then. */
  ut_a((start_lsn + buffer_size) % OS_FILE_LOG_BLOCK_SIZE >=
           LOG_BLOCK_HDR_SIZE ||
       (start_lsn + buffer_size) % OS_FILE_LOG_BLOCK_SIZE == 0);

  /* Partial writes do not touch footer of log block. */
  ut_a((start_lsn + buffer_size) % OS_FILE_LOG_BLOCK_SIZE <
       OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

  /* There are no holes. Note that possibly start_lsn is smaller,
  because it always points to the beginning of log block. */
  ut_a(start_lsn <= log.write_lsn.load());
}

static inline uint64_t compute_real_offset(const log_t &log, lsn_t start_lsn) {
  ut_a(start_lsn >= log.current_file_lsn);

  ut_a(log.current_file_real_offset % log.file_size >= LOG_FILE_HDR_SIZE);

  const auto real_offset =
      log.current_file_real_offset + (start_lsn - log.current_file_lsn);

  ut_a(real_offset % log.file_size >= LOG_FILE_HDR_SIZE ||
       real_offset == log.current_file_end_offset);

  ut_a(real_offset % OS_FILE_LOG_BLOCK_SIZE == 0);

  ut_a(log_files_real_offset_for_lsn(log, start_lsn) ==
           real_offset % log.files_real_capacity ||
       real_offset == log.current_file_end_offset);

  return (real_offset);
}

static inline bool current_file_has_space(const log_t &log, uint64_t offset,
                                          size_t size) {
  return (offset + size <= log.current_file_end_offset);
}

static void start_next_file(log_t &log, lsn_t start_lsn) {
  const auto before_update = log.current_file_end_offset;

  auto real_offset = before_update;

  ut_a(log.file_size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(real_offset / log.file_size <= ULINT_MAX);

  ut_a(real_offset <= log.files_real_capacity);

  if (real_offset == log.files_real_capacity) {
    /* Wrapped log files, start at file 0,
    just after its initial headers. */
    real_offset = LOG_FILE_HDR_SIZE;
  }

  ut_a(real_offset + OS_FILE_LOG_BLOCK_SIZE <= log.files_real_capacity);

  /* Flush header of the new log file. */
  log_files_header_flush(log, real_offset / log.file_size, start_lsn);

  /* Update following members of log:
  - current_file_lsn,
  - current_file_real_offset,
  - current_file_end_offset.
  The only reason is to optimize future calculations
  of offsets within the new log file. */
  log_files_update_offsets(log, start_lsn);

  ut_a(log.current_file_real_offset == before_update + LOG_FILE_HDR_SIZE ||
       (before_update == log.files_real_capacity &&
        log.current_file_real_offset == LOG_FILE_HDR_SIZE));

  ut_a(log.current_file_real_offset - LOG_FILE_HDR_SIZE ==
       log.current_file_end_offset - log.file_size);

  log.write_ahead_end_offset = 0;
}

static inline bool write_ahead_enough(uint64_t write_ahead_end, uint64_t offset,
                                      size_t size) {
  return (write_ahead_end >= offset + size);
}

static inline bool current_write_ahead_enough(const log_t &log, uint64_t offset,
                                              size_t size) {
  return (write_ahead_enough(log.write_ahead_end_offset, offset, size));
}

static inline uint64_t compute_next_write_ahead_end(uint64_t real_offset) {
  const auto last_wa =
      ut_uint64_align_down(real_offset, srv_log_write_ahead_size);

  const auto next_wa = last_wa + srv_log_write_ahead_size;

  ut_a(next_wa > real_offset);
  ut_a(next_wa % srv_log_write_ahead_size == 0);

  return (next_wa);
}

static inline size_t compute_how_much_to_write(const log_t &log,
                                               uint64_t real_offset,
                                               size_t buffer_size,
                                               bool &write_from_log_buffer) {
  size_t write_size;

  /* First we ensure, that we will write within single log file.
  If we had more to write and cannot fit the current log file,
  we first write what fits, then stops and returns to the main
  loop of the log writer thread. Then, the log writer will update
  maximum lsn up to which, it has data ready in the log buffer,
  and request next write operation according to its strategy. */
  if (!current_file_has_space(log, real_offset, buffer_size)) {
    /* The end of write would not fit the current log file. */

    /* But the beginning is guaranteed to fit or to be placed
    at the first byte of the next file. */
    ut_a(current_file_has_space(log, real_offset, 0));

    if (!current_file_has_space(log, real_offset, 1)) {
      /* The beginning of write is at the first byte
      of the next log file. Flush header of the next
      log file, advance current log file to the next,
      stop and return to the main loop of log writer. */
      write_from_log_buffer = false;
      return (0);

    } else {
      /* We write across at least two consecutive log files.
      Limit current write to the first one and then retry for
      next_file. */

      /* If the condition for real_offset + buffer_size holds,
      then the expression below is < buffer_size, which is
      size_t, so the typecast is ok. */
      write_size =
          static_cast<size_t>(log.current_file_end_offset - real_offset);

      ut_a(write_size <= buffer_size);
      ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE == 0);
    }

  } else {
    write_size = buffer_size;

    ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE ||
         write_size % OS_FILE_LOG_BLOCK_SIZE == 0);

    ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE <
         OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);
  }

  /* Now, we know we can write write_size bytes from the buffer,
  and we will do the write within single log file - current one. */

  ut_a(write_size > 0);
  ut_a(real_offset >= log.current_file_real_offset);
  ut_a(real_offset + write_size <= log.current_file_end_offset);
  ut_a(log.current_file_real_offset / log.file_size + 1 ==
       log.current_file_end_offset / log.file_size);

  /* We are interested in writing from log buffer only,
  if we had at least one completed block for write.
  Still we might decide not to write from the log buffer,
  because write-ahead is needed. In such case we could write
  together with the last incomplete block after copying. */
  write_from_log_buffer = write_size >= OS_FILE_LOG_BLOCK_SIZE;

  if (write_from_log_buffer) {
    MONITOR_INC(MONITOR_LOG_FULL_BLOCK_WRITES);
  } else {
    MONITOR_INC(MONITOR_LOG_PARTIAL_BLOCK_WRITES);
  }

  /* Check how much we have written ahead to avoid read-on-write. */

  if (!current_write_ahead_enough(log, real_offset, write_size)) {
    if (!current_write_ahead_enough(log, real_offset, 1)) {
      /* Current write-ahead region has no space at all. */

      const auto next_wa = compute_next_write_ahead_end(real_offset);

      if (!write_ahead_enough(next_wa, real_offset, write_size)) {
        /* ... and also the next write-ahead is too small.
        Therefore we have more data to write than size of
        the write-ahead. We write from the log buffer,
        skipping last fragment for which the write ahead
        is required. */

        ut_a(write_from_log_buffer);

        write_size = next_wa - real_offset;

        ut_a((real_offset + write_size) % srv_log_write_ahead_size == 0);

        ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE == 0);

      } else {
        /* We copy data to write_ahead buffer,
        and write from there doing write-ahead
        of the bigger region in the same time. */
        write_from_log_buffer = false;
      }

    } else {
      /* We limit write up to the end of region
      we have written ahead already. */
      write_size =
          static_cast<size_t>(log.write_ahead_end_offset - real_offset);

      ut_a(write_size >= OS_FILE_LOG_BLOCK_SIZE);
      ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE == 0);
    }

  } else {
    if (write_from_log_buffer) {
      write_size = ut_uint64_align_down(write_size, OS_FILE_LOG_BLOCK_SIZE);
    }
  }

  return (write_size);
}

static inline void prepare_full_blocks(const log_t &log, byte *buffer,
                                       size_t size, lsn_t start_lsn,
                                       checkpoint_no_t checkpoint_no) {
  /* Prepare all completed blocks which are going to be written.

  Note, that completed blocks are always prepared in the log buffer,
  even if they are later copied to write_ahead buffer.

  This guarantees that finally we should have all blocks prepared
  in the log buffer (incomplete blocks will be rewritten once they
  became completed). */

  size_t buffer_offset;

  for (buffer_offset = 0; buffer_offset + OS_FILE_LOG_BLOCK_SIZE <= size;
       buffer_offset += OS_FILE_LOG_BLOCK_SIZE) {
    byte *ptr;

    ptr = buffer + buffer_offset;

    ut_a(ptr >= log.buf);

    ut_a(ptr + OS_FILE_LOG_BLOCK_SIZE <= log.buf + log.buf_size);

    log_block_set_hdr_no(
        ptr, log_block_convert_lsn_to_no(start_lsn + buffer_offset));

    log_block_set_flush_bit(ptr, buffer_offset == 0);

    log_block_set_data_len(ptr, OS_FILE_LOG_BLOCK_SIZE);

    log_block_set_checkpoint_no(ptr, checkpoint_no);

    log_block_store_checksum(ptr);
  }
}

static inline void write_blocks(log_t &log, byte *write_buf, size_t write_size,
                                uint64_t real_offset) {
  ut_a(write_size >= OS_FILE_LOG_BLOCK_SIZE);
  ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(real_offset / UNIV_PAGE_SIZE <= PAGE_NO_MAX);

  page_no_t page_no;

  page_no = static_cast<page_no_t>(real_offset / univ_page_size.physical());

  ut_a(log.write_ahead_end_offset % srv_log_write_ahead_size == 0);

  ut_a(real_offset + write_size <= log.write_ahead_end_offset ||
       (real_offset + write_size) % srv_log_write_ahead_size == 0);

  auto err = fil_redo_io(
      IORequestLogWrite, page_id_t{log.files_space_id, page_no}, univ_page_size,
      static_cast<ulint>(real_offset % UNIV_PAGE_SIZE), write_size, write_buf);

  ut_a(err == DB_SUCCESS);
}

static inline size_t compute_write_event_slot(const log_t &log, lsn_t lsn) {
  return ((lsn / OS_FILE_LOG_BLOCK_SIZE) & (log.write_events_size - 1));
}

static inline void notify_about_advanced_write_lsn(log_t &log,
                                                   lsn_t old_write_lsn,
                                                   lsn_t new_write_lsn) {
  const auto first_slot = compute_write_event_slot(log, old_write_lsn);

  const auto last_slot = compute_write_event_slot(log, new_write_lsn);

  if (first_slot == last_slot) {
    LOG_SYNC_POINT("log_write_before_users_notify");
    os_event_set(log.write_events[first_slot]);
  } else {
    LOG_SYNC_POINT("log_write_before_notifier_notify");
    os_event_set(log.write_notifier_event);
  }
}

static inline void copy_to_write_ahead_buffer(log_t &log, const byte *buffer,
                                              size_t &size, lsn_t start_lsn,
                                              checkpoint_no_t checkpoint_no) {
  ut_a(size <= srv_log_write_ahead_size);

  ut_a(buffer >= log.buf);
  ut_a(buffer + size <= log.buf + log.buf_size);

  byte *write_buf = log.write_ahead_buf;

  LOG_SYNC_POINT("log_writer_before_copy_to_write_ahead_buffer");

  std::memcpy(write_buf, buffer, size);

  size_t completed_blocks_size;
  byte *incomplete_block;
  size_t incomplete_size;

  completed_blocks_size = ut_uint64_align_down(size, OS_FILE_LOG_BLOCK_SIZE);

  incomplete_block = write_buf + completed_blocks_size;

  incomplete_size = size % OS_FILE_LOG_BLOCK_SIZE;

  ut_a(incomplete_block + incomplete_size <=
       write_buf + srv_log_write_ahead_size);

  if (incomplete_size != 0) {
    /* Prepare the incomplete (last) block. */
    ut_a(incomplete_size >= LOG_BLOCK_HDR_SIZE);

    log_block_set_hdr_no(
        incomplete_block,
        log_block_convert_lsn_to_no(start_lsn + completed_blocks_size));

    log_block_set_flush_bit(incomplete_block, completed_blocks_size == 0);

    log_block_set_data_len(incomplete_block, incomplete_size);

    if (log_block_get_first_rec_group(incomplete_block) > incomplete_size) {
      log_block_set_first_rec_group(incomplete_block, 0);
    }

    log_block_set_checkpoint_no(incomplete_block, checkpoint_no);

    std::memset(incomplete_block + incomplete_size, 0x00,
                OS_FILE_LOG_BLOCK_SIZE - incomplete_size);

    log_block_store_checksum(incomplete_block);

    size = completed_blocks_size + OS_FILE_LOG_BLOCK_SIZE;
  }

  /* Since now, size is about completed blocks always. */
  ut_a(size % OS_FILE_LOG_BLOCK_SIZE == 0);
}

static inline size_t prepare_for_write_ahead(log_t &log, uint64_t real_offset,
                                             size_t &write_size) {
  /* We need to perform write ahead during this write. */

  const auto next_wa = compute_next_write_ahead_end(real_offset);

  ut_a(real_offset + write_size <= next_wa);

  size_t write_ahead =
      static_cast<size_t>(next_wa - (real_offset + write_size));

  if (!current_file_has_space(log, real_offset, write_size + write_ahead)) {
    /* We must not write further than to the end
    of the current log file.

    Note, that: log.file_size - LOG_FILE_HDR_SIZE
    does not have to be divisible by size of write
    ahead. Example given:
            innodb_log_file_size = 1024M,
            innodb_log_write_ahead_size = 4KiB,
            LOG_FILE_HDR_SIZE is 2KiB. */

    write_ahead = static_cast<size_t>(log.current_file_end_offset -
                                      real_offset - write_size);
  }

  ut_a(current_file_has_space(log, real_offset, write_size + write_ahead));

  LOG_SYNC_POINT("log_writer_before_write_ahead");

  std::memset(log.write_ahead_buf + write_size, 0x00, write_ahead);

  write_size += write_ahead;

  return (write_ahead);
}

static inline void update_current_write_ahead(log_t &log, uint64_t real_offset,
                                              size_t write_size) {
  const auto end = real_offset + write_size;

  if (end > log.write_ahead_end_offset) {
    log.write_ahead_end_offset =
        ut_uint64_align_down(end, srv_log_write_ahead_size);
  }
}

}  // namespace Log_files_write_impl

static void log_files_write_buffer(log_t &log, byte *buffer, size_t buffer_size,
                                   lsn_t start_lsn) {
  ut_ad(log_writer_mutex_own(log));

  using namespace Log_files_write_impl;

  validate_buffer(log, buffer, buffer_size);

  validate_start_lsn(log, start_lsn, buffer_size);

  checkpoint_no_t checkpoint_no = log.next_checkpoint_no.load();

  const auto real_offset = compute_real_offset(log, start_lsn);

  bool write_from_log_buffer;

  auto write_size = compute_how_much_to_write(log, real_offset, buffer_size,
                                              write_from_log_buffer);

  if (write_size == 0) {
    start_next_file(log, start_lsn);
    return;
  }

  prepare_full_blocks(log, buffer, write_size, start_lsn, checkpoint_no);

  byte *write_buf;
  uint64_t written_ahead = 0;
  lsn_t lsn_advance = write_size;

  if (write_from_log_buffer) {
    /* We have at least one completed log block to write.
    We write completed blocks from the log buffer. Note,
    that possibly we do not write all completed blocks,
    because of write-ahead strategy (described earlier). */

    write_buf = buffer;

    LOG_SYNC_POINT("log_writer_before_write_from_log_buffer");

  } else {
    write_buf = log.write_ahead_buf;

    /* We write all the data directly from the write-ahead buffer,
    where we first need to copy the data. */
    copy_to_write_ahead_buffer(log, buffer, write_size, start_lsn,
                               checkpoint_no);

    if (!current_write_ahead_enough(log, real_offset, 1)) {
      written_ahead = prepare_for_write_ahead(log, real_offset, write_size);
    }
  }

  srv_stats.os_log_pending_writes.inc();

  /* Now, we know, that we are going to write completed
  blocks only (originally or copied and completed). */
  write_blocks(log, write_buf, write_size, real_offset);

  LOG_SYNC_POINT("log_writer_before_lsn_update");

  const lsn_t old_write_lsn = log.write_lsn.load();

  const lsn_t new_write_lsn = start_lsn + lsn_advance;
  ut_a(new_write_lsn > log.write_lsn.load());

  log.write_lsn.store(new_write_lsn);

  notify_about_advanced_write_lsn(log, old_write_lsn, new_write_lsn);

  srv_stats.os_log_pending_writes.dec();
  srv_stats.log_writes.inc();

  /* Write ahead is included in write_size. */
  ut_a(write_size >= written_ahead);
  srv_stats.os_log_written.add(write_size - written_ahead);
  MONITOR_INC_VALUE(MONITOR_LOG_PADDED, written_ahead);

  log.n_log_ios++;

  update_current_write_ahead(log, real_offset, write_size);
}

static void log_writer_write_buffer(log_t &log, lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));

  LOG_SYNC_POINT("log_writer_write_begin");

  const lsn_t last_write_lsn = log.write_lsn.load();

  ut_a(log_lsn_validate(last_write_lsn) ||
       last_write_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  ut_a(log_lsn_validate(next_write_lsn) ||
       next_write_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  ut_a(next_write_lsn - last_write_lsn <= log.buf_size);
  ut_a(next_write_lsn > last_write_lsn);

  size_t start_offset = last_write_lsn % log.buf_size;
  size_t end_offset = next_write_lsn % log.buf_size;

  if (start_offset >= end_offset) {
    ut_a(next_write_lsn - last_write_lsn >= log.buf_size - start_offset);

    end_offset = log.buf_size;
    next_write_lsn = last_write_lsn + (end_offset - start_offset);
  }
  ut_a(start_offset < end_offset);

  ut_a(end_offset % OS_FILE_LOG_BLOCK_SIZE == 0 ||
       end_offset % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

  int count = 0;

  /* Wait until there is free space in log files.*/

  lsn_t checkpoint_limited_lsn = LSN_MAX;
  lsn_t archiver_limited_lsn = LSN_MAX;
  lsn_t min_next_lsn = last_write_lsn + OS_FILE_LOG_BLOCK_SIZE;

  while (true) {
    lsn_t checkpoint_lsn = log.last_checkpoint_lsn.load();

    checkpoint_lsn =
        ut_uint64_align_down(checkpoint_lsn, OS_FILE_LOG_BLOCK_SIZE);

    ut_a(min_next_lsn > checkpoint_lsn);

    const lsn_t lsn_diff = min_next_lsn - checkpoint_lsn;

    if (lsn_diff <= log.lsn_capacity) {
      checkpoint_limited_lsn = checkpoint_lsn + log.lsn_capacity;
      break;
    }

    if (count >= 10) {
      ib::error(ER_IB_MSG_1234) << "Log writer overwriting data after"
                                   " checkpoint - waited too long (1 second),"
                                   " lag: "
                                << lsn_diff
                                << " bytes,"
                                   " checkpoint LSN: "
                                << checkpoint_lsn;

      checkpoint_limited_lsn = min_next_lsn;
      break;
    }

    /* We would overwrite data at checkpoint_lsn. */
    log_writer_mutex_exit(log);

    /* We don't want to ask for sync checkpoint, because it
    is possible, that the oldest dirty page is latched and
    user thread, which keeps the latch, is waiting for space
    in log buffer (for log_writer writing to disk). In such
    case it would be deadlock (we can't flush the latched
    page and advance the checkpoint). We only ask for the
    checkpoint, and wait for some time. */
    log_request_checkpoint(log, false);

    ib::warn(ER_IB_MSG_1235) << "Log writer is waiting for checkpointer to"
                                " to catch up lag: "
                             << lsn_diff
                             << " bytes,"
                                " checkpoint LSN: "
                             << checkpoint_lsn;

    count++;
    os_thread_sleep(100000); /* 100ms */

    log_writer_mutex_enter(log);

    if (log.write_lsn.load() > last_write_lsn) {
      return;
    }
  }

  LOG_SYNC_POINT("log_writer_after_checkpoint_check");

  count = 0;

  while (arch_log_sys != nullptr && arch_log_sys->is_active()) {
    lsn_t archiver_lsn = arch_log_sys->get_archived_lsn();

    archiver_lsn = ut_uint64_align_down(archiver_lsn, OS_FILE_LOG_BLOCK_SIZE);

    ut_a(min_next_lsn >= archiver_lsn);

    const lsn_t lsn_diff = min_next_lsn - archiver_lsn;

    if (lsn_diff <= log.lsn_capacity) {
      /* Between archive_lsn and next_write_lsn there is less
      bytes than capacity of all log files. Writing log up to
      next_write_lsn will not overwrite data at archiver_lsn.
      There is no need to wait for the archiver. */

      archiver_limited_lsn = archiver_lsn + log.lsn_capacity;
      break;
    }

    if (count >= 10) {
      ib::error(ER_IB_MSG_1236) << "Log writer overwriting data to"
                                   " archive - waited too long (1 second),"
                                   " lag: "
                                << lsn_diff
                                << " bytes,"
                                   " archiver LSN: "
                                << archiver_lsn;

      archiver_limited_lsn = min_next_lsn;
      break;
    }

    os_event_set(archiver_thread_event);

    log_writer_mutex_exit(log);

    ib::warn(ER_IB_MSG_1237) << "Log writer is waiting for archiver to"
                                " to catch up lag: "
                             << lsn_diff
                             << " bytes,"
                                " archiver LSN: "
                             << archiver_lsn;

    count++;
    os_thread_sleep(100000); /* 100ms */

    log_writer_mutex_enter(log);

    if (log.write_lsn.load() > last_write_lsn) {
      return;
    }
  }

  LOG_SYNC_POINT("log_writer_after_archiver_check");

  ut_a(checkpoint_limited_lsn < LSN_MAX);

  ut_ad(log_writer_mutex_own(log));

  ut_a(archiver_limited_lsn < LSN_MAX || arch_log_sys == nullptr ||
       !arch_log_sys->is_active());

  const lsn_t limit_for_next_write_lsn =
      std::min(checkpoint_limited_lsn, archiver_limited_lsn);

  if (limit_for_next_write_lsn < next_write_lsn) {
    end_offset -= next_write_lsn - limit_for_next_write_lsn;
    next_write_lsn = limit_for_next_write_lsn;

    ut_a(end_offset > start_offset);
    ut_a(end_offset % OS_FILE_LOG_BLOCK_SIZE == 0 ||
         end_offset % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

    ut_a(log_lsn_validate(next_write_lsn) ||
         next_write_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  }

  DBUG_PRINT("ib_log",
             ("write " LSN_PF " to " LSN_PF, last_write_lsn, next_write_lsn));

  byte *buf_begin =
      log.buf + ut_uint64_align_down(start_offset, OS_FILE_LOG_BLOCK_SIZE);

  byte *buf_end = log.buf + end_offset;

  /* Do the write to the log files */
  log_files_write_buffer(
      log, buf_begin, buf_end - buf_begin,
      ut_uint64_align_down(last_write_lsn, OS_FILE_LOG_BLOCK_SIZE));

  LOG_SYNC_POINT("log_writer_before_limits_update");

  log_update_limits(log);

  LOG_SYNC_POINT("log_writer_write_end");

  if (srv_flush_log_at_trx_commit == 1) {
    os_event_set(log.flusher_event);
  }

  if (arch_log_sys && arch_log_sys->is_active()) {
    os_event_set(archiver_thread_event);
  }
}

void log_writer(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  ut_a(log_ptr->writer_thread_alive.load());

  log_t &log = *log_ptr;
  lsn_t ready_lsn = 0;

  log_writer_mutex_enter(log);

  for (uint64_t step = 0;; ++step) {
    bool released = false;

    auto stop_condition = [&ready_lsn, &log, &released](bool wait) {

      if (released) {
        log_writer_mutex_enter(log);
        released = false;
      }

      /* Advance lsn up to which data is ready in log buffer. */
      (void)log_advance_ready_for_write_lsn(log);

      ready_lsn = log_buffer_ready_for_write_lsn(log);

      /* Wait until any of following conditions holds:
              1) There is some unwritten data in log buffer
              2) We should close threads. */

      if (log.write_lsn.load() < ready_lsn || log.should_stop_threads.load()) {
        return (true);
      }

      if (wait) {
        log_writer_mutex_exit(log);
        released = true;
      }

      return (false);
    };

    auto max_spins = srv_log_writer_spin_delay;

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm) {
      max_spins = 0;
    }

    const auto wait_stats = os_event_wait_for(
        log.writer_event, max_spins, srv_log_writer_timeout, stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_WRITER_, wait_stats);

    /* Do the actual work. */
    if (log.write_lsn.load() < ready_lsn) {
      log_writer_write_buffer(log, ready_lsn);

      if (step % 1024 == 0) {
        log_writer_mutex_exit(log);

        os_thread_sleep(0);

        log_writer_mutex_enter(log);
      }

    } else {
      if (log.should_stop_threads.load()) {
        /* When log threads are stopped, we must first
        ensure that all writes to log buffer have been
        finished and only then we are allowed to set
        the should_stop_threads to true. */

        if (!log_advance_ready_for_write_lsn(log)) {
          break;
        }

        ready_lsn = log_buffer_ready_for_write_lsn(log);
      }
    }
  }

  log.writer_thread_alive.store(false);

  log_writer_mutex_exit(log);
}

/* @} */

/**************************************************/ /**

 @name Log flusher thread

 *******************************************************/

/* @{ */

static void log_flush_update_stats(log_t &log) {
  ut_ad(log_flusher_mutex_own(log));

  /* Note that this code is inspired by similar logic in buf0flu.cc */

  static uint64_t iterations = 0;
  static Log_clock_point prev_time;
  static lsn_t prev_lsn;
  static lsn_t lsn_avg_rate = 0;
  static Log_clock::duration fsync_max_time;
  static Log_clock::duration fsync_total_time;

  /* Calculate time of last fsync and update related counters. */

  Log_clock::duration fsync_time;

  fsync_time = log.last_flush_end_time - log.last_flush_start_time;

  fsync_max_time = std::max(fsync_max_time, fsync_time);

  fsync_total_time += fsync_time;

  MONITOR_INC_VALUE(
      MONITOR_LOG_FLUSH_TOTAL_TIME,
      std::chrono::duration_cast<std::chrono::milliseconds>(fsync_time)
          .count());

  /* Calculate time elapsed since start of last sample. */

  if (prev_time == Log_clock_point{}) {
    prev_time = log.last_flush_start_time;
  }

  const Log_clock_point curr_time = log.last_flush_end_time;

  auto time_elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(curr_time - prev_time)
          .count();

  ut_a(time_elapsed >= 0);

  if (++iterations >= srv_flushing_avg_loops ||
      time_elapsed >= static_cast<double>(srv_flushing_avg_loops)) {
    if (time_elapsed < 1) {
      time_elapsed = 1;
    }

    const lsn_t curr_lsn = log.flushed_to_disk_lsn.load();

    const lsn_t lsn_rate = static_cast<lsn_t>(
        static_cast<double>(curr_lsn - prev_lsn) / time_elapsed);

    lsn_avg_rate = (lsn_avg_rate + lsn_rate) / 2;

    MONITOR_SET(MONITOR_LOG_FLUSH_LSN_AVG_RATE, lsn_avg_rate);

    MONITOR_SET(
        MONITOR_LOG_FLUSH_MAX_TIME,
        std::chrono::duration_cast<std::chrono::microseconds>(fsync_max_time)
            .count());

    log.flush_avg_time =
        std::chrono::duration_cast<std::chrono::microseconds>(fsync_total_time)
            .count() *
        1.0 / iterations;

    MONITOR_SET(MONITOR_LOG_FLUSH_AVG_TIME, log.flush_avg_time);

    fsync_max_time = Log_clock::duration{};
    fsync_total_time = Log_clock::duration{};
    iterations = 0;
    prev_time = curr_time;
    prev_lsn = curr_lsn;
  }
}

static void log_flush_low(log_t &log) {
  ut_ad(log_flusher_mutex_own(log));

#ifndef _WIN32
  bool do_flush = srv_unix_file_flush_method != SRV_UNIX_O_DSYNC;
#else
  bool do_flush = true;
#endif

  os_event_reset(log.flusher_event);

  log.last_flush_start_time = Log_clock::now();

  const lsn_t last_flush_lsn = log.flushed_to_disk_lsn.load();

  const lsn_t flush_up_to_lsn = log.write_lsn.load();

  ut_a(flush_up_to_lsn > last_flush_lsn);

  if (do_flush) {
    LOG_SYNC_POINT("log_flush_before_fsync");

    fil_flush_file_redo();
  }

  log.last_flush_end_time = Log_clock::now();

  LOG_SYNC_POINT("log_flush_before_flushed_to_disk_lsn");

  log.flushed_to_disk_lsn.store(flush_up_to_lsn);

  /* Notify other thread(s). */

  DBUG_PRINT("ib_log", ("Flushed to disk up to " LSN_PF, flush_up_to_lsn));

  const auto first_slot =
      last_flush_lsn / OS_FILE_LOG_BLOCK_SIZE & (log.flush_events_size - 1);

  const auto last_slot = (flush_up_to_lsn - 1) / OS_FILE_LOG_BLOCK_SIZE &
                         (log.flush_events_size - 1);

  if (first_slot == last_slot) {
    LOG_SYNC_POINT("log_flush_before_users_notify");
    os_event_set(log.flush_events[first_slot]);
  } else {
    LOG_SYNC_POINT("log_flush_before_notifier_notify");
    os_event_set(log.flush_notifier_event);
  }

  /* Update stats. */

  log_flush_update_stats(log);
}

void log_flusher(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  ut_a(log_ptr->flusher_thread_alive.load());

  log_t &log = *log_ptr;

  log_flusher_mutex_enter(log);

  for (uint64_t step = 0; log.writer_thread_alive.load(); ++step) {
    bool released = false;

    auto stop_condition = [&log, &released, step](bool wait) {

      if (released) {
        log_flusher_mutex_enter(log);
        released = false;
      }

      LOG_SYNC_POINT("log_flusher_before_should_flush");

      const lsn_t last_flush_lsn = log.flushed_to_disk_lsn.load();

      ut_a(last_flush_lsn <= log.write_lsn.load());

      if (last_flush_lsn < log.write_lsn.load()) {
        /* Flush and stop waiting. */
        log_flush_low(log);

        if (step % 1024 == 0) {
          log_flusher_mutex_exit(log);

          os_thread_sleep(0);

          log_flusher_mutex_enter(log);
        }

        return (true);
      }

      /* Stop waiting if writer thread is dead. */
      if (!log.writer_thread_alive.load()) {
        return (true);
      }

      if (wait) {
        log_flusher_mutex_exit(log);
        released = true;
      }

      return (false);
    };

    auto max_spins = srv_log_flusher_spin_delay;

    if (srv_flush_log_at_trx_commit != 1) {
      const auto time_elapsed = Log_clock::now() - log.last_flush_start_time;

      using us = std::chrono::microseconds;

      const auto time_elapsed_us =
          std::chrono::duration_cast<us>(time_elapsed).count();

      const auto flush_every = srv_flush_log_at_timeout;

      const auto flush_every_us = 1000000LL * flush_every;

      if (time_elapsed_us < flush_every_us) {
        log_flusher_mutex_exit(log);

        os_event_wait_time_low(log.flusher_event,
                               flush_every_us - time_elapsed_us, 0);

        log_flusher_mutex_enter(log);
      }

      max_spins = 0;
    }

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm) {
      max_spins = 0;
    }

    const auto wait_stats = os_event_wait_for(
        log.flusher_event, max_spins, srv_log_flusher_timeout, stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_FLUSHER_, wait_stats);
  }

  if (log.write_lsn.load() > log.flushed_to_disk_lsn.load()) {
    log_flush_low(log);
  }

  log.flusher_thread_alive.store(false);

  os_event_set(log.flush_notifier_event);

  log_flusher_mutex_exit(log);
}

/* @} */

/**************************************************/ /**

 @name Log write_notifier thread

 *******************************************************/

/* @{ */

void log_write_notifier(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  ut_a(log_ptr->write_notifier_thread_alive.load());

  log_t &log = *log_ptr;
  lsn_t lsn = log.write_lsn.load() + 1;

  log_write_notifier_mutex_enter(log);

  for (uint64_t step = 0;; ++step) {
    if (!log.writer_thread_alive.load()) {
      if (lsn > log.write_lsn.load()) {
        ut_a(lsn == log.write_lsn.load() + 1);
        break;
      }
    }

    LOG_SYNC_POINT("log_write_notifier_before_check");

    bool released = false;

    auto stop_condition = [&log, lsn, &released](bool wait) {

      LOG_SYNC_POINT("log_write_notifier_after_event_reset");
      if (released) {
        log_write_notifier_mutex_enter(log);
        released = false;
      }

      LOG_SYNC_POINT("log_write_notifier_before_check");

      if (log.write_lsn.load() >= lsn || !log.writer_thread_alive.load()) {
        return (true);
      }

      if (wait) {
        log_write_notifier_mutex_exit(log);
        released = true;
      }
      LOG_SYNC_POINT("log_write_notifier_before_wait");

      return (false);
    };

    auto max_spins = srv_log_write_notifier_spin_delay;

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm) {
      max_spins = 0;
    }

    const auto wait_stats =
        os_event_wait_for(log.write_notifier_event, max_spins,
                          srv_log_write_notifier_timeout, stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_WRITE_NOTIFIER_, wait_stats);

    LOG_SYNC_POINT("log_write_notifier_before_write_lsn");

    const lsn_t write_lsn = log.write_lsn.load();

    const lsn_t notified_up_to_lsn =
        ut_uint64_align_up(write_lsn, OS_FILE_LOG_BLOCK_SIZE);

    while (lsn <= notified_up_to_lsn) {
      const auto slot =
          (lsn - 1) / OS_FILE_LOG_BLOCK_SIZE & (log.write_events_size - 1);

      lsn += OS_FILE_LOG_BLOCK_SIZE;

      LOG_SYNC_POINT("log_write_notifier_before_notify");

      os_event_set(log.write_events[slot]);
    }

    lsn = write_lsn + 1;

    if (step % 1024 == 0) {
      log_write_notifier_mutex_exit(log);

      os_thread_sleep(0);

      log_write_notifier_mutex_enter(log);
    }
  }

  log_ptr->write_notifier_thread_alive.store(false);

  log_write_notifier_mutex_exit(log);
}

/* @} */

/**************************************************/ /**

 @name Log flush_notifier thread

 *******************************************************/

/* @{ */

void log_flush_notifier(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  ut_a(log_ptr->flush_notifier_thread_alive.load());

  log_t &log = *log_ptr;
  lsn_t lsn = log.flushed_to_disk_lsn.load() + 1;

  log_flush_notifier_mutex_enter(log);

  for (uint64_t step = 0;; ++step) {
    if (!log.flusher_thread_alive.load()) {
      if (lsn > log.flushed_to_disk_lsn.load()) {
        ut_a(lsn == log.flushed_to_disk_lsn.load() + 1);
        break;
      }
    }

    LOG_SYNC_POINT("log_flush_notifier_before_check");

    bool released = false;

    auto stop_condition = [&log, lsn, &released](bool wait) {

      LOG_SYNC_POINT("log_flush_notifier_after_event_reset");
      if (released) {
        log_flush_notifier_mutex_enter(log);
        released = false;
      }

      LOG_SYNC_POINT("log_flush_notifier_before_check");

      if (log.flushed_to_disk_lsn.load() >= lsn ||
          !log.flusher_thread_alive.load()) {
        return (true);
      }

      if (wait) {
        log_flush_notifier_mutex_exit(log);
        released = true;
      }
      LOG_SYNC_POINT("log_flush_notifier_before_wait");

      return (false);
    };

    auto max_spins = srv_log_flush_notifier_spin_delay;

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm) {
      max_spins = 0;
    }

    const auto wait_stats =
        os_event_wait_for(log.flush_notifier_event, max_spins,
                          srv_log_flush_notifier_timeout, stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_FLUSH_NOTIFIER_, wait_stats);

    LOG_SYNC_POINT("log_flush_notifier_before_flushed_to_disk_lsn");

    const lsn_t flush_lsn = log.flushed_to_disk_lsn.load();

    const lsn_t notified_up_to_lsn =
        ut_uint64_align_up(flush_lsn, OS_FILE_LOG_BLOCK_SIZE);

    while (lsn <= notified_up_to_lsn) {
      const auto slot =
          (lsn - 1) / OS_FILE_LOG_BLOCK_SIZE & (log.flush_events_size - 1);

      lsn += OS_FILE_LOG_BLOCK_SIZE;

      LOG_SYNC_POINT("log_flush_notifier_before_notify");

      os_event_set(log.flush_events[slot]);
    }

    lsn = flush_lsn + 1;

    if (step % 1024 == 0) {
      log_flush_notifier_mutex_exit(log);

      os_thread_sleep(0);

      log_flush_notifier_mutex_enter(log);
    }
  }

  log_ptr->flush_notifier_thread_alive.store(false);

  log_flush_notifier_mutex_exit(log);
}

/* @} */

/**************************************************/ /**

 @name Log closer thread

 *******************************************************/

/* @{ */

void log_closer(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);
  ut_a(log_ptr->closer_thread_alive.load());

  log_t &log = *log_ptr;
  lsn_t end_lsn = 0;

  log_closer_mutex_enter(log);

  for (uint64_t step = 0;; ++step) {
    bool released = false;

    auto stop_condition = [&log, &released, step](bool wait) {

      if (released) {
        log_closer_mutex_enter(log);
        released = false;
      }

      /* Advance lsn up to which all the dirty pages have
      been added to flush lists. */

      if (log_advance_dirty_pages_added_up_to_lsn(log)) {
        if (step % 1024 == 0) {
          log_closer_mutex_exit(log);
          os_thread_sleep(0);
          log_closer_mutex_enter(log);
        }
        return (true);
      }

      if (log.should_stop_threads.load()) {
        return (true);
      }

      if (wait) {
        log_closer_mutex_exit(log);
        released = true;
      }
      return (false);
    };

    auto max_spins = srv_log_closer_spin_delay;

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm) {
      max_spins = 0;
    }

    ut_wait_for(max_spins, srv_log_closer_timeout, stop_condition);

    /* Check if we should close the thread. */
    if (log.should_stop_threads.load() && !log.flusher_thread_alive.load() &&
        !log.writer_thread_alive.load()) {
      end_lsn = log.write_lsn.load();

      ut_a(log_lsn_validate(end_lsn));
      ut_a(end_lsn == log.flushed_to_disk_lsn.load());
      ut_a(end_lsn == log_buffer_ready_for_write_lsn(log));

      ut_a(end_lsn >= log_buffer_dirty_pages_added_up_to_lsn(log));

      if (log_buffer_dirty_pages_added_up_to_lsn(log) == end_lsn) {
        /* All confirmed reservations have been written
        to redo and all dirty pages related to those
        writes have been added to flush lists.

        However, there could be user threads, which are
        in the middle of log_buffer_reserve(), reserved
        range of sn values, but could not confirm.

        Note that because log_writer is already not alive,
        the only possible reason guaranteed by its death,
        is that there is x-lock at end_lsn, in which case
        end_lsn separates two regions in log buffer:
        completely full and completely empty. */
        const lsn_t ready_lsn = log_buffer_ready_for_write_lsn(log);

        const lsn_t current_lsn = log_get_lsn(log);

        if (current_lsn > ready_lsn) {
          log.recent_written.validate_no_links(ready_lsn, current_lsn);

          log.recent_closed.validate_no_links(ready_lsn, current_lsn);
        }

        break;
      }

      /* We need to wait until remaining dirty pages
      have been added. */
    }
  }

  log.closer_thread_alive.store(false);

  log_closer_mutex_exit(log);
}

/* @} */

/**************************************************/ /**

 @name Log files encryption

 *******************************************************/

/* @{ */

bool log_read_encryption() {
  space_id_t log_space_id = dict_sys_t::s_log_space_first_id;
  const page_id_t page_id(log_space_id, 0);
  byte *log_block_buf_ptr;
  byte *log_block_buf;
  byte key[ENCRYPTION_KEY_LEN];
  byte iv[ENCRYPTION_KEY_LEN];
  fil_space_t *space = fil_space_get(log_space_id);
  dberr_t err;

  log_block_buf_ptr =
      static_cast<byte *>(ut_malloc_nokey(2 * OS_FILE_LOG_BLOCK_SIZE));
  memset(log_block_buf_ptr, 0, 2 * OS_FILE_LOG_BLOCK_SIZE);
  log_block_buf =
      static_cast<byte *>(ut_align(log_block_buf_ptr, OS_FILE_LOG_BLOCK_SIZE));

  err = fil_redo_io(IORequestLogRead, page_id, univ_page_size,
                    LOG_CHECKPOINT_1 + OS_FILE_LOG_BLOCK_SIZE,
                    OS_FILE_LOG_BLOCK_SIZE, log_block_buf);

  ut_a(err == DB_SUCCESS);

  if (memcmp(log_block_buf + LOG_HEADER_CREATOR_END, ENCRYPTION_KEY_MAGIC_V3,
             ENCRYPTION_MAGIC_SIZE) == 0) {
    /* Make sure the keyring is loaded. */
    if (!Encryption::check_keyring()) {
      ut_free(log_block_buf_ptr);
      ib::error(ER_IB_MSG_1238) << "Redo log was encrypted,"
                                << " but keyring plugin is not loaded.";
      return (false);
    }

    if (Encryption::decode_encryption_info(
            key, iv, log_block_buf + LOG_HEADER_CREATOR_END)) {
      /* If redo log encryption is enabled, set the
      space flag. Otherwise, we just fill the encryption
      information to space object for decrypting old
      redo log blocks. */
      space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
      err = fil_set_encryption(space->id, Encryption::AES, key, iv);

      if (err == DB_SUCCESS) {
        ut_free(log_block_buf_ptr);
        ib::info(ER_IB_MSG_1239) << "Read redo log encryption"
                                 << " metadata successful.";
        return (true);
      } else {
        ut_free(log_block_buf_ptr);
        ib::error(ER_IB_MSG_1240) << "Can't set redo log tablespace"
                                  << " encryption metadata.";
        return (false);
      }
    } else {
      ut_free(log_block_buf_ptr);
      ib::error(ER_IB_MSG_1241) << "Cannot read the encryption"
                                   " information in log file header, please"
                                   " check if keyring plugin loaded and"
                                   " the key file exists.";
      return (false);
    }
  }

  ut_free(log_block_buf_ptr);
  return (true);
}

static bool log_file_header_fill_encryption(byte *buf, byte *key, byte *iv,
                                            bool is_boot) {
  byte encryption_info[ENCRYPTION_INFO_SIZE];

  if (!Encryption::fill_encryption_info(key, iv, encryption_info, is_boot)) {
    return (false);
  }

  ut_a(LOG_HEADER_CREATOR_END + ENCRYPTION_INFO_SIZE < OS_FILE_LOG_BLOCK_SIZE);

  memcpy(buf + LOG_HEADER_CREATOR_END, encryption_info, ENCRYPTION_INFO_SIZE);

  return (true);
}

bool log_write_encryption(byte *key, byte *iv, bool is_boot) {
  const page_id_t page_id{dict_sys_t::s_log_space_first_id, 0};
  byte *log_block_buf_ptr;
  byte *log_block_buf;

  log_block_buf_ptr =
      static_cast<byte *>(ut_malloc_nokey(2 * OS_FILE_LOG_BLOCK_SIZE));
  memset(log_block_buf_ptr, 0, 2 * OS_FILE_LOG_BLOCK_SIZE);
  log_block_buf =
      static_cast<byte *>(ut_align(log_block_buf_ptr, OS_FILE_LOG_BLOCK_SIZE));

  if (key == NULL && iv == NULL) {
    fil_space_t *space = fil_space_get(dict_sys_t::s_log_space_first_id);

    key = space->encryption_key;
    iv = space->encryption_iv;
  }

  if (!log_file_header_fill_encryption(log_block_buf, key, iv, is_boot)) {
    ut_free(log_block_buf_ptr);
    return (false);
  }

  auto err = fil_redo_io(IORequestLogWrite, page_id, univ_page_size,
                         LOG_CHECKPOINT_1 + OS_FILE_LOG_BLOCK_SIZE,
                         OS_FILE_LOG_BLOCK_SIZE, log_block_buf);

  ut_a(err == DB_SUCCESS);

  ut_free(log_block_buf_ptr);
  return (true);
}

bool log_rotate_encryption() {
  fil_space_t *space = fil_space_get(dict_sys_t::s_log_space_first_id);

  if (!FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    return (true);
  }

  /* Rotate log tablespace */
  return (log_write_encryption(nullptr, nullptr, false));
}

void log_enable_encryption_if_set() {
  fil_space_t *space = fil_space_get(dict_sys_t::s_log_space_first_id);

  if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
    return;
  }

  /* Check encryption for redo log is enabled or not. If it's
  enabled, we will start to encrypt the redo log block from now on.
  Note: We need the server_uuid initialized, otherwise, the keyname will
  not contains server uuid. */
  if (srv_redo_log_encrypt && !FSP_FLAGS_GET_ENCRYPTION(space->flags) &&
      strlen(server_uuid) > 0) {
    dberr_t err;
    byte key[ENCRYPTION_KEY_LEN];
    byte iv[ENCRYPTION_KEY_LEN];

    if (srv_read_only_mode) {
      srv_redo_log_encrypt = false;
      ib::error(ER_IB_MSG_1242) << "Can't set redo log tablespace to be"
                                << " encrypted in read-only mode.";
      return;
    }

    Encryption::random_value(key);
    Encryption::random_value(iv);
    if (!log_write_encryption(key, iv, false)) {
      srv_redo_log_encrypt = false;
      ib::error(ER_IB_MSG_1243) << "Can't set redo log"
                                << " tablespace to be"
                                << " encrypted.";
    } else {
      space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
      err = fil_set_encryption(space->id, Encryption::AES, key, iv);
      if (err != DB_SUCCESS) {
        srv_redo_log_encrypt = false;
        ib::warn(ER_IB_MSG_1244) << "Can't set redo log"
                                 << " tablespace to be"
                                 << " encrypted.";
      } else {
        ib::info(ER_IB_MSG_1245) << "Redo log encryption is"
                                 << " enabled.";
      }
    }
  }

  /* If the redo log space is using default key, rotate it.
  We also need the server_uuid initialized. */
  if (space->encryption_type != Encryption::NONE &&
      Encryption::s_master_key_id == ENCRYPTION_DEFAULT_MASTER_KEY_ID &&
      !srv_read_only_mode && strlen(server_uuid) > 0) {
    ut_a(FSP_FLAGS_GET_ENCRYPTION(space->flags));

    log_write_encryption(nullptr, nullptr, false);
  }
}

  /* @} */

#endif /* !UNIV_HOTBACKUP */
