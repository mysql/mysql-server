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

extern bool flag_statements_digest;

/** A statement stat record based on digest. */
struct PFS_statements_digest_stat
{
  /* TBD */
  char digest[COL_DIGEST_SIZE];
  char digest_text[COL_DIGEST_TEXT_SIZE];
};

void insert_statement_digest(char*,char*);

int init_digest(unsigned int digest_sizing);
void cleanup_digest();

void reset_esms_by_digest();

#endif
