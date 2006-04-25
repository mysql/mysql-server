/* A Bison parser, made by GNU Bison 1.875d.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



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
     PARS_CONSISTENT_TOKEN = 298,
     PARS_READ_TOKEN = 299,
     PARS_ORDER_TOKEN = 300,
     PARS_BY_TOKEN = 301,
     PARS_ASC_TOKEN = 302,
     PARS_DESC_TOKEN = 303,
     PARS_INSERT_TOKEN = 304,
     PARS_INTO_TOKEN = 305,
     PARS_VALUES_TOKEN = 306,
     PARS_UPDATE_TOKEN = 307,
     PARS_SET_TOKEN = 308,
     PARS_DELETE_TOKEN = 309,
     PARS_CURRENT_TOKEN = 310,
     PARS_OF_TOKEN = 311,
     PARS_CREATE_TOKEN = 312,
     PARS_TABLE_TOKEN = 313,
     PARS_INDEX_TOKEN = 314,
     PARS_UNIQUE_TOKEN = 315,
     PARS_CLUSTERED_TOKEN = 316,
     PARS_DOES_NOT_FIT_IN_MEM_TOKEN = 317,
     PARS_ON_TOKEN = 318,
     PARS_ASSIGN_TOKEN = 319,
     PARS_DECLARE_TOKEN = 320,
     PARS_CURSOR_TOKEN = 321,
     PARS_SQL_TOKEN = 322,
     PARS_OPEN_TOKEN = 323,
     PARS_FETCH_TOKEN = 324,
     PARS_CLOSE_TOKEN = 325,
     PARS_NOTFOUND_TOKEN = 326,
     PARS_TO_CHAR_TOKEN = 327,
     PARS_TO_NUMBER_TOKEN = 328,
     PARS_TO_BINARY_TOKEN = 329,
     PARS_BINARY_TO_NUMBER_TOKEN = 330,
     PARS_SUBSTR_TOKEN = 331,
     PARS_REPLSTR_TOKEN = 332,
     PARS_CONCAT_TOKEN = 333,
     PARS_INSTR_TOKEN = 334,
     PARS_LENGTH_TOKEN = 335,
     PARS_SYSDATE_TOKEN = 336,
     PARS_PRINTF_TOKEN = 337,
     PARS_ASSERT_TOKEN = 338,
     PARS_RND_TOKEN = 339,
     PARS_RND_STR_TOKEN = 340,
     PARS_ROW_PRINTF_TOKEN = 341,
     PARS_COMMIT_TOKEN = 342,
     PARS_ROLLBACK_TOKEN = 343,
     PARS_WORK_TOKEN = 344,
     PARS_UNSIGNED_TOKEN = 345,
     PARS_EXIT_TOKEN = 346,
     PARS_FUNCTION_TOKEN = 347,
     NEG = 348
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
#define PARS_CONSISTENT_TOKEN 298
#define PARS_READ_TOKEN 299
#define PARS_ORDER_TOKEN 300
#define PARS_BY_TOKEN 301
#define PARS_ASC_TOKEN 302
#define PARS_DESC_TOKEN 303
#define PARS_INSERT_TOKEN 304
#define PARS_INTO_TOKEN 305
#define PARS_VALUES_TOKEN 306
#define PARS_UPDATE_TOKEN 307
#define PARS_SET_TOKEN 308
#define PARS_DELETE_TOKEN 309
#define PARS_CURRENT_TOKEN 310
#define PARS_OF_TOKEN 311
#define PARS_CREATE_TOKEN 312
#define PARS_TABLE_TOKEN 313
#define PARS_INDEX_TOKEN 314
#define PARS_UNIQUE_TOKEN 315
#define PARS_CLUSTERED_TOKEN 316
#define PARS_DOES_NOT_FIT_IN_MEM_TOKEN 317
#define PARS_ON_TOKEN 318
#define PARS_ASSIGN_TOKEN 319
#define PARS_DECLARE_TOKEN 320
#define PARS_CURSOR_TOKEN 321
#define PARS_SQL_TOKEN 322
#define PARS_OPEN_TOKEN 323
#define PARS_FETCH_TOKEN 324
#define PARS_CLOSE_TOKEN 325
#define PARS_NOTFOUND_TOKEN 326
#define PARS_TO_CHAR_TOKEN 327
#define PARS_TO_NUMBER_TOKEN 328
#define PARS_TO_BINARY_TOKEN 329
#define PARS_BINARY_TO_NUMBER_TOKEN 330
#define PARS_SUBSTR_TOKEN 331
#define PARS_REPLSTR_TOKEN 332
#define PARS_CONCAT_TOKEN 333
#define PARS_INSTR_TOKEN 334
#define PARS_LENGTH_TOKEN 335
#define PARS_SYSDATE_TOKEN 336
#define PARS_PRINTF_TOKEN 337
#define PARS_ASSERT_TOKEN 338
#define PARS_RND_TOKEN 339
#define PARS_RND_STR_TOKEN 340
#define PARS_ROW_PRINTF_TOKEN 341
#define PARS_COMMIT_TOKEN 342
#define PARS_ROLLBACK_TOKEN 343
#define PARS_WORK_TOKEN 344
#define PARS_UNSIGNED_TOKEN 345
#define PARS_EXIT_TOKEN 346
#define PARS_FUNCTION_TOKEN 347
#define NEG 348




/* Copy the first part of user declarations.  */
#line 13 "pars0grm.y"

/* The value of the semantic attribute is a pointer to a query tree node
que_node_t */

#include "univ.i"
#include <math.h>				/* Can't be before univ.i */
#include "pars0pars.h"
#include "mem0mem.h"
#include "que0types.h"
#include "que0que.h"
#include "row0sel.h"

#define YYSTYPE que_node_t*

/* #define __STDC__ */

