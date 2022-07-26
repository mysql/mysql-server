/*****************************************************************************

Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

/**************************************************/ /**
 @file include/log0files_io.h

 The log0files_io.{cc,h} is a low-level independent library for IO operations
 performed directly on redo log files.

 The library provides functions which allow to:
  - serialize,
  - deserialize,
  - read,
  - or write
each kind of header stored in redo log files, and individual redo log blocks.

NOTE: Parsing of individual redo records is NOT in scope of this library.

@remarks

Main goal for this library is to make IO operations simpler, no matter what
is the motivation behind reading or writing fragments of redo log files.

That's why:

1. Functions defined in this library form a set of simple independent tools.
They are state-less (they do not change state of the library, but obviously
"write" functions might change redo log files).

2. For each kind of redo header, three forms of the header are recognized:
  - structure with typed fields (e.g. struct Log_checkpoint_header),
  - array of bytes representing the serialized header,
  - data stored on disk.
Functions to translate between any two of these forms are provided for each
kind of header. In order to make life easier (when having to remind yourself
what was the name for the given function), the following naming convention
has been defined:
  - structure -> array of bytes: log_X_header_serialize,
  - array of bytes -> structure: log_X_header_deserialize,
  - structure -> disk: log_X_header_write(Log_file_id, ..., const The_struct& ),
  - disk -> structure: log_X_header_read(Log_file_id, ..., The_struct& ),
  - array of bytes -> disk: log_X_header_write(Log_file_id, ..., const byte* ),
  - disk -> array of bytes: log_X_header_read(Log_file_id, ..., byte* ).

@note There is no structure with typed fields for encryption header (yet) and
for redo data blocks.

3. The functions defined in this library MUST NOT depend on log_t or recovery
implementation, because this library is designed to be lightweight and easy
to use.

@note

Functions that operate on set of redo files are also part of this library.
This includes functions to:
  - build a path to the redo log file with the given id,
  - list existing redo files,
  - remove redo files,
  - create empty redo files,
  - mark/unmark individual redo files as unused.

*******************************************************/

#ifndef log0files_io_h
#define log0files_io_h

#include <string>

/* Log_file_header, Log_checkpoint_header, LOG_BLOCK_HDR_NO */
#include "log0types.h"

/* mach_read_from_X, mach_write_to_X */
#include "mach0data.h"

/* ut_crc32() */
#include "ut0crc32.h"

/* ut::vector */
#include "ut0new.h"

/** Atomic pointer to the log checksum calculation function. This is actually
the only remaining "state" of the library. Hopefully can become removed. */
extern Log_checksum_algorithm_atomic_ptr log_checksum_algorithm_ptr;

/** Computes checksum of the given header and verifies if the checksum
is the same as the one stored in that header.
@param[in]  buf   header to verify
@return true iff checksums are the same */
bool log_header_checksum_is_ok(const byte *buf);

/**************************************************/ /**

 @name Log - file header read/write.

 *******************************************************/

/** @{ */

/** Serializes the log file header to the buffer.
@param[in]   header    the header to serialize
@param[out]  buf       the allocated buffer */
void log_file_header_serialize(const Log_file_header &header, byte *buf);

/** Deserializes the log file header stored in the buffer.
@param[in]   buf       the buffer to deserialize
@param[out]  header    the deserialized header
@return true iff checksum is correct */
bool log_file_header_deserialize(const byte *buf, Log_file_header &header);

/** Serializes and writes the log file header to the log file.
@param[in]   file_handle  handle for the opened log file
@param[in]   header       the file header
@return DB_SUCCESS or error */
dberr_t log_file_header_write(Log_file_handle &file_handle,
                              const Log_file_header &header);

/** Writes the serialized log file header to the log file.
@param[in]   file_handle  handle for the opened log file
@param[in]   buf          the serialized file header
@return DB_SUCCESS or error */
dberr_t log_file_header_write(Log_file_handle &file_handle, const byte *buf);

