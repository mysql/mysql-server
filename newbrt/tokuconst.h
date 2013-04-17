/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."

#ifndef TOKUCONST_H
#define TOKUCONST_H
/* The number of transaction ids stored in the xids structure is 
 * represented by an 8-bit value.  The value 255 is reserved. 
 * The constant MAX_NESTED_TRANSACTIONS is one less because
 * one slot in the packed leaf entry is used for the implicit
 * root transaction (id 0).
 */
enum {MAX_NESTED_TRANSACTIONS = 253};
enum {MAX_TRANSACTION_RECORDS = MAX_NESTED_TRANSACTIONS + 1};

#define DO_IMPLICIT_PROMOTION_ON_QUERY 0

#endif

