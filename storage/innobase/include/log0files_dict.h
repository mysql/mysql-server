/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
 @file include/log0files_dict.h

 In-memory dictionary of log files (keeps their meta data).

 The dictionary is built by the @see log_files_find_and_analyze().

 *******************************************************/

#ifndef log0files_dict_h
#define log0files_dict_h

/* std::for_each */
#include <algorithm>

/* std::iterator */
#include <iterator>

/* Log_file, Log_file_id, LOG_START_LSN */
#include "log0types.h"

/* os_offset_t */
#include "os0file.h"

/* ut::map */
#include "ut0ut.h"

/** In-memory dictionary of meta data of existing log files.
This is a plain data structure only. It has no dependency. */
class Log_files_dict {
 private:
  using Log_files_map = ut::map<Log_file_id, Log_file>;
  using Log_files_map_iterator = typename Log_files_map::const_iterator;

 public:
  class Const_iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Log_file;
    using difference_type = std::ptrdiff_t;
    using pointer = const Log_file *;
    using reference = const Log_file &;

    explicit Const_iterator(Log_files_dict::Log_files_map_iterator it);
    const Log_file &operator*() const;
    const Log_file *operator->() const;
    Const_iterator &operator++();
    Const_iterator operator++(int);
    Const_iterator &operator--();
    Const_iterator operator--(int);
    bool operator==(const Const_iterator &rhs) const;
    bool operator!=(const Const_iterator &rhs) const;

   private:
    Log_files_dict::Log_files_map_iterator m_it;
  };

  /** Constructs an empty dictionary. */
  explicit Log_files_dict(Log_files_context &files_ctx);

  Log_files_dict(Log_files_dict &&other);
  Log_files_dict &operator=(Log_files_dict &&other);

  /** @return context within which files exist */
  const Log_files_context &ctx() const;

  /** Searches for an existing log file, that contains the provided lsn.
  @param[in]  lsn       lsn to search for
  @retval iterator pointing to the log file if file has been found
  @retval end() if there was no file containing the provided lsn */
  Const_iterator find(lsn_t lsn) const;

  /** Provides log file for the given file id.
  @param[in]  file_id   id of the log file
  @return pointer to the log file */
  Const_iterator file(Log_file_id file_id) const;

  /** Clears the whole dictionary. */
  void clear();

  /** Removes the meta data about the given log file (which denotes
  the file does not exist anymore) from this data structure.
  @param[in]  file_id   id of the log file */
  void erase(Log_file_id file_id);

  /** Add meta data for the existing log file. It asserts that the meta data
  for that file has not been added yet to this data structure.
  @see Log_file_handle::m_encryption_metadata
  @param[in]  file_id              id of the log file
  @param[in]  size_in_bytes        size of the log file (in bytes)
  @param[in]  start_lsn            lsn of the first data byte in file
  @param[in]  full                 true iff file is marked as full
  @param[in]  consumed             true iff file is marked as consumed
  @param[in]  encryption_metadata  encryption metadata */
  void add(Log_file_id file_id, os_offset_t size_in_bytes, lsn_t start_lsn,
           bool full, bool consumed, Encryption_metadata &encryption_metadata);

  /** Add meta data for the existing log file. It asserts that the meta data
  for that file has not been added yet to this data structure.
  @see Log_file_handle::m_encryption_metadata
  @param[in]  file_id              id of the log file
  @param[in]  size_in_bytes        size of the log file (in bytes)
  @param[in]  start_lsn            lsn of the first data byte in file
  @param[in]  full                 true iff file is marked as full
  @param[in]  encryption_metadata  encryption metadata */
  void add(Log_file_id file_id, os_offset_t size_in_bytes, lsn_t start_lsn,
           bool full, Encryption_metadata &encryption_metadata);

  /** Marks a given log file as consumed.
  @param[in]  file_id         id of the log file */
  void set_consumed(Log_file_id file_id);

  /** Marks a given log file as full.
  @param[in]  file_id         id of the log file */
  void set_full(Log_file_id file_id);

  /** Marks a given log file as incomplete (undo marking as full).
  @param[in]  file_id         id of the log file */
  void set_incomplete(Log_file_id file_id);

  /** Changes size of the file. Updates m_end_lsn accordingly.
  @param[in]  file_id         id of the log file
  @param[in]  new_size        new size (expressed in bytes) */
  void set_size(Log_file_id file_id, os_offset_t new_size);

  /** @return iterator to the first log file (with the smallest id) */
  Const_iterator begin() const;

  /** @return iterator after the last log file */
  Const_iterator end() const;

  /** @return true iff structure is empty */
  bool empty() const;

  /** @return the oldest redo log file */
  const Log_file &front() const;

  /** @return the newest redo log file */
  const Log_file &back() const;

 private:
  /** Context within which log files exist. */
  const Log_files_context &m_files_ctx;

  /** Meta information about each existing redo log file. */
  Log_files_map m_files_by_id;
};

