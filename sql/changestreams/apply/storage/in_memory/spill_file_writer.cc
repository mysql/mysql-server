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

#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"

#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

#include "my_dbug.h"  // DBUG_EXECUTE_IF
#include "my_io.h"    // File, MYSYS_STRERROR_SIZE
#include "my_sys.h"   // my_create, my_close, my_delete, my_errno, my_strerror
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"                       // WARNING_LEVEL
#include "mysql/strings/m_ctype.h"                   // my_charset_filename
#include "mysql_com.h"                               // NAME_LEN
#include "mysqld_error.h"                            // ER_LOG_PRINTF_MSG
#include "sql/binlog_ostream.h"  // Binlog_encryption_ostream, rpl_encryption
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // join_path, spill_directory_path
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"  // Trx_envelope_queue (spill disk accounting)
#include "sql/current_thd.h"  // current_thd
#include "sql/debug_sync.h"   // debug_sync_set_action
#include "sql/log_event.h"  // Format_description_log_event, BINLOG_MAGIC, BIN_LOG_HEADER_SIZE
#include "sql/mysqld_cs.h"    // system_charset_info
#include "sql/strfunc.h"      // strconvert
#include "string_with_len.h"  // STRING_WITH_LEN

namespace mysql::csa {

namespace {

std::atomic<std::uint64_t> g_spill_seq{0};

}  // namespace

Spill_file_writer::Spill_file_writer(Fde_ptr fde, std::string relay_log_dir,
                                     std::string channel_name,
                                     Trx_envelope_queue *owner_queue,
                                     std::size_t reserved_disk_bytes)
    : m_fde(std::move(fde)),
      m_relay_log_dir(std::move(relay_log_dir)),
      m_channel_name(std::move(channel_name)),
      m_owner_queue(owner_queue),
      m_reserved_disk_bytes(reserved_disk_bytes) {}

Spill_file_writer::~Spill_file_writer() {
  // Release the gauges before the delete attempt; an undeletable file no longer
  // counts. m_is_open is set exactly when open() added them, and m_end_pos plus
  // the header is the running total of every add, so this balances to zero.
  if (m_is_open && m_owner_queue != nullptr) {
    m_owner_queue->release_spill_bytes(m_end_pos + m_physical_overhead);
    m_owner_queue->dec_spill_files();
  }
  // Close the stream, if close_stream() has not (the encryption stream closes
  // the IO_CACHE it owns), then delete the temp file.
  m_pipeline_head.reset();
  // Record a failed delete on the queue; the applier reports it.
  if (!m_file_name.empty() && delete_file() && m_owner_queue != nullptr)
    m_owner_queue->set_spill_delete_failed();
  // Release the reservation only after the delete attempt, so a waiting
  // receiver cannot start the next spill file while this one is on disk. Done
  // even if open() failed: the reservation predates the file.
  if (m_owner_queue != nullptr && m_reserved_disk_bytes != 0)
    m_owner_queue->release_spill_reservation(m_reserved_disk_bytes);
}

bool Spill_file_writer::create_unique_file() {
  std::string channel_segment;
  if (!m_channel_name.empty()) {
    // Encode the channel as relay log file names do.
    char channel_to_file[FN_REFLEN];
    uint errors;
    strconvert(system_charset_info, m_channel_name.c_str(),
               &my_charset_filename, channel_to_file, NAME_LEN, &errors);
    channel_segment.assign(channel_to_file).append("_");
  }
  constexpr int kMaxAttempts = 1024;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const std::uint64_t id = g_spill_seq.fetch_add(1);
    char id_buf[32];
    // Lowercase hex id.
    std::snprintf(id_buf, sizeof(id_buf), "%llx",
                  static_cast<unsigned long long>(id));
    // "imr_sp_[<channel_name>_]<unique_id>" (see kFileNamePrefix).
    std::string leaf(kFileNamePrefix);
    leaf += channel_segment;
    leaf += id_buf;
    const std::string candidate = join_path(m_temp_dir, leaf);

    const File fd =
        my_create(candidate.c_str(), 0, O_CREAT | O_EXCL | O_WRONLY, MYF(0));
    if (fd >= 0) {
      // Reserved the name exclusively; IO_CACHE_ostream reopens it by name.
      my_close(fd, MYF(0));
      m_file_name = candidate;
      return false;
    }
    if (my_errno() != EEXIST) {
      m_error.assign("Spill_file_writer: could not create spill file");
      return true;
    }
  }
  m_error.assign("Spill_file_writer: exhausted unique spill file names");
  return true;
}

