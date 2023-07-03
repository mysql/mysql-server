/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/** @file include/detail/ut/page_metadata.h
    Implementation bits and pieces for metadata for normal and large (huge) page
    allocations.
 */

#ifndef detail_ut_page_metadata_h
#define detail_ut_page_metadata_h

#include "storage/innobase/include/detail/ut/helper.h"
#include "storage/innobase/include/detail/ut/pfs.h"

namespace ut {
namespace detail {

/** Types of pages currently supported by ut:: library functions */
enum class Page_type { system_page = 0x10, large_page = 0x20 };

/** Helper struct implementing the type which represents the metadata for all
    types of page-aligned allocations, be it regular pages or huge-pages.

    Concrete implementations such as Page_alloc or Large_page_alloc are
    both implemented in terms of this basic building block. This is one way
    which enables an easier implementation of higher-kinded convenience
    library functions, e.g. huge-page allocation with fallback to regular pages.

    Memory layout representation looks like the following:

     --------------------------------
     | DATALEN | PAGE-TYPE | VARLEN |
     --------------------------------
      \                              \
       0                              \
                              CPU_PAGE_SIZE - 1

    DATALEN field encodes total size of memory consumed and not only the size of
    the DATA segment.

    PAGE-TYPE field encodes the type of page this memory is backed up with.

    VARLEN is the leftover variable-length segment that specialized
    implementations can further make use of by deducing its size from the
    following formulae: abs(CPU_PAGE_SIZE - sizeof(DATALEN) -
    sizeof(PAGE-TYPE)). In code that would be std::abs(CPU_PAGE_SIZE -
    sizeof(Page_alloc_metadata::datalen_t) -
    sizeof(Page_alloc_metadata::page_type_t)).
    Not used by this implementation.
 */
struct Page_alloc_metadata {
  /** This is how much tise metadata segment will be big. */
  static constexpr auto len = CPU_PAGE_SIZE;

  /** These are the types representing our memory fields. */
  using datalen_t = size_t;
  using page_type_t = size_t;

  /** Sanity check so that we can be sure that the size of our metadata segment
      is such so that the next segment coming after it (DATA) is always suitably
      aligned (multiple of alignof(max_align_t).
    */
  static_assert(len % alignof(max_align_t) == 0,
                "len must be divisible by alignof(max_align_t)");

  /** Sanity check so that we can be sure that our metadata segment can fit
      all our fields (datalen_t and page_type_t).
    */
  static_assert(sizeof(datalen_t) + sizeof(page_type_t) <= len,
                "Metadata does not fit!");

  /** Accessor to the datalen_t field. Returns its value.

      @param[in] data Pointer to the DATA segment.
      @return Value held by datalen_t field.
   */
  static inline datalen_t datalen(void *data) {
    return *reinterpret_cast<datalen_t *>(static_cast<uint8_t *>(data) - len);
  }

  /** Accessor to the page_type_t field. Returns its value.

      @param[in] data Pointer to the DATA segment.
      @return Value held by page_type_t field, in particular Page_type.
   */
  static inline Page_type page_type(void *data) {
    auto type = *reinterpret_cast<page_type_t *>(static_cast<uint8_t *>(data) -
                                                 len + sizeof(datalen_t));
    return static_cast<Page_type>(type);
  }

  /** Accessor to the datalen_t field. Sets its value.

      @param[in] mem Pointer to the memory, usually allocated through
      large_page_alloc or page_alloc.
      @param[in] length New value to be set into the field.
   */
  static inline void datalen(void *mem, size_t length) {
    *reinterpret_cast<datalen_t *>(mem) = length;
  }

