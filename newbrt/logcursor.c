/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: logcursor.c 13196 2009-07-10 14:41:51Z zardosht $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

enum lc_direction { LC_FORWARD, LC_BACKWARD, LC_FIRST, LC_LAST };

struct toku_logcursor {
    char *logdir;         // absolute directory name
    char **logfiles;
    int n_logfiles;
    int cur_logfiles_index;
    FILE *cur_fp;
    size_t buffer_size;
    void *buffer;
    BOOL is_open;
    struct log_entry entry;
    BOOL entry_valid;
    LSN cur_lsn;
    enum lc_direction last_direction;
};

#define LC_LSN_ERROR (DB_RUNRECOVERY)

static void lc_print_logcursor (TOKULOGCURSOR lc) __attribute__((unused));
static void lc_print_logcursor (TOKULOGCURSOR lc)  {
    printf("lc = %p\n", lc);
    printf("  logdir = %s\n", lc->logdir);
    printf("  logfiles = %p\n", lc->logfiles);
    for (int lf=0;lf<lc->n_logfiles;lf++) {
        printf("    logfile[%d] = %p (%s)\n", lf, lc->logfiles[lf], lc->logfiles[lf]);
    }
    printf("  n_logfiles = %d\n", lc->n_logfiles);
    printf("  cur_logfiles_index = %d\n", lc->cur_logfiles_index);
    printf("  cur_fp = %p\n", lc->cur_fp);
    printf("  cur_lsn = %"PRIu64"\n", lc->cur_lsn.lsn);
    printf("  last_direction = %d\n", (int) lc->last_direction);
}

static int lc_close_cur_logfile(TOKULOGCURSOR lc) {
    int r=0;
    if ( lc->is_open ) {
        r = fclose(lc->cur_fp);
        assert(0==r);
        lc->is_open = FALSE;
    }
    return 0;
}
static int lc_open_logfile(TOKULOGCURSOR lc, int index) {
    int r=0;
    assert( !lc->is_open );
    if( index == -1 || index >= lc->n_logfiles) return DB_NOTFOUND;
    lc->cur_fp = fopen(lc->logfiles[index], "rb");
    if ( lc->cur_fp == NULL ) 
        return DB_NOTFOUND;
    // debug printf("%s:%d %s %p %u\n", __FUNCTION__, __LINE__, lc->logfiles[index], lc->buffer, (unsigned) lc->buffer_size);
#if !TOKU_WINDOWS //Windows reads logs fastest if we use default settings (not use setvbuf to change buffering)
    r = setvbuf(lc->cur_fp, lc->buffer, _IOFBF, lc->buffer_size);
    assert(r == 0);
#endif
    // position fp past header
    unsigned int version=0;
    r = toku_read_logmagic(lc->cur_fp, &version);
    if (r!=0) 
        return DB_BADFORMAT;
    if (version != TOKU_LOG_VERSION)
        return DB_BADFORMAT;
    // mark as open
    lc->is_open = TRUE;
    return r;
}

static int lc_check_lsn(TOKULOGCURSOR lc, int dir) {
    int r=0;
    LSN lsn = toku_log_entry_get_lsn(&(lc->entry));
    if (((dir == LC_FORWARD)  && ( lsn.lsn != lc->cur_lsn.lsn + 1 )) ||
        ((dir == LC_BACKWARD) && ( lsn.lsn != lc->cur_lsn.lsn - 1 ))) {
//        int index = lc->cur_logfiles_index;
//        fprintf(stderr, "Bad LSN: %d %s direction = %d, lsn.lsn = %"PRIu64", cur_lsn.lsn=%"PRIu64"\n", 
//                index, lc->logfiles[index], dir, lsn.lsn, lc->cur_lsn.lsn);
        if (tokudb_recovery_trace) 
            printf("DB_RUNRECOVERY: %s:%d r=%d\n", __FUNCTION__, __LINE__, 0);
        return LC_LSN_ERROR;
    }
    lc->cur_lsn.lsn = lsn.lsn;
    return r;
}

// toku_logcursor_create()
//   - returns a pointer to a logcursor

