/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/binlog/services/iterator/file_storage.h"
#include <my_dbug.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/service_plugin_registry.h>
#include <iomanip>
#include <string>
#include <vector>
#include "mutex_lock.h"
#include "scope_guard.h"  // Scope_guard
#include "sql/binlog.h"
#include "sql/current_thd.h"

extern bool opt_source_verify_checksum;

using Format_description_event = mysql::binlog::event::Format_description_event;
using Log_event_type = mysql::binlog::event::Log_event_type;

/// @brief This class holds the context of the iterator.
///
/// The context of the iterator contains runtime data, such as the current
/// file being read from, the reader instantiated, the pointer to the buffer
/// used to store the event read, the set of transactions to be excluded, as
/// well as the current format description event.
///
/// Note that we need to store the current format description event to be able
/// to decode the a few events, such as the GTID and the Rotate event.
class Binlog_iterator_ctx {
  /// @brief This is a passthrough allocator.
  ///
  /// This allocator returns the buffer referenced in the context always.
  /// If the reader requests more memory to be allocated and the memory
  /// requested exceeds the capacity of the buffer in use, the allocator
  /// will return nullptr, meaning that the buffer is insufficient to store
  /// the next event. This causes the reader to fail with MEM_ALLOCATE error.
  class Passthrough_allocator {
   public:
    /// Do not delegate the memory to the event as that may have the
    /// event deallocate it at the destructor. We do not want that.
    enum { DELEGATE_MEMORY_TO_EVENT_OBJECT = false };
    unsigned char *allocate(size_t size) {
      DBUG_TRACE;
      if (size >= m_ctx->get_buffer_capacity()) return nullptr;
      return m_ctx->get_buffer();
    }
    void deallocate(unsigned char *ptr [[maybe_unused]]) { DBUG_TRACE; }
    void set_context(Binlog_iterator_ctx *ctx) {
      DBUG_TRACE;
      m_ctx = ctx;
    }

   private:
    Binlog_iterator_ctx *m_ctx{nullptr};
  };

  typedef Basic_binlog_file_reader<Binlog_ifile, Binlog_event_data_istream,
                                   Binlog_event_object_istream,
                                   Passthrough_allocator>
      File_reader;

 private:
  /// @brief  This is the current file opened.
  std::string m_current_file_open{};
  /// @brief  This is a reference to the reader of the current file.
  File_reader *m_reader{nullptr};
  /// @brief  This is a reference to the current format description event.
  Format_description_event m_current_fde{BINLOG_VERSION, server_version};
  /// @brief The local tsid map
  Tsid_map m_local_tsid_map{nullptr};
  /// @brief  This is the set of gtids that are to be excluded while using this
  /// iterator.
  Gtid_set m_excluded_gtid_set{&m_local_tsid_map};
  /// @brief  This is a reference to the buffer used to store events read.
  unsigned char *m_buffer{nullptr};
  /// @brief  This a the capacity of the buffer used to store events read.
  uint64_t m_buffer_size{0};
  /// @brief specifies if this context was properly constructed and therefore is
  /// valid.
  bool m_is_valid{false};
  /// @brief The log file information to lock files from being purged,
  ///        i.e., the log one is reading from.
  Log_info m_linfo{};

  /// @brief Get the next file to open object
  ///
  /// @return a pair containing an boolean stating whether there was an error or
  /// not and the filename. If there was an error, the filename is empty.
  std::pair<bool, std::string> get_next_file_to_open() const {
    DBUG_TRACE;

    // Go through the index file and get the next file entry in it.

    // this should not happen. This member function is always called after we
    // are already reading from a file.
    assert(!m_current_file_open.empty());
    if (m_current_file_open.empty()) return std::make_pair(true, "");

    mysql_mutex_assert_owner(mysql_bin_log.get_index_lock());
    auto [error, files_in_index] = mysql_bin_log.get_log_index(false);
    if (files_in_index.empty() || error != LOG_INFO_EOF)
      return std::make_pair(true, "");
    auto index_it = std::find(files_in_index.begin(), files_in_index.end(),
                              m_current_file_open);

    // file not found !! This is an error.
    if (index_it == files_in_index.end()) return std::make_pair(true, "");

    // current file was the last one
    if ((++index_it) == files_in_index.end()) return std::make_pair(false, "");

    // next file
    return std::make_pair(false, *index_it);
  }

