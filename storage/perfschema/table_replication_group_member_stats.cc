/*
  Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/table_replication_group_member_stats.cc
  Table replication_group_member_stats (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "table_replication_group_member_stats.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "log.h"
#include "rpl_group_replication.h"

THR_LOCK table_replication_group_member_stats::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("CHANNEL_NAME")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("VIEW_ID")},
    {C_STRING_WITH_LEN("char(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MEMBER_ID")},
    {C_STRING_WITH_LEN("char(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_TRANSACTIONS_IN_QUEUE")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_TRANSACTIONS_CHECKED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_CONFLICTS_DETECTED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_TRANSACTIONS_VALIDATING")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TRANSACTIONS_COMMITTED_ALL_MEMBERS")},
    {C_STRING_WITH_LEN("text")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_CONFLICT_FREE_TRANSACTION")},
    {C_STRING_WITH_LEN("text")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_group_member_stats::m_field_def=
{ 9, field_types };

PFS_engine_table_share
table_replication_group_member_stats::m_share=
{
  { C_STRING_WITH_LEN("replication_group_member_stats") },
  &pfs_readonly_acl,
  &table_replication_group_member_stats::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_group_member_stats::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false, /* checked */
  false  /* perpetual */
};

PFS_engine_table* table_replication_group_member_stats::create(void)
{
  return new table_replication_group_member_stats();
}

table_replication_group_member_stats::table_replication_group_member_stats()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{
  m_row.trx_committed= NULL;
  m_row.trx_committed_length= 0;
  m_row.last_cert_trx_length= -1;
}

table_replication_group_member_stats::~table_replication_group_member_stats()
{
  if (m_row.trx_committed != NULL)
  {
    my_free(m_row.trx_committed);
    m_row.trx_committed= NULL;
  }
}

void table_replication_group_member_stats::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_group_member_stats::get_row_count()
{
  uint row_count= 0;

  if (is_group_replication_plugin_loaded())
    row_count= 1;

  return row_count;
}

int table_replication_group_member_stats::rnd_next(void)
{
  if (!is_group_replication_plugin_loaded())
    return HA_ERR_END_OF_FILE;

  m_pos.set_at(&m_next_pos);
  if (m_pos.m_index == 0)
  {
    make_row();
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_replication_group_member_stats::rnd_pos(const void *pos)
{
  if (get_row_count() == 0)
    return HA_ERR_END_OF_FILE;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < 1);
  make_row();

  return 0;
}

void table_replication_group_member_stats::make_row()
{
  DBUG_ENTER("table_replication_group_member_stats::make_row");
  m_row_exists= false;

  GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO* group_member_stats_info;
  if(!(group_member_stats_info=
       (GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO*)
                           my_malloc(PSI_NOT_INSTRUMENTED,
                                     sizeof(GROUP_REPLICATION_GROUP_MEMBER_STATS_INFO),
                                     MYF(MY_WME))))
  {
    sql_print_error("Unable to allocate memory on"
                    " table_replication_group_member_stats::make_row");
    DBUG_VOID_RETURN;
  }

  bool dbsm_stats_not_available
                            = get_group_replication_group_member_stats_info
                                                      (group_member_stats_info);

  if(dbsm_stats_not_available)
    DBUG_PRINT("info", ("Member's DBSM stats not available!"));
  else
  {
    m_row.channel_name_length= 0;
    if (group_member_stats_info->channel_name != NULL)
    {
      m_row.channel_name_length= strlen(group_member_stats_info->channel_name);
      memcpy(m_row.channel_name, group_member_stats_info->channel_name,
             m_row.channel_name_length);

      my_free((void*)group_member_stats_info->channel_name);
    }

    if(group_member_stats_info->view_id != NULL)
    {
      m_row.view_id_length= strlen(group_member_stats_info->view_id);
      memcpy(m_row.view_id, group_member_stats_info->view_id,
             m_row.view_id_length);

      my_free((void*)group_member_stats_info->view_id);
    }
    else
    {
      m_row.view_id_length= 0;
      m_row.view_id[0]= '\0';
    }

    m_row.member_id_length= 0;
    if (group_member_stats_info->member_id != NULL)
    {
      m_row.member_id_length= strlen(group_member_stats_info->member_id);
      memcpy(m_row.member_id, group_member_stats_info->member_id,
             m_row.member_id_length);

      my_free((void*)group_member_stats_info->member_id);
    }

    m_row.trx_in_queue= group_member_stats_info->transaction_in_queue;
    m_row.trx_checked= group_member_stats_info->transaction_certified;
    m_row.trx_conflicts= group_member_stats_info
                                             ->transaction_conflicts_detected;
    m_row.trx_validating= group_member_stats_info->transactions_in_validation;

    m_row.trx_committed_length= 0;
    if (group_member_stats_info->committed_transactions != NULL)
    {
      m_row.trx_committed_length= strlen(group_member_stats_info
                                                      ->committed_transactions);
      m_row.trx_committed= (char*) my_malloc(PSI_NOT_INSTRUMENTED,
                                             m_row.trx_committed_length + 1,
                                             MYF(0));

      memcpy(m_row.trx_committed,
             group_member_stats_info->committed_transactions,
             m_row.trx_committed_length+1);

      my_free((void*)group_member_stats_info->committed_transactions);
    }

    if(group_member_stats_info->last_conflict_free_transaction != NULL)
    {
      m_row.last_cert_trx_length=
        strlen(group_member_stats_info->last_conflict_free_transaction);
      memcpy(m_row.last_cert_trx,
             group_member_stats_info->last_conflict_free_transaction,
             m_row.last_cert_trx_length);

      my_free((void*)group_member_stats_info->last_conflict_free_transaction);
    }
    else
    {
      m_row.last_cert_trx_length= 0;
      m_row.last_cert_trx[0]= '\0';
    }

    m_row_exists= true;
  }

  my_free(group_member_stats_info);

  DBUG_VOID_RETURN;
}


int table_replication_group_member_stats::read_row_values(TABLE *table,
                                                   unsigned char *buf,
                                                   Field **fields,
                                                   bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** channel_name */
        set_field_char_utf8(f, m_row.channel_name,
                               m_row.channel_name_length);
        break;
      case 1: /** view id */
        set_field_char_utf8(f, m_row.view_id, m_row.view_id_length);
        break;
      case 2: /** member_id */
        set_field_char_utf8(f, m_row.member_id, m_row.member_id_length);
        break;
      case 3: /** transaction_in_queue */
        set_field_ulonglong(f, m_row.trx_in_queue);
        break;
      case 4: /** transactions_certified */
        set_field_ulonglong(f, m_row.trx_checked);
        break;
      case 5: /** negatively_certified_transaction */
        set_field_ulonglong(f, m_row.trx_conflicts);
        break;
      case 6: /** certification_db_size */
        set_field_ulonglong(f, m_row.trx_validating);
        break;
      case 7: /** stable_set */
        set_field_longtext_utf8(f, m_row.trx_committed,
                                m_row.trx_committed_length);
        break;
      case 8: /** last_certified_transaction */
        set_field_longtext_utf8(f, m_row.last_cert_trx,
                                m_row.last_cert_trx_length);

        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
