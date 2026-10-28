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

// Spill file header rewrap on master key rotation. See spill_encryption.h.

#include "sql/changestreams/apply/storage/in_memory/spill_encryption.h"

#include <algorithm>  // sort, unique
#include <cassert>
#include <cerrno>
#include <cstring>  // memcmp, memcpy, strncmp, strlen
#include <memory>

#include "my_dbug.h"  // DBUG_EXECUTE_IF, DBUG_EVALUATE_IF
#include "my_dir.h"   // my_dir, my_dirend, MY_DONT_SORT
#include "my_io.h"    // File
#include "my_sys.h"   // my_open, my_pread, my_pwrite, my_close, my_error
#include "my_thread_local.h"                         // my_errno, set_my_errno
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"                       // WARNING_LEVEL
#include "mysqld_error.h"  // ER_LOG_PRINTF_MSG, ER_RPL_ENCRYPTION_KEY_NOT_FOUND
#include "sql/basic_istream.h"  // Basic_istream
#include "sql/basic_ostream.h"  // StringBuffer_ostream
#include "sql/changestreams/apply/storage/in_memory/imr_runtime.h"  // Imr_runtime
#include "sql/changestreams/apply/storage/in_memory/spill_directory.h"  // join_path, spill_directory_path
#include "sql/changestreams/apply/storage/in_memory/spill_file_writer.h"  // Spill_file_writer::kFileNamePrefix
#include "sql/current_thd.h"         // current_thd
#include "sql/debug_sync.h"          // debug_sync_set_action
#include "sql/error_handler.h"       // Internal_error_handler
#include "sql/rpl_log_encryption.h"  // Rpl_encryption_header, rpl_encryption
#include "sql/rpl_mi.h"              // Master_info
#include "sql/rpl_msr.h"      // channel_map, mi_map, GROUP_REPLICATION_CHANNEL
#include "sql/rpl_replica.h"  // imr_reencrypt_spill_files
#include "sql/rpl_rli.h"      // Relay_log_info
#include "sql/sql_class.h"    // THD
#include "sql/sql_error.h"    // Sql_condition
#include "string_with_len.h"  // STRING_WITH_LEN

