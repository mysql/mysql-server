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

#ifndef BINLOG_FILE_ROTATION_H_INCLUDED
#define BINLOG_FILE_ROTATION_H_INCLUDED

/**
  @file

  The steps of a binlog rotation that differ between a standard rotation and
  a rotation that promotes a large transaction's spilled binlog cache file.
*/

#include "my_inttypes.h"                  // my_off_t
#include "sql/binlog/large_trx_commit.h"  // Large_trx_fallback_reason

class MYSQL_BIN_LOG;
class THD;
class binlog_cache_data;

/**
  The interface for the steps of a binlog rotation that differ between a
  standard rotation (see Standard_file_rotation) and the rotation that
  promotes a large transaction's spilled binlog cache file (see
  Spilled_file_rotation). The steps that differ are:
  - saving the GTIDs of the closed binlog file into mysql.gtid_executed;
  - replacing the new binlog file with the spilled file;
  - publishing the new binlog file's end position.
  MYSQL_BIN_LOG::new_file_impl() and MYSQL_BIN_LOG::open_binlog() call these
  steps. The rest of the rotation is the same for both.
*/
class File_rotation {
 public:
  virtual ~File_rotation() = default;

  /**
    Saves the GTIDs of the closed binlog file into mysql.gtid_executed.

    @return 0 on success, or the error of
            Gtid_state::save_gtids_of_last_binlog_into_table().
  */
  virtual int save_gtids_of_last_binlog() = 0;

  /**
    Called after the new binlog file's header events are written, and before
    the file is added to the binlog index. A rotation that promotes a spilled
    file replaces the new file with the spilled file (see
    MYSQL_BIN_LOG::promote_spilled_file()).

    @param log  The binlog that is rotated.

    @retval false  Success.
    @retval true   Error.
  */
  virtual bool replace_new_file_with_spilled_file(MYSQL_BIN_LOG &log) = 0;

  /**
    Publishes the new binlog file's initial end position.

    @param log  The binlog that is rotated.
  */
  virtual void publish_end_position(MYSQL_BIN_LOG &log) = 0;
};

/**
  The steps of a standard binlog rotation. A standard rotation does not
  replace the new binlog file with a spilled file, so
  replace_new_file_with_spilled_file() does nothing.
*/
class Standard_file_rotation : public File_rotation {
 public:
  int save_gtids_of_last_binlog() override;
  bool replace_new_file_with_spilled_file(MYSQL_BIN_LOG &) override {
    return false;
  }
  void publish_end_position(MYSQL_BIN_LOG &log) override;
};

/**
  The steps of the rotation that promotes a large transaction's spilled
  binlog cache file (see MYSQL_BIN_LOG::promote_spilled_file()), and
  information about the promotion.
*/
class Spilled_file_rotation : public File_rotation {
 public:
  /**
    Does not save the GTIDs, and requests that the commit saves them after
    the group commits, because the large transaction and the other members of
    its group can hold locks on mysql.gtid_executed until then (see
    save_gtids_after_large_trx_rotation()).

    @return 0.
  */
  int save_gtids_of_last_binlog() override;
  /** Promotes the spilled file (see MYSQL_BIN_LOG::promote_spilled_file()). */
  bool replace_new_file_with_spilled_file(MYSQL_BIN_LOG &log) override;
  /** Publishes only the header events of a promoted file. */
  void publish_end_position(MYSQL_BIN_LOG &log) override;

  /** The session committing the transaction. Set before the rotation. */
  THD *thd{nullptr};
  /** The transaction's cache, whose spilled file is promoted. Set before the
      rotation. */
  binlog_cache_data *cache_data{nullptr};
  /** Whether the spilled file was promoted. Set by the rotation. */
  bool promoted{false};
  /** When promoted: the end of the header events copied from the new file.
      Set by the rotation. */
  my_off_t header_end{0};
  /** Why the transaction falls back, when its spilled file is not promoted.
      Set by the rotation. */
  Large_trx_fallback_reason fallback_reason{Large_trx_fallback_reason::kNone};
};

#endif  // BINLOG_FILE_ROTATION_H_INCLUDED
