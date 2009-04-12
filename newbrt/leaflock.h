/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_LEAFLOCK_H
#define TOKU_LEAFLOCK_H
#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

typedef struct leaflock *LEAFLOCK;

void toku_leaflock_init(void);
void toku_leaflock_destroy(void);

int toku_leaflock_borrow(LEAFLOCK *leaflockp);
void toku_leaflock_unlock_and_return(LEAFLOCK *leaflockp);

void toku_leaflock_lock_by_leaf(LEAFLOCK leaflock);
void toku_leaflock_unlock_by_leaf(LEAFLOCK leaflock);

void toku_leaflock_lock_by_cursor(LEAFLOCK leaflock);
void toku_leaflock_unlock_by_cursor(LEAFLOCK leaflock);
#endif

