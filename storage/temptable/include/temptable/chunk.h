/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/chunk.h
Chunk abstraction for temptable Block allocator. Block consists of 1..N
chunks.*/

#ifndef TEMPTABLE_CHUNK_H
#define TEMPTABLE_CHUNK_H

#include <assert.h>
#include <cstddef>      // size_t
#include <cstdint>      // uint8_t, uintptr_t
#include <type_traits>  // std::alignment_of

namespace temptable {

/** Chunk is an abstraction with the purpose of representing a smallest logical
 * memory-unit within the Block. Block allocations and deallocations are
 * served in Chunks.
 *
 * Chunk structure is:
 * - bytes [0, 7]: 8 bytes that designate the relative offset of the chunk from
 *   the start of the belonging block. This is used in order to be able to
 *   deduce the block start from a given chunk.
 * - bytes [8, chunk size): actual user data, pointer to this is returned to the
 *   user after a successful allocation request.
 *
 * As it can be seen, Chunk doesn't hold almost any information (e.g. its size)
 * but merely an offset relative to the Block address it belongs to. That's
 * what it enables Block to implement allocations and deallocations in
 * constant-time.
 *
 * Part of the Chunk contract is to have its metadata properly aligned in
 * memory. Given that this memory is provided by the Block, Chunk
 * implements debug-asserts to actually check if this condition has been met.
 * If that was not the case, then accessing unaligned memory addresses would:
 *   1. Incur performance penalty cost on architectures which can
 *      handle misaligned memory access (e.g. x86).
 *   2. Result with a CPU trap (exception) on architectures which
 *      cannot handle misaligned memory access (e.g. SPARC).
 *
 * OTOH, checking if Chunk user data is properly aligned is not possible from
 * this context because actual data-type is not known to a Chunk. This check
 * however shall be implemented in the context where the type is known (e.g.
 * Allocator)
 * */
class Chunk {
 public:
  /** Type that we will be using for storing metadata information. */
  using metadata_type = uintptr_t;

  /** Chunk metadata size. As described, there is only 1 element. */
  static constexpr size_t METADATA_SIZE = sizeof(Chunk::metadata_type);

 public:
  /** Constructor which Block will use to create a fresh Chunk object at the
   * given memory-offset.
   *
   * [in] Pointer to the actual memory-location where Chunk will be located at.
   * [in] Offset relative to address of a Block that is creating this Chunk. */
  Chunk(uint8_t *offset, size_t new_offset) noexcept;

  /** Constructor which Block will use to re-create Chunk object from
   * user-provided pointer which points to the data section of already existing
   * Chunk in memory. This pointer is returned to the user upon every Chunk
   * allocation.
   *
   * [in] Pointer to the data section of existing Chunk. */
  explicit Chunk(void *data) noexcept;

  /** Deduce the memory-address of belonging Block.
   *
   * @return Memory-address of a Block this Chunk belongs to. */
  uint8_t *block() const;

  /** Get the Chunk offset relative to the start of belonging Block.
   *
   * @return Offset relative to the start of belonging Block. */
  size_t offset() const;

  /** Get the pointer to the data section which will be provided to the
   * end-user.
   *
   * @return Pointer to memory which will be used by the end-user. */
  uint8_t *data() const;

  /** For given size, how much memory will be occupied by the Chunk.
   * This calculation takes into account both the metadata and data payload.
   *
   * [in] Data payload size in bytes.
   * @return Size Chunk will occupy for given n_bytes. */
  static size_t size_hint(size_t n_bytes);

 private:
  /** Deduce a pointer to the offset of given Chunk.
   *
   * [in] Pointer to the first memory location (m_offset) which represents the
   * chunk.
   * @return Pointer to the memory location which represents the chunk offset.
   */
  static Chunk::metadata_type *chunk_offset_ptr(uint8_t *chunk);
  /** Deduce a pointer to the data payload of given Chunk.
   *
   * [in] Pointer to the first memory location (m_offset) which represents the
   * chunk.
   * @return Pointer to the memory location which represents the chunk data
   * (payload). */
  static uint8_t *chunk_data_ptr(uint8_t *chunk);

 private:
  /** A pointer to the actual memory-location where Chunk is located at. */
  uint8_t *m_offset;
};

inline Chunk::Chunk(void *data) noexcept
    : m_offset(reinterpret_cast<uint8_t *>(data) -
               sizeof(Chunk::metadata_type)) {
  assert(reinterpret_cast<Chunk::metadata_type>(m_offset) %
             alignof(Chunk::metadata_type) ==
         0);
}

inline Chunk::Chunk(uint8_t *offset, size_t new_offset) noexcept
    : m_offset(offset) {
  assert(reinterpret_cast<Chunk::metadata_type>(m_offset) %
             alignof(Chunk::metadata_type) ==
         0);
  *chunk_offset_ptr(m_offset) = new_offset;
}

inline uint8_t *Chunk::block() const { return m_offset - offset(); }

inline size_t Chunk::offset() const {
  return static_cast<size_t>(*chunk_offset_ptr(m_offset));
}

inline uint8_t *Chunk::data() const { return chunk_data_ptr(m_offset); }

inline size_t Chunk::size_hint(size_t n_bytes) {
  return Chunk::METADATA_SIZE + n_bytes;
}

inline Chunk::metadata_type *Chunk::chunk_offset_ptr(uint8_t *chunk) {
  return reinterpret_cast<Chunk::metadata_type *>(chunk);
}

inline uint8_t *Chunk::chunk_data_ptr(uint8_t *chunk) {
  return chunk + sizeof(Chunk::metadata_type);
}

} /* namespace temptable */

#endif /* TEMPTABLE_CHUNK_H */
