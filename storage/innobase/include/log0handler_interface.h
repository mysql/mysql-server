/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once

#include <array>

#include "ha0sys_var_handler_interface.h"
#include "log0common.h"
#include "ut0dbg.h"

namespace ib::redo {

/**
This Redo Log Handler interface provides an abstraction over the redo log
persistence and publishing of the redo log records. The goal here is to wrap
the existing redo log functionality in the default implementation of this
interface. The default implementation of this interface is added in the
`Handler` class.
*/
class Handler_interface {
 public:
  /**
  Returns the lsn at the requested position that is the end of the MTR data.

  @param[in]  start_lsn LSN of the first byte of MTR data in the buffer.
  @param[in]  data_len  The Length of the MTR data.
  @return The LSN at the end of the MTR data in the buffer.
  */
  [[nodiscard]] virtual Lsn compute_end_lsn(Lsn start_lsn,
                                            size_t data_len) const = 0;

  /** @name Redo Log Handler's Capabilities */
  /** @{ */
  struct Capabilities {
    /** If true, then the implementation supports "atomic writes", which
    means that if any of the bytes passed to write_mtr(...) becomes available to
    read() then all of the bytes passed in this call are also available.
    In other words it's impossible for the Log to appear to end abruptly in
    between of boundaries of chunks passed to write_mtr(...).
    Note that it doesn't mean that write_mtr(...) call itself must succeed, or
    persist the data immediately - it only means that IF and WHEN the data
    becomes available to read() it must do so in an atomic way.
    Contrast this with implementation which has atomic_write=false, which may
    cause a prefix of a buffer passed to write_mtr(...) to become available to
    the read() - say, due to OS deciding to flush part of the cache to disk and
    to crash immediately afterwards - forcing the client to carefully analyze
    data returned by read(..) to detect such unfinished writes.
    */
    bool atomic_write;
    /** CLONE assumes direct access to files in specific location and format. */
    bool supports_clone;
    /** MEB assumes direct access to files in specific location and format. */
    bool supports_meb;
    /** Is ALTER INSTANCE DISABLE INNODB REDO_LOG supported? */
    bool supports_disabling;
    /** True if REDO log encryption is supported */
    bool supports_encryption;
  };

  /** Query the capabilities of the Redo Log Handler.
  This function can be called even before start().
  @return the capabilities of this Redo Log Handler */
  [[nodiscard]] virtual Capabilities get_capabilities() = 0;

  /** @} */

  /** @name Log access lifecycle */
  /** @{ */

  /** Request synchronous creation of Log starting at a given start_lsn.
  (For historical reasons InnoDB assumes that the first byte of redo log has a
  particular non-zero value)
  This function is meant to be called once per "installation" of mysql,
  typically when `mysqld --initialize` is executed or if the redo log is
  missing, but the tablespaces are expected to be fully updated,
  so redo log was logically empty anyway, such as during an upgrade scenario.

  If the log was already created before (say, before the crash/restart) this
  method should fail with Status::LOG_ALREADY_EXISTS.

  If the function returns SUCCESS the caller may assume that the current
  `start_lsn` is the most recently written and persisted position.

  @param[in]  start_lsn  The intended lsn to mark the beginning of the log
  @return error number or Status::SUCCESS */
  [[nodiscard]] virtual Status create(Lsn start_lsn) = 0;

  /** Informs Redo Log Handler that the caller intends to start writing to the
  log. If the log was not yet created, this function should return
  Status::NO_LOG.

  In case get_capabilities().atomic_write is true, the Redo Log Handler should
  verify that the start_lsn indeed is the end of the last persisted mtr in the
  log, and return Status::WRONG_START_LSN otherwise.

  In case get_capabilities().atomic_write is false, the Redo Log Handler should
  use the provided `start_lsn` to truncate the suffix of the log to start_lsn -
  the assumption here is that the caller performed recovery using read() API,
  and has identified the end of the last complete mtr, and anything beyond it is
  a prefix of a not completely persisted mtr and thus must be discarded. The
  Redo Log Handler might still perform some rudimentary sanity check such as if
  the provided start_lsn is not smaller than the last known mtr boundary in the
  persisted portion of the redo log, or smaller than the last truncate, and
  return Status::WRONG_START_LSN in such case.

  If the caller has already called start_writing(lsn) before, then this function
  should fail with Status::WRONG_STATE.

  In case get_capabilities().atomic_write is true, if the caller has called
  create(lsn), it should return Status::WRONG_START_LSN if lsn!=start_lsn.

  If the function return SUCCESS the caller may assume that the `start_lsn` is
  the most recently written and persisted position.

  @param[in]  start_lsn  The lsn the caller believes to be the current end of
                         the log and thus the position at which the next write
                         should start.
  @return error number or Status::SUCCESS */
  [[nodiscard]] virtual Status start_writing(Lsn start_lsn) = 0;

