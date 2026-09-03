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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_ENCRYPTION_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_ENCRYPTION_H

// =============================================================================
// Master key rotation for in-memory relay log spill files.
//
// Spill files are in no relay-log index, so reencrypt_relay_logs() misses
// them. ALTER INSTANCE ROTATE BINLOG MASTER KEY therefore calls
// imr_reencrypt_spill_files() next, which rewraps the 512-byte v1 header of
// each encrypted imr_sp_* file naming an old key (same file password, current
// key, new IV) before old keys are purged.
//
// Barrier against spill writers: the channel's relay-log LOCK_log. A writer
// copies the key and flushes the header under it; reencrypt_relay_logs() takes
// it for each IMR channel after the new key is current and before this pass.
// The pass touches only bytes [0, 512), which the writer never writes again,
// and opens files without O_CREAT, so a deleted file is never re-created.
// =============================================================================

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace mysql::csa {

/// Classes of the first bytes of an IMR file.
enum class Spill_header_kind {
  /// No encryption magic: a plain file, an empty file or a partial magic.
  unencrypted,
  /// A valid v1 header; it names the key that encrypted the file password.
  encrypted,
  /// The encryption magic followed by a header the server cannot read.
  corrupt
};

/// @brief Classifies the first bytes of an IMR file and reads the key id of
/// an encrypted one. Read-only, no logging.
///
/// Follows Rpl_encryption_header_v1::deserialize(), so a header is encrypted
/// exactly when the server could deserialize it. The key id (which
/// Rpl_encryption_header does not expose) lets the pass skip current files.
///
/// @param buf    The first bytes of the file.
/// @param len    Number of bytes in @p buf; only the first 512 are read.
/// @param[out] key_id The key id the header names, set only for encrypted.
/// @return The class of the header.
Spill_header_kind parse_spill_header(const unsigned char *buf, std::size_t len,
                                     std::string *key_id);

/// Outcome of re-wrapping one encryption header.
enum class Rewrap_status {
  done,            ///< new_header holds 512 bytes naming the current key
  key_not_found,   ///< the key the header names is not in the keyring
  invalid_header,  ///< the server could not deserialize the header
  failed           ///< any other error; message says what
};

struct Rewrap_result {
  Rewrap_status status{Rewrap_status::failed};
  std::string message;  ///< reason, for every status but done
};

/// Re-wraps one v1 header: decrypts the file password with the key that
/// @p old_header (512 bytes) names and writes into @p new_header (512 bytes)
/// a header with the same password, the current master key and a new IV.
using Header_rewrapper = std::function<Rewrap_result(
    const unsigned char *old_header, unsigned char *new_header)>;

/// @brief Re-encrypts the headers of the encrypted imr_sp_* files in
/// @p spill_dirs that name a key other than @p current_key_id.
///
/// Takes no lock and touches no THD (except the debug sync hook
/// imr_spill_reencrypt_after_list). Touches only bytes [0, 512), opens without
/// O_CREAT, does not fsync. Skips with a warning a corrupt header or a key not
/// in the keyring (e.g. another server's key).
///
/// @retval false Every file was re-encrypted or skipped.
/// @retval true  A file or directory could not be handled; @p error names it.
bool reencrypt_spill_files_in_dirs(const std::vector<std::string> &spill_dirs,
                                   const std::string &current_key_id,
                                   const Header_rewrapper &rewrap,
                                   std::string *error);

/// @brief The spill directories of the channels that have a live in-memory
/// relay log runtime, sorted, without duplicates or empty paths. Takes
/// channel_map's read lock while it reads them.
std::vector<std::string> spill_dirs_of_live_runtimes();

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_ENCRYPTION_H
