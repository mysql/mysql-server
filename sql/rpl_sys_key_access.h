/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef RPL_SYS_KEY_ACCESS_INCLUDED
#define RPL_SYS_KEY_ACCESS_INCLUDED

#include "sql/table.h"

/**
  @class Rpl_sys_table_access

  The class are wrappers for handler index and random scan functions to
  simplify their usage.
*/
class Rpl_sys_key_access {
 public:
  /* Index type */
  enum class enum_key_type {
    /* Read next row via random scan using handler::ha_rnd_next. */
    RND_NEXT,
    /* Read row via random scan from position using handler::ha_rnd_pos. */
    RND_POS,
    /*
      Read [part of] row via [part of] index using
      handler::ha_index_read_map.
    */
    INDEX_NEXT_SAME,
    /* Read all rows of index using handler::ha_index_first. */
    INDEX_NEXT
  };

  /**
    Construction.
  */
  Rpl_sys_key_access() = default;

  /**
    Destruction.
    Closes all initialized index or random scan during destruction.
  */
  ~Rpl_sys_key_access();

  /**
    Construction.

    @param[in]  table     Table object from which row needs to be fetched.
    @param[in]  type      The type of scan to use to read row.

    @retval 0     Success
    @retval !0    Error
  */
  int init(TABLE *table, enum_key_type type);

  /**
    When index type enum_key_type::INDEX_NEXT_SAME needs to be used to read
    [part of] row via [part of] index.

    @param[in]  table       Table object from which row needs to be fetched.
    @param[in]  index       Index to use
    @param[in]  sorted      Use sorted order
    @param[in]  keypart_map Which part of key to use
    @param[in]  find_flag   Direction/condition on key usage

    @retval 0     Success
    @retval !0    Error
  */
  int init(TABLE *table, uint index = 0, bool sorted = true,
           key_part_map keypart_map = 1,
           enum ha_rkey_function find_flag = HA_READ_KEY_EXACT);

  /**
    When index type enum_key_type::RND_POS needs to be used to read row via
    random scan from position.

    @param[in]  table   Table object from which row needs to be fetched.
    @param[in]  pos     The position from where to read row.

    @retval 0     Success
    @retval !0    Error
  */
  int init(TABLE *table, std::string pos);

  /**
    Closes all initialized index or random scan during destruction.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool deinit();

  /**
    Get next row in the table.

    @retval 0     Success
    @retval !0    Error
  */
  int next();

  /**
    Verify if error is set, ignores HA_ERR_END_OF_FILE and
    HA_ERR_KEY_NOT_FOUND.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool is_read_error();

  /**
    Get error set during index initialization or fetching next rows.

    @retval 0     Success
    @retval !0    Error
  */
  int get_error() { return m_error; }

 private:
  /* TABLE object */
  TABLE *m_table{nullptr};

  /* The type of index used */
  enum_key_type m_key_type{enum_key_type::RND_POS};

  /* Determine if index is initialized. */
  bool m_key_init{false};

  /* Determine if index is deinitialized. */
  bool m_key_deinit{false};

  /* The buffer to store the key */
  uchar m_key[MAX_KEY_LENGTH];

  /*
    The variable stores error during index initialization or fetching next
    rows.
  */
  int m_error{1};
};
#endif /* RPL_SYS_KEY_ACCESS_INCLUDED */
