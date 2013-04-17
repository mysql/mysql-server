/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_LEAFLOCK_H
#define TOKU_LEAFLOCK_H
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

typedef struct leaflock *LEAFLOCK;
typedef struct leaflock_pool *LEAFLOCK_POOL;

int toku_leaflock_create(LEAFLOCK_POOL* pool);
int toku_leaflock_destroy(LEAFLOCK_POOL* pool);

int toku_leaflock_borrow(LEAFLOCK_POOL pool, LEAFLOCK *leaflockp);
void toku_leaflock_unlock_and_return(LEAFLOCK_POOL pool, LEAFLOCK *leaflockp);

void toku_leaflock_lock_by_leaf(LEAFLOCK leaflock);
void toku_leaflock_unlock_by_leaf(LEAFLOCK leaflock);

void toku_leaflock_lock_by_cursor(LEAFLOCK leaflock);
void toku_leaflock_unlock_by_cursor(LEAFLOCK leaflock);
#endif

