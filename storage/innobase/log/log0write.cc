/*****************************************************************************

Copyright (c) 1995, 2022, Oracle and/or its affiliates.
Copyright (c) 2009, Google Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file log/log0write.cc

 Redo log writing and flushing, including functions for:
         1. Waiting for the log written / flushed up to provided lsn.
         2. Redo log write threads: log_writer, log_flusher,
            log_write_notifier, log_flush_notifier.

 @author Paweł Olchawa

 *******************************************************/

#ifndef UNIV_HOTBACKUP

/* std::memory_order_* */
#include <atomic>

/* thd_wait_begin() / thd_wait_end() */
#include <mysql/service_thd_wait.h>

/* std::memcpy, std::memset */
#include <cstring>

/* arch_log_sys */
#include "arch0arch.h"

/* page_id_t */
#include "buf0types.h"

/* log_update_buf_limit, ... */
#include "log0buf.h"

/* log_request_checkpoint */
#include "log0chkp.h"

/* Log_files_capacity::soft_logical_capacity, ... */
#include "log0files_capacity.h"

/* log_files_produce_file, ... */
#include "log0files_governor.h"

/* log_limits_mutex_enter, ... */
#include "log0log.h"

/* redo_log_archive_produce */
#include "log0meb.h"

/* recv_no_ibuf_operations */
#include "log0recv.h"

/* log_t::X */
#include "log0sys.h"

/* log_sync_point */
#include "log0test.h"

/* Log_file::offset, OS_FILE_LOG_BLOCK_SIZE */
#include "log0types.h"

/* log_writer_mutex */
#include "log0write.h"

/* create_internal_thd, destroy_internal_thd */
#include "sql/sql_thd_internal_api.h"

/* MONITOR_INC, ... */
#include "srv0mon.h"

/* srv_read_only_mode */
#include "srv0srv.h"

/* ut_uint64_align_down */
#include "ut0byte.h"

// clang-format off
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

   @diafile storage/innobase/log/recent_written_buffer.dia "Example of links in the recent written buffer"

   @note The log buffer has no holes up to the _log.buf_ready_for_write_lsn_
   (all concurrent writes for smaller lsn have been finished).

   If there were no links to traverse, _log.buf_ready_for_write_lsn_ was not
   advanced and the log writer thread needs to wait. In such case it first
   uses spin delay and afterwards switches to wait on the _writer_event_.


 -# Prepare log blocks for writing - update their headers and footers.

   The log writer thread detects completed log blocks in the log buffer.
   Such log blocks will not receive any more writes. Hence their headers
   and footers could be easily updated (e.g. checksum is calculated).

   @diafile storage/innobase/log/log_writer_complete_blocks.dia "Complete blocks are detected and written"

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

   @diafile storage/innobase/log/log_writer_incomplete_block.dia "Incomplete block is copied"

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

   After doing single write (single log_data_blocks_write()), the log writer
   thread updates @ref subsect_redo_log_write_lsn and fallbacks to its main
   loop. That's because a lot more data could be prepared in meantime, as
   the write operation could take significant time.

   That's why the general rule is that after doing log_data_blocks_write(),
   we need to update @ref subsect_redo_log_buf_ready_for_write_lsn before
   making next decisions on how much to write within next such call.


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

 @diafile storage/innobase/log/log_notifier_notifications.dia "Notifications executed on slots"

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
 that log files are used in circular manner (lsn modulo size of redo log files,
 when size is calculated except the log file headers).

 The log file names are: _#ib_redo0_, _#ib_redo1_, ... and they are stored in
 subdirectory #innodb_redo, which is located inside the directory specified by
 the innodb_log_group_home_dir (or in the datadir if not specified).

 Whenever a new log file is being created, it is created first with the _tmp
 suffix in its name. When the file is prepared, it becomes renamed (the suffix
 is removed from the name).

 When a new data directory is being initialized, all log files that are being
 created, have LOG_HEADER_FLAG_NOT_INITIALIZED flag enabled in the log_flags
 field in the header. After the data directory is initialized, this flag is
 disabled (file header is re-flushed for the newest log file then).

 File header contains the log_uuid field. It is a randomly chosen value when
 the data directory is being initialized. It is used to detect situation,
 in which user mixed log files from different data directories.

 File header contains also start_lsn - this is start_lsn of the first log block
 within that file.

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
     Each log file has its own header. Checkpoints defined in checkpoint headers
     always refer to LSN values within that file. During the recovery one should
     find the file with the newest checkpoint.

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

     - @anchor a_redo_log_block_epoch_no epoch_no

       Log epoch number. Set by the log writer thread just before a write
       starts for the block. For details @see LOG_BLOCK_HDR_EPOCH_NO.

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
// clang-format on

/** Writes a given fragment of the log buffer to the current redo log file,
unless the file is full, in which case a new file is produced and function
exits (note, that the new log file's header is flushed in such case).
After data to the current log file has been written (log_data_blocks_write()),
the log.write_lsn is advanced accordingly to the number of written bytes,
which might be smaller than the requested number of bytes to write.
That's because this function exits after doing a single write operation.
That's because it might make sense to advance the lsn up to which data
is ready in the log buffer (for writing), before making decision about
next write (e.g. then the next write could be done for full blocks only).
@param[in]  log            redo log
@param[in]  buffer         the beginning of first log block to write
@param[in]  buffer_size    number of bytes to write since 'buffer'
@param[in]  start_lsn   lsn  corresponding to first block start
@return DB_SUCCESS or error */
static dberr_t log_write_buffer(log_t &log, byte *buffer, size_t buffer_size,
                                lsn_t start_lsn);

/** Called when the redo log writer enters the extra_margin.
Requirement: log.writer_mutex acquired and log.m_write_inside_extra_margin
being false, before calling this function.
@param[in,out]  log   redo log */
static void log_writer_enter_extra_margin(log_t &log);

/** Called when the redo log writer exits the extra_margin.
Requirement: log.writer_mutex acquired and log.m_write_inside_extra_margin
being true, before calling this function.
@param[in,out]  log   redo log */
static void log_writer_exit_extra_margin(log_t &log);

/* Waits until there is free space in log files for log_writer to proceed.
@param[in]  log             redo log
@param[in]  last_write_lsn  previous log.write_lsn
@param[in]  next_write_lsn  next log.write_lsn
@return lsn up to which possible write is limited */
static lsn_t log_writer_wait_on_checkpoint(log_t &log, lsn_t last_write_lsn,
                                           lsn_t next_write_lsn);

/* Waits until the archiver has archived enough for log_writer to proceed
or until the archiver becomes aborted.
@param[in]  log             redo log
@param[in]  next_write_lsn  next log.write_lsn */
static void log_writer_wait_on_archiver(log_t &log, lsn_t next_write_lsn);

/** Called after a write to the redo log file failed. If the reason was not
related to missing free space or busy file-lock, emits fatal error.
@param[in,out]  log   redo log
@param[in]      err   error code (non-zero) */
static void log_writer_write_failed(log_t &log, dberr_t err);

/** Writes fragment of the log buffer, not further than up to provided lsn.
Stops after the first call to log_data_blocks_write() or after producing
a new log file. If some data was written, the log.write_lsn is advanced.
For more details see @see log_write_buffer().
@param[in]  log             redo log
@param[in]  next_write_lsn  write up to this lsn value */
static void log_writer_write_buffer(log_t &log, lsn_t next_write_lsn);

/** Executes a synchronous flush of the log files (doing fsyncs).
Advances log.flushed_to_disk_lsn and notifies log flush_notifier thread.
Note: if only a single log block was flushed to disk, user threads
waiting for lsns within the block are notified directly from here,
and log flush_notifier thread is not notified! (optimization)
@param[in,out]  log   redo log */
static void log_flush_low(log_t &log);

/**************************************************/ /**

 @name Waiting for redo log written or flushed up to lsn

 *******************************************************/

/** @{ */

/** Computes index of a slot (in array of "wait events"), which should
be used when waiting until redo reached provided lsn.
@param[in]  lsn         lsn up to which waiting takes place
@param[in]  events_n    size of the array (number of slots)
@return  index of the slot (integer in range 0 .. events_n-1) */
static inline size_t log_compute_wait_event_slot(lsn_t lsn, size_t events_n) {
  /* We subtract one from lsn, because it is better to assign right boundary
  of a log block to the slot representing the given block. If write or flush
  happens within block, all threads interested in some lsn in that block should
  be notified.

  Suppose lsn % 512 == 0 (this is the only case for which subtracting 1 makes
  any difference here). All threads waiting for some lsn in (lsn-1)/512 must
  be notified anyway (previous lsn was smaller so the block wasn't closed yet).
  On the other hand, it is useless to notify threads waiting for lsn values
  within lsn / 512, because these are larger lsn values, except threads which
  are waiting exactly at this lsn. That's why this group of threads it's better
  to move to the slot corresponding to (lsn-1)/512 and then we could avoid
  waking up those in lsn/512. Note that this scenario (lsn % 512 == 0) happens
  often because our strategy is to prefer writes of full log blocks only,
  leaving the incomplete last block for next write (unless there are no full
  blocks). */
  return ((lsn - 1) / OS_FILE_LOG_BLOCK_SIZE) & (events_n - 1);
}

