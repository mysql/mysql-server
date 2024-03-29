/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

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

/** @file include/mach0data.h
 Utilities for converting data from the database file
 to the machine format.

 Created 11/28/1995 Heikki Tuuri
 ***********************************************************************/

#ifndef mach0data_h
#define mach0data_h

#include "mtr0types.h"
#include "univ.i"

#ifdef UNIV_HOTBACKUP
#include "ut0byte.h"
#endif /* UNIV_HOTBACKUP */

/* The data and all fields are always stored in a database file
in the same format: ascii, big-endian, ... .
All data in the files MUST be accessed using the functions in this
module. */

/** The following function is used to store data in one byte.
@param[in]      b       pointer to byte where to store
@param[in]      n       One byte integer to be stored, >= 0, < 256 */
static inline void mach_write_to_1(byte *b, ulint n);

/** The following function is used to fetch data from one byte.
@param[in]      b       pointer to a byte to read
@return ulint integer, >= 0, < 256 */
[[nodiscard]] static inline uint8_t mach_read_from_1(const byte *b);

/** The following function is used to store data in two consecutive bytes. We
store the most significant byte to the lower address.
@param[in]      b       pointer to 2 bytes where to store
@param[in]      n       2-byte integer to be stored, >= 0, < 64k */
static inline void mach_write_to_2(byte *b, ulint n);

/** The following function is used to fetch data from 2 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 2 bytes where to store
@return 2-byte integer, >= 0, < 64k */
[[nodiscard]] static inline uint16_t mach_read_from_2(const byte *b);

/** The following function is used to convert a 16-bit data item to the
canonical format, for fast bytewise equality test against memory.
@param[in]  n   integer in machine-dependent format
@return 16-bit integer in canonical format */
static inline uint16_t mach_encode_2(ulint n);

/** The following function is used to convert a 16-bit data item from the
canonical format, for fast bytewise equality test against memory.
@param[in]  n   16-bit integer in canonical format
@return integer in machine-dependent format */
static inline ulint mach_decode_2(uint16 n);

/** The following function is used to store data in 3 consecutive
bytes. We store the most significant byte to the lowest address.
@param[in]      b       pointer to 3 bytes where to store
@param[in]      n       3 byte integer to be stored */
static inline void mach_write_to_3(byte *b, ulint n);

/** The following function is used to fetch data from 3 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 3 bytes to read
@return 32 bit integer */
[[nodiscard]] static inline uint32_t mach_read_from_3(const byte *b);

/** The following function is used to store data in 4 consecutive
bytes. We store the most significant byte to the lowest address.
@param[in]      b       pointer to 4 bytes where to store
@param[in]      n       4 byte integer to be stored */
static inline void mach_write_to_4(byte *b, ulint n);

/** The following function is used to fetch data from 4 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 4 bytes to read
@return 32 bit integer */
[[nodiscard]] static inline uint32_t mach_read_from_4(const byte *b);

/** Write a ulint in a compressed form (1..5 bytes).
@param[in]      b       pointer to memory where to store
@param[in]      n       ulint integer to be stored
@return stored size in bytes */
static inline ulint mach_write_compressed(byte *b, ulint n);

/** Return the size of an ulint when written in the compressed form.
@param[in]      n       ulint integer to be stored
@return compressed size in bytes */
static inline uint32_t mach_get_compressed_size(ulint n);

/** Read a 32-bit integer in a compressed form.
@param[in,out]  b       pointer to memory where to read;
advanced by the number of bytes consumed
@return unsigned value */
static inline uint32_t mach_read_next_compressed(const byte **b);

/** The following function is used to store data in 6 consecutive
bytes. We store the most significant byte to the lowest address.
@param[in]      b       pointer to 6 bytes where to store
@param[in]      id      48-bit integer to write */
static inline void mach_write_to_6(byte *b, uint64_t id);

/** The following function is used to fetch data from 6 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 6 bytes to read
@return 48-bit integer */
[[nodiscard]] static inline uint64_t mach_read_from_6(const byte *b);

/** The following function is used to store data in 7 consecutive
bytes. We store the most significant byte to the lowest address.
@param[in]      b       pointer to 7 bytes where to store
@param[in]      n       56-bit integer */
static inline void mach_write_to_7(byte *b, uint64_t n);

/** The following function is used to fetch data from 7 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 7 bytes to read
@return 56-bit integer */
[[nodiscard]] static inline uint64_t mach_read_from_7(const byte *b);

/** The following function is used to store data in 8 consecutive
bytes. We store the most significant byte to the lowest address.
@param[in]      b       pointer to 8 bytes where to store
@param[in]      n       64-bit integer to be stored */
static inline void mach_write_to_8(void *b, uint64_t n);

/** The following function is used to fetch data from 8 consecutive
bytes. The most significant byte is at the lowest address.
@param[in]      b       pointer to 8 bytes to read
@return 64-bit integer */
[[nodiscard]] static inline uint64_t mach_read_from_8(const byte *b);

