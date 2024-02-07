/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_CREATE_HELPER_H
#define NDB_CREATE_HELPER_H

#include "my_inttypes.h"

struct NdbError;
class THD;

class Ndb_create_helper {
  THD *const m_thd;
  class Thd_ndb *const m_thd_ndb;
  const char *const m_table_name;

  /**
    @brief Check that warning(s) and error code have been reported
    when failure to create the table occurred. This enforces that error
    messages conforms to the rules:
      - at least one warning providing the details about what is wrong.
      - an error code and message has been set.

    Using this strategy it should be possible for the user
    to use SHOW WARNINGS after a CREATE TABLE failure to get
    better understanding of why it failed.
  */
  void check_warnings_and_error() const;

  int set_create_table_error() const;

  bool have_warning() const;

  /**
   * @brief Failed to create the table. The error code
   * and message will be pushed as warning before setting
   * the "Can't create table" error.
   * @return error code to be returned as command result
   */
  int failed(uint code, const char *message) const;

 public:
  Ndb_create_helper(THD *thd, const char *table_name);
  ~Ndb_create_helper() {}

  /**
   * @brief Failed to create the table. Warning describing the error
   * should already have been pushed. The "Can't create table" error
   * will be set
   * @return error code to be returned as command result
   */
  int failed_warning_already_pushed() const;

  /**
   * @brief Failed to create the table in NDB. The NDB
   * error will be pushed as warning before setting the
   * "Can't create table" error.
   * @return error code to be returned as command result
   */
  int failed_in_NDB(const NdbError &ndb_err) const;

  /**
   * @brief Failed to create the table due to some internal error.
   * The internal error code and message will be pushed as
   * warning before setting the "Can't create table" error.
   * @return error code to be returned as command result
   */
  int failed_internal_error(const char *message) const;

  /**
   * @brief Failed to create the table due to out of memory(or similar).
   * The out of memory error code and message will be pushed as
   * warning before setting the "Can't create table" error.
   * @return error code to be returned as command result
   */
  int failed_oom(const char *message) const;

  /**
   * @brief Failed to create the table because the create
   * options are illegal.
   * Sets the error "Illegal create option <reason>".
   *
   * @reason Short message describing why create options are illegal.
   * The length may be at most 64 bytes long since that's what's is
   * available in the error message format. If the reason need to be
   * longer, simply push a warning before calling his method.
   *
   * @return error code to be returned as command result
   */
  int failed_illegal_create_option(const char *reason) const;

  /**
   * @brief Failed to create the table because some create
   * option is missing. The error code for
   * missing create option together with description will
   * be pushed as warning and the "Can't create table" error set.
   *
   * @description Describes which create option is missing.
   *
   * @return error code to be returned as command result
   */
  int failed_missing_create_option(const char *description) const;

  /**
   * @brief Successfully created the table
   *
   * @return error code to be returned as command result
   */
  int succeeded();
};

#endif