/** Computes index of a slot (in array of "wait events"), which should
be used when waiting in log.write_events (for redo written up to lsn).
@param[in]  log  redo log
@param[in]  lsn  lsn up to which waiting (for log.write_lsn)
@return  index of the slot (integer in range 0 .. log.write_events_size-1) */
static inline size_t log_compute_write_event_slot(const log_t &log, lsn_t lsn) {
  return log_compute_wait_event_slot(lsn, log.write_events_size);
}

/** Computes index of a slot (in array of "wait events"), which should
be used when waiting in log.flush_events (for redo flushed up to lsn).
@param[in]  log  redo log
@param[in]  lsn  lsn up to which waiting (for log.flushed_to_disk_lsn)
@return  index of the slot (integer in range 0 .. log.flush_events_size-1) */
static inline size_t log_compute_flush_event_slot(const log_t &log, lsn_t lsn) {
  return log_compute_wait_event_slot(lsn, log.flush_events_size);
}

/** Computes maximum number of spin rounds which should be used when waiting
in user thread (for written or flushed redo) or 0 if busy waiting should not
be used at all.
@param[in]  min_non_zero_value    minimum allowed value (unless 0 is returned)
@return maximum number of spin rounds or 0 */
static inline uint64_t log_max_spins_when_waiting_in_user_thread(
    uint64_t min_non_zero_value) {
  uint64_t max_spins;

  /* Get current cpu usage. */
  const double cpu = srv_cpu_usage.utime_pct;

  /* Get high-watermark - when cpu usage is higher, don't spin! */
  const uint32_t hwm = srv_log_spin_cpu_pct_hwm;

  if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm || cpu >= hwm) {
    /* Don't spin because either cpu usage is too high or it's
    almost idle so no reason to bother. */
    max_spins = 0;

  } else if (cpu >= hwm / 2) {
    /* When cpu usage is more than 50% of the hwm, use the minimum allowed
    number of spin rounds, not to increase cpu usage too much (risky). */
    max_spins = min_non_zero_value;

  } else {
    /* When cpu usage is less than 50% of the hwm, choose maximum spin rounds
    in range [minimum, 10*minimum]. Smaller usage of cpu is, more spin rounds
    might be used. */
    const double r = 1.0 * (hwm / 2 - cpu) / (hwm / 2);

    max_spins =
        static_cast<uint64_t>(min_non_zero_value + r * min_non_zero_value * 9);
  }

  return max_spins;
}

/** Waits until redo log is written up to provided lsn (or greater).
We do not care if it's flushed or not.
@param[in]      log     redo log
@param[in]      lsn     wait until log.write_lsn >= lsn
@param[in,out]  interrupted     if true, was interrupted, needs retry.
@return         statistics related to waiting inside */
static Wait_stats log_wait_for_write(const log_t &log, lsn_t lsn,
                                     bool *interrupted) {
  os_event_set(log.writer_event);

  const uint64_t max_spins = log_max_spins_when_waiting_in_user_thread(
      srv_log_wait_for_write_spin_delay);

  auto stop_condition = [&log, lsn, interrupted](bool wait) {
    if (log.write_lsn.load() >= lsn) {
      *interrupted = false;
      return true;
    }

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_relaxed))) {
      *interrupted = true;
      return true;
    }

    if (wait) {
      os_event_set(log.writer_event);
    }

    ut_d(log_background_write_threads_active_validate(log));
    return false;
  };

  const size_t slot = log_compute_write_event_slot(log, lsn);

  const auto wait_stats =
      os_event_wait_for(log.write_events[slot], max_spins,
                        get_srv_log_wait_for_write_timeout(), stop_condition);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_WRITE_, wait_stats);

  return wait_stats;
}

/** Waits until redo log is flushed up to provided lsn (or greater).
@param[in]      log     redo log
@param[in]      lsn     wait until log.flushed_to_disk_lsn >= lsn
@param[in,out]  interrupted     if true, was interrupted, needs retry.
@return         statistics related to waiting inside */
static Wait_stats log_wait_for_flush(const log_t &log, lsn_t lsn,
                                     bool *interrupted) {
  if (log.write_lsn.load(std::memory_order_relaxed) < lsn) {
    os_event_set(log.writer_event);
  }
  os_event_set(log.flusher_event);

  uint64_t max_spins = log_max_spins_when_waiting_in_user_thread(
      srv_log_wait_for_flush_spin_delay);

  if (log.flush_avg_time >= srv_log_wait_for_flush_spin_hwm) {
    max_spins = 0;
  }

  auto stop_condition = [&log, lsn, interrupted](bool wait) {
    log_sync_point("log_wait_for_flush_before_flushed_to_disk_lsn");

    if (log.flushed_to_disk_lsn.load() >= lsn) {
      *interrupted = false;
      return true;
    }

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_relaxed))) {
      *interrupted = true;
      return true;
    }

    if (wait) {
      if (log.write_lsn.load(std::memory_order_relaxed) < lsn) {
        os_event_set(log.writer_event);
      }

      os_event_set(log.flusher_event);
    }

    log_sync_point("log_wait_for_flush_before_wait");
    return false;
  };

  const size_t slot = log_compute_flush_event_slot(log, lsn);

  thd_wait_begin(nullptr, THD_WAIT_GROUP_COMMIT);
  const auto wait_stats =
      os_event_wait_for(log.flush_events[slot], max_spins,
                        get_srv_log_wait_for_flush_timeout(), stop_condition);

  thd_wait_end(nullptr);

  MONITOR_INC_WAIT_STATS(MONITOR_LOG_ON_FLUSH_, wait_stats);

  return wait_stats;
}

