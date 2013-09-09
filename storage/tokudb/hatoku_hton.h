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
uint get_pk_insert_mode(THD* thd);
bool get_load_save_space(THD* thd);
bool get_disable_slow_alter(THD* thd);
bool get_disable_hot_alter(THD* thd);
bool get_create_index_online(THD* thd);
bool get_disable_prefetching(THD* thd);
bool get_prelock_empty(THD* thd);
bool get_log_client_errors(THD* thd);
uint get_tokudb_block_size(THD* thd);
uint get_tokudb_read_block_size(THD* thd);
uint get_tokudb_read_buf_size(THD* thd);
srv_row_format_t get_row_format(THD *thd);
#if TOKU_INCLUDE_UPSERT
bool get_enable_fast_update(THD *thd);
bool get_disable_slow_update(THD *thd);
bool get_enable_fast_upsert(THD *thd);
bool get_disable_slow_upsert(THD *thd);
#endif
uint get_analyze_time(THD *thd);

extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;
extern uint32_t tokudb_write_status_frequency;
extern uint32_t tokudb_read_status_frequency;

void toku_hton_update_primary_key_bytes_inserted(uint64_t row_size);

#endif //#ifdef _HATOKU_HTON
