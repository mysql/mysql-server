/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_session_connect.cc
  TABLE SESSION_CONNECT (abstract).
*/

#include "storage/perfschema/table_session_connect.h"

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "storage/perfschema/pfs_buffer_container.h"

bool
PFS_index_session_connect::match(PFS_thread *pfs)
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
PFS_index_session_connect::match(row_session_connect_attrs *row)
{
  if (m_fields >= 2)
  {
    if (!m_key_2.match(row->m_attr_name, row->m_attr_name_length))
    {
      return false;
    }
  }

  return true;
}

table_session_connect::table_session_connect(
  const PFS_engine_table_share *share)
  : cursor_by_thread_connect_attr(share)
{
  if (session_connect_attrs_size_per_thread > 0)
  {
    m_copy_session_connect_attrs = (char *)my_malloc(
      PSI_INSTRUMENT_ME, session_connect_attrs_size_per_thread, MYF(0));
  }
  else
  {
    m_copy_session_connect_attrs = NULL;
  }
  m_copy_session_connect_attrs_length = 0;
}

table_session_connect::~table_session_connect()
{
  my_free(m_copy_session_connect_attrs);
}

int
table_session_connect::index_init(uint idx MY_ATTRIBUTE((unused)), bool)
{
  DBUG_ASSERT(idx == 0);
  m_opened_index = PFS_NEW(PFS_index_session_connect);
  m_index = m_opened_index;
  return 0;
}

int
table_session_connect::index_next(void)
{
  PFS_thread *thread;
  bool has_more_thread = true;
  int rc = 0;

  for (m_pos.set_at(&m_next_pos); has_more_thread; m_pos.next_thread())
  {
    thread = global_thread_container.get(m_pos.m_index_1, &has_more_thread);
    if (thread != NULL)
    {
      if (m_opened_index->match(thread))
      {
        do
        {
          /*
            Here we materialize the row first,
            and then evaluate if it matches the index.
            This is simpler, as parsing the session attributes encoded string
            is done only once.
          */
          rc = make_row(thread, m_pos.m_index_2);

          if (rc == 0)
          {
            if (m_opened_index->match(&m_row))
            {
              m_next_pos.set_after(&m_pos);
              return 0;
            }
            m_pos.m_index_2++;
          }
        } while (rc == 0);
      }
    }
  }

  return HA_ERR_END_OF_FILE;
}

/**
  Take a length encoded string

  @arg ptr  inout       the input string array
  @arg dest             where to store the result
  @arg dest_size        max size of @c dest
  @arg copied_len       the actual length of the data copied
  @arg start_ptr        pointer to the start of input
  @arg input_length     the length of the incoming data
  @arg from_cs          character set in which @c ptr is encoded
  @arg nchars_max       maximum number of characters to read
  @return status
    @retval true    parsing failed
    @retval false   parsing succeeded
*/
static bool
parse_length_encoded_string(const char **ptr,
                            char *dest,
                            uint dest_size,
                            uint *copied_len,
                            const char *start_ptr,
                            uint input_length,
                            const CHARSET_INFO *from_cs,
                            uint nchars_max)
{
  ulong copy_length, data_length;
  const char *well_formed_error_pos = NULL, *cannot_convert_error_pos = NULL,
             *from_end_pos = NULL;

  copy_length = data_length = net_field_length((uchar **)ptr);

  /* we don't tolerate NULL as a length */
  if (data_length == NULL_LENGTH)
  {
    return true;
  }

  if (*ptr - start_ptr + data_length > input_length)
  {
    return true;
  }

  copy_length = well_formed_copy_nchars(&my_charset_utf8_bin,
                                        dest,
                                        dest_size,
                                        from_cs,
                                        *ptr,
                                        data_length,
                                        nchars_max,
                                        &well_formed_error_pos,
                                        &cannot_convert_error_pos,
                                        &from_end_pos);
  *copied_len = copy_length;
  (*ptr) += data_length;

  return false;
}

