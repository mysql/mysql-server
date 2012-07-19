/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef ROLLBACK_APPLY_H
#define ROLLBACK_APPLY_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


typedef int(*apply_rollback_item)(TOKUTXN txn, struct roll_entry *item, LSN lsn);
int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, LSN lsn);
int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, LSN lsn);

int toku_rollback_commit(TOKUTXN txn, LSN lsn);
int toku_rollback_abort(TOKUTXN txn, LSN lsn);


#endif // ROLLBACK_APPLY_H