  /** Informs Redo Log Handler that the caller no longer intends writing to the
  log. */
  virtual void stop_writing() = 0;

  /** Performs any clean up (assumes stop_writing() was already called if
  start_writing() was) */
  virtual ~Handler_interface() = default;

  /** @} */

  /** @name Capacity */
  /** @{ */

  /** InnoDB uses `write_mtr(data,..)` in mtr.commit() when a thread still holds
  latches on pages involved in the mtr and perhaps other resources. The
  `write_mtr(..)` may block in case where it can't buffer any more data, and has
  to write buffered data out, but can't because there's no space left. Such wait
  under latches is bad for performance, can lead to a crash if continues for
  long ("long semaphore wait") or even deadlock (if the space can't be
  reclaimed, because checkpoint can not be advanced, because page cleaners can
  not write dirty pages to disc, because they can not sx-latch them, because
  they are still in use by an mtr - either the one doing `write_mtr(..)`, or by
  one which indirectly waits for some other page or resource which is held by
  that one).

  To prevent or at least minimize the chance of such wait happening, InnoDB is
  calling `wait_for_space()` from any thread which wishes to perform
  `write_mtr(..)`. The caller of `reconfigure(..)` promises there are at most
  `max_threads` such threads.

  A call to `wait_for_space()` can block, and must be done while not holding any
  latches, so that waiting will not cascade to other threads. This also implies,
  that the caller of `wait_for_space()` should not be in a middle of an mtr
  which holds any latches. The caller of `reconfigure(..)` promises that a
  thread which has called `wait_for_space()` is permitted to pass at most
  `reserved_bytes_per_thread` bytes to subsequent calls to `write_mtr(..)`
  and should call `wait_for_space()` again whenever it needs to write more.
  For this to work properly the caller of `wait_for_space()` must keep in mind
  that redo logs accumulated inside an mtr_t object are not properly accounted
  for by the Handler - the simplest way to avoid problems is to not call
  `wait_for_space()` while in a middle of an mtr, i.e. before mtr.start(), which
  is the recommended pattern.

  Additionally InnoDB promises that a thread may call persist_smaller_than()
  at most once after each call to wait_for_space().

  Under these assumptions, the Handler will do its best to ensure that calls to
  `write_mtr(..)` will not block, instead throttling the callers of
  `wait_for_space()` as needed.

  The way this is achieved is up to the Handler, but the contract assumes a
  model in which Handler returns from `wait_for_space()` only when there's a
  "margin" sufficient to write `reserved_bytes_per_thread` bytes `max_thread`
  times ahead of the value of `peek_first_unassigned_lsn()` which the Handler
  has observed at some moment during the call chosen by the Handler. This
  phrasing is carefully chosen, as the way this translates to the number of
  bytes needed on physical storage involves Handler's internal details such as
  headers, footers and padding, which in turn might depend on frequency of
  actually persisting data to disc. Note the Handler is allowed (in fact
  encouraged to) to note the `peek_first_unassigned_lsn()` first, then add the
  margin to see where it would end physically, and only then start waiting for
  sufficient amount of old redo logs to be freed, completely ignoring any redo
  logs which were newly written in parallel while it waits.

  Proof that this approach works is by contradiction: suppose `write_mtr(..)`
  blocks, despite everyone following the contract. Each finished call to
  `wait_for_space()` (or a successful call to `has_space()` which from now on
  will be treated as very quick `wait_for_space()` for simplicity) is associated
  with a value of `peek_first_unassigned_lsn()` which the Handler used during
  that call. Let X be the maximum of these associated lsns. Consider all the
  redo logs above X: they were produced by calls to `write_mtr(..)` from various
  threads. If any thread finished a call to `wait_for_space()` after one of
  these `write_mtr(..)` calls, then the Handler would have to use
  `peek_first_unassigned_lsn()` value which is at least equal to `end_lsn`
  assigned by `write_mtr(..,end_lsn)`, which would be larger than X, because we
  focus only on `write_mtr(..,end_lsn)` calls which have end_lsn greater than X.
  But this would mean this `wait_for_space()` is associated with an lsn larger
  than X - which contradicts definition of X! So, we have shown that for each
  finished `write_lsn(..,end_lsn)` with X<end_lsn, there is no finished call to
  `wait_for_space()` from the same thread sequenced after such `write_lsn()`.
  This also applies to the thread which is supposedly blocked in `write_mtr(..)`
  - if it did successful `write_mtr(..)` above X, then it too, could not call
  `wait_for_space()` later. This means all the calls to `write_mtr(..)` above X,
  including the one which supposedly block, come from threads which didn't call
  `wait_for_space()` in between, there are at most `max_threads` of them, each
  is allowed to write at most `reserved_bytes_per_thread`, and call
  `persist_smaller_than(..)` at most once above X. By returning from
  `wait_for_space()` call which is associated with X, the Handler promised, that
  all of this should fit above X. Yet, we have to wait in `write_mtr(..)` - the
  contradiction ends the proof.

  The Handler might be unable to satisfy the contract due to the physical
  constraints such as the permitted max size of files. If so, then the Handler
  should still do its best to prevent blocking in `write_mtr(..)`, but should
  return false from `reconfigure(..)` to warn InnoDB that the overall
  configuration is not "safe".

  @param[in]     max_threads
                     The maximum number of threads which can call
                     `write_mtr(..)`.
  @param[in]     reserved_bytes_per_thread
                     The maximum number of bytes a thread may pass to
                     `write_mtr(..)` after a single call to `wait_for_space()`.
  @return True iff the Handler promises to honor the contract
  */
  [[nodiscard]] virtual bool reconfigure(size_t max_threads,
                                         size_t reserved_bytes_per_thread) = 0;