/**
  Take the nth attribute name/value pair

  Parse the attributes blob form the beginning, skipping the attributes
  whose number is lower than the one we seek.
  When we reach the attribute at an index we're looking for the values
  are copied to the output parameters.
  If parsing fails or no more attributes are found the function stops
  and returns an error code.

  @param connect_attrs            pointer to the connect attributes blob
  @param connect_attrs_length     length of @c connect_attrs
  @param connect_attrs_cs         character set used to encode @c connect_attrs
  @param ordinal                  index of the attribute we need
  @param [out] attr_name          buffer to receive the attribute name
  @param max_attr_name            max size of @c attr_name in bytes
  @param [out] attr_name_length   number of bytes written in @c attr_name
  @param [out] attr_value         buffer to receive the attribute name
  @param max_attr_value           max size of @c attr_value in bytes
  @param [out] attr_value_length  number of bytes written in @c attr_value
  @return status
    @retval true    requested attribute pair is found and copied
    @retval false   error. Either because of parsing or too few attributes.
*/
bool
read_nth_attr(const char *connect_attrs,
              uint connect_attrs_length,
              const CHARSET_INFO *connect_attrs_cs,
              uint ordinal,
              char *attr_name,
              uint max_attr_name,
              uint *attr_name_length,
              char *attr_value,
              uint max_attr_value,
              uint *attr_value_length)
{
  uint idx;
  const char *ptr;

  for (ptr = connect_attrs, idx = 0;
       (uint)(ptr - connect_attrs) < connect_attrs_length && idx <= ordinal;
       idx++)
  {
    uint copy_length;

    /* read the key */
    if (parse_length_encoded_string(&ptr,
                                    attr_name,
                                    max_attr_name,
                                    &copy_length,
                                    connect_attrs,
                                    connect_attrs_length,
                                    connect_attrs_cs,
                                    32) ||
        !copy_length)
    {
      return false;
    }

    if (idx == ordinal)
    {
      *attr_name_length = copy_length;
    }

    /* read the value */
    if (parse_length_encoded_string(&ptr,
                                    attr_value,
                                    max_attr_value,
                                    &copy_length,
                                    connect_attrs,
                                    connect_attrs_length,
                                    connect_attrs_cs,
                                    1024))
    {
      return false;
    }

    if (idx == ordinal)
    {
      *attr_value_length = copy_length;
    }

    if (idx == ordinal)
    {
      return true;
    }
  }

  return false;
}

int
table_session_connect::make_row(PFS_thread *pfs, uint ordinal)
{
  pfs_optimistic_state lock;
  pfs_optimistic_state session_lock;
  PFS_thread_class *safe_class;
  const CHARSET_INFO *cs;

  /* Protect this reader against thread termination */
  pfs->m_lock.begin_optimistic_lock(&lock);
  /* Protect this reader against writing on session attributes */
  pfs->m_session_lock.begin_optimistic_lock(&session_lock);

  safe_class = sanitize_thread_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
  {
    return HA_ERR_RECORD_DELETED;
  }

  /* Filtering threads must be done under the protection of the optimistic lock.
   */
  if (!thread_fits(pfs))
  {
    return HA_ERR_RECORD_DELETED;
  }

  /* Make a safe copy of the session attributes */

  if (m_copy_session_connect_attrs == NULL)
  {
    return HA_ERR_RECORD_DELETED;
  }

  m_copy_session_connect_attrs_length = pfs->m_session_connect_attrs_length;

  if (m_copy_session_connect_attrs_length >
      session_connect_attrs_size_per_thread)
  {
    return HA_ERR_RECORD_DELETED;
  }

  memcpy(m_copy_session_connect_attrs,
         pfs->m_session_connect_attrs,
         m_copy_session_connect_attrs_length);

  cs = get_charset(pfs->m_session_connect_attrs_cs_number, MYF(0));
  if (cs == NULL)
  {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs->m_session_lock.end_optimistic_lock(&session_lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock))
  {
    return HA_ERR_RECORD_DELETED;
  }

  /*
    Now we have a safe copy of the data,
    that will not change while parsing it
  */

  /* populate the row */
  if (read_nth_attr(m_copy_session_connect_attrs,
                    m_copy_session_connect_attrs_length,
                    cs,
                    ordinal,
                    m_row.m_attr_name,
                    (uint)sizeof(m_row.m_attr_name),
                    &m_row.m_attr_name_length,
                    m_row.m_attr_value,
                    (uint)sizeof(m_row.m_attr_value),
                    &m_row.m_attr_value_length))
  {
    /* we don't expect internal threads to have connection attributes */
    if (pfs->m_processlist_id == 0)
    {
      return HA_ERR_RECORD_DELETED;
    }

    m_row.m_ordinal_position = ordinal;
    m_row.m_process_id = pfs->m_processlist_id;

    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

int
table_session_connect::read_row_values(TABLE *table,
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
      case FO_PROCESS_ID:
        if (m_row.m_process_id != 0)
        {
          set_field_ulonglong(f, m_row.m_process_id);
        }
        else
        {
          f->set_null();
        }
        break;
      case FO_ATTR_NAME:
        set_field_varchar_utf8(f, m_row.m_attr_name, m_row.m_attr_name_length);
        break;
      case FO_ATTR_VALUE:
        if (m_row.m_attr_value_length)
          set_field_varchar_utf8(
            f, m_row.m_attr_value, m_row.m_attr_value_length);
        else
        {
          f->set_null();
        }
        break;
      case FO_ORDINAL_POSITION:
        set_field_ulong(f, m_row.m_ordinal_position);
        break;
      default:
        DBUG_ASSERT(false);
      }
    }
  }
  return 0;
}

bool
table_session_connect::thread_fits(PFS_thread *)
{
  return true;
}
