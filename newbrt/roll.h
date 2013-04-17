/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: roll.c 12588 2009-06-09 00:05:02Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKUDB_ROLL_H
#define TOKUDB_ROLL_H
// these flags control whether or not we send commit messages for
// various operations

// When a transaction is committed, should we send a BRT_COMMIT message
// for each BRT_INSERT message sent earlier by the transaction?
#define TOKU_DO_COMMIT_CMD_INSERT 0

// When a transaction is committed, should we send a BRT_COMMIT message
// for each BRT_DELETE_ANY message sent earlier by the transaction?
#define TOKU_DO_COMMIT_CMD_DELETE 1

// When a transaction is committed, should we send a BRT_COMMIT message
// for each BRT_DELETE_BOTH message sent earlier by the transaction?
#define TOKU_DO_COMMIT_CMD_DELETE_BOTH 1

#endif