  /** Describes the state of the capacity in sufficient detail that InnoDB knows
  if it should rush with page cleaning and checkpointing. It depicts a
  hypothetical state at the border between has_space() returning true and false,
  by providing how much of the redo log history could be retained in this state,
  and how much of space is reserved for the margin needed by wait_for_space().*/
  struct Capacity_estimate {
    /** The estimated lower bound on the maximum difference between
    peek_first_unassigned_lsn() and oldest still not removed lsn, which the
    Handler could keep. */
    Lsn max_history_length;
    /** The length of the lsn range reserved by wait_for_space(). */
    Lsn margin_length;
    /** The so called Soft Logical Capacity, which is the lower bound on the
    length of the lsn range this Handler can provide to the InnoDB, including
    the margin needed for wait_for_space(). "Soft" is in contrast to "Hard"
    limit which might be a little larger, but not exposed to InnoDB, and is the
    real limit which can't be exceeded for technical reasons, even if the
    contract of reconfigure(..) and wait_for_space(..) is not followed by
    InnoDB. "Logical" is in contrast to "Physical" which is the actual number of
    bytes taken on disc by the data structures.

    Always equal to max_history_length + margin_length. */
    [[nodiscard]] Lsn soft_logical_capacity() const {
      return max_history_length + margin_length;
    }
  };
  /** Used by InnoDB to determine if page cleaning and checkpointing should be
  speed up, because it the lagging checkpoint lsn is the reason we run out of
  capacity. In order to do that InnoDB is checking what fraction of
  estimate.soft_logical_capacity() is used up by maintaining redo logs needed
  by InnoDB, which is:
       peek_first_unassigned_lsn() + estimate.margin_length - checkpoint_lsn.

  Note: in absolute terms the difference would be the same if we compared
  estimate.max_history_length to peek_first_unassigned_lsn() - checkpoint_lsn,
  but for backward-compatibility of all arithmetic expressions InnoDB needs to
  know the percentage of space usage including the margin.

  As the percentage grows larger, InnoDB employs more and more aggressive page
  cleaning and checkpoint updating.

  Note: in practice, due to imperfections of following the contract, the
  percentage may be even larger than 100%. This is handled fine.

  In order to estimate the capacity in a way which is most useful for
  cooperation with InnoDB, the Handler should imagine reaching a state which is
  at a boundary between has_space() returning true or false. Note that
  has_space() takes into account the margin needed for concurrency, and data
  needed by all consumers known to the Handler, not just InnoDB. In particular,
  there are two cases:

  (1) If has_space() would return true if called now, then the Handler should
  try to guess how much more data can be passed to write_mtr(..), before it will
  return false. The returned estimate.max_history_length should equal
  "lsn at which has_space() would return false" - "oldest needed lsn now".

  (2) If has_space() would return false now, then imagine how much would data
  would have to be truncated from disc, before it could return true. The
  returned estimate.max_history_length should equal
  "peek_first_unassigned_lsn() now" - "oldest needed lsn after such truncation".

  As for the returned estimate.margin_length it should be the value the Handler
  uses in wait_for_space() computed based on arguments to reconfigure(..).
  As explained above, the way this value participates in computation affects
  both the denominator and numerator in additive way. */
  [[nodiscard]] virtual Capacity_estimate get_capacity_estimate() = 0;

