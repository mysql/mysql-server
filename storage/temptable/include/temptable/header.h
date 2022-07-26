/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/header.h
Header abstraction for temptable Block allocator. Each Block is described by
header. */

#ifndef TEMPTABLE_HEADER_H
#define TEMPTABLE_HEADER_H

#include <assert.h>
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t, uintptr_t

#include "storage/temptable/include/temptable/memutils.h"  // Source

namespace temptable {

/** Header is an abstraction with the purpose of holding and maintaining
 * the Block metadata.
 *
 * Block metadata has the following structure:
 * - N bytes for the block type which indicates where the block
 *   memory was allocated from.
 * - N bytes for the block size (set at block creation and never
 *   changed later).
 * - N bytes for the number of used/allocated chunks from this
 *   block (set to 0 at block creation).
 * - N bytes for the offset of the first byte relative to the
 *   block start that is free and can be used by the next allocation request
 *   (set to 4 * N at block creation). We call this first pristine offset.
 *
 * That implies Chunks occupy the following range in memory:
 * - bytes [4 * N, block size) a sequence of chunks appended to each other.
 *
 * 1st byte of [0, N) region is an actual pointer returned by memory
 * allocation functions (e.g. malloc/new/mmap/etc.). Given that we are working
 * with contiguous memory, storing that byte (offset) is just enough to be
 * able to build-up and deduce Header metadata structure.
 *
 * Part of the Header contract is to have its metadata properly aligned in
 * memory. Given that this memory is provided by the Block, Header
 * implements debug-asserts to actually check if this condition has been met.
 * If that was not the case, then accessing unaligned memory addresses would:
 *   1. Incur performance penalty cost on architectures which can
 *      handle misaligned memory access (e.g. x86).
 *   2. Result with a CPU trap (exception) on architectures which
 *      cannot handle misaligned memory access (e.g. SPARC).
 *
 * In order to maintain proper memory alignment of the whole metadata structure,
 * CPU word-size data-type is used. Our N is defined by the size of that type
 * (Header::metadata_type).
 * */
class Header {
 public:
  /** Type that we will be using for storing metadata information. */
  using metadata_type = uintptr_t;

  /** Block header (metadata) size. As described, there are 4 elements. */
  static constexpr size_t SIZE = 4 * sizeof(Header::metadata_type);

 public:
  /** Get the Block Source type (memory where it resides).
   *
   * @return One of Source values */
  Source memory_source_type() const;

  /** Get the Block size.
   *
   * @return Size of the Block */
  size_t block_size() const;

  /** Get current number of Chunks allocated by the Block.
   *
   * @return Number of Chunks allocated by this Block */
  size_t number_of_used_chunks() const;

  /** Get current first-pristine-offset. This offset is always relative to the
   * block start (block-address).
   *
   * @return Offset relative to the block start */
  size_t first_pristine_offset() const;

 protected:
  /** Default constructor which creates an empty Header. */
  Header() noexcept;

  /** Constructor which initializes the Header metadata when
   * constructing fresh Blocks.
   *
   * [in] Pointer to the allocated Block memory.
   * [in] Source where Block has allocated actual memory from.
   * [in] Size of the Block */
  Header(uint8_t *block_memory, Source block_memory_type,
         size_t block_size) noexcept;

  /** Constructor which initializes the Header metadata from
   * already existing Blocks in memory (e.g. ones that are
   * deduced from Chunks).
   *
   * [in] Pointer to the existing Block. */
  explicit Header(uint8_t *block_memory) noexcept;

  /** Enable Block to get the next available slot that it can use for next
   * Chunk allocation.
   *
   * @return An absolute memory-location offset */
  uint8_t *next_available_slot() const;

  /** Enable Block to get its memory address.
   *
   * @return An address where Block was allocated from. */
  uint8_t *block_address() const;

  /** Enable Block to increment the reference-count when (logically)
   * allocating new Chunks.
   *
   * [in] Size of the Chunk.
   * @return New number of Chunks used/allocated by the Block after this
   * operation. */
  size_t increment_number_of_used_chunks(size_t chunk_size);

  /** Enable Block to decrement the reference-count when (logically)
   * deallocating existing Chunks.
   *
   * [in] Size of the Chunk.
   * [in] Boolean which denotes if Chunk being deallocated is the
   * last (rightmost) one.
   * @return New number of Chunks used/allocated by the Block after this
   * operation. */
  size_t decrement_number_of_used_chunks(size_t chunk_size,
                                         bool rightmost_chunk);