/** Reads the serialized log file header to the buffer.
@param[in]   file_handle  handle for the opened log file
@param[out]  buf          the allocated buffer for read
@return DB_SUCCESS or error */
dberr_t log_file_header_read(Log_file_handle &file_handle, byte *buf);

/** Reads and deserializes the log file header.
@param[in]   file_handle  handle for the opened log file
@param[out]  header       the file header read
@return DB_SUCCESS or error */
dberr_t log_file_header_read(Log_file_handle &file_handle,
                             Log_file_header &header);

/** Sets a specific flag in the mask built of redo log flags.
@param[in]  log_flags   mask of log flags
@param[in]  bit         bit number to set (flag) */
void log_file_header_set_flag(Log_flags &log_flags, uint32_t bit);

/** Resets a specific flag in the mask built of redo log flags.
@param[in]  log_flags   mask of log flags
@param[in]  bit         bit number to set (flag) */
void log_file_header_reset_flag(Log_flags &log_flags, uint32_t bit);

/** Checks if a specific flag is set in the mask built of redo log flags.
@param[in]  log_flags   mask of log flags
@param[in]  bit         bit number to check (flag)
@return true, iff flag is set */
bool log_file_header_check_flag(Log_flags log_flags, uint32_t bit);

/** @} */

/**************************************************/ /**

 @name Log - encryption header read/write.

 *******************************************************/

/** @{ */

/** Writes the serialized encryption meta data to the log file.
@param[in]   file_handle  handle for the opened log file
@param[in]   buf          the filled encryption buffer to write
@return DB_SUCCESS or error */
dberr_t log_encryption_header_write(Log_file_handle &file_handle,
                                    const byte *buf);

/** Reads the serialized encryption meta data from the log file.
@param[in]   file_handle  handle for the opened log file
@param[out]  buf          the allocated buffer for read
@return DB_SUCCESS or error */
dberr_t log_encryption_header_read(Log_file_handle &file_handle, byte *buf);

/** @} */

/**************************************************/ /**

 @name Log - checkpoint header read/write.

 *******************************************************/

/** @{ */

/** Serializes the log checkpoint header to the buffer.
@param[in]   header    the header to serialize
@param[out]  buf       the allocated buffer */
void log_checkpoint_header_serialize(const Log_checkpoint_header &header,
                                     byte *buf);

/** Deserializes the log checkpoint header stored in the buffer.
@param[in]   buf       the buffer to deserialize
@param[out]  header    the deserialized header
@return true iff checksum is correct */
bool log_checkpoint_header_deserialize(const byte *buf,
                                       Log_checkpoint_header &header);

/** Serializes and writes the log checkpoint header to the log file.
@param[in]   file_handle           handle for the opened log file
@param[in]   checkpoint_header_no  checkpoint header to be written
@param[in]   header                the checkpoint header
@return DB_SUCCESS or error */
dberr_t log_checkpoint_header_write(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    const Log_checkpoint_header &header);

/** Writes the serialized checkpoint header to the log file.
@param[in]   file_handle           handle for the opened log file
@param[in]   checkpoint_header_no  checkpoint header to be written
@param[in]   buf                   buffer containing the serialized checkpoint
                                   header to write
@return DB_SUCCESS or error */
dberr_t log_checkpoint_header_write(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    const byte *buf);

/** Reads the serialized log checkpoint header to the buffer.
@param[in]   file_handle           handle for the opened log file
@param[in]   checkpoint_header_no  checkpoint header to read
@param[out]  buf                   the allocated buffer for read
@return DB_SUCCESS or error */
dberr_t log_checkpoint_header_read(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    byte *buf);

/** Reads and deserializes the log checkpoint header.
@param[in]   file_handle           handle for the opened log file
@param[in]   checkpoint_header_no  checkpoint header to read
@param[out]  header                the checkpoint header read
@return DB_SUCCESS or error */
dberr_t log_checkpoint_header_read(
    Log_file_handle &file_handle, Log_checkpoint_header_no checkpoint_header_no,
    Log_checkpoint_header &header);