  /** Called from a thread which wishes to pass no more than
  `reserved_bytes_per_thread` of data to `write_mtr(..)` in future.
  InnoDB should prefer to call log_free_check() wrapper instead, as it performs
  additional verification that the caller shouldn't hold latches.
  @see reconfigure(size_t max_threads, size_t reserved_bytes_per_thread).
  There might be at most max_threads using `write_mtr(..)` in InnoDB.
  The function is called when not holding any latches, and may block until the
  space for at most reserved_bytes_per_thread is "reserved" for the thread.
  Note there is no way to "release" the "reserved" space, as the mechanism is
  rather sophisticated, and assumes an upper bound on the number of threads and
  number of bytes is enforced by the caller.

  The Handler should behave as if it has noted lsn=peek_first_unassigned_lsn(),
  and waited for a moment when it could guarantee, that `max_threads` can each
  pass `reserved_bytes_per_thread` to `write_mtr(..)` calls above that lsn
  without blocking. @see reconfigure for more details.
  In case `reconfigure` returned false, the Handler can violate this contract,
  but still, should do its best to avoid blocking in `write_mtr(..)`.

  Note: in theory you can call it while inside an mtr if it has not yet latched
  any page, and has no accumulated redo logs or you properly include the
  already accumulated redo logs within the reserved_bytes_per_thread limit.
  In practice, it is much simpler and easier to reason about if this function is
  called *before* mtr.start(). */
  virtual void wait_for_space() = 0;

  /** This is a no-wait variant of wait_for_space(), i.e. in case
  wait_for_space() would not have to actually wait, returns true, and the caller
  might proceed to write at most reserved_bytes_per_thread as if it called
  wait_for_space(). If this function returns false, the caller must call
  wait_for_space() before calling write_mtr. As this function doesn't block, the
  caller is permitted to hold latches when calling it. Still, the caller should
  rather not be in a middle of an mtr, unless they properly include the redo
  logs already accumulated within the mtr object in the limit of
  reserved_bytes_per_thread.
  @return True iff the caller may proceed to pass reserved_bytes_per_thread
  bytes to write_mtr without calling wait_for_space(). */
  [[nodiscard]] virtual bool has_space() = 0;

  /** @} */

  /** @name Log Read */
  /** @{ */

  /** Informs the Redo Log Handler that InnoDB intends to start reading from the
  log. In principle it can be a no-op, but in case of InnoDB it actually has a
  side effect of checking if the files on disc are in correct format/state, and
  if they are not fully initialized, or in old format, but other than that fine
  and logically empty, then InnoDB will remove them and pretend they were
  missing.

  @retval SUCCESS means that there were no issues, and InnoDB can start reading
  @retval COULD_NOT_OPEN means that the log is missing and the caller should now
  simply call create(..) and everything should be fine then.
  @retval READ_ERROR means that the log is somehow broken and can not be read.
  This is a serious error, which might require manual user action to fix. */
  [[nodiscard]] virtual Status start_reading() = 0;

  /** Each call to write_mtr(..., &start_lsn, &end_lsn), or create(start_lsn),
  declares start_lsn to be a boundary between mtrs. Some of such boundaries may
  be remembered by the Redo Log Handler in its auxiliary metadata (such as block
  headers) to help to locate a starting point for read operations. This is very
  important during InnoDB's recovery process, because InnoDB doesn't store the
  exact checkpoint_lsn value, and the serialization format of mtr doesn't
  provide a way to start reading from the middle of an mtr, thus InnoDB needs
  help in finding a boundary between mtrs in the long stream of bytes.

  It is expected that the set of known boundaries is dense enough, that for any
  given lsn, align_down_to_known_boundary(lsn)-lsn is small enough that starting
  with read(align_down_to_known_boundary(checkpoint_lsn),...) and then parsing
  mtrs until we reach checkpoint_lsn (using compute_end_lsn to track progress)
  will not require too much overhead.

  @return Known boundary LSN if the caller promises to call this function with
  lsn not smaller than the start_lsn passed to create(start_lsn), nor smaller
  than the lsn passed to do_not_need_smaller_than(lsn). In other words, the
  implementation must not forget the oldest boundary LSN that is still needed.
  Under above conditions, this function must return an mtr boundary not larger
  than the end_lsn of the mtr which contains the lsn provided by the caller.
  Note: this means returned value might be slightly larger than lsn, sometimes.
  If this function doesn't know any mtr boundary which satisfies this condition,
  which should happen only if the caller violated the contract, it may return 0.
  */
  [[nodiscard]] virtual Lsn align_down_to_known_boundary(Lsn lsn) = 0;