static int lc_create(TOKULOGCURSOR *lc, const char *log_dir) {
    int failresult=0;
    int r=0;

    // malloc a cursor
    TOKULOGCURSOR cursor = (TOKULOGCURSOR) toku_malloc(sizeof(struct toku_logcursor));
    if ( cursor == NULL ) {
        failresult = ENOMEM;
        goto lc_create_fail;
    }
    // find logfiles in logdir
    cursor->is_open = FALSE;
    cursor->cur_logfiles_index = 0;
    cursor->entry_valid = FALSE;
    cursor->buffer_size = 1<<20;                       // use a 1MB stream buffer (setvbuf)
    cursor->buffer = toku_malloc(cursor->buffer_size); // it does not matter if it failes
    // cursor->logdir must be an absolute path
    if (toku_os_is_absolute_name(log_dir)) {
        cursor->logdir = (char *) toku_malloc(strlen(log_dir)+1);
        if ( cursor->logdir == NULL ) {
            failresult = ENOMEM;
            goto lc_create_fail;
        }
        sprintf(cursor->logdir, "%s", log_dir);
    } else {
        char *cwd = getcwd(NULL, 0);
        if ( cwd == NULL ) {
            failresult = -1;
            goto lc_create_fail;
        }
        cursor->logdir = (char *) toku_malloc(strlen(cwd)+strlen(log_dir)+2);
        if ( cursor->logdir == NULL ) {
            toku_free(cwd);
            failresult = ENOMEM;
            goto lc_create_fail;
        }
        sprintf(cursor->logdir, "%s/%s", cwd, log_dir);
        toku_free(cwd);
    }
    cursor->logfiles = NULL;
    cursor->n_logfiles = 0;
    cursor->cur_lsn.lsn=0;
    cursor->last_direction=LC_FIRST;
    
    *lc = cursor;
    return r;

 lc_create_fail:
    toku_logcursor_destroy(&cursor);
    *lc = NULL;
    return failresult;
}

int toku_logcursor_create(TOKULOGCURSOR *lc, const char *log_dir) {
    int r = lc_create(lc, log_dir);
    if ( r!=0 ) 
        return r;

    TOKULOGCURSOR cursor = *lc;
    r = toku_logger_find_logfiles(cursor->logdir, &(cursor->logfiles), &(cursor->n_logfiles));
    if (r!=0) {
        toku_logcursor_destroy(&cursor);
        *lc = NULL;
    }
    return r;
}

int toku_logcursor_create_for_file(TOKULOGCURSOR *lc, const char *log_dir, const char *log_file) {
    int failresult = 0;
    int r = lc_create(lc, log_dir);
    if ( r!=0 ) 
        return r;

    TOKULOGCURSOR cursor = *lc;
    int fullnamelen = strlen(cursor->logdir) + strlen(log_file) + 3;
    char *log_file_fullname = toku_malloc(fullnamelen);
    if ( log_file_fullname == NULL ) {
        failresult = ENOMEM;
        goto fail;
    }
    sprintf(log_file_fullname, "%s/%s", cursor->logdir, log_file);

    cursor->n_logfiles=1;

    char **logfiles = toku_malloc(sizeof(char**));
    if ( logfiles == NULL ) {
        failresult = ENOMEM;
        goto fail;
    }
    cursor->logfiles = logfiles;
    cursor->logfiles[0] = log_file_fullname;
    *lc = cursor;
    return r;
fail:
    toku_free(log_file_fullname);
    toku_logcursor_destroy(&cursor);
    *lc = NULL;
    return failresult;
}

int toku_logcursor_destroy(TOKULOGCURSOR *lc) {
    int r=0;
    if ( (*lc)->entry_valid ) {
        toku_log_free_log_entry_resources(&((*lc)->entry));
        (*lc)->entry_valid = FALSE;
    }
    r = lc_close_cur_logfile(*lc);
    int lf;
    for(lf=0;lf<(*lc)->n_logfiles;lf++) {
        toku_free((*lc)->logfiles[lf]);
    }
    toku_free((*lc)->logfiles);
    toku_free((*lc)->logdir);
    if ((*lc)->buffer) 
        toku_free((*lc)->buffer);
    toku_free(*lc);
    *lc = NULL;
    return r;
}

int toku_logcursor_next(TOKULOGCURSOR lc, struct log_entry **le) {
    int r=0;
    if ( lc->entry_valid ) {
        toku_log_free_log_entry_resources(&(lc->entry));
        lc->entry_valid = FALSE;
        if (lc->last_direction == LC_BACKWARD) {
            struct log_entry junk;
            r = toku_log_fread(lc->cur_fp, &junk);
            assert(r == 0);
            toku_log_free_log_entry_resources(&junk);
        }
    } else {
        r = toku_logcursor_first(lc, le);
        return r;
    }
    r = toku_log_fread(lc->cur_fp, &(lc->entry));
    while ( EOF == r ) { 
        // move to next file
        r = lc_close_cur_logfile(lc);
        if (r!=0) 
            return r;
        if ( lc->cur_logfiles_index == lc->n_logfiles-1) 
            return DB_NOTFOUND;
        lc->cur_logfiles_index++;
        r = lc_open_logfile(lc, lc->cur_logfiles_index);
        if (r!= 0) 
            return r;
        r = toku_log_fread(lc->cur_fp, &(lc->entry));
    }
    if (r!=0) {
        if (r==DB_BADFORMAT) {
            fprintf(stderr, "Bad log format in %s\n", lc->logfiles[lc->cur_logfiles_index]);
            return r;
        } else {
            fprintf(stderr, "Unexpected log format error '%s' in %s\n", strerror(r), lc->logfiles[lc->cur_logfiles_index]);
            return r;
        }
    }
    r = lc_check_lsn(lc, LC_FORWARD);
    if (r!=0)
        return r;
    lc->last_direction = LC_FORWARD;
    lc->entry_valid = TRUE;
    *le = &(lc->entry);
    return r;
}