/** @} */

/**************************************************/ /**

 @name Log functions - data blocks read/write.

 *******************************************************/

/** @{ */

/** Writes the formatted log blocks with redo records to the log file.
The given log blocks must fit within the same single log file.
@param[in]  file_handle  handle for the opened log file
@param[in]  write_offset offset from the beginning of the given file
@param[in]  write_size   size of the data to write (must be divisible
                         by OS_FILE_LOG_BLOCK_SIZE)
@param[in]  buf          formatted log blocks with the data to write
@return DB_SUCCESS or error */
dberr_t log_data_blocks_write(Log_file_handle &file_handle,
                              os_offset_t write_offset, size_t write_size,
                              const byte *buf);

/** Reads log blocks with redo records from the log file, starting at
the given offset. The log blocks must exist within single log file.
@param[in]  file_handle  handle for the opened log file
@param[in]  read_offset  offset from the beginning of the given file
@param[in]  read_size    size of the data to read (must be divisible
                         by OS_FILE_LOG_BLOCK_SIZE)
@param[out] buf          allocated buffer to fill by the read
@return DB_SUCCESS or error */
dberr_t log_data_blocks_read(Log_file_handle &file_handle,
                             os_offset_t read_offset, size_t read_size,
                             byte *buf);

/** @} */

/**************************************************/ /**

 @name Log - files creation/deletion, path computation.

 *******************************************************/

/** @{ */

/** Provides path to directory with redo log files.
@param[in]  ctx  context within which files exist
@return path to #innodb_redo directory */
std::string log_directory_path(const Log_files_context &ctx);

/** Provides name of the log file with the given file id, e.g. '#ib_redo0'.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the log file
@return file name */
std::string log_file_name(const Log_files_context &ctx, Log_file_id file_id);

/** Provides full path to the log file, e.g. '/data/#innodb_redo/#ib_redo2'.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the log file
@return path to the log file (including file name) */
std::string log_file_path(const Log_files_context &ctx, Log_file_id file_id);

/** Provides full path to the temporary log file,
e.g. '/data/#innodb_redo/#ib_redo2_tmp'.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the file
@return path to the temporary log file (including file name) */
std::string log_file_path_for_unused_file(const Log_files_context &ctx,
                                          Log_file_id file_id);

/** List existing log files in the directory (does not include unused files).
@param[in]  ctx        context within which files exist
@param[out] ret        identifiers of existing log files
@return DB_SUCCESS or DB_ERROR */
dberr_t log_list_existing_files(const Log_files_context &ctx,
                                ut::vector<Log_file_id> &ret);

/** List existing unused log files in the directory.
@param[in]  ctx        context within which files exist
@param[out] ret        identifiers of existing unused log files
@return DB_SUCCESS or DB_ERROR */
dberr_t log_list_existing_unused_files(const Log_files_context &ctx,
                                       ut::vector<Log_file_id> &ret);

/** Renames the unused file to another unused file.
@param[in]  ctx                   context within which files exist
@param[in]  old_unused_file_id    id of file to rename
@param[in]  new_unused_file_id    new file id
@return DB_SUCCESS or DB_ERROR */
dberr_t log_rename_unused_file(const Log_files_context &ctx,
                               Log_file_id old_unused_file_id,
                               Log_file_id new_unused_file_id);

/** Renames a temporary log file to the non-temporary log file.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the file to rename
@return DB_SUCCESS or DB_ERROR */
dberr_t log_mark_file_as_in_use(const Log_files_context &ctx,
                                Log_file_id file_id);

/** Renames a non-temporary log file to the temporary log file.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the file to rename
@return DB_SUCCESS or DB_ERROR */
dberr_t log_mark_file_as_unused(const Log_files_context &ctx,
                                Log_file_id file_id);

