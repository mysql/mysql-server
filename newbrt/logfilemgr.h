#ifndef TOKULOGFILEMGR_H
#define TOKULOGFILEMGR_H

#ident "$Id: logfilemgr.h 12375 2009-05-28 14:14:47Z wells $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "log_header.h"

// this is the basic information we need to keep per logfile
struct toku_logfile_info {
    int64_t index;
    LSN maxlsn;
};
typedef struct toku_logfile_info *TOKULOGFILEINFO;

struct toku_logfilemgr;
typedef struct toku_logfilemgr *TOKULOGFILEMGR;

int toku_logfilemgr_create(TOKULOGFILEMGR *lfm);
int toku_logfilemgr_destroy(TOKULOGFILEMGR *lfm);

int toku_logfilemgr_init(TOKULOGFILEMGR lfm, const char *log_dir);
int toku_logfilemgr_num_logfiles(TOKULOGFILEMGR lfm);
int toku_logfilemgr_add_logfile_info(TOKULOGFILEMGR lfm, TOKULOGFILEINFO lf_info);
TOKULOGFILEINFO toku_logfilemgr_get_oldest_logfile_info(TOKULOGFILEMGR lfm);
void toku_logfilemgr_delete_oldest_logfile_info(TOKULOGFILEMGR lfm);
LSN toku_logfilemgr_get_last_lsn(TOKULOGFILEMGR lfm);
void toku_logfilemgr_update_last_lsn(TOKULOGFILEMGR lfm, LSN lsn);

void toku_logfilemgr_print(TOKULOGFILEMGR lfm);
#endif //TOKULOGFILEMGR_H
