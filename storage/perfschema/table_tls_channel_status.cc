/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_tls_channel_status.cc
  Tble TLS_CHANNEL_STATUS (implementation).
*/

#include "storage/perfschema/table_tls_channel_status.h"
#include "storage/perfschema/pfs_tls_channel.h"

#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"

THR_LOCK table_tls_channel_status::m_table_lock;

/** Symbolic names for field offsets, keep in sync with field_types */
enum tls_channel_status_offsets { FO_CHANNEL, FO_PROPERTY, FO_VALUE };

Plugin_table table_tls_channel_status::m_table_def{
    /* Schema name */
    "performance_schema",
    /* Name */
    "tls_channel_status",
    /* Definition */
    " CHANNEL VARCHAR(128) NOT NULL,\n"
    " PROPERTY VARCHAR(128) NOT NULL,\n"
    " VALUE VARCHAR(2048) NOT NULL\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr};

PFS_engine_table_share table_tls_channel_status::m_share = {
    &pfs_readonly_acl,
    table_tls_channel_status::create,
    nullptr, /* write_row */
    nullptr, /* delete_all_rows */
    table_tls_channel_status::get_row_count,
    sizeof(PFS_simple_index), /* ref length */
    &m_table_lock,
    &m_table_def,
    false, /* Perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_tls_channel_status::create(PFS_engine_table_share *) {
  return new table_tls_channel_status();
}

table_tls_channel_status::table_tls_channel_status()
    : PFS_engine_table(&m_share, &m_pos),
      m_row(nullptr),
      m_pos(0),
      m_next_pos(0) {
  /* Make a copy of TLS context status */
  materialize();
}

void table_tls_channel_status::materialize() {
  /* Read properties for one channel */
  auto process_one_channel =
      [&](const TLS_channel_property_iterator *tls_channel) {
        property_iterator iterator;
        if (tls_channel == nullptr ||
            tls_channel->init_tls_property_iterator == nullptr ||
            tls_channel->deinit_tls_property_iterator == nullptr ||
            tls_channel->get_tls_property == nullptr ||
            tls_channel->next_tls_property == nullptr)
          return;
        if (!tls_channel->init_tls_property_iterator(&iterator)) return;
        TLS_channel_property one_property;
        for (;;) {
          /* Reset */
          memset(one_property.channel_name, 0,
                 sizeof(one_property.channel_name));
          memset(one_property.property_name, 0,
                 sizeof(one_property.property_name));
          memset(one_property.property_value, 0,
                 sizeof(one_property.property_value));

          /* Fetch property */
          if (!tls_channel->get_tls_property(iterator, &one_property)) break;

          /* Store property */
          row_tls_channel_status one_row;
          one_row.m_interface.assign(one_property.channel_name);
          one_row.m_property_name.assign(one_property.property_name);
          one_row.m_property_value.assign(one_property.property_value);
          m_row_tls_channel_status.push_back(one_row);

          /* next */
          if (!tls_channel->next_tls_property(iterator)) break;
        }
        tls_channel->deinit_tls_property_iterator(iterator);
      };

  /*
    Iterate over all registered channels and read their properties.
    In order to minimize the duration of the lock, we store all
    properties locally. ::read_row_values will use the local buffer.
  */
  pfs_tls_channels_lock_for_read();
  const tls_channels &channels = pfs_get_instrumented_tls_channels();
  for (auto *channel : channels) {
    process_one_channel(channel);
  }
  pfs_tls_channels_unlock();
}

ha_rows table_tls_channel_status::get_row_count() {
  /* A hint for optimizer - number bytes not number entries */
  return sizeof(row_tls_channel_status);
}

void table_tls_channel_status::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int table_tls_channel_status::rnd_pos(const void *pos) {
  set_position(pos);
  assert(m_pos.m_index < m_row_tls_channel_status.size());
  m_row = &m_row_tls_channel_status[m_pos.m_index];
  return 0;
}

int table_tls_channel_status::rnd_next() {
  m_pos.set_at(&m_next_pos);
  if (m_pos.m_index < m_row_tls_channel_status.size()) {
    m_row = &m_row_tls_channel_status[m_pos.m_index];
    m_next_pos.set_after(&m_pos);
    return 0;
  }
  m_row = nullptr;
  return HA_ERR_END_OF_FILE;
}

int table_tls_channel_status::read_row_values(TABLE *table, unsigned char *buf,
                                              Field **fields, bool read_all) {
  Field *f;
  assert(m_row);
  /* Set the null bits */
  assert(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case tls_channel_status_offsets::FO_CHANNEL:
          set_field_varchar_utf8mb4(
              f, m_row->m_interface.c_str(),
              static_cast<uint>(m_row->m_interface.length()));
          break;
        case tls_channel_status_offsets::FO_PROPERTY:
          set_field_varchar_utf8mb4(
              f, m_row->m_property_name.c_str(),
              static_cast<uint>(m_row->m_property_name.length()));
          break;
        case tls_channel_status_offsets::FO_VALUE:
          set_field_varchar_utf8mb4(
              f, m_row->m_property_value.c_str(),
              static_cast<uint>(m_row->m_property_value.length()));
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
