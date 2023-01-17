/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

/** @file storage/temptable/include/temptable/block.h
Block abstraction for temptable-allocator. */

#ifndef TEMPTABLE_BLOCK_H
#define TEMPTABLE_BLOCK_H

#include <cstddef>  // size_t
#include <cstdint>  // uint8_t, uintptr_t
#include <limits>   // std::numeric_limits
#include <sstream>  // std::stringstream
#include <string>   // std::string

#include "memory_debugging.h"
#include "my_dbug.h"
#include "mysql/psi/mysql_memory.h"
#include "storage/temptable/include/temptable/chunk.h"
#include "storage/temptable/include/temptable/header.h"
#include "storage/temptable/include/temptable/memutils.h"

namespace temptable {

/** Initialize the PSI memory engine. */
void Block_PSI_init();
/** Log logical (Chunk) memory allocation.
 *
 * [in] Number of bytes allocated
 * */
void Block_PSI_track_logical_allocation(size_t size);
/** Log logical (Chunk) memory deallocation.
 *
 * [in] Number of bytes deallocated
 * */
void Block_PSI_track_logical_deallocation(size_t size);
/** Log physical memory allocation of a Block located in RAM.
 *
 * [in] Pointer to user memory block
 * [in] Number of bytes allocated
 * */
void Block_PSI_track_physical_ram_allocation(void *ptr, size_t size);
/** Log physical memory deallocation of a Block located in RAM.
 *
 * [in]  Pointer to PSI header
 * */
void Block_PSI_track_physical_ram_deallocation(uint8_t *ptr);
/** Log physical memory allocation of a Block located in MMAP-ed file.
 *
 * [in] Pointer to user memory block
 * [in] Number of bytes allocated
 * */
void Block_PSI_track_physical_disk_allocation(void *ptr, size_t size);
/** Log physical memory deallocation of a Block located in MMAP-ed file.
 *
 * [in]  Pointer to PSI header
 * */
void Block_PSI_track_physical_disk_deallocation(uint8_t *ptr);

/** Memory-block abstraction whose purpose is to serve as a building block
 * for custom memory-allocator implementations.
 *
 * TL;DR How it works:
 *  Instantiation:
 *      - With given size and given memory source, Block will allocate memory
 *        and adjust its Header metadata with the relevant information.
 *  Allocation:
 *      - From allocated memory space, Block finds out what is the next
 *        available slot to fit the new Chunk into.
 *      - Creates a new Chunk with the address pointing to that slot.
 *      - Increments the number of allocated chunks.
 *      - Returns a Chunk.
 *  Deallocation:
 *      - Decrements the number of allocated chunks.
 *      - Returns current number of allocated chunks.
 *  Destruction:
 *      - Simply deallocates the memory.
 *
 * Now, more detailed description ...
 *
 * Normally, custom memory-allocators will feed clients' memory allocation
 * and deallocation requests solely through the provided Block interface which
 * enables allocators not to worry about the whole lot of low-level memory
 * byte-juggling but to focus on application-level details.
 *
 * Block, once created, will occupy at-least (see below why) the specified
 * amount of memory after which it will be able to serve client-requested
 * allocations and deallocations in logical units called Chunks. Chunk is
 * an arbitrarily-sized view over a region of memory allocated during the
 * Block creation. Block can fit as many Chunks as there is free memory space
 * left in it. Once there is no free space left, another Block of memory has
 * to be created. Block is not resizeable. E.g. 4KB-sized Block can feed 1x4KB,
 * 2x2KB, 1KB+3KB or any other combination of Chunks whose total size does not
 * exceed the Block size (4KB).
 *
 * Organizing Block memory into Chunks is implementation house-keeping detail
 * stored in its Header metadata region. Block does not maintain the list of
 * Chunks, it only ever keeps the number of currently allocated Chunks and
 * the offset to the first memory location available to feed the next
 * allocation request.
 *
 * While still using the same interface, custom memory-allocators are able to
 * choose where should the Block allocate actual memory from. It could be
 * anything defined by Source but currently only RAM and MMAP-ed files are
 * available and implemented as options.
 *
 * For the benefit of (amortized) constant-time allocations, Block does not
 * re-use or do any other special operations over deallocated Chunks so
 * memory-allocators which will be using it may suffer from block-level
 * memory-fragmentation and consequently higher memory-consumption. Exceptions
 * are deallocations of first and last Chunks in a Block when it is possible
 * to easily re-adjust the offset and therefore be able to re-use that part
 * of memory.
 *
 * Another big advantage, which is very closely related to constant-time
 * allocations, is that it minimizes the number of system-calls required to
 * allocate and deallocate the memory which consequently may lower the
 * process-level memory-fragmentation.
 *
 * Block size does not necessarily end-up being the size originally requested by
 * the client but it will be actually implicitly rounded to the next multiple
 * of CPU word-size which may result in better memory utilization. Actual block
 * size can be queried through the Block interface.
 *
 * To optimize for the CPU memory-access, but also to enable code not to
 * segfault on architectures which do not support unaligned-memory-access
 * (e.g. SPARC), Block will always adjust requested Chunk allocation size to
 * match the size which is rounded to the next multiple of CPU word-size
 * (Block::ALIGN_TO constant). End result is that Block might end up allocating
 * just a few more bytes bigger Chunk than actually requested but that
 * information, however, does not need to be maintained or cared about by the
 * client code.
 *
 * Along with the small space overhead due to the automatic word-size-adjustment
 * of Chunk size, each Block allocation will also have a few bytes overhead for
 * maintaining the Header metadata (Header::SIZE) as well as for maintaining the
 * Chunk metadata (Chunk::METADATA_SIZE). Implementation and data layout details
 * can be found at respective header file declarations.
 *
 * All dirty-implementation details are hidden in Header implementation
 * which makes sure that proper care is taken to handle chunk offsets,
 * available slots, number of present chunks etc. */
class Block : private Header {
 public:
  /** Block will self-adjust all requested allocation-sizes to the multiple of
   * this value. */
  static constexpr size_t ALIGN_TO = alignof(void *);