/** Write the redo log up to a provided lsn by itself, if necessary.
@param[in]      log             redo log
@param[in]      end_lsn         lsn to write for
@param[in]      flush_to_disk   whether the written log should also be flushed
@param[in,out]  interrupted     if true, was interrupted, needs retry
@return statistics about waiting inside */
static Wait_stats log_self_write_up_to(log_t &log, lsn_t end_lsn,
                                       bool flush_to_disk, bool *interrupted) {
  ut_ad(!mutex_own(&(log.writer_mutex)));

  uint32_t waits = 0;
  *interrupted = false;

  lsn_t ready_lsn = log_buffer_ready_for_write_lsn(log);
  ulong i = 0;
  /* must wait for (ready_lsn >= end_lsn) at first */
  while (i < srv_n_spin_wait_rounds && ready_lsn < end_lsn) {
    if (srv_spin_wait_delay) {
      ut_delay(ut::random_from_interval(0, srv_spin_wait_delay));
    }
    i++;
    ready_lsn = log_buffer_ready_for_write_lsn(log);
  }
  if (ready_lsn < end_lsn) {
    log.recent_written.advance_tail();
    ready_lsn = log_buffer_ready_for_write_lsn(log);
  }
  if (ready_lsn < end_lsn) {
    std::this_thread::yield();
    ready_lsn = log_buffer_ready_for_write_lsn(log);
  }
  while (ready_lsn < end_lsn) {
    /* wait using event */
    log_closer_mutex_enter(log);
    if (log.current_ready_waiting_lsn == 0 &&
        os_event_is_set(log.closer_event)) {
      log.current_ready_waiting_lsn = end_lsn;
      log.current_ready_waiting_sig_count = os_event_reset(log.closer_event);
    }
    const auto sig_count = log.current_ready_waiting_sig_count;
    log_closer_mutex_exit(log);
    ++waits;
    os_event_wait_time_low(log.closer_event, std::chrono::milliseconds{100},
                           sig_count);
    log.recent_written.advance_tail();
    ready_lsn = log_buffer_ready_for_write_lsn(log);
  }

  /* NOTE: Currently doesn't do dirty read for (flush_to_disk == true) case,
  because the mutex contention also works as the arbitrator for write-IO
  (fsync) bandwidth between log files and data files. */
  if (!flush_to_disk &&
      log.write_lsn.load(std::memory_order_acquire) >= end_lsn) {
    return Wait_stats{waits};
  }

  /* mysql-test compatibility */
  log_sync_point("log_wait_for_flush_before_flushed_to_disk_lsn");
  log_sync_point("log_wait_for_flush_before_wait");

  log_writer_mutex_enter(log);

  if (UNIV_UNLIKELY(
          !log.writer_threads_paused.load(std::memory_order_relaxed))) {
    log_writer_mutex_exit(log);
    *interrupted = true;
    return Wait_stats{waits};
  }

  /* write to ready_lsn */
  lsn_t write_lsn = log.write_lsn.load(std::memory_order_relaxed);
  for (uint64_t step = 0; write_lsn < ready_lsn; ++step) {
    if (step % 1024 == 0) {
      /* The first loop or just after std::this_thread::sleep_for(0) */
      const lsn_t limit_lsn =
          flush_to_disk
              ? log.flushed_to_disk_lsn.load(std::memory_order_acquire)
              : write_lsn;
      if (limit_lsn >= end_lsn) {
        log_writer_mutex_exit(log);
        return Wait_stats{waits};
      }
    }

    log_writer_write_buffer(log, log_buffer_ready_for_write_lsn(log));

    if ((step + 1) % 1024 == 0) {
      /* approximate per srv_log_write_ahead_size * 1024 written. */
      log_writer_mutex_exit(log);
      std::this_thread::sleep_for(std::chrono::seconds(0));
      log_writer_mutex_enter(log);
    }

    write_lsn = log.write_lsn.load(std::memory_order_relaxed);
  }

  /* If it is a write call we should just go ahead and do it
  as we checked that write_lsn is not where we'd like it to
  be. If we have to flush as well then we check if there is a
  pending flush and based on that we wait for it to finish
  before proceeding further. */
  if (flush_to_disk) {
    if (!os_event_is_set(log.old_flush_event)) {
      const auto sig_count = log.current_flush_sig_count;
      log_writer_mutex_exit(log);
      ++waits;
      os_event_wait_low(log.old_flush_event, sig_count);
      /* Needs to confirm actual value,
      because the log writer threads might be resumed. */
      if (log.flushed_to_disk_lsn.load(std::memory_order_relaxed) < end_lsn) {
        *interrupted = true;
      }
      return Wait_stats{waits};
    } else {
      log.current_flush_sig_count = os_event_reset(log.old_flush_event);
    }
  }

  log_writer_mutex_exit(log);

  if (flush_to_disk) {
    /* basically, no other flushers */
    if (UNIV_UNLIKELY(log_flusher_mutex_enter_nowait(log))) {
      if (!log.writer_threads_paused.load(std::memory_order_relaxed)) {
        os_event_set(log.old_flush_event);
        *interrupted = true;
        return Wait_stats{waits};
      }
      log_flusher_mutex_enter(log);
    }
    log_flush_low(log);
    log_flusher_mutex_exit(log);

    /* mysql-test compatibility */
    log_sync_point("log_flush_notifier_after_event_reset");
    log_sync_point("log_flush_notifier_before_check");
    log_sync_point("log_flush_notifier_before_wait");
    log_sync_point("log_flush_notifier_before_flushed_to_disk_lsn");
    log_sync_point("log_flush_notifier_before_notify");
  }

  return Wait_stats{waits};
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
    return Wait_stats{0};
  }

  /* We do not need to have exact numbers and we do not care if we
  lost some increments for heavy workload. The value only has usage
  when it is low workload and we need to discover that we request
  redo write or flush only from time to time. In such case we prefer
  to avoid spinning in log threads to save on CPU power usage. */
  log.write_to_file_requests_total.store(
      log.write_to_file_requests_total.load(std::memory_order_relaxed) + 1,
      std::memory_order_relaxed);

  ut_a(end_lsn != LSN_MAX);

  ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE == 0 ||
       end_lsn % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

  ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE <=
       OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

  ut_ad(end_lsn <= log_get_lsn(log));

  Wait_stats wait_stats{0};
  bool interrupted = false;

retry:
  if (log.writer_threads_paused.load(std::memory_order_acquire)) {
    /* the log writer threads are paused not to waste CPU resource. */
    wait_stats +=
        log_self_write_up_to(log, end_lsn, flush_to_disk, &interrupted);

    if (UNIV_UNLIKELY(interrupted)) {
      /* the log writer threads might be working. retry. */
      goto retry;
    }

    DEBUG_SYNC_C("log_flushed_by_self");
    return wait_stats;
  }

  /* the log writer threads are working for high concurrency scale */
  if (flush_to_disk) {
    if (log.flushed_to_disk_lsn.load() >= end_lsn) {
      DEBUG_SYNC_C("log_flushed_by_writer");
      return wait_stats;
    }

    if (srv_flush_log_at_trx_commit != 1) {
      /* We need redo flushed, but because trx != 1, we have
      disabled notifications sent from log_writer to log_flusher.

      The log_flusher might be sleeping for 1 second, and we need
      quick response here. Log_writer avoids waking up log_flusher,
      so we must do it ourselves here.

      However, before we wake up log_flusher, we must ensure that
      log.write_lsn >= lsn. Otherwise log_flusher could flush some
      data which was ready for lsn values smaller than end_lsn and
      return to sleeping for next 1 second. */

      if (log.write_lsn.load() < end_lsn) {
        wait_stats += log_wait_for_write(log, end_lsn, &interrupted);
      }
    }

    /* Wait until log gets flushed up to end_lsn. */
    wait_stats += log_wait_for_flush(log, end_lsn, &interrupted);

    if (UNIV_UNLIKELY(interrupted)) {
      /* the log writer threads might be paused. retry. */
      goto retry;
    }

    DEBUG_SYNC_C("log_flushed_by_writer");
  } else {
    if (log.write_lsn.load() >= end_lsn) {
      return wait_stats;
    }

    /* Wait until log gets written up to end_lsn. */
    wait_stats += log_wait_for_write(log, end_lsn, &interrupted);

    if (UNIV_UNLIKELY(interrupted)) {
      /* the log writer threads might be paused. retry. */
      goto retry;
    }
  }

  return wait_stats;
}

/** @} */

/**************************************************/ /**

 @name Log threads waiting strategy

 *******************************************************/

/** @{ */

/** Small utility which is used inside log threads when they have to wait for
next interesting event to happen. For performance reasons, it might make sense
to use spin-delay in front of the wait on event in such cases. The strategy is
first to spin and then to fallback to the wait on event. However, for idle
servers or work-loads which do not need redo being flushed as often, we prefer
to avoid spinning. This utility solves such problems and provides waiting
mechanism. */
struct Log_thread_waiting {
  Log_thread_waiting(const log_t &log, os_event_t event, uint64_t spin_delay,
                     std::chrono::microseconds min_timeout)
      : m_log(log),
        m_event{event},
        m_spin_delay{static_cast<uint32_t>(std::min(
            uint64_t(std::numeric_limits<uint32_t>::max()), spin_delay))},
        m_min_timeout{/* No more than 1s */
                      std::min<std::chrono::microseconds>(
                          std::chrono::seconds{1}, min_timeout)} {}

  template <typename Stop_condition>
  inline Wait_stats wait(Stop_condition stop_condition) {
    auto spin_delay = m_spin_delay;
    auto min_timeout = m_min_timeout;

    /** We might read older value, it just decides on spinning.
    Correctness does not depend on this. Only local performance might depend on
    this but it's anyway heuristic and depends on average which by definition
    has lag. No reason to make extra barriers here. */

    const auto req_interval =
        m_log.write_to_file_requests_interval.load(std::memory_order_relaxed);

    if (srv_cpu_usage.utime_abs < srv_log_spin_cpu_abs_lwm ||
        !log_write_to_file_requests_are_frequent(req_interval)) {
      /* Either:
      1. CPU usage is very low on the server, which means the server is most
         likely idle or almost idle.
      2. Request to write/flush redo to disk comes only once per 1ms in average
         or even less often.
      In both cases we prefer not to spend on CPU power, because there is no
      real gain from spinning in log threads then. */

      spin_delay = 0;
      min_timeout = std::min<std::chrono::microseconds>(
          req_interval, std::chrono::milliseconds{1});
    }

    const auto wait_stats =
        os_event_wait_for(m_event, spin_delay, min_timeout, stop_condition);

    return wait_stats;
  }

 private:
  const log_t &m_log;
  os_event_t m_event;
  const uint32_t m_spin_delay;
  const std::chrono::microseconds m_min_timeout;
};

struct Log_write_to_file_requests_monitor {
  explicit Log_write_to_file_requests_monitor(log_t &log)
      : m_log(log), m_last_requests_value{0}, m_request_interval{0} {
    m_last_requests_time = Log_clock::now();
  }