/** Writes a 64-bit integer in a compressed form (5..9 bytes).
@param[in]      b       pointer to memory where to store
@param[in]      n       64-bit integer to be stored
@return size in bytes */
static inline ulint mach_u64_write_compressed(byte *b, uint64_t n);

/** Read a 64-bit integer in a compressed form.
@param[in,out]  b       pointer to memory where to read;
advanced by the number of bytes consumed
@return unsigned value */
static inline uint64_t mach_u64_read_next_compressed(const byte **b);

/** Writes a 64-bit integer in a compressed form (1..11 bytes).
@param[in]      b       pointer to memory where to store
@param[in]      n       64-bit integer to be stored
@return size in bytes */
static inline ulint mach_u64_write_much_compressed(byte *b, uint64_t n);

/** Reads a 64-bit integer in a compressed form.
@param[in]      b       pointer to memory from where to read
@return the value read */
[[nodiscard]] static inline uint64_t mach_u64_read_much_compressed(
    const byte *b);

/** Read a 64-bit integer in a much compressed form.
@param[in,out]  ptr     pointer to memory from where to read,
advanced by the number of bytes consumed, or set NULL if out of space
@param[in]      end_ptr end of the buffer
@return unsigned 64-bit integer */
uint64_t mach_parse_u64_much_compressed(const byte **ptr, const byte *end_ptr);

/** Read a 32-bit integer in a compressed form.
@param[in,out]  ptr     pointer to memory from where to read;
advanced by the number of bytes consumed, or set NULL if out of space
@param[in]      end_ptr end of the buffer
@return unsigned value */
uint32_t mach_parse_compressed(const byte **ptr, const byte *end_ptr);

/** Read a 64-bit integer in a compressed form.
@param[in,out]  ptr     pointer to memory from where to read;
advanced by the number of bytes consumed, or set NULL if out of space
@param[in]      end_ptr end of the buffer
@return unsigned value */
static inline uint64_t mach_u64_parse_compressed(const byte **ptr,
                                                 const byte *end_ptr);

/** Reads a double. It is stored in a little-endian format.
 @return double read */
[[nodiscard]] static inline double mach_double_read(
    const byte *b); /*!< in: pointer to memory from where to read */

/** Writes a double. It is stored in a little-endian format.
@param[in]      b       pointer to memory where to write
@param[in]      d       double */
static inline void mach_double_write(byte *b, double d);

/** Reads a float. It is stored in a little-endian format.
 @return float read */
[[nodiscard]] static inline float mach_float_read(
    const byte *b); /*!< in: pointer to memory from where to read */

/** Writes a float. It is stored in a little-endian format.
@param[in]      b       pointer to memory where to write
@param[in]      d       float */
static inline void mach_float_write(byte *b, float d);

#ifndef UNIV_HOTBACKUP
/** Reads a ulint stored in the little-endian format.
@param[in] buf      From where to read.
@param[in] buf_size From how many bytes to read.
@return unsigned long int */
[[nodiscard]] static inline ulint mach_read_from_n_little_endian(
    const byte *buf, ulint buf_size);

/** Writes a ulint in the little-endian format.
@param[in]      dest            where to write
@param[in]      dest_size       into how many bytes to write
@param[in]      n               unsigned long int to write */
static inline void mach_write_to_n_little_endian(byte *dest, ulint dest_size,
                                                 ulint n);

/** Reads a ulint stored in the little-endian format.
 @return unsigned long int */
[[nodiscard]] static inline ulint mach_read_from_2_little_endian(
    const byte *buf); /*!< in: from where to read */

/** Writes a ulint in the little-endian format.
@param[in]      dest            where to write
@param[in]      n               unsigned long int to write */
static inline void mach_write_to_2_little_endian(byte *dest, ulint n);

/** Convert integral type from storage byte order (big endian) to host byte
order.
@param[in]      src             where to read from
@param[in]      len             length of src
@param[in]      unsigned_type   signed or unsigned flag
@return integer value */
static inline uint64_t mach_read_int_type(const byte *src, ulint len,
                                          bool unsigned_type);

/** Convert integral type from host byte order to (big-endian) storage
byte order.
@param[in]      dest    where to write
@param[in]      src     where to read
@param[in]      len     length of src
@param[in]      usign   signed or unsigned flag */
static inline void mach_write_int_type(byte *dest, const byte *src, ulint len,
                                       bool usign);

/** Convert a ulonglong integer from host byte order to (big-endian) storage
byte order.
@param[in]      dest    where to write
@param[in]      src     where to read from
@param[in]      len     length of dest
@param[in]      usign   signed or unsigned flag */
static inline void mach_write_ulonglong(byte *dest, ulonglong src, ulint len,
                                        bool usign);

#endif

/** Read 1 to 4 bytes from a file page buffered in the buffer pool.
@param[in]      ptr     pointer where to read
@param[in]      type    MLOG_1BYTE, MLOG_2BYTES, or MLOG_4BYTES
@return value read */
[[nodiscard]] static inline uint32_t mach_read_ulint(const byte *ptr,
                                                     mlog_id_t type);

#include "mach0data.ic"

#endif