  /** Reads previously persisted portion of the Log starting from start_lsn
  synchronously.
  By "persisted portion" we mean: if this read() call returns some byte value
  for a given LSN, then all subsequent calls to read() must return the same byte
  value for this LSN unless it was already truncated. Note that in case of
  get_capabilities().atomic_write==false, the start_writing(start_lsn) call may
  truncate a suffix (from start_lsn onwards) of the log.

  This function will return an error if the start_lsn is already truncated.
  This function attempts to read as much mtr data as possible to fill the
  provided buffer, but for performance reasons might decide to return just an
  initial fragment of the data before filling the buffer completely.
  In such case buffer.size is adjusted by the API to the actual number of bytes
  read into the buffer.data.
  Thus the caller should always check the new value of buffer.size on SUCCESS to
  know how much data was actually read into the buffer.
  A Status::SUCCESS implies that at least one byte was read.

  This is in contrast to Status::STREAM_END which means that the end of the Log
  was reached and no data was read into buffer.data.
  In other words Status::STREAM_END means that start_lsn is greater than the
  last persisted lsn and thus no data can be read.
  In case of return value other than Status::SUCCESS the buffer.size will be set
  to 0, and no data will be written to the buffer.data.
  The Status::TORN_STREAM_END is just like STREAM_END in that no data was read
  into buffer, but additionally conveys that Redo Log Handler failed to reach
  the end of stream marker. The caller may use this as a hint that the observed
  end of stream is not a result of a clean shutdown and that some of the data
  passed to write_mtr(..) wasn't successfully persisted.
  If get_capabilities().atomic_write is true, then the end of Log must be right
  after one of the previous write_mtr(...,end_lsn) calls, that is
  compute_end_lsn(start_lsn, buffer.size)==end_lsn.
  Conversely, if get_capabilities().atomic_write is false, then:
  * Status::STREAM_END might be reached in a middle of a range written by
  write_mtr(..), which also suggests an unclean shutdown, but at least we know
  that no persisted data was later lost due corruption, as the end of stream
  marker was successfully reached.
  * Status::READ_ERROR means that the Handler itself could not obtain the
  information it needed to determine what's going on, so the caller shouldn't
  start the server, but rather try to fix the root cause of I/O issues and try
  again.
  * OTOH The Status::TORN_STREAM_END means that the Handler has read the
  data which indicate that the data from recent write got torn (not written
  successfully) so the readable data definitely ends here but also its visible
  that there was some attempt to write data after it - this issue is not
  temporary or fixable on the one hand, but also a bit expected and benign on
  the other.
  To give a few examples: file access error, or network error, would be a
  READ_ERROR. OTOH successfully reading a block from disc or network,
  which has a header with a checksum field not matching the crc32 of its body is
  a TORN_STREAM_END, because the underlying low-level read has succeeded, but
  the content read clearly indicates there was some failure during write.

  If there is no data available the function should report Status::STREAM_END
  immediately (as opposed to: waiting for more data to be written).
  Note: because persist_smaller_than(x,..) returns successfully only if the
  x-1 belongs to the "persisted portion" in the sense defined above, it follows,
  that data up to and including x-1 should be available for read(..).
  The read() API can return data even for lsn greater or equal than the x passed
  to persist_smaller_than(x), but only under condition that it was previously
  passed to write_mtr() (as opposed to being some made up data) and that if any
  lsn become available to read() then all lsns before it also become available -
  in other words: there are no "holes".
  (Note: there's no requirement that the write_mtr() has successfully returned
  to the caller. That is a write_mtr() can succeed even if the caller doesn't
  know that)
  Additionally, if get_capabilities().atomic_write is true, then if any lsn
  become available to read() then all other bytes from the same mtr passed to
  write_mtr(mtr,..,) must be available to read(..) as well - in other words
  writes are "atomic". To put it in yet another way: if mtr_start_lsn is the
  first lsn of a given mtr, then the compute_end_lsn(lsn,mtr.data_len)-1 should
  also be readable.

  Because the ib::redo::Handler assigns Lsns to headers and footers, and
  read() function retrieves only the bytes of mtrs' bodies, the caller should
  use compute_end_lsn(start_lsn,i) to compute the lsn value of buffer.data[i].
  The buffer.data will not contain any headers or footers, just the mtr data.

  @param[in]     start_lsn  The position from which the read operation should
                            start retrieving bytes (included in the range)
  @param[in,out] buffer     The buffer to store the data read. The buffer.data
                            must point to an array of at least buffer.size bytes
                            owned by the caller. The read(...) function will
                            store up to buffer.size bytes in the buffer.data,
                            starting from buffer.data[0], and will update the
                            buffer.size to match the actual number of bytes
                            written to it.
                            The buffer.data does not need to be aligned.
  @retval Status::SUCCESS
          The buffer.size upon return is positive and not larger then before the
          call, and buffer.data contains buffer.size bytes successfully read
          from the redo log stream.
  @retval Status::STREAM_END
          The buffer.size upon return is 0, indicating, there's no more data in
          the stream to be read.
  @retval Status::TORN_STREAM_END
          The buffer.size upon return is 0, indicating, there's no more data in
          the stream to be read. Additionally, the handler has determined (by
          internal consistency checks such as comparing crc32 of block body to
          the checksum in its header) that there was a torn write attempt at
          start_lsn.
  @retval Status::ALREADY_TRUNCATED
          The start_lsn is older than the currently available redo log data.
  @retval Status::READ_ERROR
          The data could not be fetched due to I/O problems. Therefore it is
          unclear if end of stream was reached or not, but there is no more data
          that is readable now, and buffer.size is 0 upon return.
  */
  [[nodiscard]] virtual Status read(Lsn start_lsn, Buffer &buffer) = 0;

  /** @} */

  /** @name Log Write (only used after calling start_writing(...)) */
  /** @{ */