 public:
  Binlog_iterator_ctx(const Binlog_iterator_ctx &rhs) = delete;
  Binlog_iterator_ctx &operator=(const Binlog_iterator_ctx &rhs) = delete;

  Binlog_iterator_ctx(bool verify_checksum, const Gtid_set &excluded_gtids)
      : m_reader(new File_reader(verify_checksum)) {
    DBUG_TRACE;
    if (m_reader == nullptr) return;
    m_reader->allocator()->set_context(this);
    if (m_excluded_gtid_set.add_gtid_set(&excluded_gtids) == RETURN_STATUS_OK)
      m_is_valid = true;
    m_linfo.thread_id = current_thd->thread_id();
    mysql_bin_log.register_log_info(&m_linfo);
  }
  ~Binlog_iterator_ctx() {
    DBUG_TRACE;
    if (m_reader != nullptr) m_reader->close();
    delete m_reader;
    mysql_bin_log.unregister_log_info(&m_linfo);
    m_is_valid = false;
  }

  /// @brief Checks whether the given context is valid or not.
  /// @return true if the context is valid, false otherwise.
  bool is_valid() const { return m_is_valid; }

  /// @brief Get the current file open object
  ///
  /// @return const std::string the current file opened. If no file is opened,
  /// this returns an empty string.
  std::string get_current_file_open() const {
    DBUG_TRACE;
    return m_current_file_open;
  }

  /// @brief Set the current file open object
  ///
  /// @param filename the name of the current file opened.
  void set_current_file_open(const std::string &filename) {
    DBUG_TRACE;
    m_current_file_open = filename;
  }

  /// @brief Get the reader object
  ///
  /// @return File_reader& The current reader.
  File_reader &get_reader() {
    DBUG_TRACE;
    return *m_reader;
  }

  /// @brief Set the buffer object
  ///
  /// @param buffer A pointer to the buffer.
  void set_buffer(unsigned char *buffer) {
    DBUG_TRACE;
    m_buffer = buffer;
  }

  /// @brief Get the buffer object
  ///
  /// @return unsigned char* A pointer to the buffer.
  unsigned char *get_buffer() {
    DBUG_TRACE;
    return m_buffer;
  }

  /// @brief Get the buffer capacity object
  ///
  /// @return uint64_t The buffer capacity.
  uint64_t get_buffer_capacity() const {
    DBUG_TRACE;
    return m_buffer_size;
  }

  /// @brief Set the buffer capacity object
  ///
  /// @param buffer_size The buffer capacity.
  void set_buffer_capacity(uint64_t buffer_size) {
    DBUG_TRACE;
    m_buffer_size = buffer_size;
  }

  /// @brief Set the fde object
  ///
  /// @param new_fde The new format description event to be copied.
  void set_fde(const Format_description_event &new_fde) {
    DBUG_TRACE;
    m_current_fde = new_fde;
  }

  /// @brief Get the fde object
  ///
  /// @return const Format_description_event& The format description event in
  /// use.
  const Format_description_event &get_fde() const {
    DBUG_TRACE;
    return m_current_fde;
  }