int toku_logcursor_prev(TOKULOGCURSOR lc, struct log_entry **le) {
    int r=0;
    if ( lc->entry_valid ) {
        toku_log_free_log_entry_resources(&(lc->entry));
        lc->entry_valid = FALSE;
        if (lc->last_direction == LC_FORWARD) {
            struct log_entry junk;
            r = toku_log_fread_backward(lc->cur_fp, &junk);
            assert(r == 0);
            toku_log_free_log_entry_resources(&junk);
        }
    } else {
        r = toku_logcursor_last(lc, le);
        return r;
    }
    r = toku_log_fread_backward(lc->cur_fp, &(lc->entry));
    while ( -1 == r) { // if within header length of top of file
        // move to previous file
        r = lc_close_cur_logfile(lc);
        if (r!=0) 
            return r;
        if ( lc->cur_logfiles_index == 0 ) 
            return DB_NOTFOUND;
        lc->cur_logfiles_index--;
        r = lc_open_logfile(lc, lc->cur_logfiles_index);
        if (r!=0) 
            return r;
        // seek to end
        r = fseek(lc->cur_fp, 0, SEEK_END);
        assert(0==r);
        r = toku_log_fread_backward(lc->cur_fp, &(lc->entry));
    }
    if (r!=0) {
        if (r==DB_BADFORMAT) {
            fprintf(stderr, "Bad log format in %s\n", lc->logfiles[lc->cur_logfiles_index]);
            return r;
        } else {
            fprintf(stderr, "Unexpected log format error '%s' in %s\n", strerror(r), lc->logfiles[lc->cur_logfiles_index]);
            return r;
        }
    }
    r = lc_check_lsn(lc, LC_BACKWARD);
    if (r!=0)
        return r;
    lc->last_direction = LC_BACKWARD;
    lc->entry_valid = TRUE;
    *le = &(lc->entry);
    return r;
}

int toku_logcursor_first(TOKULOGCURSOR lc, struct log_entry **le) {
    int r=0;
    if ( lc->entry_valid ) {
        toku_log_free_log_entry_resources(&(lc->entry));
        lc->entry_valid = FALSE;
    }
    // close any but the first log file
    if ( lc->cur_logfiles_index != 0 ) {
        lc_close_cur_logfile(lc);
    }
    // open first log file if needed
    if ( !lc->is_open ) {
        r = lc_open_logfile(lc, 0);
        if (r!=0) 
            return r;
        lc->cur_logfiles_index = 0;
    }
    while (1) {
        r = toku_log_fread(lc->cur_fp, &(lc->entry));
        if (r==0) 
            break;
        // move to next file
        r = lc_close_cur_logfile(lc);
        if (r!=0) 
            return r;
        if ( lc->cur_logfiles_index == lc->n_logfiles-1) 
            return DB_NOTFOUND;
        lc->cur_logfiles_index++;
        r = lc_open_logfile(lc, lc->cur_logfiles_index);
        if (r!= 0) 
            return r;
    }
    r = lc_check_lsn(lc, LC_FIRST);
    if (r!=0)
        return r;
    lc->last_direction = LC_FIRST;
    lc->entry_valid = TRUE;
    *le = &(lc->entry);
    return r;
}

int toku_logcursor_last(TOKULOGCURSOR lc, struct log_entry **le) {
    int r=0;
    if ( lc->entry_valid ) {
        toku_log_free_log_entry_resources(&(lc->entry));
        lc->entry_valid = FALSE;
    }
    // close any but last log file
    if ( lc->cur_logfiles_index != lc->n_logfiles-1 ) {
        lc_close_cur_logfile(lc);
    }
    // open last log file if needed
    if ( !lc->is_open ) {
        r = lc_open_logfile(lc, lc->n_logfiles-1);
        if (r!=0)
            return r;
        lc->cur_logfiles_index = lc->n_logfiles-1;
    }
    while (1) {
        // seek to end
        r = fseek(lc->cur_fp, 0, SEEK_END);
        assert(0==r);
        // read backward
        r = toku_log_fread_backward(lc->cur_fp, &(lc->entry));
        if (r==0) 
            break;
        // move to previous file
        r = lc_close_cur_logfile(lc);
        if (r!=0) 
            return r;
        if ( lc->cur_logfiles_index == 0 ) 
            return DB_NOTFOUND;
        lc->cur_logfiles_index--;
        r = lc_open_logfile(lc, lc->cur_logfiles_index);
        if (r!=0) 
            return r;    
    }
    r = lc_check_lsn(lc, LC_LAST);
    if (r!=0)
        return r;
    lc->last_direction = LC_LAST;
    lc->entry_valid = TRUE;
    *le = &(lc->entry);
    return r;
}

// return 0 if log exists, ENOENT if no log
int 
toku_logcursor_log_exists(const TOKULOGCURSOR lc) {
    int r;

    if (lc->n_logfiles) 
	r = 0;
    else
	r = ENOENT;

    return r;
}