  void update() {
    const auto requests_value =
        m_log.write_to_file_requests_total.load(std::memory_order_relaxed);

    const auto current_time = Log_clock::now();
    if (current_time < m_last_requests_time) {
      m_last_requests_time = current_time;
      return;
    }

    const auto delta_time = current_time - m_last_requests_time;

    if (requests_value > m_last_requests_value) {
      const auto delta_requests = requests_value - m_last_requests_value;
      const auto request_interval = delta_time / delta_requests;
      m_request_interval =
          std::chrono::duration_cast<std::chrono::microseconds>(
              (m_request_interval * 63 + request_interval) / 64);

    } else if (delta_time > std::chrono::milliseconds{100}) {
      /* Last call to log_write_up_to() was longer than 100ms ago, so consider
      this as maximum time between calls we can expect. Tracking higher values
      does not make sense, because it is for sure already higher than any
      reasonable threshold which can be
      used to differ different activity modes. */

      m_request_interval = std::chrono::milliseconds{100};

    } else {
      /* No progress in number of requests and still no more than 1second since
      last progress. Postpone any decision. */
      return;
    }

    m_log.write_to_file_requests_interval.store(m_request_interval,
                                                std::memory_order_relaxed);

    MONITOR_SET(MONITOR_LOG_WRITE_TO_FILE_REQUESTS_INTERVAL,
                m_request_interval.count());

    m_last_requests_time = current_time;
    m_last_requests_value = requests_value;
  }

 private:
  log_t &m_log;
  uint64_t m_last_requests_value;
  Log_clock_point m_last_requests_time;
  std::chrono::microseconds m_request_interval;
};

/** @} */

/**************************************************/ /**

 @name Log writer thread

 *******************************************************/

/** @{ */

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

static inline bool current_file_has_space(const log_t &log, os_offset_t offset,
                                          size_t size) {
  return offset + size <= log.m_current_file.m_size_in_bytes;
}

static dberr_t start_next_file(log_t &log, lsn_t start_lsn) {
  ut_ad(log_writer_mutex_own(log));

  IB_mutex_guard files_latch{&(log.m_files_mutex), UT_LOCATION_HERE};

  const dberr_t err = log_files_produce_file(log);

  if (err != DB_SUCCESS) {
    return err;
  }

  ut_a(start_lsn == log.m_current_file.m_start_lsn);

  MONITOR_INC(MONITOR_LOG_NEXT_FILE);

  log.write_ahead_end_offset = 0;

  return DB_SUCCESS;
}

static inline bool write_ahead_enough(os_offset_t write_ahead_end,
                                      os_offset_t offset, size_t size) {
  return write_ahead_end >= offset + size;
}

static inline bool current_write_ahead_enough(const log_t &log,
                                              os_offset_t offset, size_t size) {
  return write_ahead_enough(log.write_ahead_end_offset, offset, size);
}

static inline os_offset_t compute_next_write_ahead_end(
    os_offset_t real_offset) {
  const auto last_wa =
      ut_uint64_align_down(real_offset, srv_log_write_ahead_size);

  const auto next_wa = last_wa + srv_log_write_ahead_size;

  ut_a(next_wa > real_offset);
  ut_a(next_wa % srv_log_write_ahead_size == 0);

  return next_wa;
}

static inline size_t compute_how_much_to_write(const log_t &log,
                                               os_offset_t real_offset,
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
      return 0;

    } else {
      /* We write across at least two consecutive log files.
      Limit current write to the first one and then retry for
      next_file. */

      /* If the condition for real_offset + buffer_size holds,
      then the expression below is < buffer_size, which is
      size_t, so the typecast is ok. */
      write_size =
          static_cast<size_t>(log.m_current_file.m_size_in_bytes - real_offset);

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
  ut_a(real_offset + write_size <= log.m_current_file.m_size_in_bytes);

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

  return write_size;
}

static inline void prepare_full_blocks(const log_t &log, byte *buffer,
                                       size_t size, lsn_t start_lsn) {
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

    const lsn_t block_lsn = start_lsn + buffer_offset;

    Log_data_block_header block_header;
    block_header.set_lsn(block_lsn);
    block_header.m_data_len = OS_FILE_LOG_BLOCK_SIZE;
    block_header.m_first_rec_group = log_block_get_first_rec_group(ptr);
    log_data_block_header_serialize(block_header, ptr);
  }
}

static inline dberr_t write_blocks(log_t &log, byte *write_buf,
                                   size_t write_size, os_offset_t real_offset) {
  ut_a(write_size >= OS_FILE_LOG_BLOCK_SIZE);
  ut_a(write_size % OS_FILE_LOG_BLOCK_SIZE == 0);
  ut_a(real_offset / UNIV_PAGE_SIZE <= PAGE_NO_MAX);

  ut_a(log.write_ahead_end_offset % srv_log_write_ahead_size == 0);

  ut_a(real_offset + write_size <= log.write_ahead_end_offset ||
       (real_offset + write_size) % srv_log_write_ahead_size == 0);

  const dberr_t err = log_data_blocks_write(log.m_current_file_handle,
                                            real_offset, write_size, write_buf);

  if (err != DB_SUCCESS) {
    return err;
  }

  meb::redo_log_archive_produce(write_buf, write_size);

  return DB_SUCCESS;
}

static inline void notify_about_advanced_write_lsn(log_t &log,
                                                   lsn_t old_write_lsn,
                                                   lsn_t new_write_lsn) {
  if (!log.writer_threads_paused.load(std::memory_order_acquire)) {
    if (srv_flush_log_at_trx_commit == 1) {
      os_event_set(log.flusher_event);
    }

    const auto first_slot =
        log_compute_write_event_slot(log, old_write_lsn + 1);

    const auto last_slot = log_compute_write_event_slot(log, new_write_lsn);

    if (first_slot == last_slot) {
      log_sync_point("log_write_before_users_notify");
      os_event_set(log.write_events[first_slot]);
    } else {
      log_sync_point("log_write_before_notifier_notify");
      os_event_set(log.write_notifier_event);
    }
  }

  if (arch_log_sys && arch_log_sys->is_active()) {
    os_event_set(log_archiver_thread_event);
  }
}

static inline void copy_to_write_ahead_buffer(log_t &log, const byte *buffer,
                                              size_t &size, lsn_t start_lsn) {
  ut_a(size <= srv_log_write_ahead_size);

  ut_a(buffer >= log.buf);
  ut_a(buffer + size <= log.buf + log.buf_size);

  byte *write_buf = log.write_ahead_buf;

  log_sync_point("log_writer_before_copy_to_write_ahead_buffer");

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

    const lsn_t block_lsn = start_lsn + completed_blocks_size;

    Log_data_block_header block_header;
    block_header.set_lsn(block_lsn);
    block_header.m_data_len = incomplete_size;
    block_header.m_first_rec_group =
        log_block_get_first_rec_group(incomplete_block);
    if (block_header.m_first_rec_group > incomplete_size) {
      block_header.m_first_rec_group = 0;
    }
    log_data_block_header_serialize(block_header, incomplete_block);

    std::memset(incomplete_block + incomplete_size, 0x00,
                OS_FILE_LOG_BLOCK_SIZE - incomplete_size);

    log_block_store_checksum(incomplete_block);

    size = completed_blocks_size + OS_FILE_LOG_BLOCK_SIZE;
  }

  /* Since now, size is about completed blocks always. */
  ut_a(size % OS_FILE_LOG_BLOCK_SIZE == 0);
}

static inline size_t prepare_for_write_ahead(log_t &log,
                                             os_offset_t real_offset,
                                             size_t &write_size) {
  /* We need to perform write ahead during this write. */

  const auto next_wa = compute_next_write_ahead_end(real_offset);

  ut_a(real_offset + write_size <= next_wa);

  size_t write_ahead =
      static_cast<size_t>(next_wa - (real_offset + write_size));

  if (!current_file_has_space(log, real_offset, write_size + write_ahead)) {
    /* We must not write further than to the end
    of the current log file.

    Note, that:log.m_current_file.m_size_in_bytes - LOG_FILE_HDR_SIZE
    does not have to be divisible by size of write ahead. For example:
            innodb_redo_log_capacity = 32M,
            innodb_log_write_ahead_size = 4KiB,
            LOG_FILE_HDR_SIZE is 2KiB. */

    write_ahead = static_cast<size_t>(log.m_current_file.m_size_in_bytes -
                                      real_offset - write_size);
  }

  ut_a(current_file_has_space(log, real_offset, write_size + write_ahead));

  log_sync_point("log_writer_before_write_ahead");

  std::memset(log.write_ahead_buf + write_size, 0x00, write_ahead);

  write_size += write_ahead;

  return write_ahead;
}

static inline void update_current_write_ahead(log_t &log,
                                              os_offset_t real_offset,
                                              size_t write_size) {
  const auto end = real_offset + write_size;

  if (end > log.write_ahead_end_offset) {
    log.write_ahead_end_offset =
        ut_uint64_align_down(end, srv_log_write_ahead_size);
  }
}

}  // namespace Log_files_write_impl

