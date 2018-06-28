/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_SECONDARY_ENGINE_MOCK_HA_MOCK_H_
#define PLUGIN_SECONDARY_ENGINE_MOCK_HA_MOCK_H_

#include "my_base.h"
#include "sql/handler.h"
#include "thr_lock.h"

class THD;
struct TABLE;
struct TABLE_SHARE;

namespace dd {
class Table;
}

namespace mock {

/**
 * The MOCK storage engine is used for testing MySQL server functionality
 * related to secondary storage engines.
 *
 * There are currently no secondary storage engines mature enough to be merged
 * into mysql-trunk. Therefore, this bare-minimum storage engine, with no
 * actual functionality and implementing only the absolutely necessary handler
 * interfaces to allow setting it as a secondary engine of a table, was created
 * to facilitate pushing MySQL server code changes to mysql-trunk with test
 * coverage without depending on ongoing work of other storage engines.
 *
 * @note This mock storage engine does not support being set as a primary
 * storage engine.
 */
class ha_mock : public handler {
 public:
  ha_mock(handlerton *hton, TABLE_SHARE *table_share);

 private:
  int create(const char *, TABLE *, HA_CREATE_INFO *, dd::Table *) override {
    return HA_ERR_WRONG_COMMAND;
  }

  int open(const char *name, int mode, unsigned int test_if_locked,
           const dd::Table *table_def) override;

  int close() override { return 0; }

  int rnd_init(bool) override { return HA_ERR_WRONG_COMMAND; }

  int rnd_next(unsigned char *) override { return HA_ERR_WRONG_COMMAND; }

  int rnd_pos(unsigned char *, unsigned char *) override {
    return HA_ERR_WRONG_COMMAND;
  }

  int info(unsigned int) override { return HA_ERR_WRONG_COMMAND; }

  void position(const unsigned char *) override {}

  unsigned long index_flags(unsigned int, unsigned int, bool) const override {
    return 0;
  }

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             thr_lock_type lock_type) override;

  Table_flags table_flags() const override { return 0; }

  const char *table_type() const override { return "MOCK"; }

  int load_table(const TABLE &table) override;

  int unload_table(const char *db_name, const char *table_name) override;

  THR_LOCK_DATA m_lock;
};

}  // namespace mock

#endif  // PLUGIN_SECONDARY_ENGINE_MOCK_HA_MOCK_H_
