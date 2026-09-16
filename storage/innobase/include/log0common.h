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

#include <cstddef>
#include <cstdint>
#include <span>

/* Common definitions and data structures for Redo Log Handler artifacts */
namespace ib::redo {
/* We don't assume that Lsn is simply a byte lsn in the Log which conceptually
is an infinite array of bytes! So for example a range <start_lsn,end_lsn) can
contain less than end_lsn - start_lsn bytes of actual data. This is actually the
case for the Redo Log Handler which adds headers and trailers.
So, what Lsn really is? (Apart from a historical accident...)
It is a kind of a "handle" which Handler_interface implementation can
*easily* use to locate a particular location inside the log, guaranteed to be
monotone w.r.t. to position of these locations.
Such "handles" are reported by @see write_mtr(..., start_lsn, end_lsn).
Use @see compute_end_lsn(start_lsn, data_len) to identify the lsn
corresponding to the log position which is data_len after start_len.

Alternative explanation:

Let's introduce a helper concept of Sequence Number (SN): imagine all the
bytes of all the mtrs in the log are concatenated into a single sequence, then
SN would simply be a position of byte in this sequence.
If we imagine the same data to be interleaved with some padding (block headers
and footers) then a position in such a padded sequence is the Lsn.
There are two mappings:
lsn_to_sn : Lsn -> SN which is non-decreasing
sn_to_lsn : SN -> Lsn which is strictly increasing
That is Lsn may increase faster than SN (due to headers and footers between
actual data).
For all sn: lsn_to_sn(sn_to_lsn(sn))==sn
compute_end_lsn(start_lsn, data_len)==sn_to_lsn(lsn_to_sn(start_lsn)+data_len)
  */
using Lsn = uint64_t;

/** Additional error constants may be added to the list here, keeping in
mind the following guidelines about our approach to error handling:
1. If there's an error which looks like a violation of a contract between the
caller and the Redo Log Handler explained in this documentation - for example:
passing end_lsn smaller than start_lsn to read(..start_lsn,end_lsn,..) - then
it should result in a crash (assertion failure) rather than an error, as such
bugs should be handled at implementation stage, not at runtime.
2. Errors should be used for situations which reasonably could not be
expected and prevented by the caller, but are rather caused by particular
situation at runtime: for example trying to write beyond current capacity may
be an effect of not being aware that somebody has changed the capacity.
3. We acknowledge the gray area between these two. When in doubt, by default
return an error. Use assertions for things we are sure are bugs in code.
4. We should keep the number of Error constants not too large and not too
small. We should ask ourselves if the caller can really perform two distinct
actions in response to two different errors - if the caller doesn't really
care then it seems better to not introduce the distinction.
5. For human-readable logging, the Redo Log Handler might internally distinguish
various narrow conditions and report them in different ways. But the Error
codes here are meant for the caller, not the human operator. So, always ask
yourself if the code which uses the API really understands the concept the
error is about and can react to it. While the user may care that a given cloud
bucket is unreachable, as opposed to having wrong authentication token, what
the code really cares about is that it was a READ_ERROR.
6. If there is a violation of some code invariant (like it was detected that
an in-memory data structure pointer is null) it should be an assertion failure
and crash, as opposed to trying to communicate it to the caller.
7. In future we might want to distinguish between transient errors (for which
the caller might want to attempt retrying) and permanent errors. For now we
implicitly assume that all are transient, as otherwise there would be no point
in reporting them - caller would need to crash/stop anyway. This will evolve
as we understand better each and every case.
8. There's just one SUCCESS value and it is 0. So, for example if a function
may succeed in several ways, these should be conveyed by additional output
argument, so that the error handling pattern is always the same: non-zero
value means a problem.
9. Same error constant may be used by multiple functions if the name seems to
fit. (If we ever revisit this, then perhaps we should also have one enum per
function).
*/
enum class Status {
  SUCCESS = 0,
  STREAM_END,
  LOG_ALREADY_EXISTS,
  NO_LOG,
  COULD_NOT_CREATE,
  COULD_NOT_OPEN,
  ALREADY_TRUNCATED,
  WRONG_START_LSN,
  WRONG_STATE,
  READ_ERROR,
  WRITE_ERROR,
  NOT_WRITTEN_YET,
  FLUSH_ERROR,
  METADATA_IS_MISSING,
  WRITE_METADATA_ERROR,
  INCONSISTENT,
  COULD_NOT_RESIZE,
  TORN_STREAM_END,
};

using Buffer = std::span<uint8_t>;
using Const_buffer = std::span<const uint8_t>;
struct Const_buffers {
  Const_buffer const *buffers;
  size_t count;
};

}  // namespace ib::redo