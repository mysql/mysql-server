/*****************************************************************************

Copyright (c) 2021, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/detail/ut/pfs.h
    Implementation bits and pieces for PFS metadata handling. Shared by
    different allocators.
 */

#ifndef detail_ut_pfs_h
#define detail_ut_pfs_h

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>

#include "mysql/psi/mysql_memory.h"

namespace ut {
namespace detail {

/** Memory layout representation of PFS metadata segment that is used by
    the allocator variants which also want to trace the memory consumption
    through PFS (PSI) interface.

     --------------------------------------------------
     | PFS-META | VARLEN | PFS-META-OFFSET |   DATA   |
     --------------------------------------------------
      ^    ^                                ^
      |    |                                |
      |   ---------------------------       |
      |   | OWNER |  DATALEN  | KEY |       |
      |   ---------------------------       |
      |                                     |
   ptr returned by                          |
   Aligned_alloc_impl                       |
                                            |
                               ptr to be returned to call-site
                                   will be pointing here

   PFS-META is a segment that will hold all the necessary details one needs
   to otherwise carry around in order to exercise the PFS memory tracing.
   Following data will be serialized into this segment:
     * Owning thread
     * Total length of bytes allocated
     * Key

   VARLEN is the leftover variable-length segment that specialized
   implementations can further make use of by deducing its size from the
   following formulae: requested_alignment - sizeof(PFS-META-OFFSET) -
   sizeof(PFS-META). In code that would be alignment -
   PFS_metadata::size. Not used by this implementation.

   PFS-META-OFFSET is a field which allows us to recover the pointer to PFS-META
   segment from a pointer to DATA segment.

   DATA is an actual segment which will keep the user data.
 */
struct PFS_metadata {
  /** Convenience types that we will be using to serialize necessary details
      into the Aligned_alloc metadata (allocator and PFS) segments.
   */
  using pfs_owning_thread_t = PSI_thread *;
  using pfs_datalen_t = std::size_t;
  using pfs_memory_key_t = PSI_memory_key;
  using pfs_meta_offset_t = std::uint32_t;
  using data_segment_ptr = void *;

  /** Metadata size */
  static constexpr auto meta_size = sizeof(pfs_memory_key_t) +
                                    sizeof(pfs_owning_thread_t) +
                                    sizeof(pfs_datalen_t);
  static constexpr auto size = meta_size + sizeof(pfs_meta_offset_t);

  /** Helper function which stores the PFS thread info into the OWNER field. */
  static inline void pfs_owning_thread(data_segment_ptr data,
                                       pfs_owning_thread_t thread) noexcept {
    *ptr_to_pfs_owning_thread(data) = thread;
  }
  /** Helper function which stores the PFS datalen info into the DATALEN field.
   */
  static inline void pfs_datalen(data_segment_ptr data,
                                 size_t datalen) noexcept {
    assert(datalen <= std::numeric_limits<pfs_datalen_t>::max());
    *ptr_to_pfs_datalen(data) = datalen;
  }
  /** Helper function which stores the PFS key info into the KEY field. */
  static inline void pfs_key(data_segment_ptr data,
                             pfs_memory_key_t key) noexcept {
    *ptr_to_pfs_key(data) = key;
  }
  /** Helper function which stores the offset to PFS metadata segment into the
   * PFS-META-OFFSET field.
   */
  static inline void pfs_metaoffset(data_segment_ptr data,
                                    std::size_t alignment) noexcept {
    assert(size <= alignment);
    *ptr_to_pfs_meta_offset(data, alignment) = alignment;
  }
  /** Helper function which recovers the information user previously stored in
      OWNER field.
   */
  static inline pfs_owning_thread_t pfs_owning_thread(
      data_segment_ptr data) noexcept {
    auto offset = pfs_meta_offset(data);
    return *reinterpret_cast<pfs_owning_thread_t *>(
        static_cast<uint8_t *>(data) - offset);
  }
  /** Helper function which recovers the information user previously stored in
      DATALEN field.
   */
  static inline pfs_datalen_t pfs_datalen(data_segment_ptr data) noexcept {
    auto offset = pfs_meta_offset(data);
    return *reinterpret_cast<pfs_datalen_t *>(
        static_cast<uint8_t *>(data) - offset + sizeof(pfs_owning_thread_t));
  }
  /** Helper function which recovers the information user previously stored in
      KEY field.
   */
  static inline pfs_memory_key_t pfs_key(data_segment_ptr data) noexcept {
    auto offset = pfs_meta_offset(data);
    return *reinterpret_cast<pfs_memory_key_t *>(
        static_cast<uint8_t *>(data) - offset + sizeof(pfs_datalen_t) +
        sizeof(pfs_owning_thread_t));
  }
  /** Helper function which deduces the pointer to the beginning of PFS metadata
      segment given the pointer to DATA segment.
    */
  static inline void *deduce_pfs_meta(data_segment_ptr data) noexcept {
    auto offset = pfs_meta_offset(data);
    return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(data) -
                                    offset);
  }

 private:
  /** Helper accessor function to OWNER metadata. */
  static inline pfs_owning_thread_t *ptr_to_pfs_owning_thread(
      data_segment_ptr data) noexcept {
    return reinterpret_cast<pfs_owning_thread_t *>(data);
  }
  /** Helper accessor function to DATALEN metadata. */
  static inline pfs_datalen_t *ptr_to_pfs_datalen(
      data_segment_ptr data) noexcept {
    return reinterpret_cast<pfs_datalen_t *>(ptr_to_pfs_owning_thread(data) +
                                             1);
  }
  /** Helper accessor function to PFS metadata. */
  static inline pfs_memory_key_t *ptr_to_pfs_key(
      data_segment_ptr data) noexcept {
    return reinterpret_cast<pfs_memory_key_t *>(ptr_to_pfs_datalen(data) + 1);
  }
  /** Helper accessor function to PFS-META-OFFSET metadata. */
  static inline pfs_meta_offset_t *ptr_to_pfs_meta_offset(
      data_segment_ptr data, std::size_t alignment) noexcept {
    return reinterpret_cast<pfs_meta_offset_t *>(
        static_cast<uint8_t *>(data) + alignment - sizeof(pfs_meta_offset_t));
  }
  /** Helper function which deduces PFS-META-OFFSET metadata value given the
      pointer to DATA segment. */
  static inline pfs_meta_offset_t pfs_meta_offset(
      data_segment_ptr data) noexcept {
    return *(reinterpret_cast<pfs_meta_offset_t *>(
        static_cast<uint8_t *>(data) - sizeof(pfs_meta_offset_t)));
  }
};

}  // namespace detail
}  // namespace ut

#endif