bool Spill_file_writer::open() {
  assert(!m_is_open);
  assert(m_fde != nullptr);

  // Debug hook: fail provisioning before any file is created, so the IO thread
  // stops with Last_IO_Error.
  DBUG_EXECUTE_IF("imr_force_spill_open_failure", {
    m_error.assign("Spill_file_writer: forced spill file open failure (debug)");
    return true;
  });

  if (m_relay_log_dir.empty()) {
    m_error.assign("Spill_file_writer: empty relay log directory");
    return true;
  }

  // Created once at server startup by Imr_service::init_spill_storage().
  m_temp_dir = spill_directory_path(m_relay_log_dir);

  // Create a uniquely named "imr_sp_[<channel>_]<unique_id>" file there.
  if (create_unique_file()) {
    return true;
  }

  auto file_ostream = std::make_unique<IO_CACHE_ostream>();
  // Wait for disk space on a full disk, with the relay log's flags.
  if (file_ostream->open(
#ifdef HAVE_PSI_INTERFACE
          PSI_NOT_INSTRUMENTED,
#endif
          m_file_name.c_str(),
          MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL |
              MY_REPORT_WAITING_IF_FULL))) {
    m_error.assign(
        "Spill_file_writer: could not open IO_CACHE over spill file");
    delete_file();
    return true;
  }
  m_pipeline_head = std::move(file_ostream);

  // The file keeps this encryption state for its whole life. is_enabled() and
  // the master key are read only here, never cached across files.
  if (rpl_encryption.is_enabled() && open_encryption_stream()) return true;

  // Relay-log prefix: the 4-byte magic.
  if (m_pipeline_head->write(
          reinterpret_cast<const unsigned char *>(BINLOG_MAGIC),
          BIN_LOG_HEADER_SIZE)) {
    m_error.assign("Spill_file_writer: failed to write BINLOG_MAGIC");
    return true;
  }
  m_end_pos = BIN_LOG_HEADER_SIZE;

  // Relay-log prefix: the serialized FDE.
  // Serialize a PRIVATE copy.
  Format_description_log_event fde_copy;
  static_cast<mysql::binlog::event::Format_description_event &>(fde_copy) =
      static_cast<const mysql::binlog::event::Format_description_event &>(
          *m_fde);

  // Mark it a relay-log FDE and PRESERVE the checksum algorithm the copy
  // carries.
  fde_copy.set_relay_log_event();
  if (fde_copy.common_footer->checksum_alg ==
      mysql::binlog::event::BINLOG_CHECKSUM_ALG_UNDEF) {
    fde_copy.common_footer->checksum_alg =
        mysql::binlog::event::BINLOG_CHECKSUM_ALG_OFF;
  }
  fde_copy.common_header->log_pos = m_end_pos;
  if (fde_copy.write(m_pipeline_head.get())) {
    m_error.assign("Spill_file_writer: failed to write FDE");
    return true;
  }
  m_end_pos += fde_copy.common_header->data_written;

  m_is_open = true;
  // Register the file in the spill gauges; the destructor cancels this. The
  // byte gauge counts bytes on disk, including the encryption header.
  if (m_owner_queue != nullptr) {
    m_owner_queue->inc_spill_files();
    m_owner_queue->add_spill_bytes(m_end_pos + m_physical_overhead);
  }
  return false;
}

