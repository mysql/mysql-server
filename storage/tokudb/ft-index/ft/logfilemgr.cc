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

#include "log-internal.h"
#include "logcursor.h"
#include "logfilemgr.h"

// for now, implement with singlely-linked-list
//   first = oldest  (delete from beginning)
//   last  = newest  (add to end)

struct lfm_entry {
    TOKULOGFILEINFO lf_info;
    struct lfm_entry *next;
};

struct toku_logfilemgr {
    struct lfm_entry *first;
    struct lfm_entry *last;
    int n_entries;
};

int toku_logfilemgr_create(TOKULOGFILEMGR *lfm) {
    // malloc a logfilemgr
    TOKULOGFILEMGR XMALLOC(mgr);
    mgr->first = NULL;
    mgr->last = NULL;
    mgr->n_entries = 0;    
    *lfm = mgr;
    return 0;
}

int toku_logfilemgr_destroy(TOKULOGFILEMGR *lfm) {
    int r=0;
    if ( *lfm != NULL ) { // be tolerant of being passed a NULL
        TOKULOGFILEMGR mgr = *lfm;
        while ( mgr->n_entries > 0 ) {
            toku_logfilemgr_delete_oldest_logfile_info(mgr);
        }
        toku_free(*lfm);
        *lfm = NULL;
    }
    return r;
}

int toku_logfilemgr_init(TOKULOGFILEMGR lfm, const char *log_dir, TXNID *last_xid_if_clean_shutdown) {
    invariant_notnull(lfm);
    invariant_notnull(last_xid_if_clean_shutdown);

    int r;
    int n_logfiles;
    char **logfiles;
    r = toku_logger_find_logfiles(log_dir, &logfiles, &n_logfiles);
    if (r!=0)
        return r;

    TOKULOGCURSOR cursor;
    struct log_entry *entry;
    TOKULOGFILEINFO lf_info;
    long long index = -1;
    char *basename;
    LSN tmp_lsn = {0};
    TXNID last_xid = TXNID_NONE;
    for(int i=0;i<n_logfiles;i++){
        XMALLOC(lf_info);
        // find the index
	// basename is the filename of the i-th logfile
        basename = strrchr(logfiles[i], '/') + 1;
        int version;
        r = sscanf(basename, "log%lld.tokulog%d", &index, &version);
        assert(r==2);  // found index and version
        assert(version>=TOKU_LOG_MIN_SUPPORTED_VERSION);
        assert(version<=TOKU_LOG_VERSION);
        lf_info->index = index;
        lf_info->version = version;
        // find last LSN in logfile
        r = toku_logcursor_create_for_file(&cursor, log_dir, basename);
        if (r!=0) {
            return r;
        }
        r = toku_logcursor_last(cursor, &entry);  // set "entry" to last log entry in logfile
        if (r == 0) {
            lf_info->maxlsn = toku_log_entry_get_lsn(entry);

            invariant(lf_info->maxlsn.lsn >= tmp_lsn.lsn);
            tmp_lsn = lf_info->maxlsn;
            if (entry->cmd == LT_shutdown) {
                last_xid = entry->u.shutdown.last_xid;
            } else {
                last_xid = TXNID_NONE;
            }
        }
        else {
            lf_info->maxlsn = tmp_lsn; // handle empty logfile (no LSN in file) case
        }

        // add to logfilemgr
        toku_logfilemgr_add_logfile_info(lfm, lf_info);
        toku_logcursor_destroy(&cursor);
    }
    for(int i=0;i<n_logfiles;i++) {
        toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    *last_xid_if_clean_shutdown = last_xid;
    return 0;
}

int toku_logfilemgr_num_logfiles(TOKULOGFILEMGR lfm) {
    assert(lfm);
    return lfm->n_entries;
}

int toku_logfilemgr_add_logfile_info(TOKULOGFILEMGR lfm, TOKULOGFILEINFO lf_info) {
    assert(lfm);
    struct lfm_entry *XMALLOC(entry);
    entry->lf_info = lf_info;
    entry->next = NULL;
    if ( lfm->n_entries != 0 )
        lfm->last->next = entry;
    lfm->last = entry;
    lfm->n_entries++;
    if (lfm->n_entries == 1 ) {
        lfm->first = lfm->last;
    }
    return 0;
}

TOKULOGFILEINFO toku_logfilemgr_get_oldest_logfile_info(TOKULOGFILEMGR lfm) {
    assert(lfm);
    return lfm->first->lf_info;
}

void toku_logfilemgr_delete_oldest_logfile_info(TOKULOGFILEMGR lfm) {
    assert(lfm);
    if ( lfm->n_entries > 0 ) {
        struct lfm_entry *entry = lfm->first;
        toku_free(entry->lf_info);
        lfm->first = entry->next;
        toku_free(entry);
        lfm->n_entries--;
        if ( lfm->n_entries == 0 ) {
            lfm->last = lfm->first = NULL;
        }
    }
}

LSN toku_logfilemgr_get_last_lsn(TOKULOGFILEMGR lfm) {
    assert(lfm);
    if ( lfm->n_entries == 0 ) {
        LSN lsn;
        lsn.lsn = 0;
        return lsn;
    }
    return lfm->last->lf_info->maxlsn;
}

void toku_logfilemgr_update_last_lsn(TOKULOGFILEMGR lfm, LSN lsn) {
    assert(lfm);
    assert(lfm->last!=NULL);
    lfm->last->lf_info->maxlsn = lsn;
}

void toku_logfilemgr_print(TOKULOGFILEMGR lfm) {
    assert(lfm);
    printf("toku_logfilemgr_print [%p] : %d entries \n", lfm, lfm->n_entries);
    struct lfm_entry *entry = lfm->first;
    for (int i=0;i<lfm->n_entries;i++) {
        printf("  entry %d : index = %" PRId64 ", maxlsn = %" PRIu64 "\n", i, entry->lf_info->index, entry->lf_info->maxlsn.lsn);
        entry = entry->next;
    }
}