  /// @brief This function checks whether the GTID event in the buffer is to be
  /// skipped or not.
  ///
  /// @param buffer Contains the GTID event serialized.
  ///
  /// @return std::tuple<bool, bool, uint64_t> A triple containing a boolean
  /// stating whether there was an error, a boolean stating whether the
  /// transaction is to be skipped or not and the size of the transaction.
  std::tuple<bool, bool, uint64_t> shall_skip_transaction(
      const unsigned char *buffer) {
    DBUG_TRACE;
    auto event_type = static_cast<Log_event_type>(buffer[EVENT_TYPE_OFFSET]);
    switch (event_type) {
      case mysql::binlog::event::GTID_LOG_EVENT:
      case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
        Gtid_log_event gtid_ev(reinterpret_cast<const char *>(buffer),
                               &m_current_fde);
        if (!gtid_ev.is_valid())
          return std::make_tuple<bool, bool, uint64_t>(true, false, 0);
        Gtid gtid{};
        const auto &tsid = gtid_ev.get_tsid();
        gtid.sidno = m_local_tsid_map.tsid_to_sidno(tsid);
        if (gtid.sidno == 0)
          return std::make_tuple<bool, bool, uint64_t>(
              false, false, static_cast<uint64_t>(gtid_ev.transaction_length));
        gtid.gno = gtid_ev.get_gno();
        return std::make_tuple<bool, bool, uint64_t>(
            false, m_excluded_gtid_set.contains_gtid(gtid),
            static_cast<uint64_t>(gtid_ev.transaction_length));
      }
      default:
        return std::make_tuple<bool, bool, uint64_t>(false, false, 0);
    }
  }

  /// @brief Locks the log files in the index that are as recent as the file
  /// provided, including the file provided.
  /// @param file the file to pin. All other subsequent and more recent are also
  /// pinned.
  /// @return false if success, true otherwise.
  bool pin_log_files(const std::string &file) {
    DBUG_TRACE;
    mysql_mutex_assert_owner(mysql_bin_log.get_index_lock());
    return mysql_bin_log.find_log_pos(&m_linfo, file.c_str(), false) != 0;
  }

  /// @brief This member function opens the next file.
  ///
  /// @return kBinlogIteratorGetEndOfChanges if there are no more files to open
  /// @return kBinlogIteratorGetErrorUnspecified if failed to open the file.
  /// @return kBinlogIteratorGetOk on successful open file.
  Binlog_iterator_service_get_status open_next_file() {
    // This is the end of this file, let's rotate if we have to
    MUTEX_LOCK(index_lock_guard, mysql_bin_log.get_index_lock());
    auto [error, next_file_to_open] = get_next_file_to_open();

    // error updating the next file to open
    if (error) return kBinlogIteratorGetErrorUnspecified;

    // are there any more files to read, if not then remove the lock
    // on the last file read and return EOF
    if (next_file_to_open.empty()) {
      DBUG_PRINT("debug", ("reached EOF after having processed file %s",
                           get_current_file_open().c_str()));
      return kBinlogIteratorGetEndOfChanges;
    }

    // release the previous file for purging activities and lock
    // the next one
    if (pin_log_files(next_file_to_open))
      return kBinlogIteratorGetErrorUnspecified;

    // open the file for reading
    if (get_reader().open(next_file_to_open.c_str()))
      return kBinlogIteratorGetErrorUnspecified;
    set_current_file_open(next_file_to_open);

    DBUG_PRINT("debug", ("continue reading from file %s",
                         get_current_file_open().c_str()));

    return kBinlogIteratorGetOk;
  }

  /// @brief This member function checks if we need to move the cursor or switch
  /// to the next file.
  ///
  /// @return std::tuple<kBinlogIteratorGetEndOfChanges, 0> If there are no more
  /// changes to move the cursor to.
  /// @return std::tuple<kBinlogIteratorGetErrorUnspecified, 0> If there was an
  /// unspecified error.
  /// @return std::tuple<kBinlogIteratorGetOk, n> If the repositioning of the
  /// cursor was successful. On the second element of the tuple we return the
  /// size of the entry to be read.
  std::tuple<Binlog_iterator_service_get_status, uint64_t> update_cursor() {
    DBUG_TRACE;

    assert(likely(mysql_bin_log.is_open()));
    auto &reader{get_reader()};
    auto saved_pos{reader.position()};

    unsigned char buffer[LOG_EVENT_HEADER_LEN];

    while (true) {
      // save the position so that we can seek to it later on
      saved_pos = reader.position();
      auto *ifile = reader.ifile();

      // read the event header
      auto saved_header_pos = ifile->position();
      auto read_bytes = ifile->read(buffer, LOG_EVENT_MINIMAL_HEADER_LEN);
      ifile->seek(saved_header_pos);

      if (read_bytes == 0) {
        // end of file, try to open the next one, if there is one
        auto ret{kBinlogIteratorGetOk};
        if ((ret = open_next_file()) != kBinlogIteratorGetOk)
          return std::make_tuple(ret, 0);
        continue;
      } else if (read_bytes < 0 ||
                 read_bytes <
                     static_cast<ssize_t>(LOG_EVENT_MINIMAL_HEADER_LEN)) {
        // corruption ?
        return std::make_tuple(kBinlogIteratorGetErrorUnspecified, 0);
      } else if (read_bytes == LOG_EVENT_MINIMAL_HEADER_LEN) {
        // have we read a gtid? if so, we need to check if we skip the
        // transaction
        switch (static_cast<Log_event_type>(buffer[EVENT_TYPE_OFFSET])) {
          case mysql::binlog::event::GTID_LOG_EVENT:
          case mysql::binlog::event::GTID_TAGGED_LOG_EVENT: {
            // seek to the beginning of the log event and deserialize the GTID
            // event
            unsigned int bytes_read{0};
            unsigned char gtid_buffer_stack
                [mysql::binlog::event::Gtid_event::get_max_event_length()];
            memset(gtid_buffer_stack, 0,
                   mysql::binlog::event::Gtid_event::get_max_event_length());
            unsigned char *gtid_buffer = gtid_buffer_stack;

            // swap buffer in the context
            auto *saved_buffer = get_buffer();
            auto saved_buffer_capacity = get_buffer_capacity();
            set_buffer(gtid_buffer);
            set_buffer_capacity(
                mysql::binlog::event::Gtid_event::get_max_event_length());
            auto swap_and_free_buffer_guard{create_scope_guard([&]() {
              set_buffer(saved_buffer);
              set_buffer_capacity(saved_buffer_capacity);
            })};

            // read the event into the temporarily allocated buffer
            if (reader.read_event_data(&gtid_buffer, &bytes_read)) {
              reader.seek(saved_pos);
              return std::make_tuple(kBinlogIteratorGetErrorUnspecified, 0);
            }

            // check if we need to skip it or not
            auto [error, is_skip_trx, trx_size] =
                shall_skip_transaction(gtid_buffer);
            if (error)
              return std::make_tuple(kBinlogIteratorGetErrorUnspecified, 0);
            if (is_skip_trx) {  // SKIP
              DBUG_PRINT("debug", ("Skip transaction. Seeking from "
                                   "%llu to %llu (in '%s')",
                                   saved_pos, saved_pos + trx_size,
                                   get_current_file_open().c_str()));
              reader.seek(saved_pos + trx_size);
              continue;
            } else {  // DO NOT SKIP
              reader.seek(saved_pos);
              return std::make_tuple(
                  kBinlogIteratorGetOk,
                  uint4korr(buffer + EVENT_LEN_OFFSET) + LOG_EVENT_HEADER_LEN);
            }
          }
          default:  // Any other event than a GTID event
            return std::make_tuple(
                kBinlogIteratorGetOk,
                uint4korr(buffer + EVENT_LEN_OFFSET) + LOG_EVENT_HEADER_LEN);
        }
      } else {
        // Read more than LOG_EVENT_MINIMAL_HEADER_LEN - wait... what? :/
        assert(false);
        return std::make_tuple(kBinlogIteratorGetErrorUnspecified, 0);
      }
    }

    assert(false);
    return std::make_tuple(kBinlogIteratorGetErrorUnspecified, 0);
  }
};

