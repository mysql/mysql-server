/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_DIGEST_H
#define PFS_DIGEST_H

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (declarations).
*/

#include "pfs_column_types.h"
#include "my_global.h"
#include "lf.h"
#include "pfs_instr.h"

extern bool flag_statements_digest;

struct PFS_digest_key
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "<digest><0x00>"
  */
  char m_hash_key[COL_DIGEST_SIZE + 1];
  unsigned int m_key_length;
};



/** A statement digest stat record. */
struct PFS_statements_digest_stat
{
  char m_digest[COL_DIGEST_SIZE];
  unsigned int m_digest_length;
  char m_digest_text[COL_DIGEST_TEXT_SIZE];
  unsigned int m_digest_text_length;
  
  PFS_digest_key m_key;
};



int init_digest(unsigned int digest_sizing);
void cleanup_digest();

int init_digest_hash(void);
void cleanup_digest_hash(void);
PFS_statements_digest_stat* search_insert_statement_digest(PFS_thread*,
                                                           char*, char*);


void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

#endif
