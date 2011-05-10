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

/**
  @file storage/perfschema/pfs_digest.h
  Statement Digest data structures (implementation).
*/

#include "pfs_digest.h"

/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_statements_digest_stat *statements_digest_stat_array= NULL;


/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param digest_sizing      
*/
int init_digest(unsigned int digest_sizing)
{
  printf("\n Initializing performance_schema_digests with\
         digest_sizing=%d\n",digest_sizing);

  /* 
    TBD. Allocate memory for statements_digest_stat_array based on 
    performance_schema_digests_size values
  */
  return 0;
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
int cleanup_digest(void)
{
  printf("\n Yes, cleaning up performance_schema_digests\n");
  /* 
    TBD. Free memory allocated to statements_digest_stat_array. 
  */
  return 0;
}
