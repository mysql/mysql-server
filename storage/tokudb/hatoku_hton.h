/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"

extern handlerton *tokudb_hton;

extern DB_ENV *db_env;
extern DB *metadata_db;

enum srv_row_format_enum {
    SRV_ROW_FORMAT_UNCOMPRESSED = 0,
    SRV_ROW_FORMAT_ZLIB = 1,
    SRV_ROW_FORMAT_QUICKLZ = 2,
    SRV_ROW_FORMAT_LZMA = 3,
    SRV_ROW_FORMAT_FAST = 4,
    SRV_ROW_FORMAT_SMALL = 5,
    SRV_ROW_FORMAT_DEFAULT = 6
};
typedef enum srv_row_format_enum srv_row_format_t;

// thread variables

static MYSQL_THDVAR_BOOL(commit_sync, 
    PLUGIN_VAR_THDLOCAL, 
    "sync on txn commit",
    /* check */ NULL, 
    /* update */ NULL,
    /* default*/ true
);

static MYSQL_THDVAR_UINT(pk_insert_mode,
    0,
    "set the primary key insert mode",
    NULL, 
    NULL, 
    1, // default
    0, // min?
    2, // max
    1 // blocksize
);

static uint get_pk_insert_mode(THD* thd) {
    return THDVAR(thd, pk_insert_mode);
}

static MYSQL_THDVAR_BOOL(load_save_space,
    0,
    "compress intermediate bulk loader files to save space",
    NULL, 
    NULL, 
    true
);

static bool get_load_save_space(THD* thd) {
    return (THDVAR(thd, load_save_space) != 0);
}

static MYSQL_THDVAR_BOOL(disable_slow_alter,
    0,
    "if on, alter tables that require copy are disabled",
    NULL, 
    NULL, 
    false
);

static bool get_disable_slow_alter(THD* thd) {
    return (THDVAR(thd, disable_slow_alter) != 0);
}

static MYSQL_THDVAR_BOOL(disable_hot_alter,
    0,
    "if on, hot alter table is disabled",
    NULL, 
    NULL, 
    false
);

static bool get_disable_hot_alter(THD* thd) {
    return THDVAR(thd, disable_hot_alter) != 0;
}

static MYSQL_THDVAR_BOOL(create_index_online,
    0,
    "if on, create index done online",
    NULL, 
    NULL, 
    true
);

static bool get_create_index_online(THD* thd) {
    return (THDVAR(thd, create_index_online) != 0);
}

static MYSQL_THDVAR_BOOL(disable_prefetching,
    0,
    "if on, prefetching disabled",
    NULL, 
    NULL, 
   false
);

static bool get_disable_prefetching(THD* thd) {
    return (THDVAR(thd, disable_prefetching) != 0);
}

static MYSQL_THDVAR_BOOL(prelock_empty,
    0,
    "Tokudb Prelock Empty Table",
    NULL, 
    NULL, 
    true
);

static bool get_prelock_empty(THD* thd) {
    return (THDVAR(thd, prelock_empty) != 0);
}

static MYSQL_THDVAR_UINT(block_size,
    0,
    "fractal tree block size",
    NULL, 
    NULL, 
    4<<20, // default
    4096,  // min
    ~0U,   // max
    1      // blocksize???
);

static uint get_tokudb_block_size(THD* thd) {
    return THDVAR(thd, block_size);
}

static MYSQL_THDVAR_UINT(read_block_size,
    0,
    "fractal tree read block size",
    NULL, 
    NULL, 
    64*1024, // default
    4096,  // min
    ~0U,   // max
    1      // blocksize???
);

static uint get_tokudb_read_block_size(THD* thd) {
    return THDVAR(thd, read_block_size);
}

static MYSQL_THDVAR_UINT(read_buf_size,
    0,
    "fractal tree read block size", //TODO: Is this a typo?
    NULL, 
    NULL, 
    128*1024, // default
    0,  // min
    1*1024*1024,   // max
    1      // blocksize???
);

static uint get_tokudb_read_buf_size(THD* thd) {
    return THDVAR(thd, read_buf_size);
}

#if TOKU_INCLUDE_UPSERT
static MYSQL_THDVAR_BOOL(disable_slow_update, 
    PLUGIN_VAR_THDLOCAL, 
    "disable slow update",
    NULL, // check
    NULL, // update
    false // default
);

static MYSQL_THDVAR_BOOL(disable_slow_upsert, 
    PLUGIN_VAR_THDLOCAL, 
    "disable slow upsert",
    NULL, // check
    NULL, // update
    false // default
);
#endif

static MYSQL_THDVAR_UINT(analyze_time,
    0,
    "analyze time",
    NULL, 
    NULL, 
    5, // default
    0,  // min
    ~0U,   // max
    1      // blocksize
);

static void tokudb_checkpoint_lock(THD * thd);
static void tokudb_checkpoint_unlock(THD * thd);

static void tokudb_checkpoint_lock_update(
    THD* thd,
    struct st_mysql_sys_var* var,
    void* var_ptr,
    const void* save) 
{
    my_bool* val = (my_bool *) var_ptr;
    *val= *(my_bool *) save ? true : false;
    if (*val) {
        tokudb_checkpoint_lock(thd);
    }
    else {
        tokudb_checkpoint_unlock(thd);
    }
}
  
static MYSQL_THDVAR_BOOL(checkpoint_lock,
    0,
    "Tokudb Checkpoint Lock",
    NULL, 
    tokudb_checkpoint_lock_update, 
    false
);

static const char *tokudb_row_format_names[] = {
    "tokudb_uncompressed",
    "tokudb_zlib",
    "tokudb_quicklz",
    "tokudb_lzma",
    "tokudb_fast",
    "tokudb_small",
    "tokudb_default",
    NullS
};

static TYPELIB tokudb_row_format_typelib = {
    array_elements(tokudb_row_format_names) - 1,
    "tokudb_row_format_typelib",
    tokudb_row_format_names,
    NULL
};

static MYSQL_THDVAR_ENUM(row_format, PLUGIN_VAR_OPCMDARG,
                         "Specifies the compression method for a table during this session. "
                         "Possible values are TOKUDB_UNCOMPRESSED, TOKUDB_ZLIB, TOKUDB_QUICKLZ, "
                         "TOKUDB_LZMA, TOKUDB_FAST, TOKUDB_SMALL and TOKUDB_DEFAULT",
                         NULL, NULL, SRV_ROW_FORMAT_ZLIB, &tokudb_row_format_typelib);

static srv_row_format_t get_row_format(THD *thd) {
    return (srv_row_format_t) THDVAR(thd, row_format);
}

static MYSQL_THDVAR_UINT(lock_timeout_debug, 0, "TokuDB lock timeout debug", NULL /*check*/, NULL /*update*/, 1 /*default*/, 0 /*min*/, ~0U /*max*/, 1);

static MYSQL_THDVAR_STR(last_lock_timeout, PLUGIN_VAR_MEMALLOC, "last TokuDB lock timeout", NULL /*check*/, NULL /*update*/, NULL /*default*/);

static MYSQL_THDVAR_BOOL(hide_default_row_format, 0, "hide the default row format", NULL /*check*/, NULL /*update*/, false);

extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;
extern uint32_t tokudb_write_status_frequency;
extern uint32_t tokudb_read_status_frequency;

void toku_hton_update_primary_key_bytes_inserted(uint64_t row_size);

#endif //#ifdef _HATOKU_HTON
