/*
      Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_replication_connection_status.cc
  Table replication_connection_status (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "table_replication_connection_status.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"
#include "gcs_replication.h"
#include "log.h"

THR_LOCK table_replication_connection_status::m_table_lock;


/* Numbers in varchar count utf8 characters. */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("CHANNEL_NAME")},
    {C_STRING_WITH_LEN("char(64)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("GROUP_NAME")},
    {C_STRING_WITH_LEN("varchar(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SOURCE_UUID")},
    {C_STRING_WITH_LEN("char(36)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("THREAD_ID")},
    {C_STRING_WITH_LEN("bigint(20)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SERVICE_STATE")},
    {C_STRING_WITH_LEN("enum('ON','OFF','CONNECTING')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("COUNT_RECEIVED_HEARTBEATS")},
    {C_STRING_WITH_LEN("bigint(20)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_HEARTBEAT_TIMESTAMP")},
    {C_STRING_WITH_LEN("timestamp")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("RECEIVED_TRANSACTION_SET")},
    {C_STRING_WITH_LEN("text")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_ERROR_NUMBER")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_ERROR_MESSAGE")},
    {C_STRING_WITH_LEN("varchar(1024)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_ERROR_TIMESTAMP")},
    {C_STRING_WITH_LEN("timestamp")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TOTAL_MESSAGES_RECEIVED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TOTAL_MESSAGES_SENT")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TOTAL_BYTES_RECEIVED")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("TOTAL_BYTES_SENT")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("LAST_MESSAGE_TIMESTAMP")},
    {C_STRING_WITH_LEN("timestamp")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MAX_MESSAGE_LENGTH")},
    {C_STRING_WITH_LEN("int")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("MIN_MESSAGE_LENGTH")},
    {C_STRING_WITH_LEN("int")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("VIEW_ID")},
    {C_STRING_WITH_LEN("varchar(40)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("NUMBER_OF_NODES")},
    {C_STRING_WITH_LEN("int")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_connection_status::m_field_def= { 20, field_types };

PFS_engine_table_share
table_replication_connection_status::m_share=
{
  { C_STRING_WITH_LEN("replication_connection_status") },
  &pfs_readonly_acl,
  table_replication_connection_status::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_connection_status::get_row_count, /* records */
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};

PFS_engine_table* table_replication_connection_status::create(void)
{
  return new table_replication_connection_status();
}

table_replication_connection_status::table_replication_connection_status()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{
}

table_replication_connection_status::~table_replication_connection_status()
{
}

void table_replication_connection_status::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_connection_status::get_row_count()
{
  uint row_count= msr_map.get_max_channels();

  if (is_gcs_plugin_loaded())
    row_count+= 1;

  return row_count;
}



int table_replication_connection_status::rnd_next(void)
{
  Master_info *mi= NULL;

  int offset = 0;

  m_pos.set_at(&m_next_pos);
  if (is_gcs_plugin_loaded())
  {

     if (m_pos.m_index == 0)
     {
       make_row(NULL, true);
       m_next_pos.set_after(&m_pos);
       return 0;
     }
    offset= 1;
  }

  mysql_mutex_lock(&LOCK_msr_map);

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index - offset < msr_map.get_max_channels();
       m_pos.next())
  {
    mi= msr_map.get_mi_at_pos(m_pos.m_index - offset);

    if (mi && mi->host[0])
    {
      make_row(mi, false);
      m_next_pos.set_after(&m_pos);

      mysql_mutex_unlock(&LOCK_msr_map);
      return 0;
    }
  }

  mysql_mutex_unlock(&LOCK_msr_map);

  return HA_ERR_END_OF_FILE;

}

int table_replication_connection_status::rnd_pos(const void *pos)
{
  Master_info *mi;

  set_position(pos);

  if (is_gcs_plugin_loaded() && m_pos.m_index == 0)
  {
    make_row(NULL, true);
    return 0;
  }

  mysql_mutex_lock(&LOCK_msr_map);

  if ((mi= msr_map.get_mi_at_pos(m_pos.m_index)))
  {
    make_row(mi, false);

    mysql_mutex_unlock(&LOCK_msr_map);
    return 0;
  }

  mysql_mutex_unlock(&LOCK_msr_map);

  return HA_ERR_RECORD_DELETED;

}

void table_replication_connection_status::make_row(Master_info *mi,
                                                   bool gcs_row)
{
  DBUG_ENTER("table_replication_connection_status::make_row");

  m_row_exists= false;

  //default values
  m_row.group_name_is_null= true;
  m_row.source_uuid_is_null= true;
  m_row.thread_id= 0;

  //Load GCS stats
  char* gcs_replication_group;
  RPL_GCS_STATS_INFO* gcs_info;

  m_row.is_gcs_plugin_loaded= is_gcs_plugin_loaded() && gcs_row;

  if (m_row.is_gcs_plugin_loaded)
  {

    if(!(gcs_info= (RPL_GCS_STATS_INFO*)my_malloc(PSI_NOT_INSTRUMENTED,
                                                  sizeof(RPL_GCS_STATS_INFO),
                                                  MYF(MY_WME))))
    {
       sql_print_error("Unable to allocate memory on"
                       " table_replication_connection_status::make_row");
       DBUG_VOID_RETURN;
    }

    //Initialization procedures
    gcs_info->view_id= NULL;

    bool stats_not_available= get_gcs_stats(gcs_info);
    if (stats_not_available)
    {
      m_row.is_gcs_plugin_loaded= false;
      my_free(gcs_info);
      /*
        Here, these stats about GCS would not be available only when plugin is
        not available/not loaded at this point in time.
        Hence, modified the flag after the check.
      */
      gcs_info= NULL;
      DBUG_PRINT("info", ("GCS stats not available!"));
    }

    if(m_row.is_gcs_plugin_loaded)
    {
      gcs_replication_group= gcs_info->group_name;
      if (gcs_replication_group)
      {
        memcpy(m_row.group_name, gcs_replication_group, UUID_LENGTH+1);
        m_row.group_name_is_null= false;
      }
    }
  }

  if(!gcs_row) //if slave channel
  {
    DBUG_ASSERT(mi != NULL);
    DBUG_ASSERT(mi->rli != NULL);

    mysql_mutex_lock(&mi->data_lock);
    mysql_mutex_lock(&mi->rli->data_lock);

    //Channel
    m_row.channel_name_length= mi->get_channel() ? strlen(mi->get_channel()):0;
    memcpy(m_row.channel_name, mi->get_channel(), m_row.channel_name_length);

    //Source uuid
    if (mi->master_uuid[0] != 0)
    {
      memcpy(m_row.source_uuid, mi->master_uuid, UUID_LENGTH+1);
      m_row.source_uuid_is_null= false;
    }

    if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
    {
      PSI_thread *psi= thd_get_psi(mi->info_thd);
      PFS_thread *pfs= reinterpret_cast<PFS_thread *> (psi);
      if(pfs)
      {
        m_row.thread_id= pfs->m_thread_internal_id;
        m_row.thread_id_is_null= false;
      }
      else
        m_row.thread_id_is_null= true;
    }
    else
      m_row.thread_id_is_null= true;

    if (mi->slave_running == MYSQL_SLAVE_RUN_CONNECT)
      m_row.service_state= PS_RPL_CONNECT_SERVICE_STATE_YES;
    else
    {
      if (mi->slave_running == MYSQL_SLAVE_RUN_NOT_CONNECT)
        m_row.service_state= PS_RPL_CONNECT_SERVICE_STATE_CONNECTING;
      else
        m_row.service_state= PS_RPL_CONNECT_SERVICE_STATE_NO;
    }

    m_row.count_received_heartbeats= mi->received_heartbeats;
    /*
      Time in Milliseconds since epoch. active_mi->last_heartbeat contains
      number of seconds so we multiply by 1000000.
    */
    m_row.last_heartbeat_timestamp= (ulonglong)mi->last_heartbeat*1000000;

    mysql_mutex_lock(&mi->err_lock);
    mysql_mutex_lock(&mi->rli->err_lock);


    global_sid_lock->wrlock();

    const Gtid_set* io_gtid_set= mi->rli->get_gtid_set();

    if ((m_row.received_transaction_set_length=
              io_gtid_set->to_string(&m_row.received_transaction_set)) < 0)
    {
      my_free(m_row.received_transaction_set);
      m_row.received_transaction_set_length= 0;
      global_sid_lock->unlock();
      return;
    }
    global_sid_lock->unlock();


    m_row.last_error_number= (unsigned int) mi->last_error().number;
    m_row.last_error_message_length= 0;
    m_row.last_error_timestamp= 0;

    /** If error, set error message and timestamp */
    if (m_row.last_error_number)
    {
      char* temp_store= (char*)mi->last_error().message;
      m_row.last_error_message_length= strlen(temp_store);
      memcpy(m_row.last_error_message, temp_store,
              m_row.last_error_message_length);

      /*
        Time in millisecond since epoch. active_mi->last_error().skr contains
        number of seconds so we multiply by 1000000. */
      m_row.last_error_timestamp= (ulonglong)mi->last_error().skr*1000000;
    }

    mysql_mutex_unlock(&mi->rli->err_lock);
    mysql_mutex_unlock(&mi->err_lock);
    mysql_mutex_unlock(&mi->rli->data_lock);
    mysql_mutex_unlock(&mi->data_lock);

  }
  else if (m_row.is_gcs_plugin_loaded) //if gcs channel
  {
    char gcs_channel[] = "group_replication_applier";
    //Channel
    m_row.channel_name_length= strlen(gcs_channel);
    memcpy(m_row.channel_name, gcs_channel , m_row.channel_name_length);

    if (m_row.is_gcs_plugin_loaded && !m_row.group_name_is_null)
      memcpy(m_row.source_uuid, gcs_replication_group, UUID_LENGTH+1);

    if (gcs_info->node_state)
      m_row.service_state= PS_RPL_CONNECT_SERVICE_STATE_YES;
    else
      m_row.service_state= PS_RPL_CONNECT_SERVICE_STATE_NO;

    m_row.total_messages_received= gcs_info->total_messages_received;
    m_row.total_messages_sent= gcs_info->total_messages_sent;
    m_row.total_bytes_received= gcs_info->total_bytes_received;
    m_row.total_bytes_sent= gcs_info->total_bytes_sent;

    m_row.last_message_timestamp= (ulonglong)
      gcs_info->last_message_timestamp*1000000;

    m_row.max_message_length= gcs_info->max_message_length;

    m_row.min_message_length= gcs_info->min_message_length;

    m_row.view_id_lenght= 0;
    if(gcs_info->view_id != NULL)
    {
      m_row.view_id_lenght= strlen(gcs_info->view_id);
      memcpy(m_row.view_id, gcs_info->view_id, m_row.view_id_lenght);
    }

    m_row.number_of_nodes= gcs_info->number_of_nodes;

    my_free(gcs_info);
  }

  m_row_exists= true;
  DBUG_VOID_RETURN;
}

int table_replication_connection_status::read_row_values(TABLE *table,
                                                         unsigned char *buf,
                                                         Field **fields,
                                                         bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(table->s->null_bytes == 1);
  buf[0]= 0;

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** channel_name*/
        if(m_row.channel_name_length > 0)
          set_field_char_utf8(f, m_row.channel_name,m_row.channel_name_length);
        break;
      case 1: /** group_name */
        if (!m_row.group_name_is_null)
          set_field_varchar_utf8(f, m_row.group_name, UUID_LENGTH);
        else
          f->set_null();
        break;
      case 2: /** source_uuid */
        if (!m_row.source_uuid_is_null)
          set_field_char_utf8(f, m_row.source_uuid, UUID_LENGTH);
        else
          f->set_null();
        break;
      case 3: /** thread_id */
        if (m_row.thread_id_is_null)
          f->set_null();
        else
          set_field_ulonglong(f, m_row.thread_id);
        break;
      case 4: /** service_state */
        set_field_enum(f, m_row.service_state);
        break;
      case 5: /** number of heartbeat events received **/
        if(!m_row.is_gcs_plugin_loaded)
          set_field_ulonglong(f, m_row.count_received_heartbeats);
        else
          f->set_null();
        break;
      case 6: /** time of receipt of last heartbeat event **/
        if(!m_row.is_gcs_plugin_loaded)
          set_field_timestamp(f, m_row.last_heartbeat_timestamp);
        else
          f->set_null();
        break;
      case 7: /** received_transaction_set */
        if(!m_row.is_gcs_plugin_loaded)
          set_field_longtext_utf8(f, m_row.received_transaction_set,
                                  m_row.received_transaction_set_length);
        else
          f->set_null();
        break;
      case 8: /*last_error_number*/
        if(!m_row.is_gcs_plugin_loaded)
          set_field_ulong(f, m_row.last_error_number);
        else
          f->set_null();
        break;
      case 9: /*last_error_message*/
        if(!m_row.is_gcs_plugin_loaded)
          set_field_varchar_utf8(f, m_row.last_error_message,
                                 m_row.last_error_message_length);
        else
          f->set_null();
        break;
      case 10: /*last_error_timestamp*/
        if(!m_row.is_gcs_plugin_loaded)
          set_field_timestamp(f, m_row.last_error_timestamp);
        else
          f->set_null();
        break;
      case 11: /*total_messages_received */
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulonglong(f, m_row.total_messages_received);
        else
          f->set_null();
        break;
      case 12: /*total_messages_sent */
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulonglong(f, m_row.total_messages_sent);
        else
          f->set_null();
        break;
      case 13: /*total_bytes_received */
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulonglong(f, m_row.total_bytes_received);
        else
          f->set_null();
        break;
      case 14: /*total_bytes_sent */
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulonglong(f, m_row.total_bytes_sent);
        else
          f->set_null();
        break;
      case 15: /*last_message_timestamp*/
        if (m_row.is_gcs_plugin_loaded)
           set_field_timestamp(f, m_row.last_message_timestamp);
        else
          f->set_null();
        break;
      case 16: /*max_message_length*/
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulong(f, m_row.max_message_length);
        else
          f->set_null();
        break;
      case 17: /*min_message_length*/
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulong(f, m_row.min_message_length);
        else
          f->set_null();
        break;
      case 18: /*view_id*/
        if (m_row.is_gcs_plugin_loaded && m_row.view_id_lenght > 0)
          set_field_varchar_utf8(f, m_row.view_id,
                                 m_row.view_id_lenght);
        else
          f->set_null();
        break;
      case 19: /*number_of_nodes*/
        if (m_row.is_gcs_plugin_loaded)
          set_field_ulong(f, m_row.number_of_nodes);
        else
          f->set_null();
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  m_row.cleanup();

  return 0;
}
