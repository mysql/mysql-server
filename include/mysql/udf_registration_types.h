/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef UDF_REGISTRATION_TYPES_H
#define UDF_REGISTRATION_TYPES_H

typedef char my_bool;
typedef unsigned char uchar;
typedef long long int longlong;
typedef unsigned long ulong;


/**
Type of the user defined function return slot and arguments
*/
enum Item_result
{
  INVALID_RESULT= -1, /** not valid for UDFs */
  STRING_RESULT= 0,   /** char * */
  REAL_RESULT,        /** double */
  INT_RESULT,         /** longlong */
  ROW_RESULT,         /** not valid for UDFs */
  DECIMAL_RESULT      /** char *, to be converted to/from a decimal */
};

typedef struct st_udf_args
{
  unsigned int arg_count;		/**< Number of arguments */
  enum Item_result *arg_type;	        /**< Pointer to item_results */
  char **args;				/**< Pointer to argument */
  unsigned long *lengths;		/**< Length of string arguments */
  char *maybe_null;			/**< Set to 1 for all maybe_null args */
  char **attributes;                    /**< Pointer to attribute name */
  unsigned long *attribute_lengths;     /**< Length of attribute arguments */
  void *extension;
} UDF_ARGS;

/**
Information about the result of a user defined function

@todo add a notion for determinism of the UDF.

@sa Item_udf_func::update_used_tables()
*/
typedef struct st_udf_init
{
  my_bool maybe_null;          /** 1 if function can return NULL */
  unsigned int decimals;       /** for real functions */
  unsigned long max_length;    /** For string functions */
  char *ptr;                   /** free pointer for function data */
  my_bool const_item;          /** 1 if function always returns the same value */
  void *extension;
} UDF_INIT;



enum Item_udftype
{
  UDFTYPE_FUNCTION=1,
  UDFTYPE_AGGREGATE
};


typedef void(*Udf_func_clear)(UDF_INIT *, uchar *, uchar *);
typedef void(*Udf_func_add)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef void(*Udf_func_deinit)(UDF_INIT *);
typedef my_bool(*Udf_func_init)(UDF_INIT *, UDF_ARGS *, char *);
typedef void(*Udf_func_any)();
typedef double(*Udf_func_double)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef longlong(*Udf_func_longlong)(UDF_INIT *, UDF_ARGS *, uchar *,
                                     uchar *);
typedef char * (*Udf_func_string)(UDF_INIT *, UDF_ARGS *, char *,
                                  ulong *, uchar *, uchar *);

#endif /* UDF_REGISTRATION_TYPES_H */