  /** Enable Block to reset the Header metadata upon Block destruction. */
  void reset();

 private:
  /** Deduce a pointer to the memory type of given Block. */
  static Header::metadata_type *block_memory_type_ptr(uint8_t *block);
  /** Deduce a pointer to the size of given Block. */
  static Header::metadata_type *block_size_ptr(uint8_t *block);
  /** Deduce a pointer to the number of used/allocated Chunks of given Block. */
  static Header::metadata_type *block_number_of_used_chunks_ptr(uint8_t *block);
  /** Deduce a pointer to the first-pristine-offset of given Block. */
  static Header::metadata_type *block_first_pristine_offset_ptr(uint8_t *block);

 private:
  /** A pointer to the allocated Block memory which is used to deduce all
   * of the other remaining metadata structure. */
  uint8_t *m_offset;
};

inline Header::Header() noexcept : Header(nullptr) {}

inline Header::Header(uint8_t *block_memory) noexcept : m_offset(block_memory) {
  assert(reinterpret_cast<Header::metadata_type>(m_offset) %
             alignof(Header::metadata_type) ==
         0);
}

inline Header::Header(uint8_t *block_memory, Source block_memory_type,
                      size_t block_size) noexcept
    : Header(block_memory) {
  *block_memory_type_ptr(m_offset) =
      static_cast<Header::metadata_type>(block_memory_type);
  *block_size_ptr(m_offset) = block_size;
  *block_number_of_used_chunks_ptr(m_offset) = 0;
  *block_first_pristine_offset_ptr(m_offset) = Header::SIZE;
}

inline uint8_t *Header::next_available_slot() const {
  return block_address() + *block_first_pristine_offset_ptr(m_offset);
}

inline uint8_t *Header::block_address() const { return m_offset; }

inline Source Header::memory_source_type() const {
  return static_cast<Source>(*block_memory_type_ptr(m_offset));
}

inline size_t Header::block_size() const {
  return static_cast<size_t>(*block_size_ptr(m_offset));
}

inline size_t Header::number_of_used_chunks() const {
  return static_cast<size_t>(*block_number_of_used_chunks_ptr(m_offset));
}

inline size_t Header::first_pristine_offset() const {
  return static_cast<size_t>(*block_first_pristine_offset_ptr(m_offset));
}

inline size_t Header::increment_number_of_used_chunks(size_t chunk_size) {
  *block_first_pristine_offset_ptr(m_offset) += chunk_size;
  return ++*block_number_of_used_chunks_ptr(m_offset);
}

inline size_t Header::decrement_number_of_used_chunks(size_t chunk_size,
                                                      bool rightmost_chunk) {
  assert(*block_number_of_used_chunks_ptr(m_offset) > 0);
  if (--*block_number_of_used_chunks_ptr(m_offset) == 0) {
    /* If we are freeing the leftmost chunk in this block, then
     * first_pristine_offset mark can be reset, so that the memory region
     * can be reused.
     */
    *block_first_pristine_offset_ptr(m_offset) = Header::SIZE;
  } else if (rightmost_chunk) {
    /* If we are freeing the rightmost chunk in this block, then lower the
     * first_pristine_offset mark, so that the memory region can be reused.
     */
    *block_first_pristine_offset_ptr(m_offset) -= chunk_size;
  }
  return *block_number_of_used_chunks_ptr(m_offset);
}

inline void Header::reset() { m_offset = nullptr; }

inline Header::metadata_type *Header::block_memory_type_ptr(uint8_t *block) {
  return reinterpret_cast<Header::metadata_type *>(block);
}

inline Header::metadata_type *Header::block_size_ptr(uint8_t *block) {
  return reinterpret_cast<Header::metadata_type *>(
      block + 1 * sizeof(Header::metadata_type));
}

inline Header::metadata_type *Header::block_number_of_used_chunks_ptr(
    uint8_t *block) {
  return reinterpret_cast<Header::metadata_type *>(
      block + 2 * sizeof(Header::metadata_type));
}

inline Header::metadata_type *Header::block_first_pristine_offset_ptr(
    uint8_t *block) {
  return reinterpret_cast<Header::metadata_type *>(
      block + 3 * sizeof(Header::metadata_type));
}

} /* namespace temptable */

#endif /* TEMPTABLE_HEADER_H */
