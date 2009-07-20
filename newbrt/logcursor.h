#ifndef TOKULOGCURSOR_H
#define TOKULOGCURSOR_H

#ident "$Id: logcursor.h 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "log_header.h"

typedef struct log_entry * TOKULOGENTRY;

struct toku_logcursor;
typedef struct toku_logcursor *TOKULOGCURSOR;

// toku_logcursor_create()
//   - returns a pointer to a logcursor
int toku_logcursor_create(TOKULOGCURSOR *lc, const char *log_dir);
int toku_logcursor_destroy(TOKULOGCURSOR *lc);

// returns 0 on success
int toku_logcursor_current(TOKULOGCURSOR lc, struct log_entry *le);
int toku_logcursor_next(TOKULOGCURSOR lc, struct log_entry *le);
int toku_logcursor_prev(TOKULOGCURSOR lc, struct log_entry *le);

int toku_logcursor_first(const TOKULOGCURSOR lc, struct log_entry *le);
int toku_logcursor_last(const TOKULOGCURSOR lc, struct log_entry *le);

#endif // TOKULOGCURSOR_H
