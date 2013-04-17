/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ifndef TOKU_LEAFLOCK_H
#define TOKU_LEAFLOCK_H
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

