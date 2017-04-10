/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/raw/raw_record_set.h"

#include <sys/types.h>

#include "dd/impl/raw/raw_key.h"            // dd::Raw_key
#include "handler.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "table.h"                          // TABLE

namespace dd {

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    Initializes the table scan operation.
    If there is no key supplied, then we do a sorted index full scan.

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Raw_record_set::open()
{
  DBUG_ENTER("Raw_record_set::open");
  uint index_no= 0;

  // Use specific index if key submitted.
  if (m_key)
    index_no= m_key->index_no;

  int rc=m_table->file->ha_index_init(index_no, true);

  if (rc)
  {
    m_table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  if (m_key)
    rc= m_table->file->ha_index_read_idx_map(m_table->record[0],
                                             m_key->index_no,
                                             m_key->key,
                                             m_key->keypart_map,
                                             HA_READ_KEY_EXACT);
  else
    rc= m_table->file->ha_index_first(m_table->record[0]);

  // Row not found.
  if (rc == HA_ERR_KEY_NOT_FOUND || rc == HA_ERR_END_OF_FILE)
  {
    DBUG_ASSERT(!m_current_record);
    DBUG_RETURN(false);
  }

  // Got unexpected error.
  if (rc)
  {
    m_table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  m_current_record= this;

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
  Move to next record in DD table that matches the supplied key.
  If there is no key supplied, then we do a sorted index full scan.

  @param r - Pointer to Raw_record after moving to next row.


  @return false - On success. 1) We found a row.
                              2) OR Either we don't have any matching rows
  @return true - On failure and my_error() is invoked.
*/
bool Raw_record_set::next(Raw_record *&r)
{
  DBUG_ENTER("Raw_record_set::next");
  int rc;

  if (!m_current_record)
  {
    m_current_record= NULL;
    r= NULL;
    DBUG_RETURN(false);
  }

  if (m_key)
    rc= m_table->file->ha_index_next_same(m_table->record[0],
                                          m_key->key,
                                          m_key->key_len);
  else
    rc= m_table->file->ha_index_next(m_table->record[0]);

  // Row not found.
  if (rc == HA_ERR_KEY_NOT_FOUND || rc == HA_ERR_END_OF_FILE)
  {
    m_current_record= NULL;
    r= NULL;
    DBUG_RETURN(false);
  }

  // Got unexpected error.
  if (rc)
  {
    m_table->file->print_error(rc, MYF(0));
    m_current_record= NULL;
    r= NULL;
    DBUG_RETURN(true);
  }

  m_current_record= this;
  r= this;

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

Raw_record_set::~Raw_record_set()
{
  if (m_table->file->inited != handler::NONE)
  {
    int rc= m_table->file->ha_index_end();

    if (rc)
    {
      /* purecov: begin inspected */
      m_table->file->print_error(rc, MYF(ME_ERRORLOG));
      DBUG_ASSERT(false);
      /* purecov: end */
    }
  }

  delete m_key;
}

///////////////////////////////////////////////////////////////////////////

}
