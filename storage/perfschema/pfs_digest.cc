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
#include "my_sys.h"
#include "pfs_global.h"
#include <string.h>

unsigned int statements_digest_size= 0;
/** EVENTS_STATEMENTS_HISTORY_LONG circular buffer. */
PFS_statements_digest_stat *statements_digest_stat_array= NULL;
/** Consumer flag for table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
bool flag_statements_digest= true;
/** Current index in Stat array where new record is to be inserted. */
int digest_index= 0;

/**
  Initialize table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST.
  @param digest_sizing      
*/
int init_digest(unsigned int statements_digest_sizing)
{
  /* 
    TBD. Allocate memory for statements_digest_stat_array based on 
    performance_schema_digests_size values
  */
  statements_digest_size= statements_digest_sizing;
 
  if (statements_digest_size == 0)
    return 0;

  statements_digest_stat_array=
    PFS_MALLOC_ARRAY(statements_digest_size, PFS_statements_digest_stat,
                     MYF(MY_ZEROFILL));

  return (statements_digest_stat_array ? 0 : 1);
}

/** Cleanup table EVENTS_STATEMENTS_SUMMARY_BY_DIGEST. */
void cleanup_digest(void)
{
  /* 
    TBD. Free memory allocated to statements_digest_stat_array. 
  */
  pfs_free(statements_digest_stat_array);
  statements_digest_stat_array= NULL;
}

void insert_statement_digest(char* digest, char* digest_text)
{
  /* Lookup LF_HASH for the computed DIGEST. */
  //TODO
  /* If stmt digest already exists, update stat and return */
  //TODO
  
  /* 
     If statement digest doesn't exist, add a new record in the 
     digest stat array. 
  */
  memcpy(statements_digest_stat_array[digest_index].m_digest, digest,
         COL_DIGEST_SIZE);
  statements_digest_stat_array[digest_index].m_digest_length=
         strlen(digest);

  memcpy(statements_digest_stat_array[digest_index].m_digest_text, digest_text,
         COL_DIGEST_TEXT_SIZE);
  statements_digest_stat_array[digest_index].m_digest_text_length=
         strlen(digest_text);

  /* Rounding Buffer. Overwrite first entry if all slots are full. */
  digest_index= (digest_index+1)%statements_digest_size;
}
 
void reset_esms_by_digest()
{
  /*TBD*/ 
}

