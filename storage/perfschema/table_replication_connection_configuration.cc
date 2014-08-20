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
  @file storage/perfschema/table_replication_connection_configuration.cc
  Table replication_connection_configuration (implementation).
*/

#define HAVE_REPLICATION

#include "my_global.h"
#include "sql_priv.h"
#include "table_replication_connection_configuration.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "rpl_slave.h"
#include "rpl_info.h"
#include "rpl_rli.h"
#include "rpl_mi.h"
#include "sql_parse.h"

THR_LOCK table_replication_connection_configuration::m_table_lock;

/* Numbers in varchar count utf8 characters. */
static const TABLE_FIELD_TYPE field_types[]=
{
  {
    {C_STRING_WITH_LEN("HOST")},
    {C_STRING_WITH_LEN("char(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("PORT")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("USER")},
    {C_STRING_WITH_LEN("char(16)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("NETWORK_INTERFACE")},
    {C_STRING_WITH_LEN("char(60)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("AUTO_POSITION")},
    {C_STRING_WITH_LEN("enum('1','0')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_ALLOWED")},
    {C_STRING_WITH_LEN("enum('YES','NO','IGNORED')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CA_FILE")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CA_PATH")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CERTIFICATE")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CIPHER")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_KEY")},
    {C_STRING_WITH_LEN("varchar(512)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_VERIFY_SERVER_CERTIFICATE")},
    {C_STRING_WITH_LEN("enum('YES','NO')")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CRL_FILE")},
    {C_STRING_WITH_LEN("varchar(255)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("SSL_CRL_PATH")},
    {C_STRING_WITH_LEN("varchar(255)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("CONNECTION_RETRY_INTERVAL")},
    {C_STRING_WITH_LEN("int(11)")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("CONNECTION_RETRY_COUNT")},
    {C_STRING_WITH_LEN("bigint")},
    {NULL, 0}
  },
  {
    {C_STRING_WITH_LEN("HEARTBEAT_INTERVAL")},
    {C_STRING_WITH_LEN("double(10,3)")},
    {NULL, 0}
   }
};

TABLE_FIELD_DEF
table_replication_connection_configuration::m_field_def=
{ 17, field_types };

PFS_engine_table_share
table_replication_connection_configuration::m_share=
{
  { C_STRING_WITH_LEN("replication_connection_configuration") },
  &pfs_readonly_acl,
  table_replication_connection_configuration::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  table_replication_connection_configuration::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false /* checked */
};


PFS_engine_table* table_replication_connection_configuration::create(void)
{
  return new table_replication_connection_configuration();
}

table_replication_connection_configuration
  ::table_replication_connection_configuration()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(0), m_next_pos(0)
{}

table_replication_connection_configuration
  ::~table_replication_connection_configuration()
{}

void table_replication_connection_configuration::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

ha_rows table_replication_connection_configuration::get_row_count()
{
  uint row_count= 0;

  mysql_mutex_lock(&LOCK_active_mi);

  if (active_mi && active_mi->host[0])
    row_count= 1;

  mysql_mutex_unlock(&LOCK_active_mi);

  return row_count;
}

int table_replication_connection_configuration::rnd_next(void)
{
  if(get_row_count() == 0)
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

int table_replication_connection_configuration::rnd_pos(const void *pos)
{
  if(get_row_count() == 0)
    return HA_ERR_END_OF_FILE;

  set_position(pos);

  DBUG_ASSERT(m_pos.m_index < 1);

  make_row();

  return 0;
}

void table_replication_connection_configuration::make_row()
{
  char * temp_store;

  m_row_exists= false;

  mysql_mutex_lock(&LOCK_active_mi);

  DBUG_ASSERT(active_mi != NULL);

  mysql_mutex_lock(&active_mi->data_lock);
  mysql_mutex_lock(&active_mi->rli->data_lock);

  m_row.host_length= strlen(active_mi->host);
  memcpy(m_row.host, active_mi->host, m_row.host_length);

  m_row.port= (unsigned int) active_mi->port;

  temp_store= (char*)active_mi->get_user();
  m_row.user_length= strlen(temp_store);
  memcpy(m_row.user, temp_store, m_row.user_length);

  temp_store= (char*)active_mi->bind_addr;
  m_row.network_interface_length= strlen(temp_store);
  memcpy(m_row.network_interface, temp_store, m_row.network_interface_length);

  if (active_mi->is_auto_position())
    m_row.auto_position= PS_RPL_YES;
  else
    m_row.auto_position= PS_RPL_NO;

#ifdef HAVE_OPENSSL
  m_row.ssl_allowed= active_mi->ssl? PS_SSL_ALLOWED_YES:PS_SSL_ALLOWED_NO;
#else
  m_row.ssl_allowed= active_mi->ssl? PS_SSL_ALLOWED_IGNORED:PS_SSL_ALLOWED_NO;
#endif

  temp_store= (char*)active_mi->ssl_ca;
  m_row.ssl_ca_file_length= strlen(temp_store);
  memcpy(m_row.ssl_ca_file, temp_store, m_row.ssl_ca_file_length);

  temp_store= (char*)active_mi->ssl_capath;
  m_row.ssl_ca_path_length= strlen(temp_store);
  memcpy(m_row.ssl_ca_path, temp_store, m_row.ssl_ca_path_length);

  temp_store= (char*)active_mi->ssl_cert;
  m_row.ssl_certificate_length= strlen(temp_store);
  memcpy(m_row.ssl_certificate, temp_store, m_row.ssl_certificate_length);

  temp_store= (char*)active_mi->ssl_cipher;
  m_row.ssl_cipher_length= strlen(temp_store);
  memcpy(m_row.ssl_cipher, temp_store, m_row.ssl_cipher_length);

  temp_store= (char*)active_mi->ssl_key;
  m_row.ssl_key_length= strlen(temp_store);
  memcpy(m_row.ssl_key, temp_store, m_row.ssl_key_length);

  if (active_mi->ssl_verify_server_cert)
    m_row.ssl_verify_server_certificate= PS_RPL_YES;
  else
    m_row.ssl_verify_server_certificate= PS_RPL_NO;

  temp_store= (char*)active_mi->ssl_crl;
  m_row.ssl_crl_file_length= strlen(temp_store);
  memcpy(m_row.ssl_crl_file, temp_store, m_row.ssl_crl_file_length);

  temp_store= (char*)active_mi->ssl_crlpath;
  m_row.ssl_crl_path_length= strlen(temp_store);
  memcpy(m_row.ssl_crl_path, temp_store, m_row.ssl_crl_path_length);

  m_row.connection_retry_interval= (unsigned int) active_mi->connect_retry;

  m_row.connection_retry_count= (ulong) active_mi->retry_count;

  m_row.heartbeat_interval= (double)active_mi->heartbeat_period;

  mysql_mutex_unlock(&active_mi->rli->data_lock);
  mysql_mutex_unlock(&active_mi->data_lock);
  mysql_mutex_unlock(&LOCK_active_mi);

  m_row_exists= true;
}

int table_replication_connection_configuration::read_row_values(TABLE *table,
                                                                unsigned char *,
                                                                Field **fields,
                                                                bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  DBUG_ASSERT(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /** host */
        set_field_char_utf8(f, m_row.host, m_row.host_length);
        break;
      case 1: /** port */
        set_field_ulong(f, m_row.port);
        break;
      case 2: /** user */
        set_field_char_utf8(f, m_row.user, m_row.user_length);
        break;
      case 3: /** network_interface */
        set_field_char_utf8(f, m_row.network_interface,
                               m_row.network_interface_length);
        break;
      case 4: /** auto_position */
        set_field_enum(f, m_row.auto_position);
        break;
      case 5: /** ssl_allowed */
        set_field_enum(f, m_row. ssl_allowed);
        break;
      case 6: /**ssl_ca_file */
        set_field_varchar_utf8(f, m_row.ssl_ca_file,
                               m_row.ssl_ca_file_length);
        break;
      case 7: /** ssl_ca_path */
        set_field_varchar_utf8(f, m_row.ssl_ca_path,
                               m_row.ssl_ca_path_length);
        break;
      case 8: /** ssl_certificate */
        set_field_varchar_utf8(f, m_row.ssl_certificate,
                               m_row.ssl_certificate_length);
        break;
      case 9: /** ssl_cipher */
        set_field_varchar_utf8(f, m_row.ssl_cipher, m_row.ssl_cipher_length);
        break;
      case 10: /** ssl_key */
        set_field_varchar_utf8(f, m_row.ssl_key, m_row.ssl_key_length);
        break;
      case 11: /** ssl_verify_server_certificate */
        set_field_enum(f, m_row.ssl_verify_server_certificate);
        break;
      case 12: /** ssl_crl_file */
        set_field_varchar_utf8(f, m_row.ssl_crl_file,
                               m_row.ssl_crl_file_length);
        break;
      case 13: /** ssl_crl_path */
        set_field_varchar_utf8(f, m_row.ssl_crl_path,
                               m_row.ssl_crl_path_length);
        break;
      case 14: /** connection_retry_interval */
        set_field_ulong(f, m_row.connection_retry_interval);
        break;
      case 15: /** connect_retry_count */
        set_field_ulonglong(f, m_row.connection_retry_count);
        break;
      case 16:/** number of seconds after which heartbeat will be sent */
        set_field_double(f, m_row.heartbeat_interval);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
