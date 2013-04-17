#ifndef TOKULOGCURSOR_H
#define TOKULOGCURSOR_H

#ident "$Id: logcursor.h 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "log_header.h"

struct toku_logcursor;
typedef struct toku_logcursor *TOKULOGCURSOR;

// All routines return 0 on success

// toku_logcursor_create()
//   - creates a logcursor (lc)
//   - following toku_logcursor_create()
//         if toku_logcursor_next() is called, it returns the first entry in the log
//         if toku_logcursor_prev() is called, it returns the last entry in the log
int toku_logcursor_create(TOKULOGCURSOR *lc, const char *log_dir);
// toku_logcursor_create_for_file()
//   - creates a logcusor (lc) that only knows about the file log_file
int toku_logcursor_create_for_file(TOKULOGCURSOR *lc, const char *log_dir, const char *log_file);
// toku_logcursor_destroy()
//    - frees all resources associated with the logcursor, including the log_entry 
//       associated with the latest cursor action
int toku_logcursor_destroy(TOKULOGCURSOR *lc);

// toku_logcursor_[next,prev,first,last] take care of malloc'ing and free'ing log_entrys.
//    - routines NULL out the **le pointers on entry, then set the **le pointers to 
//        the malloc'ed entries when successful, 
int toku_logcursor_next(TOKULOGCURSOR lc, struct log_entry **le);
int toku_logcursor_prev(TOKULOGCURSOR lc, struct log_entry **le);

int toku_logcursor_first(const TOKULOGCURSOR lc, struct log_entry **le);
int toku_logcursor_last(const TOKULOGCURSOR lc, struct log_entry **le);

// return 0 if log exists, ENOENT if no log
int toku_logcursor_log_exists(const TOKULOGCURSOR lc);

#endif // TOKULOGCURSOR_H
