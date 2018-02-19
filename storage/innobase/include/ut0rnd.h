/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "my_compiler.h"

/** @file include/ut0rnd.h
 Random numbers and hashing

 Created 1/20/1994 Heikki Tuuri
 ***********************************************************************/

#ifndef ut0rnd_h
#define ut0rnd_h

#include "univ.i"
#include "ut0byte.h"

/** The 'character code' for end of field or string (used
in folding records */
#define UT_END_OF_FIELD 257

/** The following function generates a series of 'random' ulint integers.
 @return the next 'random' number */
UNIV_INLINE
ulint ut_rnd_gen_next_ulint(
    ulint rnd); /*!< in: the previous random number value */
/** The following function generates 'random' ulint integers which
 enumerate the value space (let there be N of them) of ulint integers
 in a pseudo-random fashion. Note that the same integer is repeated
 always after N calls to the generator.
 @return the 'random' number */
UNIV_INLINE
ulint ut_rnd_gen_ulint(void);

/** Generates a random integer from a given interval.
@param[in]	low	low limit; can generate also this value
@param[in]	high	high limit; can generate also this value
@return the 'random' number */
UNIV_INLINE
ulint ut_rnd_interval(ulint low, ulint high);

/** The following function generates a hash value for a ulint integer to a
hash table of size table_size, which should be a prime or some random number
to work reliably.
@param[in]	key		value to be hashed
@param[in]	table_size	hash table size
@return hash value */
UNIV_INLINE
ulint ut_hash_ulint(ulint key, ulint table_size);

/** Folds a 64-bit integer.
 @return folded value */
UNIV_INLINE
ulint ut_fold_ull(ib_uint64_t d) /*!< in: 64-bit integer */
    MY_ATTRIBUTE((const));
/** Folds a character string ending in the null character.
 @return folded value */
UNIV_INLINE
ulint ut_fold_string(const char *str) /*!< in: null-terminated string */
    MY_ATTRIBUTE((warn_unused_result));
/** Looks for a prime number slightly greater than the given argument.
 The prime is chosen so that it is not near any power of 2.
 @return prime */
ulint ut_find_prime(ulint n) /*!< in: positive number > 100 */
    MY_ATTRIBUTE((const));

/** Folds a pair of ulints.
 @return folded value */
UNIV_INLINE
ulint ut_fold_ulint_pair(ulint n1, /*!< in: ulint */
                         ulint n2) /*!< in: ulint */
    MY_ATTRIBUTE((const));
/** Folds a binary string.
 @return folded value */
UNIV_INLINE
ulint ut_fold_binary(const byte *str, /*!< in: string of bytes */
                     ulint len)       /*!< in: length */
    MY_ATTRIBUTE((pure));

#include "ut0rnd.ic"

#endif