  /** Request an asynchronous append of the mtr's body's bytes to the log.
  The successful call to this function should also assign start_lsn and end_lsn.

  By "append" we also mean that the Redo Log Handler has to promise, that if
  any of the bytes ever get available to the read(...) operations, then all the
  bytes before it are also available - in other words: there are no "holes", but
  some suffix might be missing (for example due to a crash).
  Additionally, if get_capabilities().atomic_write is true, the write_mtr(..)
  should be atomic.

  By "atomic" we mean, that if any of the bytes ever get available to the
  read(..) operations, then all of the other bytes from the same write_mtr(...)
  call should also be available - in other words: it's all-or-nothing.

  By "asynchronous" we mean that the Redo Log Handler doesn't have to
  immediately persist the data to the storage medium. It has time up until
  persist_smaller_than(..) will request it. In case
  get_capabilities().atomic_write is false, Redo Log Handler may also persist
  data in fragments not aligned with write_mtr()'s end_lsn. Otherwise, persisted
  fragment should always be aligned with one of such boundaries.
  If a failure to persist happens some time after the return from this function,
  then persist_smaller_than() should fail with an error.
  The return value should be used to indicate errors determined during the call.

  In case the write_mtr() would require capacity larger than the one currently
  available the Redo Log Handler it should either block or crash, but it should
  not return to the caller. The caller may use get_capacity_estimate() to
  minimize the chance of this happening.

  The caller must call start_writing(...) before calling this function,
  otherwise it may return Status::WRONG_STATE.

  Due to headers and footers in the Redo Log Handler, the end_lsn - start_lsn,
  may be larger than the number of bytes in mtr_data, but end_lsn should equal
  compute_end_lsn(start_lsn, sum of mtr_data[i].count).

  @param[in]     mtr_data   The mtr's data which should be added to the log.
  @param[out]    start_lsn  The position assigned to the first byte by the Redo
                            Log Handler (included in the range).
  @param[out]    end_lsn    The next position after the one assigned to the last
                            byte of the mtr (excluded from the range).
  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status write_mtr(const Const_buffers &mtr_data,
                                         Lsn &start_lsn, Lsn &end_lsn) = 0;

  /** Intuitively returns the largest end_lsn assigned from an
  write_mtr(..,&end_lsn) call. Note that calls to write_mtr() may return in an
  order different than the order of end_lsn values returned by them.
  Because there might be concurrent threads which call write_mtr() at any
  moment, the value returned from this function may be already smaller than an
  end_lsn already seen by some other thread. Hence it is only a lower bound.

  If the caller knows that a return from write_mtr(..,&end_lsn) has
  happened-before the call to this function, then the return value must be at
  least end_lsn.

  If the caller knows that a write_mtr(..,&start_lsn,..) call happens-after the
  return from this function, then the return value must be at most start_lsn.

  The caller must call start_writing(lsn) before calling this function,
  otherwise it may return Status::WRONG_STATE. This is because at least in some
  possible implementations, the exact lsn at which log ends can only be
  established by performing recovery (reading through whole log till its end),
  and this is expensive, and requires cooperation with the caller.

  */
  virtual Lsn peek_first_unassigned_lsn() = 0;

  /** Used by persist_smaller_than(...origin) and persist_available(...origin)
  to let the caller specify the context in which the call is being made, so that
  an implementation can perform context-specific actions - such as bumping
  relevant stats. */
  enum class Origin {
    /** The call occurs during transaction commit. */
    TRX_COMMIT,
    /** The call occurs due to page cleaning activity. */
    PAGE_FLUSHING,
    /** The call occurs for some other reason. */
    OTHER,
  };

  /** Used by persist_smaller_than(...desired_guarantee) to let the caller
  specify the desired guarantee about persistence before the call returns */
  enum class Durability {
    /** A lower level of durability, which only ensures that the data written,
    should survive, even if the mysqld process crashes, but does not ensure that
    in case of a failure of something outside the process, such as a crash of
    the whole OS, or disc, or machine, or a part of Redo Log Handler which is
    outside current process, say network to its server etc. */
    OUTLIVE_PROCESS,
    /** The default level of durability, which ensures that the data was safely
    written to a stable storage, so that even a crash or unplugging the cord
    can prevent the data from being readable. Of course if the stable storage is
    itself destroyed, then nothing can help. */
    FULLY_PERSISTED,
  };