static dberr_t log_write_buffer(log_t &log, byte *buffer, size_t buffer_size,
                                lsn_t start_lsn) {
  ut_ad(log_writer_mutex_own(log));

  using namespace Log_files_write_impl;

  validate_buffer(log, buffer, buffer_size);

  validate_start_lsn(log, start_lsn, buffer_size);

  const auto real_offset = log.m_current_file.offset(start_lsn);

  bool write_from_log_buffer;

  auto write_size = compute_how_much_to_write(log, real_offset, buffer_size,
                                              write_from_log_buffer);

  if (write_size == 0) {
    return start_next_file(log, start_lsn);
  }

  prepare_full_blocks(log, buffer, write_size, start_lsn);

  byte *write_buf;
  os_offset_t written_ahead = 0;
  lsn_t lsn_advance = write_size;

  if (write_from_log_buffer) {
    /* We have at least one completed log block to write.
    We write completed blocks from the log buffer. Note,
    that possibly we do not write all completed blocks,
    because of write-ahead strategy (described earlier). */
    DBUG_PRINT("ib_log",
               ("write from log buffer start_lsn=" LSN_PF " write_lsn=" LSN_PF
                " -> " LSN_PF,
                start_lsn, log.write_lsn.load(), start_lsn + lsn_advance));

    write_buf = buffer;

    log_sync_point("log_writer_before_write_from_log_buffer");

  } else {
    DBUG_PRINT("ib_log",
               ("incomplete write start_lsn=" LSN_PF " write_lsn=" LSN_PF
                " -> " LSN_PF,
                start_lsn, log.write_lsn.load(), start_lsn + lsn_advance));

#ifdef UNIV_DEBUG
    if (start_lsn == log.write_lsn.load()) {
      log_sync_point("log_writer_before_write_new_incomplete_block");
    }
    /* Else: we are doing yet another incomplete block write within the
    same block as the one in which we did the previous write. */
#endif /* UNIV_DEBUG */

    write_buf = log.write_ahead_buf;

    /* We write all the data directly from the write-ahead buffer,
    where we first need to copy the data. */
    copy_to_write_ahead_buffer(log, buffer, write_size, start_lsn);

    if (!current_write_ahead_enough(log, real_offset, 1)) {
      written_ahead = prepare_for_write_ahead(log, real_offset, write_size);
    }
  }

  srv_stats.os_log_pending_writes.inc();

  /* Now, we know, that we are going to write completed
  blocks only (originally or copied and completed). */
  const dberr_t err = write_blocks(log, write_buf, write_size, real_offset);
  if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
    return err;
  }

  log_sync_point("log_writer_before_lsn_update");

  const lsn_t old_write_lsn = log.write_lsn.load();

  const lsn_t new_write_lsn = start_lsn + lsn_advance;
  ut_a(new_write_lsn > log.write_lsn.load());

  log.write_lsn.store(new_write_lsn);

  notify_about_advanced_write_lsn(log, old_write_lsn, new_write_lsn);

  log_sync_point("log_writer_before_buf_limit_update");

  log_update_buf_limit(log, new_write_lsn);

  srv_stats.os_log_pending_writes.dec();
  srv_stats.log_writes.inc();

  /* Write ahead is included in write_size. */
  ut_a(write_size >= written_ahead);
  srv_stats.os_log_written.add(write_size - written_ahead);
  MONITOR_INC_VALUE(MONITOR_LOG_PADDED, written_ahead);

  int64_t free_space = log.m_capacity.soft_logical_capacity();

  /* The free space may be negative (up to -extra_margin),
  in which case we are in the emergency mode, eating the
  extra margin and asking to pause next user threads. */
  free_space -= new_write_lsn - log.last_checkpoint_lsn.load();

  MONITOR_SET(MONITOR_LOG_FREE_SPACE, free_space);

  log.n_log_ios++;

  update_current_write_ahead(log, real_offset, write_size);

  return DB_SUCCESS;
}

static void log_writer_enter_extra_margin(log_t &log) {
  ut_ad(log_writer_mutex_own(log));
  ut_ad(!log.m_writer_inside_extra_margin);
  log_limits_mutex_enter(log);
  log.m_writer_inside_extra_margin = true;
  ib::warn(ER_IB_MSG_LOG_WRITER_ENTERED_EXTRA_MARGIN);
  log_limits_mutex_exit(log);
  log_sync_point("log_writer_entered_extra_margin");
}

static void log_writer_exit_extra_margin(log_t &log) {
  ut_ad(log_writer_mutex_own(log));
  ut_ad(log.m_writer_inside_extra_margin);
  log_limits_mutex_enter(log);
  log.m_writer_inside_extra_margin = false;
  ib::info(ER_IB_MSG_LOG_WRITER_EXITED_EXTRA_MARGIN);
  log_limits_mutex_exit(log);
  log_sync_point("log_writer_exited_extra_margin");
}

static inline bool log_writer_extra_margin_check(log_t &log,
                                                 lsn_t checkpoint_lsn,
                                                 lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));

  const lsn_t soft_limited_lsn =
      ut_uint64_align_down(checkpoint_lsn, OS_FILE_LOG_BLOCK_SIZE) +
      log.m_capacity.soft_logical_capacity();

  if (next_write_lsn <= soft_limited_lsn) {
    if (log.m_writer_inside_extra_margin) {
      log_writer_exit_extra_margin(log);
    }
    return false;
  } else {
    if (!log.m_writer_inside_extra_margin) {
      log_writer_enter_extra_margin(log);
    }
    return true;
  }
}

void log_writer_check_if_exited_extra_margin(log_t &log) {
  ut_ad(log_writer_mutex_own(log));
  ut_ad(log.m_writer_inside_extra_margin);

  const lsn_t checkpoint_lsn = log_get_checkpoint_lsn(log);

  /* Choose an option which minimizes the possibility that the current
  value of log.m_writer_inside_extra_margin changes. That's to avoid
  flipping when doing changes opposite to changes performed by the log
  writer thread, which knows what is the exact value of next_write_lsn
  in log_writer_wait_on_checkpoint (as opposed to log_checkpointer,
  which does not know when advancing checkpoint). This function exists
  and is exported, because the log_checkpointer thread needs to update
  the log.m_writer_inside_extra_margin when it advances checkpoint_lsn
  if the log_writer is idle (because has nothing more to write). */

  log_writer_extra_margin_check(log, checkpoint_lsn, log_get_lsn(log));
}

static inline std::pair<lsn_t, bool> log_writer_wait_on_checkpoint_optimistic(
    log_t &log, lsn_t last_write_lsn, lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));

  const lsn_t checkpoint_lsn = log.last_checkpoint_lsn.load();

  const lsn_t hard_limited_lsn =
      ut_uint64_align_down(checkpoint_lsn, OS_FILE_LOG_BLOCK_SIZE) +
      log.m_capacity.hard_logical_capacity();

  ut_a(last_write_lsn <= hard_limited_lsn);
  ut_a(checkpoint_lsn < next_write_lsn);

  return {hard_limited_lsn,
          !log_writer_extra_margin_check(log, checkpoint_lsn, next_write_lsn)};
}

static lsn_t log_writer_wait_on_checkpoint_pessimistic(log_t &log,
                                                       lsn_t last_write_lsn,
                                                       lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));

  auto missing_space_started = Log_clock::now();

  while (true) {
    const int64_t next_checkpoint_sig_count =
        os_event_reset(log.next_checkpoint_event);

    const lsn_t checkpoint_lsn = log.last_checkpoint_lsn.load();

    auto [hard_limited_lsn, write_allowed] =
        log_writer_wait_on_checkpoint_optimistic(log, last_write_lsn,
                                                 next_write_lsn);

    if (write_allowed) {
      return hard_limited_lsn;
    }

    os_event_set(log.checkpointer_event);

    if (last_write_lsn + OS_FILE_LOG_BLOCK_SIZE <= hard_limited_lsn) {
      /* Write what we have - adjust the speed to speed of checkpoints
      going forward (to speed of page-cleaners). */
      return hard_limited_lsn;
    }

    if (!log.writer_threads_paused.load(std::memory_order_acquire)) {
      log_advance_ready_for_write_lsn(log);
    }

    if (Log_clock::now() - missing_space_started >= std::chrono::seconds(5)) {
      /* We could not reclaim even single redo block for 5sec */
      ib::error(ER_IB_MSG_LOG_WRITER_OUT_OF_SPACE, ulonglong{checkpoint_lsn});
      missing_space_started = Log_clock::now();
      log_sync_point("log_writer_ran_out_space");
    }

    log_writer_mutex_exit(log);

    if (!log.m_allow_checkpoints.load()) {
      if (srv_force_recovery < 4) {
        ib::fatal(UT_LOCATION_HERE,
                  ER_IB_MSG_RECOVERY_NO_SPACE_IN_REDO_LOG__SKIP_IBUF_MERGES);
      } else {
        ib::fatal(UT_LOCATION_HERE,
                  ER_IB_MSG_RECOVERY_NO_SPACE_IN_REDO_LOG__UNEXPECTED);
      }
    }

    /* We don't want to ask for sync checkpoint, because it
    is possible, that the oldest dirty page is latched and
    user thread, which keeps the latch, is waiting for space
    in log buffer (for log_writer writing to disk). In such
    case it would be deadlock (we can't flush the latched
    page and advance the checkpoint). We only ask for the
    checkpoint, and wait for some time. */
    log_request_checkpoint(log, false);

    os_event_wait_time_low(log.next_checkpoint_event,
                           std::chrono::microseconds(100),
                           next_checkpoint_sig_count);

    MONITOR_INC(MONITOR_LOG_WRITER_ON_FREE_SPACE_WAITS);

    log_writer_mutex_enter(log);
  }
}