bool Spill_file_writer::open_encryption_stream() {
  // Runs under the channel's relay-log LOCK_log, which key rotation also takes
  // before rewrapping spill headers (see spill_encryption.h), so the rewrap
  // sees this header or this writer copies the new key. After this flush the
  // writer never writes below offset 512.
  auto encrypted = std::make_unique<Binlog_encryption_ostream>();
  const char *failure = nullptr;
  if (encrypted->open(std::move(m_pipeline_head))) {
    failure =
        "Spill_file_writer: could not write the spill file encryption header";
  } else {
    // Test hook for the LOCK_log barrier: park with the key copied and the
    // header still in the IO_CACHE buffer, so the file is still 0 bytes.
    DBUG_EXECUTE_IF("imr_spill_pause_after_key_taken", {
      const char act[] = "now SIGNAL imr_key_taken WAIT_FOR imr_key_go";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });
    if (encrypted->flush())
      failure =
          "Spill_file_writer: could not flush the spill file encryption header";
  }
  if (failure != nullptr) {
    m_error.assign(failure);
    // Close the file (if Binlog_encryption_ostream::open() has not), then
    // delete it while LOCK_log is still held.
    encrypted.reset();
    delete_file();
    return true;
  }
  m_physical_overhead = static_cast<my_off_t>(encrypted->get_header_size());
  m_pipeline_head = std::move(encrypted);
  return false;
}

bool Spill_file_writer::delete_file() {
  const bool failed = my_delete(m_file_name.c_str(), MYF(0)) != 0;
  if (failed) {
    char errbuf[MYSYS_STRERROR_SIZE];
    const int err = my_errno();
    std::string msg("[In-memory relay log] could not delete spill file '");
    msg += m_file_name;
    msg += "' (errno " + std::to_string(err) + " - " +
           my_strerror(errbuf, sizeof(errbuf), err) +
           "); it is left for the purge at the next server start";
    LogErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, msg.c_str());
  }
  m_file_name.clear();
  return failed;
}

void Spill_file_writer::discard_after_setup_failure() {
  m_pipeline_head.reset();
  // The result is ignored: a setup failure only logs the warning.
  if (!m_file_name.empty()) delete_file();
  // Release the gauges now; clearing m_is_open stops the dtor doing it again.
  if (m_is_open && m_owner_queue != nullptr) {
    m_owner_queue->release_spill_bytes(m_end_pos + m_physical_overhead);
    m_owner_queue->dec_spill_files();
  }
  m_is_open = false;
  // Return the reservation now; zeroing it stops the dtor doing it again.
  if (m_owner_queue != nullptr && m_reserved_disk_bytes != 0)
    m_owner_queue->release_spill_reservation(m_reserved_disk_bytes);
  m_reserved_disk_bytes = 0;
}

bool Spill_file_writer::append_raw(const char *buf, std::size_t len) {
  assert(m_is_open);
  if (!m_is_open) {
    m_error.assign("Spill_file_writer: append_raw before open");
    return true;
  }

  // Debug hook: fail a mid-transaction write, so the receiver truncates the
  // group and the IO thread stops with Last_IO_Error.
  DBUG_EXECUTE_IF("imr_force_spill_append_failure", {
    m_error.assign("Spill_file_writer: forced spill append failure (debug)");
    return true;
  });

  if (len == 0) return false;
  if (m_pipeline_head->write(reinterpret_cast<const unsigned char *>(buf),
                             len)) {
    m_error.assign("Spill_file_writer: failed to append event bytes");
    return true;
  }
  m_end_pos += len;
  // The file grew by len on disk: reflect it in the channel's live byte gauge.
  if (m_owner_queue != nullptr) m_owner_queue->add_spill_bytes(len);
  return false;
}

bool Spill_file_writer::flush() {
  assert(m_is_open);
  if (!m_is_open) {
    m_error.assign("Spill_file_writer: flush before open");
    return true;
  }
  if (m_pipeline_head->flush()) {
    m_error.assign("Spill_file_writer: failed to flush spill file");
    return true;
  }
  return false;
}

void Spill_file_writer::close_stream() {
  // After the terminal append everything is flushed, so this only frees the
  // descriptor, buffer and encryptor. After a write failure end_io_cache()
  // retries the buffered bytes once, as the destructor would.
  m_pipeline_head.reset();
}

}  // namespace mysql::csa