/** Removes a temporary log file, if it existed.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the file to remove
@return DB_SUCCESS, DB_NOT_FOUND or DB_ERROR */
dberr_t log_remove_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id);

/** Removes all temporary log files in the directory. When failed to
remove a file, stops and returns error. In such case the last element
of the returned identifiers of files, represents the file for which
error has been encountered when trying to remove it.
@param[in]  ctx        context within which files exist
@return first: DB_SUCCESS or DB_ERROR
        second: identifiers of files for which remove has been called */
std::pair<dberr_t, ut::vector<Log_file_id>> log_remove_unused_files(
    const Log_files_context &ctx);

/** Removes a log file, if it existed.
@param[in]  ctx        context within which files exist
@param[in]  file_id    id of the file to remove
@return DB_SUCCESS, DB_NOT_FOUND or DB_ERROR */
dberr_t log_remove_file(const Log_files_context &ctx, Log_file_id file_id);

/** Removes a single existing log file (if it existed).
@param[in]  ctx         context within which files exist
@return first: DB_SUCCESS, DB_NOT_FOUND or DB_ERROR
        second: id of the removed file (if removed) */
std::pair<dberr_t, Log_file_id> log_remove_file(const Log_files_context &ctx);

/** Removes existing log files. When failed to remove a file, stops and
returns error. In such case the last element of the returned identifiers
of files, represents the file for which error has been encountered when
trying to remove it.
@param[in]  ctx         context within which files exist
@return first: DB_SUCCESS or DB_ERROR
        second: identifiers of files for which remove has been called */
std::pair<dberr_t, ut::vector<Log_file_id>> log_remove_files(
    const Log_files_context &ctx);

/** Creates a new temporary log file and resizes the file to the given size.
@param[in]  ctx            context within which files exist
@param[in]  file_id        id of the file to create
@param[in]  size_in_bytes  size of the file, in bytes
@return DB_SUCCESS or DB_ERROR */
dberr_t log_create_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id, os_offset_t size_in_bytes);

/** Resizes an existing temporary log file to the given size.
@param[in]  ctx            context within which files exist
@param[in]  file_id        id of the file to resize
@param[in]  size_in_bytes  requested size of the file, in bytes
@return DB_SUCCESS, DB_NOT_FOUND, DB_OUT_OF_DISK_SPACE or DB_ERROR */
dberr_t log_resize_unused_file(const Log_files_context &ctx,
                               Log_file_id file_id, os_offset_t size_in_bytes);

/** Resizes an existing log file to the given size.
@param[in]  ctx            context within which files exist
@param[in]  file_id        id of the file to resize
@param[in]  size_in_bytes  requested size of the file, in bytes
@return DB_SUCCESS, DB_NOT_FOUND, DB_OUT_OF_DISK_SPACE or DB_ERROR */
dberr_t log_resize_file(const Log_files_context &ctx, Log_file_id file_id,
                        os_offset_t size_in_bytes);

/** Searches for all possible log files existing on disk in the log directory.
Performs only very minimal validation of the files, checking if files could be
opened and have valid file size.
@param[in]   ctx        context within which files exist
@param[in]   read_only  true: check file permissions only for reading,
                        false: check for both reading and writing
@param[out]  found      list of <file_id, size of file> for each file found
@return DB_SUCCESS, DB_NOT_FOUND or DB_ERROR */
dberr_t log_collect_existing_files(const Log_files_context &ctx, bool read_only,
                                   ut::vector<Log_file_id_and_size> &found);

/** Generate unique identifier for the redo log files.
@return random uuid > 0 */
Log_uuid log_generate_uuid();

/** @} */

/**************************************************/ /**

 @name Log - log blocks format.

 *******************************************************/

/** @{ */

/* Definition of inline functions. */