 public:
  /** Default constructor which creates an empty Block. */
  Block() noexcept = default;

  /** Constructor which creates a Block of given size from the given
   * memory source.
   *
   * [in] Block size in bytes.
   * [in] Source where Block will allocate actual memory from. */
  Block(size_t size, Source memory_source);

  /** Constructor which creates a Block from given Chunk. Chunk holds
   * just enough information so we can deduce which Block does it belong to.
   *
   * [in] Existing Chunk in memory. */
  explicit Block(Chunk chunk) noexcept;

  /** Equality operator.
   *
   * [in] Block to compare it against.
   * @return true if two Blocks are equal (pointing to the same memory
   * location). */
  bool operator==(const Block &other) const;

  /** Inequality operator.
   *
   * [in] Block to compare it against.
   * @return true if two Blocks are not equal (not pointing to the same
   * memory location). */
  bool operator!=(const Block &other) const;

  /** Allocate a Chunk from a Block.
   *
   * [in] Size of the Chunk to be allocated.
   * @return Chunk of memory. */
  Chunk allocate(size_t chunk_size) noexcept;

  /** Deallocate a Chunk from a Block.
   *
   * [in] Chunk to be deallocated.
   * [in] Size of the Chunk to be deallocated.
   * @return Remaining number of Chunks in a Block. */
  size_t deallocate(Chunk chunk, size_t chunk_size) noexcept;

  /** Destroy the whole Block. This operation will release all occupied memory
   * by the Block so client code must make sure that it doesn't keep dangling
   * Chunks in the memory. */
  void destroy() noexcept;

  /** Check if Block is empty (not holding any data).
   *
   * @return true if it is */
  bool is_empty() const;

  /** Check if Block can fit (allocate) a Chunk of given size.
   *
   * [in] Desired chunk size in bytes.
   * @return true if can */
  bool can_accommodate(size_t chunk_size) const;

  /** Get the Block Source type (memory where it resides).
   *
   * @return one of Source values */
  Source type() const;

