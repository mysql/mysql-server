/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

/**
  @file storage/perfschema/table_esgs_by_account_by_event_name.cc
  Table EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME (implementation).
*/

#include "storage/perfschema/table_esgs_by_account_by_event_name.h"

#include <stddef.h>

#include "field.h"
#include "my_dbug.h"
#include "my_thread.h"
#include "pfs_buffer_container.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_visitor.h"

THR_LOCK table_esgs_by_account_by_event_name::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("USER") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HOST") },
    { C_STRING_WITH_LEN("char(60)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("EVENT_NAME") },
    { C_STRING_WITH_LEN("varchar(128)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COUNT_STAR") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MIN_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("AVG_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("MAX_TIMER_WAIT") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_esgs_by_account_by_event_name::m_field_def = {8, field_types};

PFS_engine_table_share table_esgs_by_account_by_event_name::m_share = {
  {C_STRING_WITH_LEN("events_stages_summary_by_account_by_event_name")},
  &pfs_truncatable_acl,
  table_esgs_by_account_by_event_name::create,
  NULL, /* write_row */
  table_esgs_by_account_by_event_name::delete_all_rows,
  table_esgs_by_account_by_event_name::get_row_count,
  sizeof(pos_esgs_by_account_by_event_name),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_esgs_by_account_by_event_name::match(PFS_account *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(pfs))
    {
      return false;
    }
  }

  if (m_fields >= 2)
  {
    if (!m_key_2.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_esgs_by_account_by_event_name::match(PFS_instr_class *instr_class)
{
  if (m_fields >= 3)
  {
    if (!m_key_3.match(instr_class))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_esgs_by_account_by_event_name::create(void)
{
  return new table_esgs_by_account_by_event_name();
}

int
table_esgs_by_account_by_event_name::delete_all_rows(void)
{
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  return 0;
}

ha_rows
table_esgs_by_account_by_event_name::get_row_count(void)
{
  return global_account_container.get_row_count() * stage_class_max;
}

table_esgs_by_account_by_event_name::table_esgs_by_account_by_event_name()
  : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos()
{
}

void
table_esgs_by_account_by_event_name::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int
table_esgs_by_account_by_event_name::rnd_init(bool)
{
  m_normalizer = time_normalizer::get(stage_timer);
  return 0;
}

int
table_esgs_by_account_by_event_name::rnd_next(void)
{
  PFS_account *account;
  PFS_stage_class *stage_class;
  bool has_more_account = true;

  for (m_pos.set_at(&m_next_pos); has_more_account; m_pos.next_account())
  {
    account = global_account_container.get(m_pos.m_index_1, &has_more_account);
    if (account != NULL)
    {
      stage_class = find_stage_class(m_pos.m_index_2);
      if (stage_class)
      {
        m_next_pos.set_after(&m_pos);
        return make_row(account, stage_class);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esgs_by_account_by_event_name::rnd_pos(const void *pos)
{
  PFS_account *account;
  PFS_stage_class *stage_class;

  set_position(pos);

  account = global_account_container.get(m_pos.m_index_1);
  if (account != NULL)
  {
    stage_class = find_stage_class(m_pos.m_index_2);
    if (stage_class)
    {
      return make_row(account, stage_class);
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_esgs_by_account_by_event_name::index_init(uint idx, bool)
{
  m_normalizer = time_normalizer::get(stage_timer);

  PFS_index_esgs_by_account_by_event_name *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_esgs_by_account_by_event_name);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_esgs_by_account_by_event_name::index_next(void)
{
  PFS_account *account;
  PFS_stage_class *stage_class;
  bool has_more_account = true;

  for (m_pos.set_at(&m_next_pos); has_more_account; m_pos.next_account())
  {
    account = global_account_container.get(m_pos.m_index_1, &has_more_account);
    if (account != NULL)
    {
      if (m_opened_index->match(account))
      {
        do
        {
          stage_class = find_stage_class(m_pos.m_index_2);
          if (stage_class)
          {
            if (m_opened_index->match(stage_class))
            {
              if (!make_row(account, stage_class))
              {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
            m_pos.m_index_2++;
          }
        } while (stage_class != NULL);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esgs_by_account_by_event_name::make_row(PFS_account *account,
                                              PFS_stage_class *klass)
{
  pfs_optimistic_state lock;

  account->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_account.make_row(account))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_event_name.make_row(klass);

  PFS_connection_stage_visitor visitor(klass);
  PFS_connection_iterator::visit_account(account,
                                         true,  /* threads */
                                         false, /* THDs */
                                         &visitor);

  if (!account->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(m_normalizer, &visitor.m_stat);

  return 0;
}

int
table_esgs_by_account_by_event_name::read_row_values(TABLE *table,
                                                     unsigned char *buf,
                                                     Field **fields,
                                                     bool read_all)
{
  Field *f;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* USER */
      case 1: /* HOST */
        m_row.m_account.set_field(f->field_index, f);
        break;
      case 2: /* EVENT_NAME */
        m_row.m_event_name.set_field(f);
        break;
      default: /* 3, ... COUNT/SUM/MIN/AVG/MAX */
        m_row.m_stat.set_field(f->field_index - 3, f);
        break;
      }
    }
  }

  return 0;
}
