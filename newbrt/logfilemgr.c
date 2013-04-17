/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: logcursor.c 13196 2009-07-10 14:41:51Z zardosht $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

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
    int failresult=0;
    // malloc a logfilemgr
    TOKULOGFILEMGR mgr = toku_malloc(sizeof(struct toku_logfilemgr));
//    printf("toku_logfilemgr_create [%p]\n", mgr);
    if ( mgr == NULL ) {
        failresult = ENOMEM;
        goto createfail;
    }
    mgr->first = NULL;
    mgr->last = NULL;
    mgr->n_entries = 0;
    
    *lfm = mgr;
    return 0;

createfail:
    toku_logfilemgr_destroy(&mgr);
    *lfm = NULL;
    return failresult;
}

int toku_logfilemgr_destroy(TOKULOGFILEMGR *lfm) {
    int r=0;
    if ( *lfm != NULL ) { // be tolerant of being passed a NULL
        TOKULOGFILEMGR mgr = *lfm;
//        printf("toku_logfilemgr_destroy [%p], %d entries\n", mgr, mgr->n_entries);
        while ( mgr->n_entries > 0 ) {
            toku_logfilemgr_delete_oldest_logfile_info(mgr);
        }
        toku_free(*lfm);
        *lfm = NULL;
    }
    return r;
}

int toku_logfilemgr_init(TOKULOGFILEMGR lfm, const char *log_dir) {
//    printf("toku_logfilemgr_init [%p]\n", lfm);
    assert(lfm!=0);

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
    for(int i=0;i<n_logfiles;i++){
        lf_info = toku_malloc(sizeof(struct toku_logfile_info));
        if ( lf_info == NULL ) {
            return ENOMEM;
        }
        // find the index
        basename = strrchr(logfiles[i], '/') + 1;
        r = sscanf(basename, "log%lld.tokulog", &index);
        assert(r==1);  // found index
        lf_info->index = index;
        // find last LSN
        r = toku_logcursor_create_for_file(&cursor, log_dir, basename);
        if (r!=0) 
            return r;
        r = toku_logcursor_last(cursor, &entry);
        if ( r == 0 ) {
            lf_info->maxlsn = toku_log_entry_get_lsn(entry);
        }
        else {
            lf_info->maxlsn.lsn = 0;
        }

        // add to logfilemgr
        toku_logfilemgr_add_logfile_info(lfm, lf_info);
        toku_logcursor_destroy(&cursor);
    }
    for(int i=0;i<n_logfiles;i++) {
        toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    return 0;
}

int toku_logfilemgr_num_logfiles(TOKULOGFILEMGR lfm) {
    assert(lfm!=NULL);
    return lfm->n_entries;
}

int toku_logfilemgr_add_logfile_info(TOKULOGFILEMGR lfm, TOKULOGFILEINFO lf_info) {
    assert(lfm!=NULL);
    struct lfm_entry *entry = toku_malloc(sizeof(struct lfm_entry));
//    printf("toku_logfilemgr_add_logfile_info [%p] : entry [%p]\n", lfm, entry);
    int r=0;
    if ( entry != NULL ) {
        entry->lf_info=lf_info;
        entry->next = NULL;
        if ( lfm->n_entries != 0 )
            lfm->last->next = entry;
        lfm->last = entry;
        lfm->n_entries++;
        if (lfm->n_entries == 1 ) {
            lfm->first = lfm->last;
        }
    }
    else 
        r = ENOMEM;
    return r;
}

TOKULOGFILEINFO toku_logfilemgr_get_oldest_logfile_info(TOKULOGFILEMGR lfm) {
    assert(lfm!=NULL);
    return lfm->first->lf_info;
}

void toku_logfilemgr_delete_oldest_logfile_info(TOKULOGFILEMGR lfm) {
    assert(lfm!=NULL);
    if ( lfm->n_entries > 0 ) {
        struct lfm_entry *entry = lfm->first;
//        printf("toku_logfilemgr_delete_oldest_logfile_info [%p] : entry [%p]\n", lfm, entry);
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
    assert(lfm!=NULL);
    if ( lfm->n_entries == 0 ) {
        LSN lsn;
        lsn.lsn = 0;
        return lsn;
    }
    return lfm->last->lf_info->maxlsn;
}

void toku_logfilemgr_update_last_lsn(TOKULOGFILEMGR lfm, LSN lsn) {
    assert(lfm!=NULL);
    assert(lfm->last!=NULL);
    lfm->last->lf_info->maxlsn = lsn;
}

void toku_logfilemgr_print(TOKULOGFILEMGR lfm) {
    assert(lfm!=NULL);
    printf("toku_logfilemgr_print [%p] : %d entries \n", lfm, lfm->n_entries);
    int i;
    struct lfm_entry *entry = lfm->first;
    for (i=0;i<lfm->n_entries;i++) {
        printf("  entry %d : index = %"PRId64", maxlsn = %"PRIu64"\n", i, entry->lf_info->index, entry->lf_info->maxlsn.lsn);
        entry = entry->next;
    }
}