  /** Accessor to the page_type_t field. Sets its value.

      @param[in] mem Pointer to the memory, usually allocated through
      large_page_alloc or page_alloc.
      @param[in] type New value to be set into the field.
   */
  static inline void page_type(void *mem, Page_type type) {
    *reinterpret_cast<page_type_t *>(static_cast<uint8_t *>(mem) +
                                     sizeof(datalen_t)) =
        static_cast<page_type_t>(type);
  }
};

/**
    Helper struct implementing the type which represents the metadata for all
    types of PFS-aware page-aligned allocations, be it regular pages or
    huge-pages.

    Concrete implementations such as Page_alloc_pfs or Large_page_alloc_pfs are
    both implemented in terms of this basic building block. This is one way
    which enables an easier implementation of higher-kinded convenience
    library functions, e.g. huge-page allocation with fallback to regular
    pages.

    Memory layout representation looks like the following:

     ---------------------------------------------------
     | PFS-META | PAGE-TYPE | VARLEN | PFS-META-OFFSET |
     ---------------------------------------------------
      \   ^                                             \
       0  |                                              \
          |                                       CPU_PAGE_SIZE - 1
          |
          |
         ---------------------------
         | OWNER |  DATALEN  | KEY |
         ---------------------------

    OWNER field encodes the owning thread.
    DATALEN field encodes total size of memory consumed and not only the size of
    the DATA segment.
    KEY field encodes the PFS/PSI key.

    PAGE-TYPE field encodes the type of page this memory is backed up with.

    VARLEN is the leftover variable-length segment that specialized
    implementations can further make use of by deducing its size from the
    following formulae: abs(CPU_PAGE_SIZE - sizeof(PFS-META-OFFSET) -
    sizeof(PFS-META)). In code that would be std::abs(CPU_PAGE_SIZE -
    PFS_metadata::size). Not used by this implementation.

    PFS-META-OFFSET, strictly speaking, isn't necessary in this case of
    system-pages, where alignment is always known in compile-time and thus the
    offset we will be storing into the PFS-META-OFFSET field is always going
    to be the same for the given platform. So, rather than serializing this
    piece of information into the memory as we do right now, we could very
    well be storing it into the compile-time evaluated constexpr constant. The
    reason why we don't do it is that there is no advantage (*) of doing so
    while we would be introducing a disadvantage of having to maintain separate
    specialization of PFS_metadata and code would be somewhat more fragmented.

     (*) Extra space that we need to allocate in order to be able to fit the
         PFS_metadata is going to be the same regardless if there is
         PFS-META-OFFSET field or not. This is due to the fact that PFS-META
         segment alone is larger than alignof(max_align_t) so in order to
         keep the DATA segment suitably aligned (% alignof(max_align_t) == 0)
         we must choose the size for the whole PFS segment that is a multiple
         of alignof(max_align_t).

    PFS-META-OFFSET is a field which allows us to recover the pointer to
    PFS-META segment from a pointer to DATA segment.
 */
struct Page_alloc_metadata_pfs {
  using pfs_metadata = PFS_metadata;
  using page_type_t = size_t;

  /** This is how much this metadata segment will be big. */
  static constexpr auto len = CPU_PAGE_SIZE;

  /** Suitably-aligned offset for PAGE-TYPE field. */
  static constexpr auto page_type_offset =
      calc_align(pfs_metadata::meta_size, alignof(page_type_t));

  /** Sanity check so that we can be sure that the size of our metadata segment
      is such so that the pointer to DATA segment is always suitably aligned
      (multiple of alignof(max_align_t).
    */
  static_assert(len % alignof(max_align_t) == 0,
                "len must be divisible by alignof(max_align_t)");

  /** Accessor to the page_type_t field. Returns its value.

      @param[in] data Pointer to the DATA segment.
      @return Value held by page_type_t field, in particular Page_type.
   */
  static inline Page_type page_type(void *data) {
    auto type = *reinterpret_cast<page_type_t *>(
        static_cast<uint8_t *>(pfs_metadata::deduce_pfs_meta(data)) +
        page_type_offset);
    return static_cast<Page_type>(type);
  }

  /** Accessor to the page_type_t field. Sets its value.

      @param[in] mem Pointer to the memory, usually allocated through
      large_page_alloc or page_alloc.
      @param[in] type New value to be set into the field.
   */
  static inline void page_type(void *mem, Page_type type) {
    *reinterpret_cast<page_type_t *>(static_cast<uint8_t *>(mem) +
                                     page_type_offset) =
        static_cast<page_type_t>(type);
  }
};

}  // namespace detail
}  // namespace ut

#endif