static lsn_t log_writer_wait_on_checkpoint(log_t &log, lsn_t last_write_lsn,
                                           lsn_t next_write_lsn) {
  auto [hard_limited_lsn, write_allowed] =
      log_writer_wait_on_checkpoint_optimistic(log, last_write_lsn,
                                               next_write_lsn);
  if (write_allowed) {
    return hard_limited_lsn;
  }
  return log_writer_wait_on_checkpoint_pessimistic(log, last_write_lsn,
                                                   next_write_lsn);
}

static void log_writer_wait_on_archiver(log_t &log, lsn_t next_write_lsn) {
  const int32_t SLEEP_BETWEEN_RETRIES_IN_US = 100; /* 100us */

  const int32_t TIME_BETWEEN_WARNINGS_IN_US = 100000; /* 100ms */

  const int32_t TIME_UNTIL_ERROR_IN_US = 1000000; /* 1s */

  ut_ad(log_writer_mutex_own(log));

  int32_t count = 0;

  while (arch_log_sys != nullptr && arch_log_sys->is_active()) {
    lsn_t archiver_lsn = arch_log_sys->get_archived_lsn();

    archiver_lsn = ut_uint64_align_down(archiver_lsn, OS_FILE_LOG_BLOCK_SIZE);

    const lsn_t archiver_limited_lsn =
        archiver_lsn + log.m_capacity.hard_logical_capacity();

    ut_a(next_write_lsn > archiver_lsn);

    if (next_write_lsn <= archiver_limited_lsn) {
      /* Between archive_lsn and next_write_lsn there is less bytes than
      logical capacity provided by the redo log. There is no need to wait
      for the archiver. */
      break;
    }

    if (!log.writer_threads_paused.load(std::memory_order_acquire)) {
      log_advance_ready_for_write_lsn(log);
    }

    const int32_t ATTEMPTS_UNTIL_ERROR =
        TIME_UNTIL_ERROR_IN_US / SLEEP_BETWEEN_RETRIES_IN_US;

    if (count >= ATTEMPTS_UNTIL_ERROR) {
      log_writer_mutex_exit(log);

      arch_log_sys->force_abort();

      const lsn_t lag = next_write_lsn - archiver_limited_lsn;

      ib::error(ER_IB_MSG_LOG_WRITER_ABORTS_LOG_ARCHIVER, ulonglong{lag},
                ulonglong{archiver_lsn});

      log_writer_mutex_enter(log);
      break;
    }

    os_event_set(log_archiver_thread_event);

    log_writer_mutex_exit(log);

    const int32_t ATTEMPTS_BETWEEN_WARNINGS =
        TIME_BETWEEN_WARNINGS_IN_US / SLEEP_BETWEEN_RETRIES_IN_US;

    if (count % ATTEMPTS_BETWEEN_WARNINGS == 0) {
      const lsn_t lag = next_write_lsn - archiver_limited_lsn;

      ib::warn(ER_IB_MSG_LOG_WRITER_WAITING_FOR_ARCHIVER, ulonglong{lag},
               ulonglong{archiver_lsn});
    }

    count++;
    std::this_thread::sleep_for(
        std::chrono::microseconds(SLEEP_BETWEEN_RETRIES_IN_US));

    MONITOR_INC(MONITOR_LOG_WRITER_ON_ARCHIVER_WAITS);

    log_writer_mutex_enter(log);
  }
}

static void log_writer_wait_on_consumers(log_t &log, lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));
  constexpr auto SLEEP_BETWEEN_RETRIES = std::chrono::milliseconds(10);
  constexpr auto TIME_BETWEEN_WARNINGS = std::chrono::seconds(1);
  constexpr size_t ATTEMPTS_BETWEEN_WARNINGS =
      TIME_BETWEEN_WARNINGS / SLEEP_BETWEEN_RETRIES;
  size_t attempt = 0;
  while (log.m_oldest_need_lsn_lowerbound +
             log.m_capacity.hard_logical_capacity() <
         next_write_lsn) {
    log_files_mutex_enter(*log_sys);
    lsn_t oldest_needed_lsn;
    const auto consumer = log_consumer_get_oldest(log, oldest_needed_lsn);
    ut_ad(log.m_oldest_need_lsn_lowerbound <= oldest_needed_lsn);
    log.m_oldest_need_lsn_lowerbound = oldest_needed_lsn;
    if (next_write_lsn <=
        oldest_needed_lsn + log.m_capacity.hard_logical_capacity()) {
      log_files_mutex_exit(*log_sys);
      break;
    }
    const std::string name = consumer->get_name();
    log_files_mutex_exit(*log_sys);
    /* This should not be a checkpointer nor archiver, as we've used dedicated
    log_writer_wait_on_checkpoint() and log_writer_wait_on_archiver() to wait
    for them already */
    ut_ad(name == "MEB");
    log_writer_mutex_exit(log);
    if (attempt++ % ATTEMPTS_BETWEEN_WARNINGS == 0) {
      ib::log_warn(ER_IB_MSG_LOG_WRITER_WAIT_ON_CONSUMER, name.c_str(),
                   ulonglong{oldest_needed_lsn});
    }
    std::this_thread::sleep_for(SLEEP_BETWEEN_RETRIES);
    log_writer_mutex_enter(log);
  }
}
static void log_writer_write_failed(log_t &log, dberr_t err) {
  ut_ad(log_writer_mutex_own(log));
  ut_ad(!log_files_mutex_own(log));
  const auto file_path =
      log_file_path(log.m_files_ctx, log.m_current_file.m_id);
  switch (err) {
    case DB_OUT_OF_DISK_SPACE:
      ib::warn(ER_IB_MSG_LOG_WRITER_WAIT_ON_NEW_LOG_FILE);
      log_writer_mutex_exit(log);
      log_files_wait_for_next_file_available(log);
      log_writer_mutex_enter(log);
      break;
    default:
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_LOG_WRITER_WRITE_FAILED,
                static_cast<int>(err), file_path.c_str());
  }
}

