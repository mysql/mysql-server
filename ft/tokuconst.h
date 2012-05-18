/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009-2010 Tokutek Inc.  All rights reserved."

#ifndef TOKUCONST_H
#define TOKUCONST_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* The number of transaction ids stored in the xids structure is 
 * represented by an 8-bit value.  The value 255 is reserved. 
 * The constant MAX_NESTED_TRANSACTIONS is one less because
 * one slot in the packed leaf entry is used for the implicit
 * root transaction (id 0).
 */

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

enum {MAX_NESTED_TRANSACTIONS = 253};
enum {MAX_TRANSACTION_RECORDS = MAX_NESTED_TRANSACTIONS + 1};

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

