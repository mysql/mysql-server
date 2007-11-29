/************************************************************************
The index tree general types

(c) 1996 Innobase Oy

Created 2/17/1996 Heikki Tuuri
*************************************************************************/

#ifndef btr0types_h
#define btr0types_h

#include "univ.i"

#include "rem0types.h"
#include "page0types.h"

typedef struct btr_pcur_struct		btr_pcur_t;
typedef struct btr_cur_struct		btr_cur_t;
typedef struct btr_search_struct	btr_search_t;

/* The size of a reference to data stored on a different page.
The reference is stored at the end of the prefix of the field
in the index record. */
#define BTR_EXTERN_FIELD_REF_SIZE	20

/* A BLOB field reference full of zero, for use in assertions and tests.
Initially, BLOB field references are set to zero, in
dtuple_convert_big_rec(). */
extern const byte field_ref_zero[BTR_EXTERN_FIELD_REF_SIZE];

#endif