namespace mysql::csa {

namespace {

/// Size of the v1 encryption header, the only bytes the pass reads or writes.
constexpr std::size_t kHeaderSize = Rpl_encryption_header_v1::HEADER_SIZE;
constexpr std::size_t kMagicSize = Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE;
constexpr std::size_t kPasswordSize =
    Rpl_encryption_header_v1::PASSWORD_FIELD_SIZE;
constexpr std::size_t kIvSize = Rpl_encryption_header_v1::IV_FIELD_SIZE;

/// Field types of the v1 header, as Rpl_encryption_header_v1::Field_type.
constexpr unsigned char kFieldEnd = 0;
constexpr unsigned char kFieldKeyId = 1;
constexpr unsigned char kFieldPassword = 2;
constexpr unsigned char kFieldIv = 3;

void log_warning(const std::string &msg) {
  LogErr(WARNING_LEVEL, ER_LOG_PRINTF_MSG, msg.c_str());
}

bool starts_with(const char *name, const char *prefix) {
  return std::strncmp(name, prefix, std::strlen(prefix)) == 0;
}

void log_invalid_header(const std::string &path) {
  log_warning("[In-memory relay log] spill file '" + path +
              "' has an invalid encryption header; it is left unchanged");
}

/// " (errno <n>)" for the failure messages.
std::string errno_suffix() {
  return " (errno " + std::to_string(my_errno()) + ")";
}

/// Re-encrypts the header of one file.
/// @retval false The file was re-encrypted, skipped or has vanished.
/// @retval true  The file could not be handled; @p reason says why.
bool reencrypt_file(const std::string &path, const std::string &current_key_id,
                    const Header_rewrapper &rewrap, std::string *reason) {
  // No O_CREAT: a file deleted since my_dir() listed it stays deleted. No
  // MY_WME: the pass runs on the ALTER INSTANCE session, whose diagnostics
  // area must stay clean.
  const File fd = my_open(path.c_str(), O_RDWR, MYF(0));
  if (fd < 0) {
    // ENOENT: deleted since the listing. EISDIR: a directory with a spill
    // prefix, which the startup purge skips too.
    if (my_errno() == ENOENT || my_errno() == EISDIR) return false;
    *reason = "could not open the file" + errno_suffix();
    return true;
  }

  bool failed = false;
  unsigned char buf[kHeaderSize];
  const size_t len = my_pread(fd, buf, sizeof(buf), 0, MYF(0));
  if (len == MY_FILE_ERROR) {
    *reason = "could not read the header" + errno_suffix();
    failed = true;
  }

  std::string key_id;
  const Spill_header_kind kind = failed ? Spill_header_kind::unencrypted
                                        : parse_spill_header(buf, len, &key_id);
  bool write_header = false;
  unsigned char new_header[kHeaderSize];
  switch (kind) {
    case Spill_header_kind::unencrypted:
      break;
    case Spill_header_kind::corrupt:
      // No key can decrypt this file, so a rewrap could not help it.
      log_invalid_header(path);
      break;
    case Spill_header_kind::encrypted: {
      if (key_id == current_key_id) break;
      const Rewrap_result result = rewrap(buf, new_header);
      switch (result.status) {
        case Rewrap_status::done:
          write_header = true;
          break;
        case Rewrap_status::key_not_found:
          log_warning("[In-memory relay log] spill file '" + path +
                      "' names the master key '" + key_id +
                      "', which is not in the keyring; it is left unchanged "
                      "and cannot be read");
          break;
        case Rewrap_status::invalid_header:
          log_invalid_header(path);
          break;
        case Rewrap_status::failed:
          *reason = result.message;
          failed = true;
          break;
      }
      break;
    }
  }

  if (write_header) {
    // The test point skips the write, so the file keeps its old header.
    const size_t written = DBUG_EVALUATE_IF(
        "imr_spill_reencrypt_fail_write", MY_FILE_ERROR,
        my_pwrite(fd, new_header, kHeaderSize, 0, MYF(MY_NABP)));
    DBUG_EXECUTE_IF("imr_spill_reencrypt_fail_write", set_my_errno(EIO););
    if (written != 0) {
      *reason = "could not write the new header" + errno_suffix();
      failed = true;
    }
  }
  // As Binlog_encryption_ostream::reencrypt(), the close result is ignored.
  my_close(fd, MYF(0));
  return failed;
}

/// A Basic_istream over a byte buffer, for Rpl_encryption_header::get_header().
class Bytes_istream : public Basic_istream {
 public:
  Bytes_istream(const unsigned char *buf, std::size_t len)
      : m_buf(buf), m_len(len) {}
  ssize_t read(unsigned char *buffer, size_t length) override {
    const std::size_t n = std::min(length, m_len - m_pos);
    std::memcpy(buffer, m_buf + m_pos, n);
    m_pos += n;
    return static_cast<ssize_t>(n);
  }

 private:
  const unsigned char *m_buf;
  std::size_t m_len;
  std::size_t m_pos{0};
};

/// Records the first error raised on the THD and swallows every condition, so
/// a file the pass skips leaves the ALTER INSTANCE session clean.
class Rewrap_error_handler : public Internal_error_handler {
 public:
  bool handle_condition(THD *, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *msg) override {
    if (*level == Sql_condition::SL_ERROR && m_errno == 0) {
      m_errno = sql_errno;
      m_message.assign(msg != nullptr ? msg : "");
    }
    return true;
  }
  uint m_errno{0};
  std::string m_message;
};

/// Pushes @p handler on @p thd for the scope.
class Error_handler_guard {
 public:
  Error_handler_guard(THD *thd, Internal_error_handler *handler) : m_thd(thd) {
    m_thd->push_internal_handler(handler);
  }
  ~Error_handler_guard() { m_thd->pop_internal_handler(); }
  Error_handler_guard(const Error_handler_guard &) = delete;
  Error_handler_guard &operator=(const Error_handler_guard &) = delete;