/** Gets a log block number stored in the header. The number corresponds
to lsn range for data stored in the block.

During recovery, when a next block is being parsed, a next range of lsn
values is expected to be read. This corresponds to a log block number
increased by one (modulo LOG_BLOCK_MAX_NO). However, if an unexpected
number is read from the header, it is then considered the end of the
redo log and recovery is finished. In such case, the next block is most
likely an empty block or a block from the past, because the redo log
files might be reused.

@param[in]	log_block	log block (may be invalid or empty block)
@return log block number stored in the block header */
inline uint32_t log_block_get_hdr_no(const byte *log_block) {
  return ~LOG_BLOCK_FLUSH_BIT_MASK &
         mach_read_from_4(log_block + LOG_BLOCK_HDR_NO);
}

/** Sets the log block number stored in the header.
NOTE that this must be set before the flush bit!

@param[in,out]	log_block	log block
@param[in]	n		log block number: must be in (0, 1G] */
inline void log_block_set_hdr_no(byte *log_block, uint32_t n) {
  ut_a(n > 0);
  ut_a(n < LOG_BLOCK_FLUSH_BIT_MASK);
  ut_a(n <= LOG_BLOCK_MAX_NO);
  mach_write_to_4(log_block + LOG_BLOCK_HDR_NO, n);
}

/** Gets a log block data length.
@param[in]	log_block	log block
@return log block data length measured as a byte offset from the block start */
inline uint32_t log_block_get_data_len(const byte *log_block) {
  return mach_read_from_2(log_block + LOG_BLOCK_HDR_DATA_LEN);
}

/** Sets the log block data length.
@param[in,out]	log_block	log block
@param[in]	len		data length (@see log_block_get_data_len) */
inline void log_block_set_data_len(byte *log_block, uint32_t len) {
  mach_write_to_2(log_block + LOG_BLOCK_HDR_DATA_LEN, len);
}

/** Gets an offset to the beginning of the first group of log records
in a given log block.
@param[in]	log_block	log block
@return first mtr log record group byte offset from the block start,
0 if none. */
inline uint32_t log_block_get_first_rec_group(const byte *log_block) {
  return mach_read_from_2(log_block + LOG_BLOCK_FIRST_REC_GROUP);
}

/** Sets an offset to the beginning of the first group of log records
in a given log block.
@param[in,out]	log_block	log block
@param[in]	offset		offset, 0 if none */
inline void log_block_set_first_rec_group(byte *log_block, uint32_t offset) {
  mach_write_to_2(log_block + LOG_BLOCK_FIRST_REC_GROUP, offset);
}

/** Gets a log block epoch_no. For details: @see LOG_BLOCK_EPOCH_NO.
@param[in]	log_block	log block
@return epoch number */
inline uint32_t log_block_get_epoch_no(const byte *log_block) {
  return mach_read_from_4(log_block + LOG_BLOCK_EPOCH_NO);
}

/** Sets a log block epoch_no. For details: @see LOG_BLOCK_EPOCH_NO.
@param[in,out]  log_block  log block
@param[in]      no         epoch number */
inline void log_block_set_epoch_no(byte *log_block, uint32_t no) {
  mach_write_to_4(log_block + LOG_BLOCK_EPOCH_NO, no);
}

/** Converts a lsn to a log block epoch number.
For details @see LOG_BLOCK_EPOCH_NO.
@param[in]	lsn	lsn of a byte within the block
@return log block epoch number, it is > 0 */
inline uint32_t log_block_convert_lsn_to_epoch_no(lsn_t lsn) {
  return 1 +
         static_cast<uint32_t>(lsn / OS_FILE_LOG_BLOCK_SIZE / LOG_BLOCK_MAX_NO);
}

/** Converts a lsn to a log block number. Consecutive log blocks have
consecutive numbers (unless the sequence wraps). It is guaranteed that
the calculated number is greater than zero.

@param[in]	lsn	lsn of a byte within the block
@return log block number, it is > 0 and <= 1G */
inline uint32_t log_block_convert_lsn_to_hdr_no(lsn_t lsn) {
  return 1 +
         static_cast<uint32_t>(lsn / OS_FILE_LOG_BLOCK_SIZE % LOG_BLOCK_MAX_NO);
}

