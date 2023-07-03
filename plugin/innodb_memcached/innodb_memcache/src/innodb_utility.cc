/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

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

/**************************************************/ /**
 @file

 Created 03/15/2011	Jimmy Yang (most macros and defines are adopted
                         from utility files in the InnoDB. Namely,
                         ut/ut0lst.c, ut0rnd.c and hash0hash.c etc.)
 *******************************************************/

#include "innodb_utility.h"
#include <stdlib.h>
#include <string.h>

#define UT_HASH_RANDOM_MASK 1463735687
#define UT_HASH_RANDOM_MASK2 1653893711
#define UT_RANDOM_1 1.0412321
#define UT_RANDOM_2 1.1131347
#define UT_RANDOM_3 1.0132677

/*************************************************************/ /**
 Folds a pair of uint64_ts.
 @return folded value */
static uint64_t ut_fold_uint64_t_pair(
    /*====================*/
    uint64_t n1, /*!< in: uint64_t */
    uint64_t n2) /*!< in: uint64_t */
{
  return (
      ((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1) ^ UT_HASH_RANDOM_MASK) +
      n2);
}

/*************************************************************/ /**
 Folds a character string ending in the null character.
 @return folded value */
uint64_t ut_fold_string(
    /*===========*/
    const char *str) /*!< in: null-terminated string */
{
  uint64_t fold = 0;

  while (*str != '\0') {
    fold = ut_fold_uint64_t_pair(fold, (uint64_t)(*str));
    str++;
  }

  return (fold);
}

/***********************************************************/ /**
 Looks for a prime number slightly greater than the given argument.
 The prime is chosen so that it is not near any power of 2.
 @return prime */
static uint64_t ut_find_prime(
    /*==========*/
    uint64_t n) /*!< in: positive number > 100 */
{
  uint64_t pow2;
  uint64_t i;

  n += 100;

  pow2 = 1;
  while (pow2 * 2 < n) {
    pow2 = 2 * pow2;
  }

  if ((double)n < 1.05 * (double)pow2) {
    n = (uint64_t)((double)n * UT_RANDOM_1);
  }

  pow2 = 2 * pow2;

  if ((double)n > 0.95 * (double)pow2) {
    n = (uint64_t)((double)n * UT_RANDOM_2);
  }

  if (n > pow2 - 20) {
    n += 30;
  }

  /* Now we have n far enough from powers of 2. To make
  n more random (especially, if it was not near
  a power of 2), we then multiply it by a random number. */

  n = (uint64_t)((double)n * UT_RANDOM_3);

  for (;; n++) {
    i = 2;
    while (i * i <= n) {
      if (n % i == 0) {
        goto next_n;
      }
      i++;
    }

    /* Found a prime */
    break;
  next_n:;
  }

  return (n);
}

/*************************************************************/ /**
 Creates a hash table with >= n array cells. The actual number of cells is
 chosen to be a prime number slightly bigger than n.
 @return own: created table */
hash_table_t *hash_create(
    /*========*/
    uint64_t n) /*!< in: number of array cells */
{
  hash_cell_t *array;
  uint64_t prime;
  hash_table_t *table;

  prime = ut_find_prime(n);

  table = (hash_table_t *)malloc(sizeof(hash_table_t));

  array = (hash_cell_t *)malloc(sizeof(hash_cell_t) * prime);

  /* The default type of hash_table is HASH_TABLE_SYNC_NONE i.e.:
  the caller is responsible for access control to the table. */
  table->array = array;
  table->n_cells = prime;

  /* Initialize the cell array */
  memset(table->array, 0x0, table->n_cells * sizeof(*table->array));

  return (table);
}

/*******************************************************/ /**
 The following function generates a hash value for a ulint integer
 to a hash table of size table_size, which should be a prime
 or some random number for the hash table to work reliably.
 @return hash value */
static uint64_t ut_hash_ulint(
    /*==========*/
    uint64_t key,        /*!< in: value to be hashed */
    uint64_t table_size) /*!< in: hash table size */
{
  key = key ^ UT_HASH_RANDOM_MASK2;

  return (key % table_size);
}

/**************************************************************/ /**
 Calculates the hash value from a folded value.
 @return hashed value */
uint64_t hash_calc_hash(
    /*===========*/
    uint64_t fold,       /*!< in: folded value */
    hash_table_t *table) /*!< in: hash table */
{
  return (ut_hash_ulint(fold, table->n_cells));
}

/************************************************************/ /**
 Gets the nth cell in a hash table.
 @return pointer to cell */
hash_cell_t *hash_get_nth_cell(
    /*==============*/
    hash_table_t *table, /*!< in: hash table */
    uint64_t n)          /*!< in: cell index */
{
  return (table->array + n);
}