 private:
  THD *m_thd;
};

/// The server's rewrap of one header, as Binlog_encryption_ostream::reencrypt()
/// does for a relay log: same file password, current master key, new IV.
Rewrap_result rewrap_with_server_keys(THD *thd, const unsigned char *old_header,
                                      unsigned char *new_header) {
  Rewrap_error_handler handler;
  const Error_handler_guard guard(thd, &handler);

  // get_header() is called after the magic has been read.
  Bytes_istream in(old_header + kMagicSize, kHeaderSize - kMagicSize);
  const std::unique_ptr<Rpl_encryption_header> old_hdr =
      Rpl_encryption_header::get_header(&in);
  if (old_hdr == nullptr)
    return {Rewrap_status::invalid_header, handler.m_message};

  const Key_string password = old_hdr->decrypt_file_password();
  if (password.empty()) {
    if (handler.m_errno == ER_RPL_ENCRYPTION_KEY_NOT_FOUND)
      return {Rewrap_status::key_not_found, handler.m_message};
    std::string message = "could not decrypt the file password";
    if (!handler.m_message.empty()) message += ": " + handler.m_message;
    return {Rewrap_status::failed, message};
  }

  const std::unique_ptr<Rpl_encryption_header> new_hdr =
      Rpl_encryption_header::get_new_default_header();
  if (new_hdr->encrypt_file_password(password))
    return {Rewrap_status::failed,
            "could not encrypt the file password with the current master key"};
  StringBuffer_ostream<kHeaderSize> out;
  if (new_hdr->serialize(&out) || out.length() != kHeaderSize)
    return {Rewrap_status::failed, "could not serialize the new header"};
  std::memcpy(new_header, out.ptr(), kHeaderSize);
  return {Rewrap_status::done, {}};
}

Header_rewrapper server_header_rewrapper(THD *thd) {
  return [thd](const unsigned char *old_header, unsigned char *new_header) {
    return rewrap_with_server_keys(thd, old_header, new_header);
  };
}

}  // namespace

Spill_header_kind parse_spill_header(const unsigned char *buf, std::size_t len,
                                     std::string *key_id) {
  constexpr std::size_t magic_size =
      Rpl_encryption_header::ENCRYPTION_MAGIC_SIZE;

  // 1-2. An empty file, a partial BINLOG_MAGIC or a plain file.
  if (len < magic_size) return Spill_header_kind::unencrypted;
  if (std::memcmp(buf, Rpl_encryption_header::ENCRYPTION_MAGIC, magic_size) !=
      0)
    return Spill_header_kind::unencrypted;

  // 3-4. The whole v1 header must be there.
  if (len < kHeaderSize) return Spill_header_kind::corrupt;
  if (buf[magic_size] != 1) return Spill_header_kind::corrupt;

  // 5. The field walk of Rpl_encryption_header_v1::deserialize(): every field
  // must end inside the header, and reading a type byte at offset 512 is an
  // error there too.
  std::size_t pos = magic_size + 1;
  std::string found_key_id;
  bool has_password = false;
  bool has_iv = false;
  for (;;) {
    if (pos >= kHeaderSize) return Spill_header_kind::corrupt;
    const unsigned char type = buf[pos++];
    if (type == kFieldEnd) break;
    switch (type) {
      case kFieldKeyId: {
        if (pos >= kHeaderSize) return Spill_header_kind::corrupt;
        const std::size_t length = buf[pos++];
        if (length > kHeaderSize - pos) return Spill_header_kind::corrupt;
        // A repeated key id replaces the earlier one, as m_key_id.assign().
        found_key_id.assign(reinterpret_cast<const char *>(buf + pos), length);
        pos += length;
        break;
      }
      case kFieldPassword:
        if (kPasswordSize > kHeaderSize - pos)
          return Spill_header_kind::corrupt;
        pos += kPasswordSize;
        has_password = true;
        break;
      case kFieldIv:
        if (kIvSize > kHeaderSize - pos) return Spill_header_kind::corrupt;
        pos += kIvSize;
        has_iv = true;
        break;
      default:
        return Spill_header_kind::corrupt;
    }
  }
  if (found_key_id.empty() || !has_password || !has_iv)
    return Spill_header_kind::corrupt;

  *key_id = found_key_id;
  return Spill_header_kind::encrypted;
}

bool reencrypt_spill_files_in_dirs(const std::vector<std::string> &spill_dirs,
                                   const std::string &current_key_id,
                                   const Header_rewrapper &rewrap,
                                   std::string *error) {
  for (const std::string &dir : spill_dirs) {
    // No MY_WANT_STAT: it drops entries without the owner read bit, which must
    // fail the rotation, not be skipped. No MY_WME (see reencrypt_file()).
    MY_DIR *dirp = my_dir(dir.c_str(), MYF(MY_DONT_SORT));
    if (dirp == nullptr) {
      if (my_errno() == ENOENT) continue;
      *error = "Failed to read the in-memory relay log spill directory '" +
               dir + "'";
      return true;
    }
    // Test hook: park after the listing, holding only
    // LOCK_rotate_binlog_master_key, so a test can delete a listed file.
    DBUG_EXECUTE_IF("imr_spill_reencrypt_after_list", {
      const char act[] =
          "now SIGNAL imr_reencrypt_listed WAIT_FOR imr_reencrypt_go";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });
    bool failed = false;
    for (uint i = 0; i < dirp->number_off_files && !failed; ++i) {
      const fileinfo &entry = dirp->dir_entry[i];
      if (entry.name == nullptr ||
          !starts_with(entry.name, Spill_file_writer::kFileNamePrefix))
        continue;
      const std::string path = join_path(dir, entry.name);
      std::string reason;
      failed = reencrypt_file(path, current_key_id, rewrap, &reason);
      if (failed)
        *error = "Failed to re-encrypt in-memory relay log file '" + path +
                 "': " + reason;
    }
    my_dirend(dirp);
    if (failed) return true;
  }
  return false;
}

std::vector<std::string> spill_dirs_of_live_runtimes() {
  std::vector<std::string> dirs;
  // Runtimes change only under the channel_map write lock and relay_log_dir()
  // is fixed at construction, so the read lock suffices. No file I/O under it.
  channel_map.rdlock();
  const enum_channel_type channel_types[] = {SLAVE_REPLICATION_CHANNEL,
                                             GROUP_REPLICATION_CHANNEL};
  for (auto channel_type : channel_types) {
    for (mi_map::iterator it = channel_map.begin(channel_type);
         it != channel_map.end(channel_type); it++) {
      Master_info *mi = it->second;
      if (mi == nullptr || mi->rli == nullptr || mi->rli->imr() == nullptr)
        continue;
      dirs.push_back(
          spill_directory_path(mi->rli->imr()->queue()->relay_log_dir()));
    }
  }
  channel_map.unlock();

  dirs.erase(std::remove(dirs.begin(), dirs.end(), std::string()), dirs.end());
  std::sort(dirs.begin(), dirs.end());
  dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());
  return dirs;
}

}  // namespace mysql::csa

bool imr_reencrypt_spill_files() {
  THD *thd = current_thd;
  // Reached only from the ROTATE_LOGS step of rotate_master_key(), which runs
  // only when m_master_key_recovered && current_thd.
  assert(thd != nullptr);
  const std::vector<std::string> dirs =
      mysql::csa::spill_dirs_of_live_runtimes();
  if (dirs.empty()) return false;  // no IMR runtime: touch no file
  std::string error;
  if (!mysql::csa::reencrypt_spill_files_in_dirs(
          dirs, rpl_encryption.get_master_key().m_id,
          mysql::csa::server_header_rewrapper(thd), &error))
    return false;
  // As MYSQL_BIN_LOG::reencrypt_logs(): one error for the statement.
  if (thd->is_error()) thd->clear_error();
  my_error(ER_BINLOG_MASTER_KEY_ROTATION_FAIL_TO_REENCRYPT_LOG, MYF(0),
           error.c_str());
  return true;
}