  /** Get the Block size.
   *
   * @return Block size */
  size_t size() const;

  /** Get current number of Chunks allocated by the Block.
   *
   * @return Number of Chunks allocated by this Block */
  size_t number_of_used_chunks() const;

  /** A human-readable string that describes a Block.
   *
   * @return human-readable string */
  std::string to_string() const;

  /** For given size, how much memory will Block with single Chunk actually
   * occupy. This calculation takes into account both the Header/Chunk metadata
   * and the data payload.
   *
   * [in] Data payload size in bytes.
   * @return Size Block would allocate for given n_bytes.
   * */
  static size_t size_hint(size_t n_bytes);

 private:
  /** Delegating constructor which populates Header with provided information.
   *
   * [in] Address of a memory region allocated by the Block
   * [in] Source of memory region
   * [in] Block size in bytes. */
  Block(uint8_t *block_memory, Source block_type, size_t block_size) noexcept;

  /** Are we looking at the last (rightmost) chunk in a Block.
   *
   * [in] Reference to a Chunk
   * [in] Chunk size
   * @return true if we are */
  bool is_rightmost_chunk(const Chunk &chunk, size_t chunk_size) const;

  /** What is the word-size (ALIGN_TO) aligned size of an input size?
   *
   * [in] Input size
   * @return Block-size rounded up to the next ALIGN_TO size */
  static size_t aligned_size(size_t size);
};

static inline uint8_t *allocate_from(Source src, size_t size) {
  void *ptr = nullptr;
  size_t raw_size = size;
#ifdef HAVE_PSI_MEMORY_INTERFACE
  raw_size += PSI_HEADER_SIZE;
#endif
  if (src == Source::RAM) {
    ptr = Memory<Source::RAM>::allocate(raw_size);
    Block_PSI_track_physical_ram_allocation(ptr, size);
  } else if (src == Source::MMAP_FILE) {
    ptr = Memory<Source::MMAP_FILE>::allocate(raw_size);
    Block_PSI_track_physical_disk_allocation(ptr, size);
  }
#ifdef HAVE_PSI_MEMORY_INTERFACE
  return reinterpret_cast<uint8_t *>(HEADER_TO_USER(ptr));
#else
  return reinterpret_cast<uint8_t *>(ptr);
#endif
}

static inline void deallocate_from(Source src, size_t size,
                                   uint8_t *block_address) {
  size_t raw_size = size;
  uint8_t *raw_block_address = block_address;
#ifdef HAVE_PSI_MEMORY_INTERFACE
  raw_size += PSI_HEADER_SIZE;
  raw_block_address = USER_TO_HEADER_UINT8_T(block_address);
#endif
  if (src == Source::RAM) {
    Block_PSI_track_physical_ram_deallocation(raw_block_address);
    Memory<Source::RAM>::deallocate(raw_block_address, raw_size);
  } else if (src == Source::MMAP_FILE) {
    Block_PSI_track_physical_disk_deallocation(raw_block_address);
    Memory<Source::MMAP_FILE>::deallocate(raw_block_address, raw_size);
  }
}

inline Block::Block(Chunk chunk) noexcept : Header(chunk.block()) {
  assert(!is_empty());
}

inline Block::Block(size_t size, Source memory_source)
    : Block(allocate_from(memory_source, Block::aligned_size(size)),
            memory_source, Block::aligned_size(size)) {
  assert(!is_empty());
}

inline Block::Block(uint8_t *block_memory, Source block_memory_type,
                    size_t block_size) noexcept
    : Header(block_memory, block_memory_type, block_size) {
  assert(!is_empty());

  /* Prevent writes to the memory which we took from the OS but still have
   * not shipped outside of the Allocator. This will also prevent reads, but
   * reads would have been reported even without this because the memory we
   * took from the OS is "undefined" by default. */
  MEM_NOACCESS(Header::block_address() + Header::SIZE,
               block_size - Header::SIZE);

  DBUG_PRINT("temptable_allocator", ("block create: size=%zu, new_block=(%s)",
                                     block_size, to_string().c_str()));
}

inline bool Block::operator==(const Block &other) const {
  return Header::block_address() == other.block_address();
}

inline bool Block::operator!=(const Block &other) const {
  return !(Header::block_address() == other.block_address());
}

inline Chunk Block::allocate(size_t chunk_size) noexcept {
  assert(!is_empty());
  assert(can_accommodate(chunk_size));

  const size_t chunk_size_aligned = Block::aligned_size(chunk_size);

  /* Remove the "no access" flag we set on this memory during block
   * creation. Relax it to report read+depend_on_contents. */
  MEM_UNDEFINED(Header::next_available_slot(),
                Chunk::size_hint(chunk_size_aligned));

  Chunk chunk{Header::next_available_slot(), Header::first_pristine_offset()};
  Header::increment_number_of_used_chunks(Chunk::size_hint(chunk_size_aligned));

  Block_PSI_track_logical_allocation(chunk_size_aligned);
  DBUG_PRINT("temptable_allocator",
             ("allocate from block: chunk_size=%zu, from_block=(%s); "
              "return=%p",
              chunk_size, to_string().c_str(), chunk.data()));

  return chunk;
}

inline size_t Block::deallocate(Chunk chunk, size_t chunk_size) noexcept {
  assert(!is_empty());
  DBUG_PRINT("temptable_allocator",
             ("deallocate from block: size=%zu, from_block=(%s), chunk_data=%p",
              chunk_size, to_string().c_str(), chunk.data()));

  const size_t chunk_size_aligned = Block::aligned_size(chunk_size);
  Block_PSI_track_logical_deallocation(chunk_size_aligned);

  return Header::decrement_number_of_used_chunks(
      Chunk::size_hint(chunk_size_aligned),
      is_rightmost_chunk(chunk, Chunk::size_hint(chunk_size_aligned)));
}

inline void Block::destroy() noexcept {
  assert(!is_empty());
  assert(Header::number_of_used_chunks() == 0);
  DBUG_PRINT("temptable_allocator",
             ("destroying the block: (%s)", to_string().c_str()));

  deallocate_from(Header::memory_source_type(), Header::block_size(),
                  Header::block_address());
  Header::reset();
}

inline bool Block::is_empty() const {
  return Header::block_address() == nullptr;
}

inline bool Block::can_accommodate(size_t n_bytes) const {
  assert(!is_empty());

  const size_t n_bytes_aligned = Block::aligned_size(n_bytes);
  const size_t block_size = Header::block_size();
  const size_t first_pristine_offset = Header::first_pristine_offset();
  assert(first_pristine_offset <=
         std::numeric_limits<decltype(block_size)>::max() -
             Chunk::size_hint(n_bytes_aligned));

  return first_pristine_offset + Chunk::size_hint(n_bytes_aligned) <=
         block_size;
}

inline Source Block::type() const {
  assert(!is_empty());
  return Header::memory_source_type();
}

inline size_t Block::size() const {
  assert(!is_empty());
  return Header::block_size();
}

inline size_t Block::number_of_used_chunks() const {
  assert(!is_empty());
  return Header::number_of_used_chunks();
}

inline std::string Block::to_string() const {
  assert(!is_empty());
  std::stringstream s;
  s << "address=" << static_cast<void *>(Header::block_address())
    << ", size=" << Header::block_size()
    << ", num_chunks=" << Header::number_of_used_chunks()
    << ", first_pristine=" << Header::first_pristine_offset();
  return s.str();
}

inline bool Block::is_rightmost_chunk(const Chunk &chunk,
                                      size_t size_bytes) const {
  assert(!is_empty());
  return chunk.offset() + size_bytes == Header::first_pristine_offset();
}

inline size_t Block::size_hint(size_t n_bytes) {
  return Block::aligned_size(Header::SIZE + Chunk::size_hint(n_bytes));
}

inline size_t Block::aligned_size(size_t size) {
  return (size + Block::ALIGN_TO - 1) & ~(Block::ALIGN_TO - 1);
}

} /* namespace temptable */

#endif /* TEMPTABLE_BLOCK_H */