  /** A synchronous request for the Redo Log Handler to promise that previously
  written data is durable at least up to but not including the end_lsn.
  By "synchronous" we mean that this function can not return until it can
  guarantee the data is available to subsequent read(...) calls even after crash
  (or an error occurs).

  If desired_guarantee is FULLY_PERSISTED the data must reach a secure storage,
  such that no failure (except of a future damage of the storage itself) can
  prevent recovery of the data. This is the default behaviour.

  The desired_guarantee can also be OUTLIVE_PROCESS. The Redo Log Handler might
  simply implement it same way as FULLY_PERSISTED. But, for performance reasons
  it can choose a faster implementation which does not achieve full persistence,
  but only a weaker set of guarantees:
  Provided that the crash/failure is limited just to the mysqld process issuing
  this call, the lsns below end_lsn should be readable after mysqld restarts.
  Provided that nothing (neither mysqld, nor Redo Log Handler, nor OS, etc.)
  crashed the lsns below end_lsn should be readable after this function returns.

  NOTE: This spec of OUTLIVE_PROCESS is a bit fuzzy, but it tries to capture the
  expectations needed by places which called log_write_up_to(...,sync=false):
  Arch_Log_Sys::wait_archive_complete()
      which basically wants to read the redo log files to transmit them over
      network, so it is sufficient for these files to be considered "written" by
      the OS, because, any crash would terminate the cloning process anyway, and
      if there is no crash, then reads should see the writes even if they are
      still in OS cache.
  trx_flush_log_if_needed_low()
      which does not use fsync in cases of:
      srv_unix_file_flush_method == SRV_UNIX_NOSYNC
          which is not officially supported setting used for testing performance
      srv_flush_log_at_trx_commit == 2
          which is meant to offer a compromise on ACID in which the redo log
          should be recoverable if the crash was limited to the mysqld process
          (but the OS, disc, machine survived, so had a chance to fsync).
  innobase_flush_logs()
      which does not use fsync in case of being called from
      MYSQL_BIN_LOG::fetch_and_process_flush_stage_queue() or
      Commit_order_manager::flush_engine_and_signal_threads()
      when srv_flush_log_at_trx_commit == 2, because it only needs to provide
      guarantees similar to those described for trx_flush_log_if_needed_low().


  Calling it with end_lsn larger than the end_lsn returned by the most recent
  successful write_mtr(..,start_lsn,end_lsn) (or start_lsn passed to create()
  or start_writing() in case no write_mtr() was performed yet) should fail with
  Status::NOT_WRITTEN_YET.

  The caller must call start_writing(lsn) before calling this function,
  otherwise it may return Status::WRONG_STATE.

  @param[in]  end_lsn
  The caller wants to wait for all non-truncated bytes at lsns strictly smaller
  than end_lsn to become persisted.

  @param[in]  desired_guarantee
  The desired durability guarantee.

  @param[in]  origin
  The location from which the call has occurred, which can be used for
  statistics and other diagnostics

  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status persist_smaller_than(
      Lsn end_lsn, Durability desired_guarantee = Durability::FULLY_PERSISTED,
      Origin origin = Origin::OTHER) = 0;

  /** Similar to persist_smaller_than(end_lsn, origin), except that the caller
  politely asks the Redo Log Handler to persist all that it can persist, without
  specifying a specific end_lsn as the caller does not really care about any
  specific lsn being persisted, it just wants to ensure that the value of
  peek_first_nonpersisted_lsn will become the largest possible now.

  @param[in]  origin
  The location from which the call has occurred, which can be used for
  statistics and other diagnostics

  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status persist_available(
      const Origin &origin = Origin::OTHER) = 0;

  /** Intuitively returns the largest end_lsn passed to a successfully finished
  persist_smaller_than(end_lsn) call. Because there might be concurrent threads
  which call persist_smaller_than() at any moment, the value returned from this
  function may be already smaller than an end_lsn already passed by some other
  thread. Hence it is only a lower bound.
  Also, the Redo Log Handler might voluntarily persist next portion of the
  log in the background even if persist_smaller_than() wasn't called, so the
  returned value doesn't really have to be equal to the most recent value passed
  to persist_smaller_than(end_lsn).
  Also, for get_capabilities().atomic_write=true case, the persisted range of
  lsns is always aligned with mtr boundary, which implies that the Redo Log
  Handler has to persist more than requested when unaligned lsn is passed to
  persist_smaller_than(lsn).

  If the caller knows that a return from persist_smaller_than(end_lsn) has
  happened-before the call to this function, then the return value must be at
  least end_lsn.

  The caller must call start_writing(lsn) before calling this function,
  otherwise it may return Status::WRONG_STATE. This is because at least in some
  possible implementations, the exact lsn at which persisted fragment of the log
  ends can only be established by performing recovery (reading through whole log
  til its end), and this is expensive, and requires cooperation with the caller.

  */
  [[nodiscard]] virtual Lsn peek_first_nonpersisted_lsn() = 0;

