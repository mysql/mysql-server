// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/storage/relay_log/log_prefetcher.h"
#include "mysql/psi/mysql_file.h"  // mysql_file_close
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/mysqld.h"  // slave_trans_retries
#include "sql/psi_memory_resource.h"
#include "sql/rpl_mi.h"  // Master_info

using namespace mysql::binlog::event;

namespace mysql::csa {

using Vector_type = Data_source::Data_type;

Log_prefetcher::Log_prefetcher(MYSQL_BIN_LOG *log_ptr, Mt_key mt_key_wait,
                               Cv_key cv_key_wait, Mt_key mt_key_file_move,
                               Cv_key cv_key_file_move,
                               Th_key key_th_prefetcher, Mem_key memory_key)
    : m_log(log_ptr),
      m_cache(psi_memory_resource(memory_key)),
      m_mt_prefetcher(MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(mt_key_wait)),
      m_cv_prefetcher(MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(cv_key_wait)),
      m_mt_move_file(MYSQL_CONCURRENCY_DEFINE_MT_PSI_KEY(mt_key_file_move)),
      m_cv_move_file(MYSQL_CONCURRENCY_DEFINE_CV_PSI_KEY(cv_key_file_move)),
      m_key_th_prefetcher(key_th_prefetcher),
      m_allocator(psi_memory_resource(memory_key)) {}

Log_prefetcher::~Log_prefetcher() {}

void Log_prefetcher::start_prefetcher() {
  m_prefetcher = MDEF_CREATE_THREAD(m_key_th_prefetcher,
                                    &Log_prefetcher::run_prefetch_thread, this);
}

bool Log_prefetcher::open_file(bool next_file) {
  m_current_offset = 0;
  if (m_istream.is_open()) {
    m_istream.close();
  }
  std::string log_name = get_current_file();
  m_log->lock_index();
  Log_info m_linfo{};
  if (m_log->find_log_pos(&m_linfo, log_name.c_str(), false)) {
    set_error("Could not find relay log file in the index.");
    m_log->unlock_index();
    return false;
  }
  // ensure path is relative to run directory
  log_name.assign(m_linfo.log_file_name);

  if (next_file) {
    if (!m_log->is_open() || m_log->find_next_log(&m_linfo, false)) {
      set_error("Cannot find next log.");
      m_log->unlock_index();
      return false;
    }
    log_name.assign(m_linfo.log_file_name);
  }
  m_log->unlock_index();

  auto active = m_log->is_active(log_name.c_str());
  update_current_file(log_name.c_str(), active);
  if (!active) {
    m_istream.open(log_name, std::ios::binary | std::ios::ate);
    if (!m_istream.is_open()) {
      set_error("Got an error while opening the file.");
      return false;
    }
    m_current_file_length = m_istream.tellg();
    m_istream.seekg(0, m_istream.beg);
    return true;
  }
  return !active;
}

bool Log_prefetcher::open(const char *file_name) {
  update_current_file(file_name, true);
  bool is_ok = open_file(false);
  if (is_ok) {
    m_cv_prefetcher.notify_one();
  }
  return is_ok;
}

void Log_prefetcher::run_prefetch_thread() {
  while (!is_stopped()) {
    // prefetch data
    auto stop_waiting = [this]() -> bool {
      return is_stopped() || !m_log_active;
    };

    Vector_type data_read(m_batch_size, m_allocator);
    std::streamsize act_bytes_read{0};
    std::string current_log_name{""};
    {
      std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_prefetcher);
      m_cv_prefetcher.wait(lock, stop_waiting);
      if (is_stopped()) {
        m_end = true;
        m_end.notify_one();
        return;
      }
      /// get file name after wait (updated by open)
      current_log_name = m_current_log_name;
    }
    // prefetch max m_batch_size bytes
    act_bytes_read = m_istream.readsome(
        reinterpret_cast<char *>(data_read.data()), m_batch_size);

    if (act_bytes_read < 0) {
      set_error("Error while reading from the stream");
      return;
    }
    bool is_eof = m_current_offset + act_bytes_read == m_current_file_length;

    if (act_bytes_read > 0) {
      data_read.resize(act_bytes_read);
      m_bytes_fetched += static_cast<std::size_t>(act_bytes_read);
      Elem_type data_slot(new Data_type(std::move(data_read), current_log_name,
                                        m_current_offset, m_current_file_length,
                                        is_eof));

      if (is_eof) {
        std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_move_file);
        m_move_file = current_log_name;
      }

      m_cache.enqueue(std::move(data_slot));
      m_current_offset += act_bytes_read;
    }
    if (is_eof) {
      open_file(true);
      {
        std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_move_file);
        m_move_file = "";
      }
      m_cv_move_file.notify_one();
    }
  }
}

void Log_prefetcher::stop() {
  m_stopped = true;
  m_cv_prefetcher.notify_one();
  m_end.wait(false);
  m_prefetcher.join();
  return;
}

void Log_prefetcher::ensure_file_done(const std::string &file_name) {
  std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_move_file);
  auto stop_waiting = [ this, &file_name ]() -> auto{
    return m_move_file != file_name;
  };
  m_cv_move_file.wait(lock, stop_waiting);
}

bool Log_prefetcher::is_error() const { return m_is_error; }

std::string Log_prefetcher::get_current_file() const {
  std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_prefetcher);
  return m_current_log_name;
}

void Log_prefetcher::update_current_file(const char *file_name, bool active) {
  std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_prefetcher);
  m_current_log_name.assign(file_name);
  m_log_active = active;
}

bool Log_prefetcher::is_waiting_for_next_file(const std::string &prev_file,
                                              const std::string &next_file) {
  // if prefetcher moves from prev_file, wait for finishing the move;
  ensure_file_done(prev_file);
  std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_prefetcher);
  if (m_log_active) {
    return m_current_log_name == next_file;
  }
  return false;
}

void Log_prefetcher::set_error(const char *msg) {
  std::unique_lock<mysql::concurrency::Mutex> lock(m_mt_prefetcher);
  m_is_error = true;
  m_stopped = true;
  m_end = true;
  m_error_message.assign(msg);
}

bool Log_prefetcher::is_stopped() const { return m_stopped; }

}  // namespace mysql::csa
