/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.
Copyright (c) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software
Foundation, Inc.

As a special exception, when this file is copied by Bison into a
Bison output file, you may use that output file without restriction.
This special exception was added by the Free Software Foundation
in version 1.24 of Bison.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/* A Bison parser, made by GNU Bison 1.875d.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     PARS_INT_LIT = 258,
     PARS_FLOAT_LIT = 259,
     PARS_STR_LIT = 260,
     PARS_FIXBINARY_LIT = 261,
     PARS_BLOB_LIT = 262,
     PARS_NULL_LIT = 263,
     PARS_ID_TOKEN = 264,
     PARS_AND_TOKEN = 265,
     PARS_OR_TOKEN = 266,
     PARS_NOT_TOKEN = 267,
     PARS_GE_TOKEN = 268,
     PARS_LE_TOKEN = 269,
     PARS_NE_TOKEN = 270,
     PARS_PROCEDURE_TOKEN = 271,
     PARS_IN_TOKEN = 272,
     PARS_OUT_TOKEN = 273,
     PARS_BINARY_TOKEN = 274,
     PARS_BLOB_TOKEN = 275,
     PARS_INT_TOKEN = 276,
     PARS_INTEGER_TOKEN = 277,
     PARS_FLOAT_TOKEN = 278,
     PARS_CHAR_TOKEN = 279,
     PARS_IS_TOKEN = 280,
     PARS_BEGIN_TOKEN = 281,
     PARS_END_TOKEN = 282,
     PARS_IF_TOKEN = 283,
     PARS_THEN_TOKEN = 284,
     PARS_ELSE_TOKEN = 285,
     PARS_ELSIF_TOKEN = 286,
     PARS_LOOP_TOKEN = 287,
     PARS_WHILE_TOKEN = 288,
     PARS_RETURN_TOKEN = 289,
     PARS_SELECT_TOKEN = 290,
     PARS_SUM_TOKEN = 291,
     PARS_COUNT_TOKEN = 292,
     PARS_DISTINCT_TOKEN = 293,
     PARS_FROM_TOKEN = 294,
     PARS_WHERE_TOKEN = 295,
     PARS_FOR_TOKEN = 296,
     PARS_DDOT_TOKEN = 297,
     PARS_READ_TOKEN = 298,
     PARS_ORDER_TOKEN = 299,
     PARS_BY_TOKEN = 300,
     PARS_ASC_TOKEN = 301,
     PARS_DESC_TOKEN = 302,
     PARS_INSERT_TOKEN = 303,
     PARS_INTO_TOKEN = 304,
     PARS_VALUES_TOKEN = 305,
     PARS_UPDATE_TOKEN = 306,
     PARS_SET_TOKEN = 307,
     PARS_DELETE_TOKEN = 308,
     PARS_CURRENT_TOKEN = 309,
     PARS_OF_TOKEN = 310,
     PARS_CREATE_TOKEN = 311,
     PARS_TABLE_TOKEN = 312,
     PARS_INDEX_TOKEN = 313,
     PARS_UNIQUE_TOKEN = 314,
     PARS_CLUSTERED_TOKEN = 315,
     PARS_DOES_NOT_FIT_IN_MEM_TOKEN = 316,
     PARS_ON_TOKEN = 317,
     PARS_ASSIGN_TOKEN = 318,
     PARS_DECLARE_TOKEN = 319,
     PARS_CURSOR_TOKEN = 320,
     PARS_SQL_TOKEN = 321,
     PARS_OPEN_TOKEN = 322,
     PARS_FETCH_TOKEN = 323,
     PARS_CLOSE_TOKEN = 324,
     PARS_NOTFOUND_TOKEN = 325,
     PARS_TO_CHAR_TOKEN = 326,
     PARS_TO_NUMBER_TOKEN = 327,
     PARS_TO_BINARY_TOKEN = 328,
     PARS_BINARY_TO_NUMBER_TOKEN = 329,
     PARS_SUBSTR_TOKEN = 330,
     PARS_REPLSTR_TOKEN = 331,
     PARS_CONCAT_TOKEN = 332,
     PARS_INSTR_TOKEN = 333,
     PARS_LENGTH_TOKEN = 334,
     PARS_SYSDATE_TOKEN = 335,
     PARS_PRINTF_TOKEN = 336,
     PARS_ASSERT_TOKEN = 337,
     PARS_RND_TOKEN = 338,
     PARS_RND_STR_TOKEN = 339,
     PARS_ROW_PRINTF_TOKEN = 340,
     PARS_COMMIT_TOKEN = 341,
     PARS_ROLLBACK_TOKEN = 342,
     PARS_WORK_TOKEN = 343,
     PARS_UNSIGNED_TOKEN = 344,
     PARS_EXIT_TOKEN = 345,
     PARS_FUNCTION_TOKEN = 346,
     PARS_LOCK_TOKEN = 347,
     PARS_SHARE_TOKEN = 348,
     PARS_MODE_TOKEN = 349,
     NEG = 350
   };
#endif
#define PARS_INT_LIT 258
#define PARS_FLOAT_LIT 259
#define PARS_STR_LIT 260
#define PARS_FIXBINARY_LIT 261
#define PARS_BLOB_LIT 262
#define PARS_NULL_LIT 263
#define PARS_ID_TOKEN 264
#define PARS_AND_TOKEN 265
#define PARS_OR_TOKEN 266
#define PARS_NOT_TOKEN 267
#define PARS_GE_TOKEN 268
#define PARS_LE_TOKEN 269
#define PARS_NE_TOKEN 270
#define PARS_PROCEDURE_TOKEN 271
#define PARS_IN_TOKEN 272
#define PARS_OUT_TOKEN 273
#define PARS_BINARY_TOKEN 274
#define PARS_BLOB_TOKEN 275
#define PARS_INT_TOKEN 276
#define PARS_INTEGER_TOKEN 277
#define PARS_FLOAT_TOKEN 278
#define PARS_CHAR_TOKEN 279
#define PARS_IS_TOKEN 280
#define PARS_BEGIN_TOKEN 281
#define PARS_END_TOKEN 282
#define PARS_IF_TOKEN 283
#define PARS_THEN_TOKEN 284
#define PARS_ELSE_TOKEN 285
#define PARS_ELSIF_TOKEN 286
#define PARS_LOOP_TOKEN 287
#define PARS_WHILE_TOKEN 288
#define PARS_RETURN_TOKEN 289
#define PARS_SELECT_TOKEN 290
#define PARS_SUM_TOKEN 291
#define PARS_COUNT_TOKEN 292
#define PARS_DISTINCT_TOKEN 293
#define PARS_FROM_TOKEN 294
#define PARS_WHERE_TOKEN 295
#define PARS_FOR_TOKEN 296
#define PARS_DDOT_TOKEN 297
#define PARS_READ_TOKEN 298
#define PARS_ORDER_TOKEN 299
#define PARS_BY_TOKEN 300
#define PARS_ASC_TOKEN 301
#define PARS_DESC_TOKEN 302
#define PARS_INSERT_TOKEN 303
#define PARS_INTO_TOKEN 304
#define PARS_VALUES_TOKEN 305
#define PARS_UPDATE_TOKEN 306
#define PARS_SET_TOKEN 307
#define PARS_DELETE_TOKEN 308
#define PARS_CURRENT_TOKEN 309
#define PARS_OF_TOKEN 310
#define PARS_CREATE_TOKEN 311
#define PARS_TABLE_TOKEN 312
#define PARS_INDEX_TOKEN 313
#define PARS_UNIQUE_TOKEN 314
#define PARS_CLUSTERED_TOKEN 315
#define PARS_DOES_NOT_FIT_IN_MEM_TOKEN 316
#define PARS_ON_TOKEN 317
#define PARS_ASSIGN_TOKEN 318
#define PARS_DECLARE_TOKEN 319
#define PARS_CURSOR_TOKEN 320
#define PARS_SQL_TOKEN 321
#define PARS_OPEN_TOKEN 322
#define PARS_FETCH_TOKEN 323
#define PARS_CLOSE_TOKEN 324
#define PARS_NOTFOUND_TOKEN 325
#define PARS_TO_CHAR_TOKEN 326
#define PARS_TO_NUMBER_TOKEN 327
#define PARS_TO_BINARY_TOKEN 328
#define PARS_BINARY_TO_NUMBER_TOKEN 329
#define PARS_SUBSTR_TOKEN 330
#define PARS_REPLSTR_TOKEN 331
#define PARS_CONCAT_TOKEN 332
#define PARS_INSTR_TOKEN 333
#define PARS_LENGTH_TOKEN 334
#define PARS_SYSDATE_TOKEN 335
#define PARS_PRINTF_TOKEN 336
#define PARS_ASSERT_TOKEN 337
#define PARS_RND_TOKEN 338
#define PARS_RND_STR_TOKEN 339
#define PARS_ROW_PRINTF_TOKEN 340
#define PARS_COMMIT_TOKEN 341
#define PARS_ROLLBACK_TOKEN 342
#define PARS_WORK_TOKEN 343
#define PARS_UNSIGNED_TOKEN 344
#define PARS_EXIT_TOKEN 345
#define PARS_FUNCTION_TOKEN 346
#define PARS_LOCK_TOKEN 347
#define PARS_SHARE_TOKEN 348
#define PARS_MODE_TOKEN 349
#define NEG 350




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