/** Calculates the checksum for a log block.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_calc_checksum(const byte *log_block) {
  return log_checksum_algorithm_ptr.load()(log_block);
}

/** Calculates the checksum for a log block using the MySQL 5.7 algorithm.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_calc_checksum_crc32(const byte *log_block) {
  return ut_crc32(log_block, OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);
}

/** Calculates the checksum for a log block using the "no-op" algorithm.
@return        checksum */
inline uint32_t log_block_calc_checksum_none(const byte *) {
  return LOG_NO_CHECKSUM_MAGIC;
}

/** Gets value of a log block checksum field.
@param[in]	log_block	log block
@return checksum */
inline uint32_t log_block_get_checksum(const byte *log_block) {
  return mach_read_from_4(log_block + OS_FILE_LOG_BLOCK_SIZE -
                          LOG_BLOCK_CHECKSUM);
}

/** Sets value of a log block checksum field.
@param[in,out]	log_block	log block
@param[in]	checksum	checksum */
inline void log_block_set_checksum(byte *log_block, uint32_t checksum) {
  mach_write_to_4(log_block + OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_CHECKSUM,
                  checksum);
}

/** Stores a 4-byte checksum to the trailer checksum field of a log block.
This is used before writing the log block to disk. The checksum in a log
block is used in recovery to check the consistency of the log block.
@param[in]	log_block	 log block (completely filled in!) */
inline void log_block_store_checksum(byte *log_block) {
  log_block_set_checksum(log_block, log_block_calc_checksum(log_block));
}

/** Gets value of a log block encrypt bit (true or false).
@param[in]  log_block  log block
@return true iff encrypt bit is set */
inline bool log_block_get_encrypt_bit(const byte *log_block) {
  if (LOG_BLOCK_ENCRYPT_BIT_MASK &
      mach_read_from_2(log_block + LOG_BLOCK_HDR_DATA_LEN)) {
    return true;
  }

  return false;
}

/** Sets value of a log block encrypt bit (true or false).
@param[in]  log_block  log block to modify
@param[in]  val        the value to set (true or false) */
inline void log_block_set_encrypt_bit(byte *log_block, bool val) {
  uint32_t field;

  field = mach_read_from_2(log_block + LOG_BLOCK_HDR_DATA_LEN);

  if (val) {
    field = field | LOG_BLOCK_ENCRYPT_BIT_MASK;
  } else {
    field = field & ~LOG_BLOCK_ENCRYPT_BIT_MASK;
  }

  mach_write_to_2(log_block + LOG_BLOCK_HDR_DATA_LEN, field);
}

/** Serializes the log data block header to the redo log block buffer which
already contains redo log data (must have the redo data before this call).
@param[in]   header    the header to serialize
@param[out]  buf       the buffer containing the redo log block with the data */
inline void log_data_block_header_serialize(const Log_data_block_header &header,
                                            byte *buf) {
  log_block_set_epoch_no(buf, header.m_epoch_no);
  log_block_set_hdr_no(buf, header.m_hdr_no);
  log_block_set_data_len(buf, header.m_data_len);
  log_block_set_first_rec_group(buf, header.m_first_rec_group);
  log_block_store_checksum(buf);
}

/** Deserializes the log data block header stored in the buffer.
@param[in]   buf       the buffer to deserialize
@param[out]  header    the deserialized header
@return true iff checksum is correct */
inline bool log_data_block_header_deserialize(const byte *buf,
                                              Log_data_block_header &header) {
  header.m_epoch_no = log_block_get_epoch_no(buf);
  header.m_hdr_no = log_block_get_hdr_no(buf);
  header.m_data_len = log_block_get_data_len(buf);
  header.m_first_rec_group = log_block_get_first_rec_group(buf);
  return log_block_calc_checksum(buf) == log_block_get_checksum(buf);
}

/** @} */

#endif /* !log0files_io_h */