template <typename F>
void log_files_for_each(const Log_files_dict &files, F functor) {
  std::for_each(files.begin(), files.end(), functor);
}

/** Calls the given functor for each of existing log files on path
from a file containing start_lsn to a file containing end_lsn - 1.
Asserts that such a path exists (going through existing log files).
When the range is empty (start_lsn >= end_lsn), no file is visited.
@param[in]  files       in-memory dictionary of log files
@param[in]  start_lsn   path starts at file reported by find(start_lsn)
@param[in]  end_lsn     path ends in file with m_end_lsn >= end_lsn
@param[in]  functor     functor receiving a reference to Log_file */
template <typename F>
void log_files_for_each(const Log_files_dict &files, lsn_t start_lsn,
                        lsn_t end_lsn, F functor) {
  ut_a(start_lsn >= LOG_START_LSN);
  ut_a(start_lsn <= end_lsn);
  if (start_lsn == end_lsn) {
    return;
  }

  auto begin = files.find(start_lsn);
  ut_a(begin != files.end());
  ut_a(begin->m_start_lsn <= start_lsn);

  auto end = files.find(end_lsn - 1);
  ut_a(end != files.end());
  ut_a(end_lsn <= end->m_end_lsn);
  end++;

  ut_a(end == files.end() || end_lsn <= end->m_start_lsn);

  std::for_each(begin, end, functor);
}

/** Computes logical capacity for the given physical size of the redo log file.
@param[in]  file_size_in_bytes    total size of file, expressed in bytes
@param[out] lsn_capacity          logical capacity of the file
@retval true    succeeded to compute the logical capacity
@retval false   params were invalid (file size was too small or too big) */
bool log_file_compute_logical_capacity(os_offset_t file_size_in_bytes,
                                       lsn_t &lsn_capacity);

/** Computes end_lsn for the given: start_lsn and size of the redo log file.
@param[in]  start_lsn             LSN of the first data byte within the file
@param[in]  file_size_in_bytes    total size of file, expressed in bytes
@param[out] end_lsn               LSN after the last data byte within the file
@retval true    succeeded to compute end_lsn
@retval false   params were invalid */
bool log_file_compute_end_lsn(lsn_t start_lsn, os_offset_t file_size_in_bytes,
                              lsn_t &end_lsn);

/** Counts the total number of existing log files.
@param[in]  files   in-memory dictionary of log files
@return number of existing log files. */
size_t log_files_number_of_existing_files(const Log_files_dict &files);

/** Counts the total number of existing and marked as consumed log files.
@param[in]  files   in-memory dictionary of log files
@return number of existing and marked as consumed log files. */
size_t log_files_number_of_consumed_files(const Log_files_dict &files);

/** Computes the total size of the existing log files (sum of sizes).
@note Each file starts with LOG_FILE_HDR_SIZE bytes of headers.
@param[in]  files   in-memory dictionary of log files
@return computed total size of existing log files */
os_offset_t log_files_size_of_existing_files(const Log_files_dict &files);

/** Computes the total capacity of the existing log files (sum of capacities).
@note Capacity of a file is smaller than size of the file by LOG_FILE_HDR_SIZE.
@param[in]  files   in-memory dictionary of log files
@return computed total capacity of existing log files */
lsn_t log_files_capacity_of_existing_files(const Log_files_dict &files);

/** Finds the largest existing log file (with the largest m_size_in_bytes).
@param[in]  files   in-memory dictionary of log files
@return the largest file or files.end() if there is no file at all */
Log_files_dict::Const_iterator log_files_find_largest(
    const Log_files_dict &files);

#endif /* !log0files_dict_h */
