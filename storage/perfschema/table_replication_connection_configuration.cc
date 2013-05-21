/*
      Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
   
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
  @file storage/perfschema/table_replication_connection_configuration.cc
  Table replication_connection_configuration (implementation).
*/

#include "sql_priv.h"
#include "table_replication_connection_configuration.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include  "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_connection_configuration::m_table_lock;

#define max(x, y) ((x) > (y) ? (x) : (y))

/*
  Numbers in varchar count utf8 characters.
*/
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("Host")},
    {C_STRING_WITH_LEN("varchar(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Port")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("User")},
    {C_STRING_WITH_LEN("varchar(16)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Network_Interface")},
    {C_STRING_WITH_LEN("varchar(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Auto_Position")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Allowed")},
    {C_STRING_WITH_LEN("enum('Yes','No','Ignored')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CA_File")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CA_Path")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Certificate")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Cipher")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Key")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Verify_Server_Certificate")},
    {C_STRING_WITH_LEN("enum('Yes','No')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Crl_File")},
    {C_STRING_WITH_LEN("varchar(255)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_Crl_Path")},
    {C_STRING_WITH_LEN("varchar(255)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Connection_Retry_Interval")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("Connection_Retry_Count")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  }
};

TABLE_FIELD_DEF
table_replication_connection_configuration::m_field_def=
{ 16, field_types };

PFS_engine_table_share
table_replication_connection_configuration::m_share=
{
  { C_STRING_WITH_LEN("replication_connection_configuration") },
  &pfs_readonly_acl,
  &table_replication_connection_configuration::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  NULL,    
  1,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};


PFS_engine_table* table_replication_connection_configuration::create(void)
{
  return new table_replication_connection_configuration();
}

table_replication_connection_configuration::table_replication_connection_configuration()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{}

table_replication_connection_configuration::~table_replication_connection_configuration()
{}

void table_replication_connection_configuration::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int table_replication_connection_configuration::rnd_next(void)
{
  Master_info *mi= active_mi;

  if (!m_filled)
  {
    if (mi->host[0])
      fill_rows(active_mi);
    else
      return HA_ERR_END_OF_FILE;
  }

  m_pos.set_at(&m_next_pos);
  m_next_pos.set_after(&m_pos);
  if (m_pos.m_index == m_share.m_records)
    return HA_ERR_END_OF_FILE;

  return 0;
}

int table_replication_connection_configuration::rnd_pos(const void *pos)
{
  Master_info *mi= active_mi;
  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < m_share.m_records);

  if (!m_filled)
    fill_rows(mi);
  return 0;
}

