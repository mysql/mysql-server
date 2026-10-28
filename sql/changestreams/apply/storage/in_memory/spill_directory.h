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

#ifndef MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_DIRECTORY_H
#define MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_DIRECTORY_H

// =============================================================================
// Directory-level operations for the in-memory relay log's spill store: the
// Spill_file_writer::kTempSubdirName subdirectory of the relay log directory.
// Spill_file_writer owns the files; this module owns the directory.
// =============================================================================

#include <string>

namespace mysql::csa {

/// Joins @p dir and @p leaf with the platform directory separator, avoiding a
/// double separator when @p dir already ends with one.
std::string join_path(const std::string &dir, const std::string &leaf);

/// @brief Full path of the spill-file subdirectory for a channel.
///
/// @param relay_log_dir A relay log directory, with or without a trailing
///        separator.
/// @return @p relay_log_dir joined with Spill_file_writer::kTempSubdirName, or
///         an empty string when @p relay_log_dir is empty.
std::string spill_directory_path(const std::string &relay_log_dir);

/// @brief Create the spill-file subdirectory for a channel if it is absent.
///
/// Called once at startup from Imr_service::init_spill_storage(), so failures
/// surface at setup time, not mid-stream. An existing directory or an empty
/// @p relay_log_dir is success. The path is unusable when it cannot be
/// created, is a symbolic link, is not a directory, or lacks read, write and
/// search permission. That is logged and returned; the caller then keeps
/// in-memory relay log channels from starting, but the server starts.
///
/// @param relay_log_dir The server's relay log directory.
/// @retval false The directory is usable (created now, already present, or
///         nothing to do for an empty @p relay_log_dir).
/// @retval true  The directory is unusable (error logged).
[[nodiscard]] bool ensure_spill_directory(const std::string &relay_log_dir);

/// @brief Delete stale spill files that a previous server run left behind.
///
/// Files left at startup are orphans of a crash or kill. Because the name
/// counter restarts at zero, enough orphans would exhaust the O_EXCL retries
/// of Spill_file_writer::create_unique_file() and disable spilling.
///
/// Only entries starting with Spill_file_writer::kFileNamePrefix are deleted.
/// Subdirectories and the spill directory itself are never touched. A missing
/// directory is nothing to do; a symlink or non-directory path is skipped
/// silently (ensure_spill_directory() rejects it). A failed delete is logged
/// as ER_BINLOG_CANT_DELETE_FILE and the purge goes on. Delete and listing
/// failures are returned, so the caller keeps IMR channels from starting.
///
/// @param relay_log_dir A relay log directory; empty means nothing to do.
/// @retval false Every stale spill file was deleted, or there was none.
/// @retval true  The directory could not be listed, or at least one stale
///         spill file could not be deleted.
[[nodiscard]] bool purge_orphan_spill_files(const std::string &relay_log_dir);

}  // namespace mysql::csa

#endif  // MYSQL_CSA_STORAGE_IN_MEMORY_SPILL_DIRECTORY_H