struct my_h_binlog_storage_iterator_imp {
  Binlog_iterator_ctx *m_ctx{nullptr};
  my_h_binlog_storage_iterator_imp() : m_ctx(nullptr) {}
  ~my_h_binlog_storage_iterator_imp() {
    if (m_ctx != nullptr) {
      delete m_ctx;
      m_ctx = nullptr;
    }
  }
  my_h_binlog_storage_iterator_imp &operator=(
      const my_h_binlog_storage_iterator_imp &rhs) = delete;
  my_h_binlog_storage_iterator_imp(
      const my_h_binlog_storage_iterator_imp &other) = delete;
};

namespace binlog::services::iterator {

/// @brief Convenient function to cast the opaque iterator pointer.
/// @param iterator the opaque iterator pointer
/// @return a pointer to the casted iterator.
static my_h_binlog_storage_iterator_imp *iterator_cast(
    my_h_binlog_storage_iterator iterator) {
  return reinterpret_cast<my_h_binlog_storage_iterator_imp *>(iterator);
}

const std::string FileStorage::SERVICE_NAME{"binlog_storage_iterator.file"};

/// @brief Gets the previous gtids log event from the given reader.
///
/// @param binlog_file_reader the reader open
/// @return a pointer to the previous gtids log event
static Previous_gtids_log_event *find_previous_gtids_event(
    Binlog_file_reader &binlog_file_reader) {
  DBUG_TRACE;
  while (true) {
    auto *ev = binlog_file_reader.read_event_object();
    if (ev == nullptr) return nullptr;

    if (ev->get_type_code() == mysql::binlog::event::PREVIOUS_GTIDS_LOG_EVENT) {
      return dynamic_cast<Previous_gtids_log_event *>(ev);
    }

    delete ev;
  }

  return nullptr;
}

/// @brief Checks whether transactions requested have been purged already or
/// not.
///
/// @param excluded The set of transactions excluded transactions/to be ignored.
/// @return true if there are transactions in the purged set that were needed.
/// @return false if we are ignoring all purged transactions.
static bool has_purged_needed_gtids_already(const Gtid_set &excluded) {
  DBUG_TRACE;
  const auto *purged = gtid_state->get_lost_gtids();
  Checkable_rwlock::Guard guard(*purged->get_tsid_map()->get_tsid_lock(),
                                Checkable_rwlock::enum_lock_type::WRITE_LOCK);
  return !purged->is_subset(&excluded);
}

/// @brief Computes the binlog files that one needs to handle to get the
///        specified transactions.
///
/// @param files_in_index The contents of the index file.
/// @param excluded The transactions that the consumer does not care about.
/// @param files Output parameter. If successful, this contains the list of
/// files.
/// @return true if there was an error or there are not enough binlog to serve
/// the request.
/// @return false success.
static bool find_files(std::list<std::string> &files_in_index,
                       const Gtid_set &excluded,
                       std::list<std::string> &files) {
  DBUG_TRACE;

  // reverse the list of files so that we start from the most recent one
  // and iterate backwards until the oldest one
  std::reverse(files_in_index.begin(), files_in_index.end());

  for (const auto &file : files_in_index) {
    // open the file
    Binlog_file_reader binlog_file_reader(opt_source_verify_checksum);
    if (binlog_file_reader.open(file.c_str())) return true;

    // remove it from the set to process
    files.push_front(file);

    // search the previous event
    auto *prev_gtids_ev = find_previous_gtids_event(binlog_file_reader);
    // it can happen that after a crash while the binary log is being
    // rotated there is no Previous_gtid_log_event in one file in the
    // binary log file sequence. In that case, we continue the iteration.
    if (prev_gtids_ev == nullptr) continue;

    Tsid_map local_tsid_map{nullptr};
    Gtid_set previous{&local_tsid_map};
    prev_gtids_ev->add_to_set(&previous);
    delete prev_gtids_ev;

    // check if there are still gtids to fetch from the previous file
    if (previous.is_subset(&excluded))
      // there is no need to look into older files
      return false;
  }
  return true;
}

DEFINE_METHOD(Binlog_iterator_service_init_status, FileStorage::init,
              (my_h_binlog_storage_iterator * iterator,
               const char *excluded_gtids_as_string)) {
  DBUG_TRACE;
  // this should never happen, even if the binary log is closed due to binlog
  // error action.
  *iterator = nullptr;
  if (unlikely(!mysql_bin_log.is_open()))
    return kBinlogIteratorInitErrorLogClosed;
  MUTEX_LOCK(index_lock_guard, mysql_bin_log.get_index_lock());
  auto [error, files_in_index] = mysql_bin_log.get_log_index(false);
  DBUG_PRINT("debug", ("Iterator initialising with excluded gtid set: %s",
                       excluded_gtids_as_string));

  if (files_in_index.empty() || error != LOG_INFO_EOF)
    return kBinlogIteratorInitErrorUnspecified;
  // initialize the excluded gtid set
  Tsid_map local_tsid_map{nullptr};
  Gtid_set excluded{&local_tsid_map};
  if (excluded.add_gtid_text(excluded_gtids_as_string) != RETURN_STATUS_OK)
    return kBinlogIteratorInitErrorUnspecified;

  if (has_purged_needed_gtids_already(excluded))
    return kBinlogIteratorIniErrorPurgedGtids;

  // find files to open, based on the excluded gtid set
  std::list<std::string> files_to_open;
  if (find_files(files_in_index, excluded, files_to_open))
    return kBinlogIteratorInitErrorUnspecified;
  if (files_to_open.empty()) return kBinlogIteratorInitErrorUnspecified;

  // lock first file to open (ie, the oldest one) against purge and
  // open it
  auto oldest_file = files_to_open.front();
  auto *ctx = new Binlog_iterator_ctx(opt_source_verify_checksum, excluded);
  if (ctx == nullptr || !ctx->is_valid() || ctx->pin_log_files(oldest_file) ||
      ctx->get_reader().open(oldest_file.c_str())) {
    delete ctx;
    return kBinlogIteratorInitErrorUnspecified;
  }

  ctx->set_current_file_open(oldest_file);

  // assign context
  auto *iterator_imp = new my_h_binlog_storage_iterator_imp{};
  if (iterator_imp == nullptr) {
    delete ctx;
    return kBinlogIteratorInitErrorUnspecified;
  }
  iterator_imp->m_ctx = ctx;
  *iterator = iterator_imp;

  DBUG_PRINT("debug", ("Iterator initialised started reading from file %s",
                       ctx->get_current_file_open().c_str()));
  return kBinlogIteratorInitOk;
}

DEFINE_BOOL_METHOD(FileStorage::get_storage_details,
                   (my_h_binlog_storage_iterator iterator, char *buffer,
                    uint64_t *size)) {
  DBUG_TRACE;
  // this should never happen, even if the binary log is closed due to binlog
  // error action.
  if (unlikely(!mysql_bin_log.is_open())) return 1;
  auto *it = iterator_cast(iterator);
  if (it == nullptr || it->m_ctx == nullptr) {
    buffer[0] = '\0';
    *size = 0;
    return 1;
  }

  std::stringstream ss;
  ss << "{"
     << " \"filename\" : " << std::quoted(it->m_ctx->get_current_file_open())
     << ", "
     << " \"position\" : " << it->m_ctx->get_reader().position() << " }";

  auto len =
      static_cast<uint64_t>(ss.str().size()) > *size ? *size : ss.str().size();
  *size = len;
  ss.str().copy(buffer, len, 0);
  return 0;
}

DEFINE_METHOD(void, FileStorage::deinit,
              (my_h_binlog_storage_iterator iterator)) {
  DBUG_TRACE;
  auto *it = iterator_cast(iterator);
  if (it == nullptr) return;
  delete it;
}

DEFINE_METHOD(Binlog_iterator_service_get_status, FileStorage::get,
              (my_h_binlog_storage_iterator iterator, unsigned char *buffer,
               uint64_t buffer_capacity, uint64_t *bytes_read)) {
  DBUG_TRACE;
  // this should never happen, even if the binary log is closed due to binlog
  // error action.
  if (unlikely(!mysql_bin_log.is_open())) return kBinlogIteratorGetErrorClosed;
  auto *it = iterator_cast(iterator);
  if (it == nullptr || it->m_ctx == nullptr)
    return kBinlogIteratorGetErrorInvalid;
  auto &ctx = *it->m_ctx;
  auto &reader = ctx.get_reader();
  *bytes_read = 0;

  // update the file cursor (and get the event size, which we can disregard
  // here)
  auto [cursor_update_ret, event_size] = ctx.update_cursor();
  if (cursor_update_ret != kBinlogIteratorGetOk) return cursor_update_ret;

  // read the event
  ctx.set_buffer(buffer);
  ctx.set_buffer_capacity(buffer_capacity);

  unsigned int bytes_read_by_lowlevel_function{0};
  auto saved_position = reader.position();
  // Now we read the next event.
  //
  // Note that the reader has a passthrough allocator, therefore
  // it just reuses the buffer passed as a parameter. If the size
  // of the event to read exceeds the capacity of the buffer, the
  // passthrough allocator returns nullptr once it is asked to
  // allocate more memory and the read operation fails with MEM_ALLOCATE.
  if (reader.read_event_data(&buffer, &bytes_read_by_lowlevel_function)) {
    switch (reader.get_error_type()) {
      case Binlog_read_error::READ_EOF:
        return kBinlogIteratorGetEndOfChanges;
      case Binlog_read_error::MEM_ALLOCATE:
        // to be able to reuse the reader on the next iteration
        reader.seek(saved_position);
        reader.reset_error();
        return kBinlogIteratorGetInsufficientBuffer;
      default:
        reader.seek(saved_position);
        return kBinlogIteratorGetErrorUnspecified;
    }
  }

  // save FORMAT_DESCRIPTION event
  *bytes_read = bytes_read_by_lowlevel_function;
  switch (static_cast<Log_event_type>(buffer[EVENT_TYPE_OFFSET])) {
    case mysql::binlog::event::FORMAT_DESCRIPTION_EVENT: {
      // If we are processing a format description event we save it. This is
      // probably a bit pedantic, since we read binary logs generated by
      // this server. Therefore, instantiating a format description event
      // from this server version would suffice. However, do to upgrades and
      // to the fact that some users may edit the index file and force
      // binary logs into the server from different versions, we play it
      // safe here.
      mysql::binlog::event::Format_description_event fde(
          reinterpret_cast<const char *>(buffer), &ctx.get_fde());
      ctx.set_fde(fde);
      break;
    }
    default:
      break;
  }

  // We're good, return ok
  return kBinlogIteratorGetOk;
}

DEFINE_BOOL_METHOD(FileStorage::get_next_entry_size,
                   (my_h_binlog_storage_iterator iterator, uint64_t *size)) {
  DBUG_TRACE;
  // this should never happen, even if the binary log is closed due to binlog
  // error action.
  if (unlikely(!mysql_bin_log.is_open())) return 1;
  auto *it = iterator_cast(iterator);
  if (it == nullptr || it->m_ctx == nullptr) return 1;

  // update the file cursor
  auto [cursor_update_ret, event_size] = it->m_ctx->update_cursor();
  if (cursor_update_ret != kBinlogIteratorGetOk) return 1;

  // events cannot be larger than 1GB (MAX_MAX_ALLOWED_PACKET)
  // if this limitation is ever lifted, this needs to be removed
  if (event_size >
      static_cast<uint64_t>(mysql::binlog::event::max_log_event_size))
    return 1;
  *size = event_size;

  return 0;
}

bool FileStorage::register_service() {
  DBUG_TRACE;
  SERVICE_TYPE(registry) * r{mysql_plugin_registry_acquire()};

  if (r == nullptr) {
    DBUG_PRINT("info",
               ("Unable to grab reference to the service registry. "
                "Cannot register binary log File Storage Iterator service."));
    return true;
  }

  DBUG_PRINT("info", ("Registering binary log File Storage Iterator service."));
  my_service<SERVICE_TYPE(registry_registration)> reg{"registry_registration",
                                                      r};
  reg->register_service(
      SERVICE_NAME.c_str(),
      pointer_cast<my_h_service>(const_cast<s_mysql_binlog_storage_iterator *>(
          &binlog::services::iterator::imp_server_binlog_storage_iterator)));

  mysql_plugin_registry_release(r);
  return false;
}

bool FileStorage::unregister_service() {
  DBUG_TRACE;
  SERVICE_TYPE(registry) * r{mysql_plugin_registry_acquire()};
  if (r == nullptr) {
    DBUG_PRINT("info",
               ("Unable to grab reference to the service registry. "
                "Cannot unregister binary log File Storage Iterator service."));
    return true;
  }
  DBUG_PRINT("info",
             ("Unregistering binary log File Storage Iterator service."));
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      r);
  reg->unregister(SERVICE_NAME.c_str());
  mysql_plugin_registry_release(r);
  return false;
}

}  // namespace binlog::services::iterator