static void log_writer_write_buffer(log_t &log, lsn_t next_write_lsn) {
  ut_ad(log_writer_mutex_own(log));

  log_sync_point("log_writer_write_begin");

  const lsn_t last_write_lsn = log.write_lsn.load();

  ut_a(log_is_data_lsn(last_write_lsn) ||
       last_write_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  ut_a(log_is_data_lsn(next_write_lsn) ||
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

  /* Wait until there is free space in log files.*/

  const lsn_t checkpoint_limited_lsn =
      log_writer_wait_on_checkpoint(log, last_write_lsn, next_write_lsn);

  ut_ad(log_writer_mutex_own(log));
  ut_a(checkpoint_limited_lsn > last_write_lsn);

  log_sync_point("log_writer_after_checkpoint_check");

  if (arch_log_sys != nullptr) {
    log_writer_wait_on_archiver(log, next_write_lsn);
  }

  ut_ad(log_writer_mutex_own(log));

  log_sync_point("log_writer_after_archiver_check");

  const lsn_t limit_for_next_write_lsn = checkpoint_limited_lsn;

  if (limit_for_next_write_lsn < next_write_lsn) {
    end_offset -= next_write_lsn - limit_for_next_write_lsn;
    next_write_lsn = limit_for_next_write_lsn;

    ut_a(end_offset > start_offset);
    ut_a(end_offset % OS_FILE_LOG_BLOCK_SIZE == 0 ||
         end_offset % OS_FILE_LOG_BLOCK_SIZE >= LOG_BLOCK_HDR_SIZE);

    ut_a(log_is_data_lsn(next_write_lsn) ||
         next_write_lsn % OS_FILE_LOG_BLOCK_SIZE == 0);
  }

  log_writer_wait_on_consumers(log, next_write_lsn);
  ut_ad(log_writer_mutex_own(log));

  DBUG_PRINT("ib_log",
             ("write " LSN_PF " to " LSN_PF, last_write_lsn, next_write_lsn));

  byte *buf_begin =
      log.buf + ut_uint64_align_down(start_offset, OS_FILE_LOG_BLOCK_SIZE);

  byte *buf_end = log.buf + end_offset;

  /* Do the write to the log files */

  const dberr_t err = log_write_buffer(
      log, buf_begin, buf_end - buf_begin,
      ut_uint64_align_down(last_write_lsn, OS_FILE_LOG_BLOCK_SIZE));

  if (UNIV_UNLIKELY(err != DB_SUCCESS)) {
    ut_a(log.write_lsn.load() == last_write_lsn);
    log_writer_write_failed(log, err);
  }

  log_sync_point("log_writer_write_end");
}

static bool log_writer_is_allowed_to_stop(log_t &log) {
  ut_ad(log_writer_mutex_own(log));
  log_writer_mutex_exit(log);
  log_files_dummy_records_disable(log);
  log_writer_mutex_enter(log);

  /* When log threads are stopped, we must first
  ensure that all writes to log buffer have been
  finished and only then we are allowed to set
  the should_stop_threads to true. */

  log_advance_ready_for_write_lsn(log);

  return log.write_lsn.load() == log_buffer_ready_for_write_lsn(log);
}

void log_writer(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);

  log_t &log = *log_ptr;
  lsn_t ready_lsn = 0;

  ut_d(log.m_writer_thd = create_internal_thd());

  log_writer_mutex_enter(log);

  Log_thread_waiting waiting{log, log.writer_event, srv_log_writer_spin_delay,
                             get_srv_log_writer_timeout()};

  Log_write_to_file_requests_monitor write_to_file_requests_monitor{log};

  for (uint64_t step = 0;; ++step) {
    bool released = false;

    auto stop_condition = [&ready_lsn, &log, &released,
                           &write_to_file_requests_monitor](bool wait) {
      if (released) {
        log_writer_mutex_enter(log);
        released = false;
      }

      /* Advance lsn up to which data is ready in log buffer. */
      log_advance_ready_for_write_lsn(log);

      ready_lsn = log_buffer_ready_for_write_lsn(log);

      /* Wait until any of following conditions holds:
              1) There is some unwritten data in log buffer
              2) We should close threads. */

      if (log.write_lsn.load() < ready_lsn || log.should_stop_threads.load()) {
        return true;
      }

      if (UNIV_UNLIKELY(
              log.writer_threads_paused.load(std::memory_order_acquire))) {
        return true;
      }

      if (wait) {
        write_to_file_requests_monitor.update();
        log_writer_mutex_exit(log);
        released = true;
      }

      return false;
    };

    const auto wait_stats = waiting.wait(stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_WRITER_, wait_stats);

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_acquire) &&
            !log.should_stop_threads.load())) {
      log_writer_mutex_exit(log);

      os_event_wait(log.writer_threads_resume_event);

      log_writer_mutex_enter(log);
      ready_lsn = log_buffer_ready_for_write_lsn(log);
    }

    /* Do the actual work. */
    if (log.write_lsn.load() < ready_lsn) {
      log_writer_write_buffer(log, ready_lsn);

      if (step % 1024 == 0) {
        write_to_file_requests_monitor.update();

        log_writer_mutex_exit(log);

        std::this_thread::sleep_for(std::chrono::seconds(0));

        log_writer_mutex_enter(log);
      }

    } else if (log.should_stop_threads.load() &&
               log_writer_is_allowed_to_stop(log)) {
      break;
    }
  }

  log_writer_mutex_exit(log);

  ut_d(destroy_internal_thd(log.m_writer_thd));
}

/** @} */

/**************************************************/ /**

 @name Log flusher thread

 *******************************************************/

/** @{ */

static void log_flush_update_stats(log_t &log) {
  ut_ad(log_flusher_mutex_own(log));

  /* Note that this code is inspired by similar logic in buf0flu.cc */

  static uint64_t iterations = 0;
  static Log_clock_point prev_time{};
  static lsn_t prev_lsn;
  static lsn_t lsn_avg_rate = 0;
  static Log_clock::duration fsync_max_time;
  static Log_clock::duration fsync_total_time;

  /* Calculate time of last fsync and update related counters. */

  Log_clock::duration fsync_time;

  fsync_time = log.last_flush_end_time - log.last_flush_start_time;

  fsync_max_time = std::max(fsync_max_time, fsync_time);

  if (fsync_time.count() > 0) {
    fsync_total_time += fsync_time;

    MONITOR_INC_VALUE(
        MONITOR_LOG_FLUSH_TOTAL_TIME,
        std::chrono::duration_cast<std::chrono::milliseconds>(fsync_time)
            .count());
  }

  /* Calculate time elapsed since start of last sample. */

  if (prev_time == Log_clock_point{}) {
    prev_time = log.last_flush_start_time;
    prev_lsn = log.flushed_to_disk_lsn.load();
  }

  const Log_clock_point curr_time = log.last_flush_end_time;

  if (curr_time < prev_time) {
    /* Time was moved backward since we set prev_time.
    We cannot determine how much time passed since then. */
    prev_time = curr_time;
  }

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

uint64_t log_total_flushes() { return Log_file_handle::total_fsyncs(); }

uint64_t log_pending_flushes() { return Log_file_handle::fsyncs_in_progress(); }

static void log_flush_low(log_t &log) {
  ut_ad(log_flusher_mutex_own(log));

#ifndef _WIN32
  bool do_flush = srv_unix_file_flush_method != SRV_UNIX_O_DSYNC;
#else
  bool do_flush = true;
#endif

  if (!log.writer_threads_paused.load(std::memory_order_acquire)) {
    os_event_reset(log.flusher_event);
  }

  const lsn_t last_flush_lsn = log.flushed_to_disk_lsn.load();

  const lsn_t flush_up_to_lsn = log.write_lsn.load();

  if (flush_up_to_lsn == last_flush_lsn) {
    os_event_set(log.old_flush_event);
    return;
  }

  log.last_flush_start_time = Log_clock::now();

  ut_a(flush_up_to_lsn > last_flush_lsn);

  if (do_flush) {
    log_sync_point("log_flush_before_fsync");
    log.m_current_file_handle.fsync();
  }

  log.last_flush_end_time = Log_clock::now();

  if (log.last_flush_end_time < log.last_flush_start_time) {
    /* Time was moved backward after we set start_time.
    Let assume that the fsync operation was instant.

    We move start_time backward, because we don't want
    it to remain in the future. */
    log.last_flush_start_time = log.last_flush_end_time;
  }

  log_sync_point("log_flush_before_flushed_to_disk_lsn");

  log.flushed_to_disk_lsn.store(flush_up_to_lsn);

  /* Notify other thread(s). */

  DBUG_PRINT("ib_log", ("Flushed to disk up to " LSN_PF, flush_up_to_lsn));

  if (!log.writer_threads_paused.load(std::memory_order_acquire)) {
    const auto first_slot =
        log_compute_flush_event_slot(log, last_flush_lsn + 1);

    const auto last_slot = log_compute_flush_event_slot(log, flush_up_to_lsn);

    if (first_slot == last_slot) {
      log_sync_point("log_flush_before_users_notify");
      os_event_set(log.flush_events[first_slot]);
    } else {
      log_sync_point("log_flush_before_notifier_notify");
      os_event_set(log.flush_notifier_event);
    }
  } else {
    log_sync_point("log_flush_before_users_notify");
    log_sync_point("log_flush_before_notifier_notify");
    os_event_set(log.old_flush_event);
  }

  /* Update stats. */

  log_flush_update_stats(log);
}

void log_flusher(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);

  log_t &log = *log_ptr;

  Log_thread_waiting waiting{log, log.flusher_event, srv_log_flusher_spin_delay,
                             get_srv_log_flusher_timeout()};

  log_flusher_mutex_enter(log);

  for (uint64_t step = 0;; ++step) {
    if (log.should_stop_threads.load()) {
      if (!log_writer_is_active()) {
        /* If write_lsn > flushed_to_disk_lsn, we are going to execute
        one more fsync just after the for-loop and before this thread
        exits (inside log_flush_low at the very end of function def.). */
        break;
      }
    }

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_acquire))) {
      log_flusher_mutex_exit(log);

      os_event_wait(log.writer_threads_resume_event);

      log_flusher_mutex_enter(log);
    }

    bool released = false;

    auto stop_condition = [&log, &released, step](bool wait) {
      if (released) {
        log_flusher_mutex_enter(log);
        released = false;
      }

      log_sync_point("log_flusher_before_should_flush");

      const lsn_t last_flush_lsn = log.flushed_to_disk_lsn.load();

      ut_a(last_flush_lsn <= log.write_lsn.load());

      if (last_flush_lsn < log.write_lsn.load()) {
        /* Flush and stop waiting. */
        log_flush_low(log);

        if (step % 1024 == 0) {
          log_flusher_mutex_exit(log);

          std::this_thread::sleep_for(std::chrono::seconds(0));

          log_flusher_mutex_enter(log);
        }

        return true;
      }

      /* Stop waiting if writer thread is dead. */
      if (log.should_stop_threads.load()) {
        if (!log_writer_is_active()) {
          return true;
        }
      }

      if (UNIV_UNLIKELY(
              log.writer_threads_paused.load(std::memory_order_acquire))) {
        return true;
      }

      if (wait) {
        log_flusher_mutex_exit(log);
        released = true;
      }

      return false;
    };

    if (srv_flush_log_at_trx_commit != 1) {
      const auto current_time = Log_clock::now();

      ut_ad(log.last_flush_end_time >= log.last_flush_start_time);

      if (current_time < log.last_flush_end_time) {
        /* Time was moved backward, possibly by a lot, so we need to
        adjust the last_flush times, because otherwise we could stop
        flushing every innodb_flush_log_at_timeout for a while. */
        log.last_flush_start_time = current_time;
        log.last_flush_end_time = current_time;
      }

      const auto time_elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              current_time - log.last_flush_start_time);

      ut_a(time_elapsed >= std::chrono::seconds::zero());

      const auto flush_every = get_srv_flush_log_at_timeout();

      if (time_elapsed < flush_every) {
        log_flusher_mutex_exit(log);

        /* When we are asked to stop threads, do not respect the limit
        for flushes per second. */
        if (!log.should_stop_threads.load()) {
          os_event_wait_time_low(log.flusher_event, flush_every - time_elapsed,
                                 0);
        }

        log_flusher_mutex_enter(log);
      }
    }

    const auto wait_stats = waiting.wait(stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_FLUSHER_, wait_stats);
  }

  if (log.write_lsn.load() > log.flushed_to_disk_lsn.load()) {
    log_flush_low(log);
  }

  ut_a(log.write_lsn.load() == log.flushed_to_disk_lsn.load());

  log_flusher_mutex_exit(log);
}

