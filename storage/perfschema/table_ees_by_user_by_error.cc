/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_ees_by_user_by_error.cc
  Table EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR (implementation).
*/

#include "storage/perfschema/table_ees_by_user_by_error.h"

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

THR_LOCK table_ees_by_user_by_error::m_table_lock;

/* clang-format off */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    { C_STRING_WITH_LEN("USER") },
    { C_STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ERROR_NUMBER") },
    { C_STRING_WITH_LEN("int(11)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ERROR_NAME") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SQL_STATE") },
    { C_STRING_WITH_LEN("varchar(5)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ERROR_RAISED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("SUM_ERROR_HANDLED") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("FIRST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("LAST_SEEN") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0}
  }
};
/* clang-format on */

TABLE_FIELD_DEF
table_ees_by_user_by_error::m_field_def = {8, field_types};

PFS_engine_table_share table_ees_by_user_by_error::m_share = {
  {C_STRING_WITH_LEN("events_errors_summary_by_user_by_error")},
  &pfs_truncatable_acl,
  table_ees_by_user_by_error::create,
  NULL, /* write_row */
  table_ees_by_user_by_error::delete_all_rows,
  table_ees_by_user_by_error::get_row_count,
  sizeof(pos_ees_by_user_by_error),
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

bool
PFS_index_ees_by_user_by_error::match(PFS_user *pfs)
{
  if (m_fields >= 1)
  {
    if (!m_key_1.match(pfs))
    {
      return false;
    }
  }
  return true;
}

bool
PFS_index_ees_by_user_by_error::match_error_index(uint error_index)
{
  if (m_fields >= 2)
  {
    if (!m_key_2.match_error_index(error_index))
    {
      return false;
    }
  }
  return true;
}

PFS_engine_table *
table_ees_by_user_by_error::create(void)
{
  return new table_ees_by_user_by_error();
}

int
table_ees_by_user_by_error::delete_all_rows(void)
{
  reset_events_errors_by_thread();
  reset_events_errors_by_account();
  reset_events_errors_by_user();
  return 0;
}

ha_rows
table_ees_by_user_by_error::get_row_count(void)
{
  return global_user_container.get_row_count() * error_class_max *
         max_server_errors;
}

table_ees_by_user_by_error::table_ees_by_user_by_error()
  : PFS_engine_table(&m_share, &m_pos), m_pos(), m_next_pos()
{
}

void
table_ees_by_user_by_error::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int
table_ees_by_user_by_error::rnd_init(bool)
{
  return 0;
}

int
table_ees_by_user_by_error::rnd_next(void)
{
  PFS_user *user;
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user())
  {
    user = global_user_container.get(m_pos.m_index_1, &has_more_user);
    if (user != NULL)
    {
      for (; m_pos.has_more_error(); m_pos.next_error())
      {
        if (!make_row(user, m_pos.m_index_2))
        {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ees_by_user_by_error::rnd_pos(const void *pos)
{
  PFS_user *user;

  set_position(pos);

  user = global_user_container.get(m_pos.m_index_1);
  if (user != NULL)
  {
    for (; m_pos.has_more_error(); m_pos.next_error())
    {
      if (!make_row(user, m_pos.m_index_2))
      {
        return 0;
      }
    }
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_ees_by_user_by_error::index_init(uint idx, bool)
{
  PFS_index_ees_by_user_by_error *result = NULL;
  DBUG_ASSERT(idx == 0);
  result = PFS_NEW(PFS_index_ees_by_user_by_error);
  m_opened_index = result;
  m_index = result;
  return 0;
}

int
table_ees_by_user_by_error::index_next(void)
{
  PFS_user *user;
  bool has_more_user = true;

  for (m_pos.set_at(&m_next_pos); has_more_user; m_pos.next_user())
  {
    user = global_user_container.get(m_pos.m_index_1, &has_more_user);
    if (user != NULL)
    {
      if (m_opened_index->match(user))
      {
        for (; m_pos.has_more_error(); m_pos.next_error())
        {
          if (m_opened_index->match_error_index(m_pos.m_index_2))
          {
            if (!make_row(user, m_pos.m_index_2))
            {
              m_next_pos.set_after(&m_pos);
              return 0;
            }
          }
        }
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
table_ees_by_user_by_error::make_row(PFS_user *user, int error_index)
{
  PFS_error_class *klass = &global_error_class;
  pfs_optimistic_state lock;

  user->m_lock.begin_optimistic_lock(&lock);

  if (m_row.m_user.make_row(user))
  {
    return HA_ERR_RECORD_DELETED;
  }

  PFS_connection_error_visitor visitor(klass, error_index);
  PFS_connection_iterator::visit_user(user,
                                      true,  /* accounts */
                                      true,  /* threads */
                                      false, /* THDs */
                                      &visitor);

  if (!user->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_row.m_stat.set(&visitor.m_stat, error_index);

  return 0;
}

int
table_ees_by_user_by_error::read_row_values(TABLE *table,
                                            unsigned char *buf,
                                            Field **fields,
                                            bool read_all)
{
  Field *f;
  server_error *temp_error = NULL;

  /* Set the null bits */
  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0] = 0;

  if (m_row.m_stat.m_error_index > 0 &&
      m_row.m_stat.m_error_index < PFS_MAX_SERVER_ERRORS)
    temp_error =
      &error_names_array[pfs_to_server_error_map[m_row.m_stat.m_error_index]];

  for (; (f = *fields); fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch (f->field_index)
      {
      case 0: /* USER */
        m_row.m_user.set_field(f);
        break;
      case 1: /* ERROR NUMBER */
      case 2: /* ERROR NAME */
      case 3: /* SQLSTATE */
      case 4: /* SUM_ERROR_RAISED */
      case 5: /* SUM_ERROR_HANDLED */
      case 6: /* FIRST_SEEN */
      case 7: /* LAST_SEEN */
        /** ERROR STATS */
        m_row.m_stat.set_field(f->field_index - 1, f, temp_error);
        break;
      default:
        /** We should never reach here */
        DBUG_ASSERT(0);
        break;
      }
    }
  }

  return 0;
}