void table_replication_connection_configuration::fill_rows(Master_info *mi)
{
  char * temp_store;
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  
  m_row.Host_length= strlen(mi->host) + 1;
  memcpy(m_row.Host, mi->host, m_row.Host_length);

  m_row.Port= (unsigned int) mi->port;

  temp_store= (char*)mi->get_user();
  m_row.User_length= strlen(temp_store) + 1;
  memcpy(m_row.User, temp_store, m_row.User_length);

  temp_store= (char*)mi->bind_addr;
  m_row.Network_Interface_length= strlen(temp_store) + 1;
  memcpy(m_row.Network_Interface, temp_store, m_row.Network_Interface_length);

  if (mi->is_auto_position())
    m_row.Auto_Position= 1;
  else
    m_row.Auto_Position= 0;

#ifdef HAVE_OPENSSL
  if (mi->ssl)
    m_row.SSL_Allowed= PS_SSL_ALLOWED_YES
  else
    m_row.SSL_Allowed= PS_SSL_ALLOWED_NO;
#else
  if (mi->ssl)
    m_row.SSL_Allowed= PS_SSL_ALLOWED_IGNORED;
  else 
    m_row.SSL_Allowed= PS_SSL_ALLOWED_NO;
#endif

  temp_store= (char*)mi->ssl_ca;
  m_row.SSL_CA_File_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_CA_File, temp_store, m_row.SSL_CA_File_length);

  temp_store= (char*)mi->ssl_capath;
  m_row.SSL_CA_Path_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_CA_Path, temp_store, m_row.SSL_CA_Path_length);

  temp_store= (char*)mi->ssl_cert;
  m_row.SSL_Certificate_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_Certificate, temp_store, m_row.SSL_Certificate_length);

  temp_store= (char*)mi->ssl_cipher;
  m_row.SSL_Cipher_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_Cipher, temp_store, m_row.SSL_Cipher_length);

  temp_store= (char*)mi->ssl_key;
  m_row.SSL_Key_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_Key, temp_store, m_row.SSL_Key_length);
  
  if (mi->ssl_verify_server_cert)
    m_row.SSL_Verify_Server_Certificate= PS_RPL_YES;
  else
    m_row.SSL_Verify_Server_Certificate= PS_RPL_NO;

  temp_store= (char*)mi->ssl_ca;
  m_row.SSL_Crl_File_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_Crl_File, temp_store, m_row.SSL_Crl_File_length);

  temp_store= (char*)mi->ssl_capath;
  m_row.SSL_Crl_Path_length= strlen(temp_store) + 1;
  memcpy(m_row.SSL_Crl_Path, m_row.SSL_Crl_Path, m_row.SSL_Crl_Path_length);

  m_row.Connection_Retry_Interval= (unsigned int) mi->connect_retry;  

  m_row.Connection_Retry_Count= (ulong) mi->retry_count;  

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);
  
  m_filled= true;
}


int table_replication_connection_configuration::read_row_values(TABLE *table,
                                                                    unsigned char *,
                                                                    Field **fields,
                                                                    bool read_all)
{
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** Host */
        set_field_varchar_utf8(f, m_row.Host, m_row.Host_length);
        break;
      case 1: /** Port */
        set_field_ulong(f, m_row.Port);
        break;
      case 2: /** User */
        set_field_varchar_utf8(f, m_row.User, m_row.User_length);
        break;
      case 3: /** Network_Interface */
        set_field_varchar_utf8(f, m_row.Network_Interface,
                               m_row.Network_Interface_length);
        break;
      case 4: /** Auto_Position */
        set_field_ulong(f, m_row.Auto_Position);
        break;
      case 5: /** SSL_Allowed */
        set_field_enum(f, m_row. SSL_Allowed);
        break;
      case 6: /**SSL_CA_File */
        set_field_varchar_utf8(f, m_row.SSL_CA_File,
                               m_row.SSL_CA_File_length);
        break;
      case 7: /** SSL_CA_Path */
        set_field_varchar_utf8(f, m_row.SSL_CA_Path,
                               m_row.SSL_CA_Path_length);
        break;
      case 8: /** SSL_Certificate */
        set_field_varchar_utf8(f, m_row.SSL_Certificate,
                                m_row.SSL_Certificate_length);
        break;
      case 9: /** SSL_Cipher */
        set_field_varchar_utf8(f, m_row.SSL_Cipher, m_row.SSL_Cipher_length);
        break;
      case 10: /** SSL_Key */
        set_field_varchar_utf8(f, m_row.SSL_Key, m_row.SSL_Key_length);
        break;
      case 11: /** SSL_Verify_Server_Certificate */
        set_field_enum(f, m_row.SSL_Verify_Server_Certificate);
        break;
      case 12: /** SSL_Crl_File */
        set_field_varchar_utf8(f, m_row.SSL_Crl_File,
                               m_row.SSL_Crl_File_length);
        break;
      case 13: /** SSL_Crl_Path */
        set_field_varchar_utf8(f, m_row.SSL_Crl_Path,
                               m_row.SSL_Crl_Path_length);
        break;
      case 14: /** Connection_Retry_Interval */
        set_field_ulong(f, m_row.Connection_Retry_Interval);
        break;
      case 15: /** Connect_Retry_Count */
        set_field_ulong(f, m_row.Connection_Retry_Count);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
