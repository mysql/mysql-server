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

#include "sql/rpl_sys_key_access.h"

#include "sql/handler.h"

int Rpl_sys_key_access::init(TABLE *table, enum_key_type ktype) {
  m_table = table;
  m_key_type = ktype;
  table->use_all_columns();

  if (m_key_init) return true;

  switch (ktype) {
    case enum_key_type::RND_NEXT:
      if ((m_key_init = !table->file->ha_rnd_init(true))) {
        m_error = m_table->file->ha_rnd_next(m_table->record[0]);
      }
      break;

    case enum_key_type::INDEX_NEXT_SAME:
      if ((m_key_init = !table->file->ha_index_init(0, true))) {
        key_copy(m_key, m_table->record[0], m_table->key_info,
                 m_table->key_info->key_length);
        m_error = m_table->file->ha_index_read_map(
            m_table->record[0], m_key, HA_WHOLE_KEY, HA_READ_KEY_EXACT);
      }
      break;

    case enum_key_type::INDEX_NEXT:
      if ((m_key_init = !table->file->ha_index_init(0, true))) {
        m_error = m_table->file->ha_index_first(m_table->record[0]);
      }
      break;

    case enum_key_type::RND_POS:
    default:
      assert(false);
  }

  return m_error;
}

int Rpl_sys_key_access::init(TABLE *table, uint index, bool sorted,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag) {
  m_table = table;
  m_key_type = enum_key_type::INDEX_NEXT_SAME;
  table->use_all_columns();

  if (m_key_init) return true;

  if ((m_key_init = !table->file->ha_index_init(index, sorted))) {
    KEY *key_info = table->key_info + index;
    key_copy(m_key, m_table->record[0], key_info, key_info->key_length);
    m_error = m_table->file->ha_index_read_map(m_table->record[0], m_key,
                                               keypart_map, find_flag);
  }

  return m_error;
}

int Rpl_sys_key_access::init(TABLE *table, std::string pos) {
  m_table = table;
  m_key_type = enum_key_type::RND_POS;
  table->use_all_columns();

  if (m_key_init) return true;

  if ((m_key_init = !table->file->ha_rnd_init(false))) {
    char *cpos = const_cast<char *>(pos.c_str());
    m_error = m_table->file->ha_rnd_pos(m_table->record[0], (uchar *)cpos);
  }

  return m_error;
}

Rpl_sys_key_access::~Rpl_sys_key_access() { deinit(); }

bool Rpl_sys_key_access::deinit() {
  int end_error{0};
  if (!m_key_init) return true;
  if (m_key_deinit) return false;

  switch (m_key_type) {
    case enum_key_type::RND_NEXT:
    case enum_key_type::RND_POS:
      end_error = m_table->file->ha_rnd_end();
      break;

    case enum_key_type::INDEX_NEXT_SAME:
    case enum_key_type::INDEX_NEXT:
      end_error = m_table->file->ha_index_end();
      break;
  }

  m_key_deinit = true;
  if ((m_error == HA_ERR_END_OF_FILE || m_error == HA_ERR_KEY_NOT_FOUND) &&
      !end_error) {
    m_error = 0;
  } else if (end_error) {
    m_error = end_error;
  }

  return m_error;
}

int Rpl_sys_key_access::next() {
  if (!m_key_init) return 1;
  if (m_error) return m_error;

  switch (m_key_type) {
    case enum_key_type::RND_NEXT:
      m_error = m_table->file->ha_rnd_next(m_table->record[0]);
      break;

    case enum_key_type::INDEX_NEXT_SAME:
      m_error = m_table->file->ha_index_next_same(
          m_table->record[0], m_key, m_table->key_info->key_length);
      break;

    case enum_key_type::INDEX_NEXT:
      m_error = m_table->file->ha_index_next(m_table->record[0]);
      break;

    case enum_key_type::RND_POS:
    default:
      assert(false);
  }

  return m_error;
}

bool Rpl_sys_key_access::is_read_error() {
  if (m_error != HA_ERR_END_OF_FILE && m_error != HA_ERR_KEY_NOT_FOUND)
    return true;

  return false;
}
