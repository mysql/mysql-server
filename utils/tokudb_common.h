#if !defined(TOKUDB_COMMON_H)
#define TOKUDB_COMMON_H

/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <db.h>
#include <inttypes.h>
#include <signal.h>

typedef u_int8_t bool;

#define true ((bool)1)
#define false ((bool)0)

#define SET_BITS(bitvector, bits)      ((bitvector) |= (bits))
#define REMOVE_BITS(bitvector, bits)   ((bitvector) &= ~(bits))
#define IS_SET_ANY(bitvector, bits)    ((bitvector) & (bits))
#define IS_SET_ALL(bitvector, bits)    (((bitvector) & (bits)) == (bits))

#define IS_POWER_OF_2(num)             ((num) > 0 && ((num) & ((num) - 1)) == 0)

#endif /* #if !defined(TOKUDB_COMMON_H) */
