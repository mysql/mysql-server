/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_DIGEST_STREAM_H
#define SQL_DIGEST_STREAM_H

#include "sql_digest.h"

/**
  State data storage for @c digest_start, @c digest_add_token.
  This structure extends the @c sql_digest_storage structure
  with temporary state used only during parsing.
*/
struct sql_digest_state
{
  /**
    Index, in the digest token array, of the last identifier seen.
    Reduce rules used in the digest computation can not
    apply to tokens seen before an identifier.
    @sa digest_add_token
  */
  int m_last_id_index;
  sql_digest_storage m_digest_storage;

  inline void reset(unsigned char *token_array, uint length)
  {
    m_last_id_index= 0;
    m_digest_storage.reset(token_array, length);
  }

  inline bool is_empty()
  {
    return m_digest_storage.is_empty();
  }
};
typedef struct sql_digest_state sql_digest_state;

#endif

