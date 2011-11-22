/* Copyright (c) 2011, Monty Program Ab
   Copyright (c) 2011, Oleksandr Byelkin

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

#ifndef ma_dyncol_h
#define ma_dyncol_h

#include <decimal.h>
#include <my_decimal_limits.h>
#include <mysql_time.h>

/*
  Max length for data in a dynamic colums. This comes from how the
  how the offset are stored.
*/
#define MAX_DYNAMIC_COLUMN_LENGTH 0X1FFFFFFFL

/* NO and OK is the same used just to show semantics */
#define ER_DYNCOL_NO ER_DYNCOL_OK

enum enum_dyncol_func_result
{
  ER_DYNCOL_OK= 0,
  ER_DYNCOL_YES= 1,                /* For functions returning 0/1 */
  ER_DYNCOL_FORMAT= -1,            /* Wrong format of the encoded string */
  ER_DYNCOL_LIMIT=  -2,            /* Some limit reached */
  ER_DYNCOL_RESOURCE= -3,          /* Out of resourses */
  ER_DYNCOL_DATA= -4,              /* Incorrect input data */
  ER_DYNCOL_UNKNOWN_CHARSET= -5    /* Unknown character set */
};

typedef DYNAMIC_STRING DYNAMIC_COLUMN;

enum enum_dynamic_column_type
{
  DYN_COL_NULL= 0,
  DYN_COL_INT,
  DYN_COL_UINT,
  DYN_COL_DOUBLE,
  DYN_COL_STRING,
  DYN_COL_DECIMAL,
  DYN_COL_DATETIME,
  DYN_COL_DATE,
  DYN_COL_TIME
};

typedef enum enum_dynamic_column_type DYNAMIC_COLUMN_TYPE;

struct st_dynamic_column_value
{
  DYNAMIC_COLUMN_TYPE type;
  union
  {
    long long long_value;
    unsigned long long ulong_value;
    double double_value;
    struct {
      LEX_STRING value;
      CHARSET_INFO *charset;
    } string;
    struct {
      decimal_digit_t buffer[DECIMAL_BUFF_LENGTH];
      decimal_t value;
    } decimal;
    MYSQL_TIME time_value;
  } x;
};

typedef struct st_dynamic_column_value DYNAMIC_COLUMN_VALUE;

enum enum_dyncol_func_result
dynamic_column_create(DYNAMIC_COLUMN *str,
                      uint column_nr, DYNAMIC_COLUMN_VALUE *value);

enum enum_dyncol_func_result
dynamic_column_create_many(DYNAMIC_COLUMN *str,
                           uint column_count,
                           uint *column_numbers,
                           DYNAMIC_COLUMN_VALUE *values);

enum enum_dyncol_func_result
dynamic_column_update(DYNAMIC_COLUMN *org, uint column_nr,
                      DYNAMIC_COLUMN_VALUE *value);
enum enum_dyncol_func_result
dynamic_column_update_many(DYNAMIC_COLUMN *str,
                           uint add_column_count,
                           uint *column_numbers,
                           DYNAMIC_COLUMN_VALUE *values);

enum enum_dyncol_func_result
dynamic_column_delete(DYNAMIC_COLUMN *org, uint column_nr);

enum enum_dyncol_func_result
dynamic_column_exists(DYNAMIC_COLUMN *org, uint column_nr);

/* List of not NULL columns */
enum enum_dyncol_func_result
dynamic_column_list(DYNAMIC_COLUMN *org, DYNAMIC_ARRAY *array_of_uint);

/*
   if the column do not exists it is NULL
*/
enum enum_dyncol_func_result
dynamic_column_get(DYNAMIC_COLUMN *org, uint column_nr,
                   DYNAMIC_COLUMN_VALUE *store_it_here);

#define dynamic_column_initialize(A) memset((A), 0, sizeof(*(A)))
#define dynamic_column_column_free(V) dynstr_free(V)

/***************************************************************************
 Internal functions, don't use if you don't know what you are doing...
***************************************************************************/

#define dynamic_column_reassociate(V,P,L, A) dynstr_reassociate((V),(P),(L),(A))

#define dynamic_column_value_init(V) (V)->type= DYN_COL_NULL

/*
  Prepare value for using as decimal
*/
void dynamic_column_prepare_decimal(DYNAMIC_COLUMN_VALUE *value);

#endif
