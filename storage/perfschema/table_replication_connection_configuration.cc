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

/* 
   Number of bytes for buffers holding the values for user and host.
*/
#define HOST_MAX_LEN  HOSTNAME_LENGTH * SYSTEM_CHARSET_MBMAXLEN
#define USER_MAX_LEN  USERNAME_CHAR_LENGTH * SYSTEM_CHARSET_MBMAXLEN

//TODO : consider replacing with std::max
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

static ST_STATUS_FIELD_INFO slave_field_info[]=
{
  {"Host", HOST_MAX_LEN, MYSQL_TYPE_STRING, FALSE},
  {"Port", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"User", USER_MAX_LEN, MYSQL_TYPE_STRING, FALSE},
  {"Network_Interface", 60, MYSQL_TYPE_STRING, FALSE},
  {"Auto_Position", 1, MYSQL_TYPE_LONG, FALSE},
  {"SSL_Allowed",  sizeof(ulonglong), MYSQL_TYPE_ENUM, FALSE},
  {"SSL_CA_File", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_CA_Path", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_Certificate", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_Cipher", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_Key", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_Verify_Server_Certificate", sizeof(ulonglong), MYSQL_TYPE_ENUM, FALSE},
  {"SSL_Crl_File", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"SSL_Crl_Path", FN_REFLEN, MYSQL_TYPE_STRING, FALSE},
  {"Connect_Retry_Interval", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
  {"Connection_Retry_Count", sizeof(ulonglong), MYSQL_TYPE_LONG, FALSE},
};

table_replication_connection_configuration::table_replication_connection_configuration()
  : PFS_engine_table(&m_share, &m_pos),
    m_filled(false), m_pos(0), m_next_pos(0)
{
  for (int i= HOST; i <= _RPL_CONNECT_CONFIG_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING)
      m_fields[i].u.s.str= NULL;  // str_store() makes allocation
    if (slave_field_info[i].can_be_null)
      m_fields[i].is_null= false;
  }
}

table_replication_connection_configuration::~table_replication_connection_configuration()
{
  for (int i= HOST; i <= _RPL_CONNECT_CONFIG_LAST_FIELD_; i++)
  {
    if (slave_field_info[i].type == MYSQL_TYPE_STRING &&
        m_fields[i].u.s.str != NULL)
      my_free(m_fields[i].u.s.str);
  }
}

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

void table_replication_connection_configuration::drop_null(enum enum_rpl_connect_config_field_names name)
{
  if (slave_field_info[name].can_be_null)
    m_fields[name].is_null= false;
}

void table_replication_connection_configuration::set_null(enum enum_rpl_connect_config_field_names name)
{
  DBUG_ASSERT(slave_field_info[name].can_be_null);
  m_fields[name].is_null= true;
}

void table_replication_connection_configuration::str_store(enum enum_rpl_connect_config_field_names name, const char* val)
{
  m_fields[name].u.s.length= strlen(val);
  DBUG_ASSERT(m_fields[name].u.s.length <= slave_field_info[name].max_size);
  if (m_fields[name].u.s.str == NULL)
    m_fields[name].u.s.str= (char *) my_malloc(m_fields[name].u.s.length, MYF(0));

  /*
    \0 may be stripped off since there is no need for \0-termination of
    m_fields[name].u.s.str
  */
  memcpy(m_fields[name].u.s.str, val, m_fields[name].u.s.length);
  m_fields[name].u.s.length= m_fields[name].u.s.length;

  drop_null(name);
}

void table_replication_connection_configuration::int_store(enum enum_rpl_connect_config_field_names name, longlong val)
{
  m_fields[name].u.n= val;
  drop_null(name);
}

void table_replication_connection_configuration::fill_rows(Master_info *mi)
{
  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);
  
  str_store(HOST, mi->host);
  int_store(PORT, (long int) mi->port);
  str_store(USER, mi->get_user());
  str_store(NETWORK_INTERFACE, mi->bind_addr);
  int_store(AUTO_POSITION, mi->is_auto_position() ? 1 : 0);
#ifdef HAVE_OPENSSL
  enum_store(SSL_ALLOWED, mi->ssl ? 
             PS_SSL_ALLOWED_YES : PS_SSL_ALLOWED_NO);
#else
  enum_store(SSL_ALLOWED, mi->ssl ?
            PS_SSL_ALLOWED_IGNORED : PS_SSL_ALLOWED_NO);
#endif
  str_store(SSL_CA_FILE, mi->ssl_ca);
  str_store(SSL_CA_PATH, mi->ssl_capath);
  str_store(SSL_CERTIFICATE, mi->ssl_cert);
  str_store(SSL_CIPHER, mi->ssl_cipher);
  str_store(SSL_KEY, mi->ssl_key);
  
  enum_store(SSL_VERIFY_SERVER_CERTIFICATE, mi->ssl_verify_server_cert ?
             PS_RPL_YES : PS_RPL_NO);
  str_store(SSL_CRL_FILE, mi->ssl_ca);
  str_store(SSL_CRL_PATH, mi->ssl_capath);
  int_store(CONNECTION_RETRY_INTERVAL, (long int) mi->connect_retry);
  int_store(CONNECTION_RETRY_COUNT, (ulonglong) mi->retry_count);

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
      if (slave_field_info[f->field_index].can_be_null)
      {
        if (m_fields[f->field_index].is_null)
        {
          f->set_null();
          continue;
        }
        else
          f->set_notnull();
      }

      switch(f->field_index)
      {
      case HOST:
      case USER:
      case NETWORK_INTERFACE:
      case SSL_CA_FILE:
      case SSL_CA_PATH:
      case SSL_CERTIFICATE:
      case SSL_CIPHER:
      case SSL_KEY:
      case SSL_CRL_FILE:
      case SSL_CRL_PATH:       

        set_field_varchar_utf8(f,
                               m_fields[f->field_index].u.s.str,
                               m_fields[f->field_index].u.s.length);
        break;

      case PORT:
      case CONNECTION_RETRY_COUNT:
      case CONNECTION_RETRY_INTERVAL:
      case AUTO_POSITION:

        set_field_ulonglong(f, m_fields[f->field_index].u.n);
        break;

      case SSL_ALLOWED:
      case SSL_VERIFY_SERVER_CERTIFICATE:

        set_field_enum(f, m_fields[f->field_index].u.n);
        break;

      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}