int
yylex(void);


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 293 "pars0grm.tab.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   define YYSTACK_ALLOC alloca
#  endif
# else
#  if defined (alloca) || defined (_ALLOCA_H)
#   define YYSTACK_ALLOC alloca
#  else
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  99
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   756

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  109
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  69
/* YYNRULES -- Number of rules. */
#define YYNRULES  175
/* YYNRULES -- Number of states. */
#define YYNSTATES  337

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   348

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   101,     2,     2,
     103,   104,    98,    97,   106,    96,     2,    99,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   102,
      94,    93,    95,   105,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   107,     2,   108,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,   100
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     6,     8,    11,    14,    17,    20,    23,
      26,    29,    32,    35,    38,    41,    44,    47,    50,    53,
      56,    59,    62,    65,    68,    71,    73,    76,    78,    83,
      85,    87,    89,    91,    93,    95,    97,   101,   105,   109,
     113,   116,   120,   124,   128,   132,   136,   140,   144,   148,
     152,   155,   159,   163,   165,   167,   169,   171,   173,   175,
     177,   179,   181,   183,   185,   186,   188,   192,   199,   204,
     206,   208,   210,   214,   216,   220,   221,   223,   227,   228,
     230,   234,   236,   241,   247,   252,   253,   255,   259,   261,
     265,   267,   268,   271,   272,   275,   276,   279,   280,   282,
     284,   285,   290,   299,   303,   309,   312,   316,   318,   322,
     327,   332,   335,   338,   342,   345,   348,   351,   355,   360,
     362,   365,   366,   369,   371,   379,   386,   397,   399,   401,
     404,   407,   412,   417,   423,   425,   429,   430,   434,   435,
     437,   438,   441,   442,   444,   452,   454,   458,   459,   461,
     462,   464,   475,   478,   481,   483,   485,   487,   489,   491,
     495,   499,   500,   502,   506,   510,   511,   513,   516,   523,
     528,   530,   532,   533,   535,   538
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     110,     0,    -1,   177,   102,    -1,   115,    -1,   116,   102,
      -1,   148,   102,    -1,   149,   102,    -1,   150,   102,    -1,
     147,   102,    -1,   151,   102,    -1,   143,   102,    -1,   130,
     102,    -1,   132,   102,    -1,   142,   102,    -1,   140,   102,
      -1,   141,   102,    -1,   137,   102,    -1,   138,   102,    -1,
     152,   102,    -1,   154,   102,    -1,   153,   102,    -1,   166,
     102,    -1,   167,   102,    -1,   161,   102,    -1,   165,   102,
      -1,   110,    -1,   111,   110,    -1,     9,    -1,   113,   103,
     121,   104,    -1,     3,    -1,     4,    -1,     5,    -1,     6,
      -1,     7,    -1,     8,    -1,    67,    -1,   112,    97,   112,
      -1,   112,    96,   112,    -1,   112,    98,   112,    -1,   112,
      99,   112,    -1,    96,   112,    -1,   103,   112,   104,    -1,
     112,    93,   112,    -1,   112,    94,   112,    -1,   112,    95,
     112,    -1,   112,    13,   112,    -1,   112,    14,   112,    -1,
     112,    15,   112,    -1,   112,    10,   112,    -1,   112,    11,
     112,    -1,    12,   112,    -1,     9,   101,    71,    -1,    67,
     101,    71,    -1,    72,    -1,    73,    -1,    74,    -1,    75,
      -1,    76,    -1,    78,    -1,    79,    -1,    80,    -1,    81,
      -1,    84,    -1,    85,    -1,    -1,   105,    -1,   114,   106,
     105,    -1,   107,     9,   103,   114,   104,   108,    -1,   117,
     103,   121,   104,    -1,    77,    -1,    82,    -1,    83,    -1,
       9,   103,   104,    -1,     9,    -1,   119,   106,     9,    -1,
      -1,     9,    -1,   120,   106,     9,    -1,    -1,   112,    -1,
     121,   106,   112,    -1,   112,    -1,    37,   103,    98,   104,
      -1,    37,   103,    38,     9,   104,    -1,    36,   103,   112,
     104,    -1,    -1,   122,    -1,   123,   106,   122,    -1,    98,
      -1,   123,    50,   120,    -1,   123,    -1,    -1,    40,   112,
      -1,    -1,    41,    52,    -1,    -1,    43,    44,    -1,    -1,
      47,    -1,    48,    -1,    -1,    45,    46,     9,   128,    -1,
      35,   124,    39,   119,   125,   126,   127,   129,    -1,    49,
      50,     9,    -1,   131,    51,   103,   121,   104,    -1,   131,
     130,    -1,     9,    93,   112,    -1,   133,    -1,   134,   106,
     133,    -1,    40,    55,    56,     9,    -1,    52,     9,    53,
     134,    -1,   136,   125,    -1,   136,   135,    -1,    54,    39,
       9,    -1,   139,   125,    -1,   139,   135,    -1,    86,   130,
      -1,     9,    64,   112,    -1,    31,   112,    29,   111,    -1,
     144,    -1,   145,   144,    -1,    -1,    30,   111,    -1,   145,
      -1,    28,   112,    29,   111,   146,    27,    28,    -1,    33,
     112,    32,   111,    27,    32,    -1,    41,     9,    17,   112,
      42,   112,    32,   111,    27,    32,    -1,    91,    -1,    34,
      -1,    68,     9,    -1,    70,     9,    -1,    69,     9,    50,
     120,    -1,    69,     9,    50,   118,    -1,     9,   168,   157,
     158,   159,    -1,   155,    -1,   156,   106,   155,    -1,    -1,
     103,     3,   104,    -1,    -1,    90,    -1,    -1,    12,     8,
      -1,    -1,    62,    -1,    57,    58,     9,   103,   156,   104,
     160,    -1,     9,    -1,   162,   106,     9,    -1,    -1,    60,
      -1,    -1,    61,    -1,    57,   163,   164,    59,     9,    63,
       9,   103,   162,   104,    -1,    87,    89,    -1,    88,    89,
      -1,    21,    -1,    22,    -1,    24,    -1,    19,    -1,    20,
      -1,     9,    17,   168,    -1,     9,    18,   168,    -1,    -1,
     169,    -1,   170,   106,   169,    -1,     9,   168,   102,    -1,
      -1,   171,    -1,   172,   171,    -1,    65,    66,     9,    25,
     130,   102,    -1,    65,    92,     9,   102,    -1,   173,    -1,
     174,    -1,    -1,   175,    -1,   176,   175,    -1,    16,     9,
     103,   170,   104,    25,   172,   176,    26,   111,    27,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   136,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   162,   163,   168,   169,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   195,   200,   201,   202,   203,   205,   206,   207,
     208,   209,   210,   211,   214,   216,   217,   221,   226,   231,
     232,   233,   237,   241,   242,   247,   248,   249,   254,   255,
     256,   260,   261,   266,   272,   279,   280,   281,   286,   288,
     290,   294,   295,   299,   300,   305,   306,   311,   312,   313,
     317,   318,   323,   333,   338,   340,   345,   349,   350,   355,
     361,   368,   373,   378,   384,   389,   394,   399,   404,   410,
     411,   416,   417,   419,   423,   430,   436,   444,   448,   452,
     458,   464,   466,   471,   476,   477,   482,   483,   488,   489,
     495,   496,   502,   503,   509,   515,   516,   521,   522,   526,
     527,   531,   539,   544,   549,   550,   551,   552,   553,   557,
     560,   566,   567,   568,   573,   577,   579,   580,   584,   590,
     595,   596,   599,   601,   602,   606
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PARS_INT_LIT", "PARS_FLOAT_LIT",
  "PARS_STR_LIT", "PARS_FIXBINARY_LIT", "PARS_BLOB_LIT", "PARS_NULL_LIT",
  "PARS_ID_TOKEN", "PARS_AND_TOKEN", "PARS_OR_TOKEN", "PARS_NOT_TOKEN",
  "PARS_GE_TOKEN", "PARS_LE_TOKEN", "PARS_NE_TOKEN",
  "PARS_PROCEDURE_TOKEN", "PARS_IN_TOKEN", "PARS_OUT_TOKEN",
  "PARS_BINARY_TOKEN", "PARS_BLOB_TOKEN", "PARS_INT_TOKEN",
  "PARS_INTEGER_TOKEN", "PARS_FLOAT_TOKEN", "PARS_CHAR_TOKEN",
  "PARS_IS_TOKEN", "PARS_BEGIN_TOKEN", "PARS_END_TOKEN", "PARS_IF_TOKEN",
  "PARS_THEN_TOKEN", "PARS_ELSE_TOKEN", "PARS_ELSIF_TOKEN",
  "PARS_LOOP_TOKEN", "PARS_WHILE_TOKEN", "PARS_RETURN_TOKEN",
  "PARS_SELECT_TOKEN", "PARS_SUM_TOKEN", "PARS_COUNT_TOKEN",
  "PARS_DISTINCT_TOKEN", "PARS_FROM_TOKEN", "PARS_WHERE_TOKEN",
  "PARS_FOR_TOKEN", "PARS_DDOT_TOKEN", "PARS_CONSISTENT_TOKEN",
  "PARS_READ_TOKEN", "PARS_ORDER_TOKEN", "PARS_BY_TOKEN", "PARS_ASC_TOKEN",
  "PARS_DESC_TOKEN", "PARS_INSERT_TOKEN", "PARS_INTO_TOKEN",
  "PARS_VALUES_TOKEN", "PARS_UPDATE_TOKEN", "PARS_SET_TOKEN",
  "PARS_DELETE_TOKEN", "PARS_CURRENT_TOKEN", "PARS_OF_TOKEN",
  "PARS_CREATE_TOKEN", "PARS_TABLE_TOKEN", "PARS_INDEX_TOKEN",
  "PARS_UNIQUE_TOKEN", "PARS_CLUSTERED_TOKEN",
  "PARS_DOES_NOT_FIT_IN_MEM_TOKEN", "PARS_ON_TOKEN", "PARS_ASSIGN_TOKEN",
  "PARS_DECLARE_TOKEN", "PARS_CURSOR_TOKEN", "PARS_SQL_TOKEN",
  "PARS_OPEN_TOKEN", "PARS_FETCH_TOKEN", "PARS_CLOSE_TOKEN",
  "PARS_NOTFOUND_TOKEN", "PARS_TO_CHAR_TOKEN", "PARS_TO_NUMBER_TOKEN",
  "PARS_TO_BINARY_TOKEN", "PARS_BINARY_TO_NUMBER_TOKEN",
  "PARS_SUBSTR_TOKEN", "PARS_REPLSTR_TOKEN", "PARS_CONCAT_TOKEN",
  "PARS_INSTR_TOKEN", "PARS_LENGTH_TOKEN", "PARS_SYSDATE_TOKEN",
  "PARS_PRINTF_TOKEN", "PARS_ASSERT_TOKEN", "PARS_RND_TOKEN",
  "PARS_RND_STR_TOKEN", "PARS_ROW_PRINTF_TOKEN", "PARS_COMMIT_TOKEN",
  "PARS_ROLLBACK_TOKEN", "PARS_WORK_TOKEN", "PARS_UNSIGNED_TOKEN",
  "PARS_EXIT_TOKEN", "PARS_FUNCTION_TOKEN", "'='", "'<'", "'>'", "'-'",
  "'+'", "'*'", "'/'", "NEG", "'%'", "';'", "'('", "')'", "'?'", "','",
  "'{'", "'}'", "$accept", "statement", "statement_list", "exp",
  "function_name", "question_mark_list", "stored_procedure_call",
  "predefined_procedure_call", "predefined_procedure_name",
  "user_function_call", "table_list", "variable_list", "exp_list",
  "select_item", "select_item_list", "select_list", "search_condition",
  "for_update_clause", "consistent_read_clause", "order_direction",
  "order_by_clause", "select_statement", "insert_statement_start",
  "insert_statement", "column_assignment", "column_assignment_list",
  "cursor_positioned", "update_statement_start",
  "update_statement_searched", "update_statement_positioned",
  "delete_statement_start", "delete_statement_searched",
  "delete_statement_positioned", "row_printf_statement",
  "assignment_statement", "elsif_element", "elsif_list", "else_part",
  "if_statement", "while_statement", "for_statement", "exit_statement",
  "return_statement", "open_cursor_statement", "close_cursor_statement",
  "fetch_statement", "column_def", "column_def_list", "opt_column_len",
  "opt_unsigned", "opt_not_null", "not_fit_in_memory", "create_table",
  "column_list", "unique_def", "clustered_def", "create_index",
  "commit_statement", "rollback_statement", "type_name",
  "parameter_declaration", "parameter_declaration_list",
  "variable_declaration", "variable_declaration_list",
  "cursor_declaration", "function_declaration", "declaration",
  "declaration_list", "procedure_definition", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,    61,    60,    62,    45,    43,    42,    47,
     348,    37,    59,    40,    41,    63,    44,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   109,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   111,   111,   112,   112,   112,
     112,   112,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   112,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   114,   114,   114,   115,   116,   117,
     117,   117,   118,   119,   119,   120,   120,   120,   121,   121,
     121,   122,   122,   122,   122,   123,   123,   123,   124,   124,
     124,   125,   125,   126,   126,   127,   127,   128,   128,   128,
     129,   129,   130,   131,   132,   132,   133,   134,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     145,   146,   146,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   154,   155,   156,   156,   157,   157,   158,   158,
     159,   159,   160,   160,   161,   162,   162,   163,   163,   164,
     164,   165,   166,   167,   168,   168,   168,   168,   168,   169,
     169,   170,   170,   170,   171,   172,   172,   172,   173,   174,
     175,   175,   176,   176,   176,   177
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     2,     1,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     1,     2,     1,     4,     1,
       1,     1,     1,     1,     1,     1,     3,     3,     3,     3,
       2,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       2,     3,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     1,     3,     6,     4,     1,
       1,     1,     3,     1,     3,     0,     1,     3,     0,     1,
       3,     1,     4,     5,     4,     0,     1,     3,     1,     3,
       1,     0,     2,     0,     2,     0,     2,     0,     1,     1,
       0,     4,     8,     3,     5,     2,     3,     1,     3,     4,
       4,     2,     2,     3,     2,     2,     2,     3,     4,     1,
       2,     0,     2,     1,     7,     6,    10,     1,     1,     2,
       2,     4,     4,     5,     1,     3,     0,     3,     0,     1,
       0,     2,     0,     1,     7,     1,     3,     0,     1,     0,
       1,    10,     2,     2,     1,     1,     1,     1,     1,     3,
       3,     0,     1,     3,     3,     0,     1,     2,     6,     4,
       1,     1,     0,     1,     2,    11
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,     0,     0,   128,    85,     0,     0,     0,
       0,   147,     0,     0,     0,    69,    70,    71,     0,     0,
       0,   127,     0,     0,     3,     0,     0,     0,     0,     0,
      91,     0,     0,    91,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,    30,    31,    32,    33,    34,    27,
       0,    35,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,     0,     0,     0,     0,     0,     0,     0,
      88,    81,    86,    90,     0,     0,     0,     0,     0,     0,
     148,   149,   129,     0,   130,   116,   152,   153,     0,     1,
       4,    78,    11,     0,   105,    12,     0,   111,   112,    16,
      17,   114,   115,    14,    15,    13,    10,     8,     5,     6,
       7,     9,    18,    20,    19,    23,    24,    21,    22,     2,
     117,   161,     0,    50,     0,    40,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      78,     0,     0,     0,    75,     0,     0,     0,   103,     0,
     113,     0,   150,     0,    75,    64,    79,     0,    78,     0,
      92,     0,   162,     0,    51,    52,    41,    48,    49,    45,
      46,    47,    25,   121,    42,    43,    44,    37,    36,    38,
      39,     0,     0,     0,     0,     0,    76,    89,    87,    73,
      91,     0,     0,   107,   110,     0,     0,    76,   132,   131,
      65,     0,    68,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    26,   119,   123,     0,    28,     0,    84,     0,
      82,     0,     0,     0,    93,     0,     0,     0,     0,   134,
       0,     0,     0,     0,     0,    80,   104,   109,   157,   158,
     154,   155,   156,   159,   160,   165,   163,   122,     0,   120,
       0,   125,    83,    77,    74,     0,    95,     0,   106,   108,
     136,   142,     0,     0,    72,    67,    66,     0,   166,   172,
       0,   124,    94,     0,   100,     0,     0,   138,   143,   144,
     135,     0,     0,     0,   167,   170,   171,   173,     0,   118,
      96,     0,   102,     0,     0,   139,   140,     0,   164,     0,
       0,     0,   174,     0,     0,   137,     0,   133,   145,     0,
       0,     0,     0,    97,   126,   141,   151,     0,     0,   169,
     175,    98,    99,   101,   146,     0,   168
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,   182,   183,   166,    76,   211,    24,    25,    26,   208,
     200,   197,   167,    82,    83,    84,   107,   266,   284,   333,
     302,    27,    28,    29,   203,   204,   108,    30,    31,    32,
      33,    34,    35,    36,    37,   223,   224,   225,    38,    39,
      40,    41,    42,    43,    44,    45,   239,   240,   287,   306,
     317,   289,    46,   319,    91,   163,    47,    48,    49,   253,
     172,   173,   278,   279,   295,   296,   297,   298,    50
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -209
static const short int yypact[] =
{
     578,   -30,    40,   256,   256,  -209,    19,    44,     7,    55,
      26,   -16,    62,    69,    73,  -209,  -209,  -209,    48,    -5,
      -4,  -209,    78,    75,  -209,   -13,   -15,    -6,   -18,     4,
      67,     6,    12,    67,    17,    18,    21,    29,    30,    32,
      33,    39,    47,    50,    51,    64,    65,    70,    82,    83,
      84,   256,    13,  -209,  -209,  -209,  -209,  -209,  -209,     8,
     256,    20,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,
    -209,  -209,  -209,   256,   256,   295,    15,   421,    77,    86,
    -209,   657,  -209,   -44,   129,   152,   178,   137,   182,   189,
    -209,   142,  -209,   154,  -209,  -209,  -209,  -209,   104,  -209,
    -209,   256,  -209,   105,  -209,  -209,   170,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,
     657,   200,   139,   582,   140,   198,    66,   256,   256,   256,
     256,   256,   578,   256,   256,   256,   256,   256,   256,   256,
     256,   578,   256,   -31,   205,   121,   206,   256,  -209,   207,
    -209,   115,  -209,   160,   212,   117,   657,   -63,   256,   167,
     657,    -2,  -209,   -59,  -209,  -209,  -209,   582,   582,    14,
      14,   657,  -209,   330,    14,    14,    14,     3,     3,   198,
     198,   -58,   392,   279,   217,   123,  -209,   122,  -209,  -209,
     -32,   607,   136,  -209,   124,   223,   224,   133,  -209,   122,
    -209,   -52,  -209,   256,   -46,   229,    16,    16,   214,   200,
     578,   256,  -209,  -209,   209,   220,  -209,   221,  -209,   148,
    -209,   232,   256,   247,   226,   256,   256,   207,    16,  -209,
     -43,   195,   165,   162,   166,   657,  -209,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,   263,  -209,   578,   483,  -209,
     246,  -209,  -209,  -209,  -209,   225,   233,   626,   657,  -209,
     172,   216,   223,   270,  -209,  -209,  -209,    16,  -209,     1,
     578,  -209,  -209,   236,   237,   578,   278,   193,  -209,  -209,
    -209,   181,   183,   -53,  -209,  -209,  -209,  -209,   -14,   578,
    -209,   240,  -209,   454,   184,  -209,   275,   282,  -209,   286,
     287,   578,  -209,   288,   266,  -209,   292,  -209,  -209,   -36,
     276,   202,   516,   -28,  -209,  -209,  -209,   293,    48,  -209,
    -209,  -209,  -209,  -209,  -209,   210,  -209
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -209,     0,  -130,    -1,  -209,  -209,  -209,  -209,  -209,  -209,
    -209,   143,  -136,   158,  -209,  -209,   -29,  -209,  -209,  -209,
    -209,   -17,  -209,  -209,    79,  -209,   281,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,    91,  -209,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,  -209,    45,  -209,  -209,  -209,
    -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -209,  -208,
      99,  -209,    41,  -209,  -209,  -209,    23,  -209,  -209
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned short int yytable[] =
{
      23,    95,    75,    77,   111,    81,   154,   194,   232,   254,
     277,   104,   311,   309,   191,   216,   217,     6,   141,   331,
     332,   192,    53,    54,    55,    56,    57,    58,    59,   141,
     270,    60,   214,   103,    51,   248,   249,   250,   251,   310,
     252,   212,    89,   213,    90,   218,   226,   219,   213,    52,
     130,   293,   243,    85,   244,    78,    79,    86,   246,   133,
     213,   271,   155,   272,    87,    88,   293,   195,   326,   292,
     327,    92,   135,   136,   233,    99,   137,   138,    93,   139,
     140,   141,    94,     6,    96,    97,    61,    98,   101,   100,
     257,    62,    63,    64,    65,    66,   102,    67,    68,    69,
      70,   148,   149,    71,    72,   170,   105,   106,   109,   132,
     146,   147,   148,   149,   110,    73,   131,    80,   150,   113,
     114,   134,    74,   115,    53,    54,    55,    56,    57,    58,
      59,   116,   117,    60,   118,   119,   177,   178,   179,   180,
     181,   120,   184,   185,   186,   187,   188,   189,   190,   121,
     299,   193,   122,   123,    81,   303,   201,    78,    79,   143,
     144,   145,   146,   147,   148,   149,   124,   125,   156,   157,
     176,   234,   126,    53,    54,    55,    56,    57,    58,    59,
     152,   322,    60,   222,   127,   128,   129,   158,    61,   153,
     159,   160,   222,    62,    63,    64,    65,    66,   161,    67,
      68,    69,    70,   162,   164,    71,    72,   165,   168,   171,
     174,   175,   245,   141,   196,   199,   202,    73,   205,   206,
     258,   207,   210,   215,    74,   169,   229,   230,   231,   236,
     237,   170,   238,   241,   267,   268,   242,    61,   247,   255,
     221,   263,    62,    63,    64,    65,    66,   260,    67,    68,
      69,    70,   262,   261,    71,    72,   264,   222,   273,    53,
      54,    55,    56,    57,    58,    59,    73,   265,    60,   274,
     275,   276,   277,    74,   281,   286,   283,   282,   288,   291,
     300,   304,   301,   305,   307,   308,   313,   316,   315,   137,
     138,   318,   139,   140,   141,   320,   321,   323,   324,   222,
     325,   328,   334,   222,   329,   137,   138,   209,   139,   140,
     141,   335,   336,   198,   112,   259,   269,   290,   256,     0,
     294,   312,   222,    61,   142,     0,     0,     0,    62,    63,
      64,    65,    66,     0,    67,    68,    69,    70,     0,     1,
      71,    72,     0,     0,     0,     0,     2,     0,     0,     0,
       0,     0,    73,     0,     0,     0,     0,     0,     3,    74,
     220,   221,     0,     4,     5,     6,     0,     0,     0,     0,
       0,     7,   143,   144,   145,   146,   147,   148,   149,     8,
       0,     0,     9,   228,    10,     0,     0,    11,   143,   144,
     145,   146,   147,   148,   149,     0,     0,     0,    12,    13,
      14,     1,     0,     0,     0,     0,     0,    15,     2,     0,
       0,     0,    16,    17,     0,     0,    18,    19,    20,   227,
       3,    21,     0,     0,     0,     4,     5,     6,     0,     0,
       0,   137,   138,     7,   139,   140,   141,    22,     0,     0,
       0,     8,     0,     0,     9,     0,    10,     0,     0,    11,
       0,     0,     0,   151,     0,     0,     0,     0,     0,     0,
      12,    13,    14,     1,     0,     0,     0,     0,     0,    15,
       2,     0,     0,     0,    16,    17,     0,     0,    18,    19,
      20,   314,     3,    21,     0,     0,     0,     4,     5,     6,
       0,     0,     0,   137,   138,     7,   139,   140,   141,    22,
       0,     0,     0,     8,     0,     0,     9,     0,    10,     0,
       0,    11,   280,     0,   143,   144,   145,   146,   147,   148,
     149,     0,    12,    13,    14,     1,     0,     0,     0,     0,
       0,    15,     2,     0,     0,     0,    16,    17,     0,     0,
      18,    19,    20,   330,     3,    21,     0,     0,     0,     4,
       5,     6,     0,     0,     0,     0,     0,     7,     0,     0,
       0,    22,     0,     0,     0,     8,     0,     0,     9,     0,
      10,     0,     0,    11,     0,     0,   143,   144,   145,   146,
     147,   148,   149,     0,    12,    13,    14,     1,     0,     0,
       0,     0,     0,    15,     2,   139,   140,   141,    16,    17,
       0,     0,    18,    19,    20,     0,     3,    21,     0,     0,
       0,     4,     5,     6,     0,     0,     0,   137,   138,     7,
     139,   140,   141,    22,     0,     0,     0,     8,     0,     0,
       9,     0,    10,     0,     0,    11,   137,   138,     0,   139,
     140,   141,     0,     0,     0,     0,    12,    13,    14,   235,
       0,     0,     0,     0,     0,    15,     0,     0,   285,     0,
      16,    17,     0,     0,    18,    19,    20,   137,   138,    21,
     139,   140,   141,     0,     0,   143,   144,   145,   146,   147,
     148,   149,     0,     0,     0,    22,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     143,   144,   145,   146,   147,   148,   149,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   143,
     144,   145,   146,   147,   148,   149,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     143,   144,   145,   146,   147,   148,   149
};

static const short int yycheck[] =
{
       0,    18,     3,     4,    33,     6,    50,    38,    40,   217,
       9,    28,    26,    66,   150,    17,    18,    35,    15,    47,
      48,   151,     3,     4,     5,     6,     7,     8,     9,    15,
     238,    12,   168,    51,    64,    19,    20,    21,    22,    92,
      24,   104,    58,   106,    60,   104,   104,   106,   106,     9,
      51,    65,   104,     9,   106,    36,    37,    50,   104,    60,
     106,   104,   106,   106,     9,    39,    65,    98,   104,   277,
     106,     9,    73,    74,   106,     0,    10,    11,     9,    13,
      14,    15,     9,    35,    89,    89,    67,     9,   103,   102,
     220,    72,    73,    74,    75,    76,   102,    78,    79,    80,
      81,    98,    99,    84,    85,   106,   102,    40,   102,   101,
      96,    97,    98,    99,   102,    96,   103,    98,   103,   102,
     102,   101,   103,   102,     3,     4,     5,     6,     7,     8,
       9,   102,   102,    12,   102,   102,   137,   138,   139,   140,
     141,   102,   143,   144,   145,   146,   147,   148,   149,   102,
     280,   152,   102,   102,   155,   285,   157,    36,    37,    93,
      94,    95,    96,    97,    98,    99,   102,   102,    39,    17,
     104,   200,   102,     3,     4,     5,     6,     7,     8,     9,
     103,   311,    12,   183,   102,   102,   102,     9,    67,   103,
      53,     9,   192,    72,    73,    74,    75,    76,     9,    78,
      79,    80,    81,    61,    50,    84,    85,   103,   103,     9,
      71,    71,   213,    15,     9,     9,     9,    96,   103,    59,
     221,     9,   105,    56,   103,    55,     9,   104,   106,    93,
     106,   232,     9,     9,   235,   236,   103,    67,     9,    25,
      31,     9,    72,    73,    74,    75,    76,    27,    78,    79,
      80,    81,   104,    32,    84,    85,     9,   257,    63,     3,
       4,     5,     6,     7,     8,     9,    96,    41,    12,   104,
     108,   105,     9,   103,    28,   103,    43,    52,    62,     9,
      44,     3,    45,    90,   103,   102,    46,    12,   104,    10,
      11,     9,    13,    14,    15,     9,     9,     9,    32,   299,
       8,    25,     9,   303,   102,    10,    11,   164,    13,    14,
      15,   328,   102,   155,    33,   224,   237,   272,   219,    -1,
     279,   298,   322,    67,    29,    -1,    -1,    -1,    72,    73,
      74,    75,    76,    -1,    78,    79,    80,    81,    -1,     9,
      84,    85,    -1,    -1,    -1,    -1,    16,    -1,    -1,    -1,
      -1,    -1,    96,    -1,    -1,    -1,    -1,    -1,    28,   103,
      30,    31,    -1,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    41,    93,    94,    95,    96,    97,    98,    99,    49,
      -1,    -1,    52,   104,    54,    -1,    -1,    57,    93,    94,
      95,    96,    97,    98,    99,    -1,    -1,    -1,    68,    69,
      70,     9,    -1,    -1,    -1,    -1,    -1,    77,    16,    -1,
      -1,    -1,    82,    83,    -1,    -1,    86,    87,    88,    27,
      28,    91,    -1,    -1,    -1,    33,    34,    35,    -1,    -1,
      -1,    10,    11,    41,    13,    14,    15,   107,    -1,    -1,
      -1,    49,    -1,    -1,    52,    -1,    54,    -1,    -1,    57,
      -1,    -1,    -1,    32,    -1,    -1,    -1,    -1,    -1,    -1,
      68,    69,    70,     9,    -1,    -1,    -1,    -1,    -1,    77,
      16,    -1,    -1,    -1,    82,    83,    -1,    -1,    86,    87,
      88,    27,    28,    91,    -1,    -1,    -1,    33,    34,    35,
      -1,    -1,    -1,    10,    11,    41,    13,    14,    15,   107,
      -1,    -1,    -1,    49,    -1,    -1,    52,    -1,    54,    -1,
      -1,    57,    29,    -1,    93,    94,    95,    96,    97,    98,
      99,    -1,    68,    69,    70,     9,    -1,    -1,    -1,    -1,
      -1,    77,    16,    -1,    -1,    -1,    82,    83,    -1,    -1,
      86,    87,    88,    27,    28,    91,    -1,    -1,    -1,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    41,    -1,    -1,
      -1,   107,    -1,    -1,    -1,    49,    -1,    -1,    52,    -1,
      54,    -1,    -1,    57,    -1,    -1,    93,    94,    95,    96,
      97,    98,    99,    -1,    68,    69,    70,     9,    -1,    -1,
      -1,    -1,    -1,    77,    16,    13,    14,    15,    82,    83,
      -1,    -1,    86,    87,    88,    -1,    28,    91,    -1,    -1,
      -1,    33,    34,    35,    -1,    -1,    -1,    10,    11,    41,
      13,    14,    15,   107,    -1,    -1,    -1,    49,    -1,    -1,
      52,    -1,    54,    -1,    -1,    57,    10,    11,    -1,    13,
      14,    15,    -1,    -1,    -1,    -1,    68,    69,    70,    42,
      -1,    -1,    -1,    -1,    -1,    77,    -1,    -1,    32,    -1,
      82,    83,    -1,    -1,    86,    87,    88,    10,    11,    91,
      13,    14,    15,    -1,    -1,    93,    94,    95,    96,    97,
      98,    99,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      93,    94,    95,    96,    97,    98,    99,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,
      94,    95,    96,    97,    98,    99,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      93,    94,    95,    96,    97,    98,    99
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     9,    16,    28,    33,    34,    35,    41,    49,    52,
      54,    57,    68,    69,    70,    77,    82,    83,    86,    87,
      88,    91,   107,   110,   115,   116,   117,   130,   131,   132,
     136,   137,   138,   139,   140,   141,   142,   143,   147,   148,
     149,   150,   151,   152,   153,   154,   161,   165,   166,   167,
     177,    64,     9,     3,     4,     5,     6,     7,     8,     9,
      12,    67,    72,    73,    74,    75,    76,    78,    79,    80,
      81,    84,    85,    96,   103,   112,   113,   112,    36,    37,
      98,   112,   122,   123,   124,     9,    50,     9,    39,    58,
      60,   163,     9,     9,     9,   130,    89,    89,     9,     0,
     102,   103,   102,    51,   130,   102,    40,   125,   135,   102,
     102,   125,   135,   102,   102,   102,   102,   102,   102,   102,
     102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
     112,   103,   101,   112,   101,   112,   112,    10,    11,    13,
      14,    15,    29,    93,    94,    95,    96,    97,    98,    99,
     103,    32,   103,   103,    50,   106,    39,    17,     9,    53,
       9,     9,    61,   164,    50,   103,   112,   121,   103,    55,
     112,     9,   169,   170,    71,    71,   104,   112,   112,   112,
     112,   112,   110,   111,   112,   112,   112,   112,   112,   112,
     112,   121,   111,   112,    38,    98,     9,   120,   122,     9,
     119,   112,     9,   133,   134,   103,    59,     9,   118,   120,
     105,   114,   104,   106,   121,    56,    17,    18,   104,   106,
      30,    31,   110,   144,   145,   146,   104,    27,   104,     9,
     104,   106,    40,   106,   125,    42,    93,   106,     9,   155,
     156,     9,   103,   104,   106,   112,   104,     9,    19,    20,
      21,    22,    24,   168,   168,    25,   169,   111,   112,   144,
      27,    32,   104,     9,     9,    41,   126,   112,   112,   133,
     168,   104,   106,    63,   104,   108,   105,     9,   171,   172,
      29,    28,    52,    43,   127,    32,   103,   157,    62,   160,
     155,     9,   168,    65,   171,   173,   174,   175,   176,   111,
      44,    45,   129,   111,     3,    90,   158,   103,   102,    66,
      92,    26,   175,    46,    27,   104,    12,   159,     9,   162,
       9,     9,   111,     9,    32,     8,   104,   106,    25,   102,
      27,    47,    48,   128,     9,   130,   102
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)		\
   ((Current).first_line   = (Rhs)[1].first_line,	\
    (Current).first_column = (Rhs)[1].first_column,	\
    (Current).last_line    = (Rhs)[N].last_line,	\
    (Current).last_column  = (Rhs)[N].last_column)
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if defined (YYMAXDEPTH) && YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;


  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 25:
#line 162 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 26:
#line 164 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 27:
#line 168 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 28:
#line 170 "pars0grm.y"
    { yyval = pars_func(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 29:
#line 171 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 30:
#line 172 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 31:
#line 173 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 32:
#line 174 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 33:
#line 175 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 34:
#line 176 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 35:
#line 177 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 36:
#line 178 "pars0grm.y"
    { yyval = pars_op('+', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 37:
#line 179 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 38:
#line 180 "pars0grm.y"
    { yyval = pars_op('*', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 39:
#line 181 "pars0grm.y"
    { yyval = pars_op('/', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 40:
#line 182 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[0], NULL); ;}
    break;

  case 41:
#line 183 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 42:
#line 184 "pars0grm.y"
    { yyval = pars_op('=', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 43:
#line 185 "pars0grm.y"
    { yyval = pars_op('<', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 44:
#line 186 "pars0grm.y"
    { yyval = pars_op('>', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 45:
#line 187 "pars0grm.y"
    { yyval = pars_op(PARS_GE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 46:
#line 188 "pars0grm.y"
    { yyval = pars_op(PARS_LE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 47:
#line 189 "pars0grm.y"
    { yyval = pars_op(PARS_NE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 48:
#line 190 "pars0grm.y"
    { yyval = pars_op(PARS_AND_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 49:
#line 191 "pars0grm.y"
    { yyval = pars_op(PARS_OR_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 50:
#line 192 "pars0grm.y"
    { yyval = pars_op(PARS_NOT_TOKEN, yyvsp[0], NULL); ;}
    break;

  case 51:
#line 194 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 52:
#line 196 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 53:
#line 200 "pars0grm.y"
    { yyval = &pars_to_char_token; ;}
    break;

  case 54:
#line 201 "pars0grm.y"
    { yyval = &pars_to_number_token; ;}
    break;

  case 55:
#line 202 "pars0grm.y"
    { yyval = &pars_to_binary_token; ;}
    break;

  case 56:
#line 204 "pars0grm.y"
    { yyval = &pars_binary_to_number_token; ;}
    break;

  case 57:
#line 205 "pars0grm.y"
    { yyval = &pars_substr_token; ;}
    break;

  case 58:
#line 206 "pars0grm.y"
    { yyval = &pars_concat_token; ;}
    break;

  case 59:
#line 207 "pars0grm.y"
    { yyval = &pars_instr_token; ;}
    break;

  case 60:
#line 208 "pars0grm.y"
    { yyval = &pars_length_token; ;}
    break;

  case 61:
#line 209 "pars0grm.y"
    { yyval = &pars_sysdate_token; ;}
    break;

  case 62:
#line 210 "pars0grm.y"
    { yyval = &pars_rnd_token; ;}
    break;

  case 63:
#line 211 "pars0grm.y"
    { yyval = &pars_rnd_str_token; ;}
    break;

  case 67:
#line 222 "pars0grm.y"
    { yyval = pars_stored_procedure_call(yyvsp[-4]); ;}
    break;

  case 68:
#line 227 "pars0grm.y"
    { yyval = pars_procedure_call(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 69:
#line 231 "pars0grm.y"
    { yyval = &pars_replstr_token; ;}
    break;

  case 70:
#line 232 "pars0grm.y"
    { yyval = &pars_printf_token; ;}
    break;

  case 71:
#line 233 "pars0grm.y"
    { yyval = &pars_assert_token; ;}
    break;

  case 72:
#line 237 "pars0grm.y"
    { yyval = yyvsp[-2]; ;}
    break;

  case 73:
#line 241 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 74:
#line 243 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 75:
#line 247 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 76:
#line 248 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 77:
#line 250 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 78:
#line 254 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 79:
#line 255 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]);;}
    break;

  case 80:
#line 256 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 81:
#line 260 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 82:
#line 262 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); ;}
    break;

  case 83:
#line 267 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, yyvsp[-1])))); ;}
    break;

  case 84:
#line 273 "pars0grm.y"
    { yyval = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									yyvsp[-1])); ;}
    break;

  case 85:
#line 279 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 86:
#line 280 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 87:
#line 282 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 88:
#line 286 "pars0grm.y"
    { yyval = pars_select_list(&pars_star_denoter,
								NULL); ;}
    break;

  case 89:
#line 289 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 90:
#line 290 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[0], NULL); ;}
    break;

  case 91:
#line 294 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 92:
#line 295 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 93:
#line 299 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 94:
#line 301 "pars0grm.y"
    { yyval = &pars_update_token; ;}
    break;

  case 95:
#line 305 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 96:
#line 307 "pars0grm.y"
    { yyval = &pars_consistent_token; ;}
    break;

  case 97:
#line 311 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 98:
#line 312 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 99:
#line 313 "pars0grm.y"
    { yyval = &pars_desc_token; ;}
    break;

  case 100:
#line 317 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 101:
#line 319 "pars0grm.y"
    { yyval = pars_order_by(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 102:
#line 328 "pars0grm.y"
    { yyval = pars_select_statement(yyvsp[-6], yyvsp[-4], yyvsp[-3],
								yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 103:
#line 334 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 104:
#line 339 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-4], yyvsp[-1], NULL); ;}
    break;

  case 105:
#line 341 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 106:
#line 345 "pars0grm.y"
    { yyval = pars_column_assignment(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 107:
#line 349 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 108:
#line 351 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 109:
#line 357 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 110:
#line 363 "pars0grm.y"
    { yyval = pars_update_statement_start(FALSE,
								yyvsp[-2], yyvsp[0]); ;}
    break;

  case 111:
#line 369 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 112:
#line 374 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 113:
#line 379 "pars0grm.y"
    { yyval = pars_update_statement_start(TRUE,
								yyvsp[0], NULL); ;}
    break;

  case 114:
#line 385 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 115:
#line 390 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 116:
#line 395 "pars0grm.y"
    { yyval = pars_row_printf_statement(yyvsp[0]); ;}
    break;

  case 117:
#line 400 "pars0grm.y"
    { yyval = pars_assignment_statement(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 118:
#line 406 "pars0grm.y"
    { yyval = pars_elsif_element(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 119:
#line 410 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 120:
#line 412 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 121:
#line 416 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 122:
#line 418 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 123:
#line 419 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 124:
#line 426 "pars0grm.y"
    { yyval = pars_if_statement(yyvsp[-5], yyvsp[-3], yyvsp[-2]); ;}
    break;

  case 125:
#line 432 "pars0grm.y"
    { yyval = pars_while_statement(yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 126:
#line 440 "pars0grm.y"
    { yyval = pars_for_statement(yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 127:
#line 444 "pars0grm.y"
    { yyval = pars_exit_statement(); ;}
    break;

  case 128:
#line 448 "pars0grm.y"
    { yyval = pars_return_statement(); ;}
    break;

  case 129:
#line 453 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_OPEN_CURSOR, yyvsp[0]); ;}
    break;

  case 130:
#line 459 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR, yyvsp[0]); ;}
    break;

  case 131:
#line 465 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], yyvsp[0], NULL); ;}
    break;

  case 132:
#line 467 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], NULL, yyvsp[0]); ;}
    break;

  case 133:
#line 472 "pars0grm.y"
    { yyval = pars_column_def(yyvsp[-4], yyvsp[-3], yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 134:
#line 476 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 135:
#line 478 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 136:
#line 482 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 137:
#line 484 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 138:
#line 488 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 139:
#line 490 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 140:
#line 495 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 141:
#line 497 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 142:
#line 502 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 143:
#line 504 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 144:
#line 511 "pars0grm.y"
    { yyval = pars_create_table(yyvsp[-4], yyvsp[-2], yyvsp[0]); ;}
    break;

  case 145:
#line 515 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 146:
#line 517 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 147:
#line 521 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 148:
#line 522 "pars0grm.y"
    { yyval = &pars_unique_token; ;}
    break;

  case 149:
#line 526 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 150:
#line 527 "pars0grm.y"
    { yyval = &pars_clustered_token; ;}
    break;

  case 151:
#line 535 "pars0grm.y"
    { yyval = pars_create_index(yyvsp[-8], yyvsp[-7], yyvsp[-5], yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 152:
#line 540 "pars0grm.y"
    { yyval = pars_commit_statement(); ;}
    break;

  case 153:
#line 545 "pars0grm.y"
    { yyval = pars_rollback_statement(); ;}
    break;

  case 154:
#line 549 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 155:
#line 550 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 156:
#line 551 "pars0grm.y"
    { yyval = &pars_char_token; ;}
    break;

  case 157:
#line 552 "pars0grm.y"
    { yyval = &pars_binary_token; ;}
    break;

  case 158:
#line 553 "pars0grm.y"
    { yyval = &pars_blob_token; ;}
    break;

  case 159:
#line 558 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_INPUT, yyvsp[0]); ;}
    break;

  case 160:
#line 561 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_OUTPUT, yyvsp[0]); ;}
    break;

  case 161:
#line 566 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 162:
#line 567 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 163:
#line 569 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 164:
#line 574 "pars0grm.y"
    { yyval = pars_variable_declaration(yyvsp[-2], yyvsp[-1]); ;}
    break;

  case 168:
#line 586 "pars0grm.y"
    { yyval = pars_cursor_declaration(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 169:
#line 591 "pars0grm.y"
    { yyval = pars_function_declaration(yyvsp[-1]); ;}
    break;

  case 175:
#line 612 "pars0grm.y"
    { yyval = pars_procedure_definition(yyvsp[-9], yyvsp[-7],
								yyvsp[-1]); ;}
    break;


    }

/* Line 1010 of yacc.c.  */
#line 2337 "pars0grm.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {
		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
		 yydestruct (yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
	  yydestruct (yytoken, &yylval);
	  yychar = YYEMPTY;

	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

  yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 616 "pars0grm.y"