  /* An asynchronous permission to delete a prefix of the Log up to but not
  including align_down_to_known_boundary(needed_lsn) [That is, the byte at
  align_down_to_known_boundary(needed_lsn) should not be removed]. This is the
  way in which InnoDB lets the Redo Log Handler know that it no
  longer needs data below this lsn. It should return immediately even if the
  garbage collection process takes longer.

  The caller must call start_writing(lsn) before calling this function,
  otherwise it may return Status::WRONG_STATE.

  It is an error to pass needed_lsn which is larger than last persisted lsn+1.
  (Last persisted lsn is at least as large as the end_lsn-1 where end_lsn is the
  value passed to last successful persist_smaller_than(end_lsn,...) call. Taken
  together it means it is ok to call do_not_need_smaller_than(end_lsn) but a
  larger value may fail with Status::NOT_WRITTEN_YET.) The Redo Log Handler can
  delete data up to and including needed_lsn-1, or some arbitrarily shorter
  prefix of it, which the user of the API will not be able to tell anyway,
  because by calling do_not_need_smaller_than(x) the caller promises to never
  call read(..start_lsn,..) with start_lsn < align_down_to_known_boundary(x)
  - such a read may fail with Status::ALREADY_TRUNCATED. In particular,
  the Redo Log Handler does not need to ensure that data is removed exactly up
  to x nor to a position aligned to a write boundary - any value
  y <= align_down_to_known_boundary(x) should be fine, as it is a responsibility
  of the user of the API to correctly find a
  point >= align_down_to_known_boundary(x) at which it wants to start reading.
  NOTE: Calling it with a value smaller than the one passed to an earlier call
  to this function is permitted and results in immediate Status::SUCCESS, but
  may indicate some issue in the InnoDB's implementation.

  The Redo Log Handler must ensure that after truncation there will still be at
  least one known mtr boundary with lsn <= needed_lsn - i.e. that a call to
  align_down_to_known_boundary(needed_lsn) will not fail. One way to achieve it
  is to avoid truncating past the last known boundary before needed_lsn.

  @param[in] needed_lsn  The smallest needed lsn. All data at smaller lsns can
                         be deleted by Redo Log Handler, as the user of the API
                         will not attempt to ever read it again.
  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status do_not_need_smaller_than(Lsn needed_lsn) = 0;

  /** @} */

  /** @name Log Metadata */
  /** @{ */

  /** We abstract the Log Metadata - the data associated with the whole Log, as
  opposed to the data stored in the log's stream itself which is accessed by
  write() and read()) - as Blocks of bytes associated with keys 0,..,MAX_KEY,
  which the Redo Log Handler does not interpret in any way, just atomically
  stores and retrieves when requested. There are exactly MAX_KEY+1 Blocks, 512
  bytes each. */

  /** The maximum possible value for first argument to store_metadata(key,..)
  and load_metadata(key,..). */
  static constexpr uint16_t MAX_KEY = 1;

  /** Each metadata block has the same fixed size of 512 bytes. */
  static constexpr uint32_t METADATA_BLOCK_SIZE = 512;
  using Metadata_value = std::array<unsigned char, METADATA_BLOCK_SIZE>;

  /** Persists synchronously the metadata atomically overwriting the old value
  for a given key.
  By "atomically" we mean that the Redo Log Handler has to ensure that in case
  of a crash either the old value of metadata or the new value of metadata will
  be returned for the given key (no "torn writes"). By "synchronous" we mean
  that the call can not return to the caller until the operation is complete (or
  failed). In case of success, calls to get_metadata(key) should see this or
  newer value.

  It must be called only after successful create() or start_writing().

  @param[in]  key      A value in range 0,...,MAX_KEY inclusive.
  @param[in]  value    The 512 bytes to be persisted as associated with the key.
  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status store_metadata(uint16_t key,
                                              const Metadata_value &value) = 0;

  /** Retrieves the previously stored metadata block for a given key.
  The Redo Log Handler must guarantee that if the get_metadata(key, &r)
  happens-after the call to store_metadata(key, &w), then the content of r will
  be that of w, or from some later store. If there was no previous
  store_metadata for a given key, should fail with Status::METADATA_IS_MISSING.

  It must be called only after successful start_reading(...) or start_writing().

  @param[in]   key      A value in range 0,...,MAX_KEY inclusive.
  @param[out]  value    The 512 bytes buffer to store the retrieved metadata.
  @return error number or Status::SUCCESS
  */
  [[nodiscard]] virtual Status get_metadata(uint16_t key,
                                            Metadata_value &value) = 0;

  /** @} */

  /** Returns the handler to configure the redo log related system variables.

  @return Object of type Sys_var_handler_interface
  */
  [[nodiscard]] virtual Sys_var_handler_interface &config_handler() = 0;
};

/** Sets the concrete Redo Log Handler to be used.
 @param[in] handler A Redo Log Handler Object */
void set_handler(Handler_interface *handler);

extern Handler_interface *handler;

}  // namespace ib::redo

/** A wrapper for ib::redo::handler->wait_for_space(), which should be used in
InnoDB code, as it verifies that InnoDB doesn't hold latches which could cause
deadlocks. @see ib::redo::Handler_interface::wait_for_space(). */
void log_free_check();
