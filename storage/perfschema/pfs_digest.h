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

/** A statement digest stat record. */
struct PFS_statements_digest_stat
{
  char m_digest[COL_DIGEST_SIZE];
  unsigned int m_digest_length;
  char m_digest_text[COL_DIGEST_TEXT_SIZE];
  unsigned int m_digest_text_length;
  
  /**
    Digest hash/LF Hash search key.
  */
  PFS_digest_hash m_md5_hash;
};



int init_digest(unsigned int digest_sizing);
void cleanup_digest();

int init_digest_hash(void);
void cleanup_digest_hash(void);
PFS_statements_digest_stat* search_insert_statement_digest(PFS_thread*,
                                                           unsigned char*, char*,
                                                           unsigned int);


void reset_esms_by_digest();

/* Exposing the data directly, for iterators. */
extern PFS_statements_digest_stat *statements_digest_stat_array;

#endif
