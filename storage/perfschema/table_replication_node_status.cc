/*
  Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_node_status.cc
  Table replication_node_status (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "query_options.h"
#include "table_replication_node_status.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"
#include "gcs_replication.h"
#include "log.h"

THR_LOCK table_replication_node_status::m_table_lock;

static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("GROUP_NAME")},
    {C_STRING_WITH_LEN("varchar(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("NODE_ID")},
    {C_STRING_WITH_LEN("char(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TRANSACTIONS_IN_QUEUE")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("CERTIFIED_TRANSACTIONS")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("POSITIVELY_CERTIFIED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("NEGATIVELY_CERTIFIED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("CERTIFICATION_DB_SIZE")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("STABLE_SET")},
    {C_STRING_WITH_LEN("text")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_CERTIFIED_TRANSACTION")},
    {C_STRING_WITH_LEN("text")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("APPLIER_MODULE_STATUS")},
    {C_STRING_WITH_LEN("enum('ON','OFF','ERROR')")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_node_status::m_field_def=
{ 10, field_types };

PFS_engine_table_share
table_replication_node_status::m_share=
{
  { C_STRING_WITH_LEN("replication_node_status") },
  &pfs_readonly_acl,
  &table_replication_node_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_node_status::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_node_status::create(void)
{
  return new table_replication_node_status();
}

table_replication_node_status::table_replication_node_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{
  m_row.stable_set= NULL;
  m_row.stable_set_length= 0;
  m_row.last_cert_trx_length= -1;
}

table_replication_node_status::~table_replication_node_status()
{
  if (m_row.stable_set != NULL)
  {
    my_free(m_row.stable_set);
    m_row.stable_set= NULL;
  }
}

void table_replication_node_status::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_node_status::get_row_count()
{
  uint row_count= 0;

  if (is_gcs_plugin_loaded())
    row_count= 1;

  return row_count;
}

int table_replication_node_status::rnd_next(void)
{
  if (!is_gcs_plugin_loaded())
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

int table_replication_node_status::rnd_pos(const void *pos)
{
  if (get_row_count() == 0)
    return HA_ERR_END_OF_FILE;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < 1);
  make_row();

  return 0;
}

void table_replication_node_status::make_row()
{
  DBUG_ENTER("table_replication_node_status::make_row");
  m_row_exists= false;

  RPL_GCS_NODE_STATS_INFO* node_stats_info;
  if(!(node_stats_info=
       (RPL_GCS_NODE_STATS_INFO*)my_malloc(PSI_NOT_INSTRUMENTED,
                                           sizeof(RPL_GCS_NODE_STATS_INFO),
                                           MYF(MY_WME))))
  {
    sql_print_error("Unable to allocate memory on"
                    " table_replication_node_status::make_row");
    DBUG_VOID_RETURN;
  }

  bool dbsm_stats_not_available= get_gcs_nodes_dbsm_stats(node_stats_info);
  if(dbsm_stats_not_available)
    DBUG_PRINT("info", ("Node's DBSM stats not available!"));
  else
  {
    char* gcs_replication_group= node_stats_info->group_name;
    if (gcs_replication_group)
    {
      memcpy(m_row.group_name, gcs_replication_group, UUID_LENGTH);
      m_row.is_group_name_null= false;
    }
    else
      m_row.is_group_name_null= true;

    m_row.node_id_length= strlen(node_stats_info->node_id);
    memcpy(m_row.node_id, node_stats_info->node_id, m_row.node_id_length);

    m_row.trx_in_queue= node_stats_info->transaction_in_queue;
    m_row.trx_cert= node_stats_info->transaction_certified;
    m_row.pos_cert= node_stats_info->positively_certified;
    m_row.neg_cert= node_stats_info->negatively_certified;
    m_row.cert_db_size= node_stats_info->certification_db_size;

    if(node_stats_info->stable_set)
    {
      m_row.stable_set_length= strlen(node_stats_info->stable_set);
      m_row.stable_set= (char*) my_malloc(PSI_NOT_INSTRUMENTED,
                                          m_row.stable_set_length + 1,
                                          MYF(0));

      memcpy(m_row.stable_set, node_stats_info->stable_set,
             m_row.stable_set_length+1);
    }
    else
    {
      m_row.stable_set_length= 0;
    }

    if(node_stats_info->last_certified_transaction)
    {
      m_row.last_cert_trx_length=
        strlen(node_stats_info->last_certified_transaction);
      memcpy(m_row.last_cert_trx, node_stats_info->last_certified_transaction, m_row.last_cert_trx_length);
    }
    else
    {
      m_row.last_cert_trx_length= 0;
      m_row.last_cert_trx[0]= '\0';
    }

    m_row.applier_state= node_stats_info->applier_state;
    m_row_exists= true;
  }

  if(node_stats_info->stable_set)
    my_free((void*)node_stats_info->stable_set);
  if(node_stats_info->last_certified_transaction)
    my_free((void*)node_stats_info->last_certified_transaction);
  my_free(node_stats_info);
  DBUG_VOID_RETURN;
}


int table_replication_node_status::read_row_values(TABLE *table,
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
      case 0: /** group_name */
        if (!m_row.is_group_name_null)
          set_field_varchar_utf8(f, m_row.group_name, UUID_LENGTH);
        else
          f->set_null();
        break;
      case 1: /** node_id */
        set_field_char_utf8(f, m_row.node_id, m_row.node_id_length);
        break;
      case 2: /** transaction_in_queue */
        set_field_ulonglong(f, m_row.trx_in_queue);
        break;
      case 3: /** transactions_certified */
        set_field_ulonglong(f, m_row.trx_cert);
        break;
      case 4: /** positively_certified_transaction */
        set_field_ulonglong(f, m_row.pos_cert);
        break;
      case 5: /** negatively_certified_transaction */
        set_field_ulonglong(f, m_row.neg_cert);
        break;
      case 6: /** certification_db_size */
        set_field_ulonglong(f, m_row.cert_db_size);
        break;
      case 7: /** stable_set */
        set_field_longtext_utf8(f, m_row.stable_set, m_row.stable_set_length);
        break;
      case 8: /** last_certified_transaction */
        set_field_longtext_utf8(f, m_row.last_cert_trx,
                                m_row.last_cert_trx_length);

        break;
      case 9: /** applier_state */
        set_field_enum(f, m_row.applier_state);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