/** @} */

/**************************************************/ /**

 @name Log write_notifier thread

 *******************************************************/

/** @{ */

void log_write_notifier(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);

  log_t &log = *log_ptr;
  lsn_t lsn = log.write_lsn.load() + 1;

  log_write_notifier_mutex_enter(log);

  Log_thread_waiting waiting{log, log.write_notifier_event,
                             srv_log_write_notifier_spin_delay,
                             get_srv_log_write_notifier_timeout()};

  for (uint64_t step = 0;; ++step) {
    if (log.should_stop_threads.load()) {
      if (!log_writer_is_active()) {
        if (lsn > log.write_lsn.load()) {
          ut_a(lsn == log.write_lsn.load() + 1);
          break;
        }
      }
    }

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_acquire))) {
      ut_ad(log.write_notifier_resume_lsn.load(std::memory_order_acquire) == 0);
      log_write_notifier_mutex_exit(log);

      /* set to acknowledge */
      log.write_notifier_resume_lsn.store(lsn, std::memory_order_release);

      os_event_wait(log.writer_threads_resume_event);
      ut_ad(log.write_notifier_resume_lsn.load(std::memory_order_acquire) + 1 >=
            lsn);
      lsn = log.write_notifier_resume_lsn.load(std::memory_order_acquire) + 1;
      /* clears to acknowledge */
      log.write_notifier_resume_lsn.store(0, std::memory_order_release);

      log_write_notifier_mutex_enter(log);
    }

    log_sync_point("log_write_notifier_before_check");

    bool released = false;

    auto stop_condition = [&log, lsn, &released](bool wait) {
      log_sync_point("log_write_notifier_after_event_reset");
      if (released) {
        log_write_notifier_mutex_enter(log);
        released = false;
      }

      log_sync_point("log_write_notifier_before_check");

      if (log.write_lsn.load() >= lsn) {
        return true;
      }

      if (log.should_stop_threads.load()) {
        if (!log_writer_is_active()) {
          return true;
        }
      }

      if (UNIV_UNLIKELY(
              log.writer_threads_paused.load(std::memory_order_acquire))) {
        return true;
      }

      if (wait) {
        log_write_notifier_mutex_exit(log);
        released = true;
      }
      log_sync_point("log_write_notifier_before_wait");

      return false;
    };

    const auto wait_stats = waiting.wait(stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_WRITE_NOTIFIER_, wait_stats);

    log_sync_point("log_write_notifier_before_write_lsn");

    const lsn_t write_lsn = log.write_lsn.load();

    const lsn_t notified_up_to_lsn =
        ut_uint64_align_up(write_lsn, OS_FILE_LOG_BLOCK_SIZE);

    while (lsn <= notified_up_to_lsn) {
      const auto slot = log_compute_write_event_slot(log, lsn);

      lsn += OS_FILE_LOG_BLOCK_SIZE;

      log_sync_point("log_write_notifier_before_notify");

      os_event_set(log.write_events[slot]);
    }

    lsn = write_lsn + 1;

    if (step % 1024 == 0) {
      log_write_notifier_mutex_exit(log);

      std::this_thread::sleep_for(std::chrono::seconds(0));

      log_write_notifier_mutex_enter(log);
    }
  }

  log_write_notifier_mutex_exit(log);
}

/** @} */

/**************************************************/ /**

 @name Log flush_notifier thread

 *******************************************************/

/** @{ */

void log_flush_notifier(log_t *log_ptr) {
  ut_a(log_ptr != nullptr);

  log_t &log = *log_ptr;
  lsn_t lsn = log.flushed_to_disk_lsn.load() + 1;

  log_flush_notifier_mutex_enter(log);

  Log_thread_waiting waiting{log, log.flush_notifier_event,
                             srv_log_flush_notifier_spin_delay,
                             get_srv_log_flush_notifier_timeout()};

  for (uint64_t step = 0;; ++step) {
    if (log.should_stop_threads.load()) {
      if (!log_flusher_is_active()) {
        if (lsn > log.flushed_to_disk_lsn.load()) {
          ut_a(lsn == log.flushed_to_disk_lsn.load() + 1);
          break;
        }
      }
    }

    if (UNIV_UNLIKELY(
            log.writer_threads_paused.load(std::memory_order_acquire))) {
      ut_ad(log.flush_notifier_resume_lsn.load(std::memory_order_acquire) == 0);
      log_flush_notifier_mutex_exit(log);

      /* set to acknowledge */
      log.flush_notifier_resume_lsn.store(lsn, std::memory_order_release);

      os_event_wait(log.writer_threads_resume_event);
      ut_ad(log.flush_notifier_resume_lsn.load(std::memory_order_acquire) + 1 >=
            lsn);
      lsn = log.flush_notifier_resume_lsn.load(std::memory_order_acquire) + 1;
      /* clears to acknowledge */
      log.flush_notifier_resume_lsn.store(0, std::memory_order_release);

      log_flush_notifier_mutex_enter(log);
    }

    log_sync_point("log_flush_notifier_before_check");

    bool released = false;

    auto stop_condition = [&log, lsn, &released](bool wait) {
      log_sync_point("log_flush_notifier_after_event_reset");
      if (released) {
        log_flush_notifier_mutex_enter(log);
        released = false;
      }

      log_sync_point("log_flush_notifier_before_check");

      if (log.flushed_to_disk_lsn.load() >= lsn) {
        return true;
      }

      if (log.should_stop_threads.load()) {
        if (!log_flusher_is_active()) {
          return true;
        }
      }

      if (UNIV_UNLIKELY(
              log.writer_threads_paused.load(std::memory_order_acquire))) {
        return true;
      }

      if (wait) {
        log_flush_notifier_mutex_exit(log);
        released = true;
      }
      log_sync_point("log_flush_notifier_before_wait");

      return false;
    };

    const auto wait_stats = waiting.wait(stop_condition);

    MONITOR_INC_WAIT_STATS(MONITOR_LOG_FLUSH_NOTIFIER_, wait_stats);

    log_sync_point("log_flush_notifier_before_flushed_to_disk_lsn");

    const lsn_t flush_lsn = log.flushed_to_disk_lsn.load();

    const lsn_t notified_up_to_lsn =
        ut_uint64_align_up(flush_lsn, OS_FILE_LOG_BLOCK_SIZE);

    while (lsn <= notified_up_to_lsn) {
      const auto slot = log_compute_flush_event_slot(log, lsn);

      lsn += OS_FILE_LOG_BLOCK_SIZE;

      log_sync_point("log_flush_notifier_before_notify");

      os_event_set(log.flush_events[slot]);
    }

    lsn = flush_lsn + 1;

    if (step % 1024 == 0) {
      log_flush_notifier_mutex_exit(log);

      std::this_thread::sleep_for(std::chrono::seconds(0));

      log_flush_notifier_mutex_enter(log);
    }
  }

  log_flush_notifier_mutex_exit(log);
}

/** @} */

#endif /* !UNIV_HOTBACKUP */
