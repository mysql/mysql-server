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
#line 297 "pars0grm.tab.c"

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
#define YYLAST   706

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  111
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  69
/* YYNRULES -- Number of rules. */
#define YYNRULES  175
/* YYNRULES -- Number of states. */
#define YYNSTATES  339

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   350

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   103,     2,     2,
     105,   106,   100,    99,   108,    98,     2,   101,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   104,
      96,    95,    97,   107,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   109,     2,   110,     2,     2,     2,     2,
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
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
     102
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
     265,   267,   268,   271,   272,   275,   276,   281,   282,   284,
     286,   287,   292,   301,   305,   311,   314,   318,   320,   324,
     329,   334,   337,   340,   344,   347,   350,   353,   357,   362,
     364,   367,   368,   371,   373,   381,   388,   399,   401,   403,
     406,   409,   414,   419,   425,   427,   431,   432,   436,   437,
     439,   440,   443,   444,   446,   454,   456,   460,   461,   463,
     464,   466,   477,   480,   483,   485,   487,   489,   491,   493,
     497,   501,   502,   504,   508,   512,   513,   515,   518,   525,
     530,   532,   534,   535,   537,   540
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     112,     0,    -1,   179,   104,    -1,   117,    -1,   118,   104,
      -1,   150,   104,    -1,   151,   104,    -1,   152,   104,    -1,
     149,   104,    -1,   153,   104,    -1,   145,   104,    -1,   132,
     104,    -1,   134,   104,    -1,   144,   104,    -1,   142,   104,
      -1,   143,   104,    -1,   139,   104,    -1,   140,   104,    -1,
     154,   104,    -1,   156,   104,    -1,   155,   104,    -1,   168,
     104,    -1,   169,   104,    -1,   163,   104,    -1,   167,   104,
      -1,   112,    -1,   113,   112,    -1,     9,    -1,   115,   105,
     123,   106,    -1,     3,    -1,     4,    -1,     5,    -1,     6,
      -1,     7,    -1,     8,    -1,    66,    -1,   114,    99,   114,
      -1,   114,    98,   114,    -1,   114,   100,   114,    -1,   114,
     101,   114,    -1,    98,   114,    -1,   105,   114,   106,    -1,
     114,    95,   114,    -1,   114,    96,   114,    -1,   114,    97,
     114,    -1,   114,    13,   114,    -1,   114,    14,   114,    -1,
     114,    15,   114,    -1,   114,    10,   114,    -1,   114,    11,
     114,    -1,    12,   114,    -1,     9,   103,    70,    -1,    66,
     103,    70,    -1,    71,    -1,    72,    -1,    73,    -1,    74,
      -1,    75,    -1,    77,    -1,    78,    -1,    79,    -1,    80,
      -1,    83,    -1,    84,    -1,    -1,   107,    -1,   116,   108,
     107,    -1,   109,     9,   105,   116,   106,   110,    -1,   119,
     105,   123,   106,    -1,    76,    -1,    81,    -1,    82,    -1,
       9,   105,   106,    -1,     9,    -1,   121,   108,     9,    -1,
      -1,     9,    -1,   122,   108,     9,    -1,    -1,   114,    -1,
     123,   108,   114,    -1,   114,    -1,    37,   105,   100,   106,
      -1,    37,   105,    38,     9,   106,    -1,    36,   105,   114,
     106,    -1,    -1,   124,    -1,   125,   108,   124,    -1,   100,
      -1,   125,    49,   122,    -1,   125,    -1,    -1,    40,   114,
      -1,    -1,    41,    51,    -1,    -1,    92,    17,    93,    94,
      -1,    -1,    46,    -1,    47,    -1,    -1,    44,    45,     9,
     130,    -1,    35,   126,    39,   121,   127,   128,   129,   131,
      -1,    48,    49,     9,    -1,   133,    50,   105,   123,   106,
      -1,   133,   132,    -1,     9,    95,   114,    -1,   135,    -1,
     136,   108,   135,    -1,    40,    54,    55,     9,    -1,    51,
       9,    52,   136,    -1,   138,   127,    -1,   138,   137,    -1,
      53,    39,     9,    -1,   141,   127,    -1,   141,   137,    -1,
      85,   132,    -1,     9,    63,   114,    -1,    31,   114,    29,
     113,    -1,   146,    -1,   147,   146,    -1,    -1,    30,   113,
      -1,   147,    -1,    28,   114,    29,   113,   148,    27,    28,
      -1,    33,   114,    32,   113,    27,    32,    -1,    41,     9,
      17,   114,    42,   114,    32,   113,    27,    32,    -1,    90,
      -1,    34,    -1,    67,     9,    -1,    69,     9,    -1,    68,
       9,    49,   122,    -1,    68,     9,    49,   120,    -1,     9,
     170,   159,   160,   161,    -1,   157,    -1,   158,   108,   157,
      -1,    -1,   105,     3,   106,    -1,    -1,    89,    -1,    -1,
      12,     8,    -1,    -1,    61,    -1,    56,    57,     9,   105,
     158,   106,   162,    -1,     9,    -1,   164,   108,     9,    -1,
      -1,    59,    -1,    -1,    60,    -1,    56,   165,   166,    58,
       9,    62,     9,   105,   164,   106,    -1,    86,    88,    -1,
      87,    88,    -1,    21,    -1,    22,    -1,    24,    -1,    19,
      -1,    20,    -1,     9,    17,   170,    -1,     9,    18,   170,
      -1,    -1,   171,    -1,   172,   108,   171,    -1,     9,   170,
     104,    -1,    -1,   173,    -1,   174,   173,    -1,    64,    65,
       9,    25,   132,   104,    -1,    64,    91,     9,   104,    -1,
     175,    -1,   176,    -1,    -1,   177,    -1,   178,   177,    -1,
      16,     9,   105,   172,   106,    25,   174,   178,    26,   113,
      27,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   138,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   164,   165,   170,   171,   173,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   197,   202,   203,   204,   205,   207,   208,   209,
     210,   211,   212,   213,   216,   218,   219,   223,   228,   233,
     234,   235,   239,   243,   244,   249,   250,   251,   256,   257,
     258,   262,   263,   268,   274,   281,   282,   283,   288,   290,
     292,   296,   297,   301,   302,   307,   308,   313,   314,   315,
     319,   320,   325,   335,   340,   342,   347,   351,   352,   357,
     363,   370,   375,   380,   386,   391,   396,   401,   406,   412,
     413,   418,   419,   421,   425,   432,   438,   446,   450,   454,
     460,   466,   468,   473,   478,   479,   484,   485,   490,   491,
     497,   498,   504,   505,   511,   517,   518,   523,   524,   528,
     529,   533,   541,   546,   551,   552,   553,   554,   555,   559,
     562,   568,   569,   570,   575,   579,   581,   582,   586,   592,
     597,   598,   601,   603,   604,   608
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
  "PARS_FOR_TOKEN", "PARS_DDOT_TOKEN", "PARS_READ_TOKEN",
  "PARS_ORDER_TOKEN", "PARS_BY_TOKEN", "PARS_ASC_TOKEN", "PARS_DESC_TOKEN",
  "PARS_INSERT_TOKEN", "PARS_INTO_TOKEN", "PARS_VALUES_TOKEN",
  "PARS_UPDATE_TOKEN", "PARS_SET_TOKEN", "PARS_DELETE_TOKEN",
  "PARS_CURRENT_TOKEN", "PARS_OF_TOKEN", "PARS_CREATE_TOKEN",
  "PARS_TABLE_TOKEN", "PARS_INDEX_TOKEN", "PARS_UNIQUE_TOKEN",
  "PARS_CLUSTERED_TOKEN", "PARS_DOES_NOT_FIT_IN_MEM_TOKEN",
  "PARS_ON_TOKEN", "PARS_ASSIGN_TOKEN", "PARS_DECLARE_TOKEN",
  "PARS_CURSOR_TOKEN", "PARS_SQL_TOKEN", "PARS_OPEN_TOKEN",
  "PARS_FETCH_TOKEN", "PARS_CLOSE_TOKEN", "PARS_NOTFOUND_TOKEN",
  "PARS_TO_CHAR_TOKEN", "PARS_TO_NUMBER_TOKEN", "PARS_TO_BINARY_TOKEN",
  "PARS_BINARY_TO_NUMBER_TOKEN", "PARS_SUBSTR_TOKEN", "PARS_REPLSTR_TOKEN",
  "PARS_CONCAT_TOKEN", "PARS_INSTR_TOKEN", "PARS_LENGTH_TOKEN",
  "PARS_SYSDATE_TOKEN", "PARS_PRINTF_TOKEN", "PARS_ASSERT_TOKEN",
  "PARS_RND_TOKEN", "PARS_RND_STR_TOKEN", "PARS_ROW_PRINTF_TOKEN",
  "PARS_COMMIT_TOKEN", "PARS_ROLLBACK_TOKEN", "PARS_WORK_TOKEN",
  "PARS_UNSIGNED_TOKEN", "PARS_EXIT_TOKEN", "PARS_FUNCTION_TOKEN",
  "PARS_LOCK_TOKEN", "PARS_SHARE_TOKEN", "PARS_MODE_TOKEN", "'='", "'<'",
  "'>'", "'-'", "'+'", "'*'", "'/'", "NEG", "'%'", "';'", "'('", "')'",
  "'?'", "','", "'{'", "'}'", "$accept", "statement", "statement_list",
  "exp", "function_name", "question_mark_list", "stored_procedure_call",
  "predefined_procedure_call", "predefined_procedure_name",
  "user_function_call", "table_list", "variable_list", "exp_list",
  "select_item", "select_item_list", "select_list", "search_condition",
  "for_update_clause", "lock_shared_clause", "order_direction",
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
     345,   346,   347,   348,   349,    61,    60,    62,    45,    43,
      42,    47,   350,    37,    59,    40,    41,    63,    44,   123,
     125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   111,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   112,   112,   112,   112,   112,   112,   112,   112,
     112,   112,   112,   112,   112,   113,   113,   114,   114,   114,
     114,   114,   114,   114,   114,   114,   114,   114,   114,   114,
     114,   114,   114,   114,   114,   114,   114,   114,   114,   114,
     114,   114,   114,   115,   115,   115,   115,   115,   115,   115,
     115,   115,   115,   115,   116,   116,   116,   117,   118,   119,
     119,   119,   120,   121,   121,   122,   122,   122,   123,   123,
     123,   124,   124,   124,   124,   125,   125,   125,   126,   126,
     126,   127,   127,   128,   128,   129,   129,   130,   130,   130,
     131,   131,   132,   133,   134,   134,   135,   136,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     147,   148,   148,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   156,   157,   158,   158,   159,   159,   160,   160,
     161,   161,   162,   162,   163,   164,   164,   165,   165,   166,
     166,   167,   168,   169,   170,   170,   170,   170,   170,   171,
     171,   172,   172,   172,   173,   174,   174,   174,   175,   176,
     177,   177,   178,   178,   178,   179
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
       1,     0,     2,     0,     2,     0,     4,     0,     1,     1,
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
       0,     0,   102,     0,     0,   139,   140,     0,   164,     0,
       0,     0,   174,     0,     0,     0,   137,     0,   133,   145,
       0,     0,     0,     0,    96,    97,   126,   141,   151,     0,
       0,   169,   175,    98,    99,   101,   146,     0,   168
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,   182,   183,   166,    76,   211,    24,    25,    26,   208,
     200,   197,   167,    82,    83,    84,   107,   266,   284,   335,
     302,    27,    28,    29,   203,   204,   108,    30,    31,    32,
      33,    34,    35,    36,    37,   223,   224,   225,    38,    39,
      40,    41,    42,    43,    44,    45,   239,   240,   287,   306,
     318,   289,    46,   320,    91,   163,    47,    48,    49,   253,
     172,   173,   278,   279,   295,   296,   297,   298,    50
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -208
static const short int yypact[] =
{
     541,   -51,     8,   210,   210,  -208,    20,    13,    22,    39,
      23,   -39,    57,    69,    73,  -208,  -208,  -208,    40,    -5,
      -4,  -208,    76,    87,  -208,   -15,    -9,     2,   -16,     3,
      48,     5,     6,    48,     7,    19,    29,    30,    45,    49,
      53,    54,    64,    65,    80,    81,    82,    83,    84,    86,
      89,   210,    12,  -208,  -208,  -208,  -208,  -208,  -208,     9,
     210,    16,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,
    -208,  -208,  -208,   210,   210,   315,    36,   443,    47,    75,
    -208,   605,  -208,   -41,    85,    91,   112,    70,   161,   182,
    -208,   139,  -208,   155,  -208,  -208,  -208,  -208,   100,  -208,
    -208,   210,  -208,   103,  -208,  -208,   170,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,
     605,   200,   140,   557,   141,   208,    66,   210,   210,   210,
     210,   210,   541,   210,   210,   210,   210,   210,   210,   210,
     210,   541,   210,   -31,   216,   123,   217,   210,  -208,   218,
    -208,   124,  -208,   172,   223,   126,   605,   -66,   210,   183,
     605,    37,  -208,   -63,  -208,  -208,  -208,   557,   557,    15,
      15,   605,  -208,   286,    15,    15,    15,     1,     1,   208,
     208,   -62,   350,   250,   228,   133,  -208,   132,  -208,  -208,
     -34,   538,   151,  -208,   143,   243,   246,   153,  -208,   132,
    -208,   -59,  -208,   210,   -55,   247,    17,    17,   234,   200,
     541,   210,  -208,  -208,   231,   239,  -208,   235,  -208,   163,
    -208,   261,   210,   262,   232,   210,   210,   218,    17,  -208,
     -48,   212,   166,   167,   171,   605,  -208,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,   270,  -208,   541,   585,  -208,
     252,  -208,  -208,  -208,  -208,   240,   194,   592,   605,  -208,
     187,   236,   243,   287,  -208,  -208,  -208,    17,  -208,     4,
     541,  -208,  -208,   281,   256,   541,   298,   215,  -208,  -208,
    -208,   201,   203,   -56,  -208,  -208,  -208,  -208,   -12,   541,
     219,   260,  -208,   414,   204,  -208,   297,   302,  -208,   309,
     313,   541,  -208,   230,   322,   300,  -208,   325,  -208,  -208,
     -45,   310,   237,   478,  -208,    18,  -208,  -208,  -208,   327,
      40,  -208,  -208,  -208,  -208,  -208,  -208,   248,  -208
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -208,     0,  -130,    -1,  -208,  -208,  -208,  -208,  -208,  -208,
    -208,   174,  -135,   185,  -208,  -208,   -29,  -208,  -208,  -208,
    -208,   -17,  -208,  -208,   106,  -208,   324,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,   134,  -208,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,  -208,    88,  -208,  -208,  -208,
    -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -208,  -207,
     142,  -208,    90,  -208,  -208,  -208,    67,  -208,  -208
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned short int yytable[] =
{
      23,    95,    75,    77,   111,    81,   232,   194,   154,   309,
     254,   104,    51,   277,   311,   191,   141,    52,    89,     6,
      90,   192,    85,    53,    54,    55,    56,    57,    58,    59,
     141,   270,    60,   214,   103,   310,   248,   249,   250,   251,
     212,   252,   213,   218,   226,   219,   213,   243,    87,   244,
     130,   246,   293,   213,   216,   217,    78,    79,   271,   133,
     272,   328,    88,   329,   333,   334,    92,   155,   293,   195,
     292,    86,   135,   136,   233,     6,   137,   138,    93,   139,
     140,   141,    94,    96,    97,    98,    61,    99,   106,   100,
     257,    62,    63,    64,    65,    66,   101,    67,    68,    69,
      70,   148,   149,    71,    72,   170,   102,   105,   157,   109,
     110,   113,   132,   146,   147,   148,   149,   131,    73,   134,
      80,   158,   159,   114,   156,    74,    53,    54,    55,    56,
      57,    58,    59,   115,   116,    60,   177,   178,   179,   180,
     181,   150,   184,   185,   186,   187,   188,   189,   190,   117,
     299,   193,   152,   118,    81,   303,   201,   119,   120,    78,
      79,   143,   144,   145,   146,   147,   148,   149,   121,   122,
     160,   234,   176,    53,    54,    55,    56,    57,    58,    59,
     153,   323,    60,   222,   123,   124,   125,   126,   127,    61,
     128,   161,   222,   129,    62,    63,    64,    65,    66,   162,
      67,    68,    69,    70,   164,   165,    71,    72,   168,   171,
     174,   175,   245,    53,    54,    55,    56,    57,    58,    59,
     258,    73,    60,   141,   169,   196,   199,   202,    74,   205,
     206,   170,   207,   210,   267,   268,    61,   229,   215,   230,
     231,    62,    63,    64,    65,    66,   236,    67,    68,    69,
      70,   237,   238,    71,    72,   241,   247,   222,   242,   255,
     137,   138,   221,   139,   140,   141,   260,   261,    73,   262,
     263,   264,   274,   265,   273,    74,    61,   275,   276,   277,
     281,    62,    63,    64,    65,    66,   283,    67,    68,    69,
      70,   282,   286,    71,    72,     1,   291,   288,   300,   222,
     301,   304,     2,   222,   305,   314,   307,   308,    73,   317,
     316,   319,   313,   337,     3,    74,   220,   221,   321,     4,
       5,     6,   322,   222,   324,   137,   138,     7,   139,   140,
     141,   325,   326,   327,     8,   330,   336,     9,   209,    10,
     198,   331,    11,   269,   142,   143,   144,   145,   146,   147,
     148,   149,   338,    12,    13,    14,   228,   112,   259,     1,
     290,   256,    15,     0,     0,   312,     2,    16,    17,   294,
       0,    18,    19,    20,     0,     0,    21,   227,     3,     0,
       0,     0,     0,     4,     5,     6,     0,     0,     0,     0,
       0,     7,     0,     0,     0,    22,     0,     0,     8,     0,
       0,     9,     0,    10,     0,     0,    11,     0,     0,     0,
     143,   144,   145,   146,   147,   148,   149,    12,    13,    14,
       0,     0,     0,     1,     0,     0,    15,     0,     0,     0,
       2,    16,    17,     0,     0,    18,    19,    20,     0,     0,
      21,   315,     3,     0,     0,     0,     0,     4,     5,     6,
       0,     0,     0,   137,   138,     7,   139,   140,   141,    22,
       0,     0,     8,     0,     0,     9,     0,    10,     0,     0,
      11,     0,     0,     0,     0,   151,     0,     0,     0,     0,
       0,    12,    13,    14,     0,     0,     0,     1,     0,     0,
      15,     0,     0,     0,     2,    16,    17,     0,     0,    18,
      19,    20,     0,     0,    21,   332,     3,     0,     0,     0,
       0,     4,     5,     6,     0,     0,     0,     0,     0,     7,
       0,     0,     0,    22,     0,     0,     8,     0,     0,     9,
       0,    10,     0,     0,    11,     0,     0,     0,   143,   144,
     145,   146,   147,   148,   149,    12,    13,    14,   137,   138,
       1,   139,   140,   141,    15,     0,     0,     2,     0,    16,
      17,     0,     0,    18,    19,    20,     0,     0,    21,     3,
     139,   140,   141,     0,     4,     5,     6,     0,     0,     0,
     235,     0,     7,     0,     0,     0,     0,    22,     0,     8,
       0,     0,     9,     0,    10,   137,   138,    11,   139,   140,
     141,     0,   137,   138,     0,   139,   140,   141,    12,    13,
      14,     0,     0,     0,   280,   137,   138,    15,   139,   140,
     141,     0,    16,    17,   285,     0,    18,    19,    20,     0,
       0,    21,     0,   143,   144,   145,   146,   147,   148,   149,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      22,     0,   143,   144,   145,   146,   147,   148,   149,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     143,   144,   145,   146,   147,   148,   149,   143,   144,   145,
     146,   147,   148,   149,     0,     0,     0,     0,     0,     0,
     143,   144,   145,   146,   147,   148,   149
};

static const short int yycheck[] =
{
       0,    18,     3,     4,    33,     6,    40,    38,    49,    65,
     217,    28,    63,     9,    26,   150,    15,     9,    57,    35,
      59,   151,     9,     3,     4,     5,     6,     7,     8,     9,
      15,   238,    12,   168,    50,    91,    19,    20,    21,    22,
     106,    24,   108,   106,   106,   108,   108,   106,     9,   108,
      51,   106,    64,   108,    17,    18,    36,    37,   106,    60,
     108,   106,    39,   108,    46,    47,     9,   108,    64,   100,
     277,    49,    73,    74,   108,    35,    10,    11,     9,    13,
      14,    15,     9,    88,    88,     9,    66,     0,    40,   104,
     220,    71,    72,    73,    74,    75,   105,    77,    78,    79,
      80,   100,   101,    83,    84,   106,   104,   104,    17,   104,
     104,   104,   103,    98,    99,   100,   101,   105,    98,   103,
     100,     9,    52,   104,    39,   105,     3,     4,     5,     6,
       7,     8,     9,   104,   104,    12,   137,   138,   139,   140,
     141,   105,   143,   144,   145,   146,   147,   148,   149,   104,
     280,   152,   105,   104,   155,   285,   157,   104,   104,    36,
      37,    95,    96,    97,    98,    99,   100,   101,   104,   104,
       9,   200,   106,     3,     4,     5,     6,     7,     8,     9,
     105,   311,    12,   183,   104,   104,   104,   104,   104,    66,
     104,     9,   192,   104,    71,    72,    73,    74,    75,    60,
      77,    78,    79,    80,    49,   105,    83,    84,   105,     9,
      70,    70,   213,     3,     4,     5,     6,     7,     8,     9,
     221,    98,    12,    15,    54,     9,     9,     9,   105,   105,
      58,   232,     9,   107,   235,   236,    66,     9,    55,   106,
     108,    71,    72,    73,    74,    75,    95,    77,    78,    79,
      80,   108,     9,    83,    84,     9,     9,   257,   105,    25,
      10,    11,    31,    13,    14,    15,    27,    32,    98,   106,
       9,     9,   106,    41,    62,   105,    66,   110,   107,     9,
      28,    71,    72,    73,    74,    75,    92,    77,    78,    79,
      80,    51,   105,    83,    84,     9,     9,    61,    17,   299,
      44,     3,    16,   303,    89,    45,   105,   104,    98,    12,
     106,     9,    93,   330,    28,   105,    30,    31,     9,    33,
      34,    35,     9,   323,    94,    10,    11,    41,    13,    14,
      15,     9,    32,     8,    48,    25,     9,    51,   164,    53,
     155,   104,    56,   237,    29,    95,    96,    97,    98,    99,
     100,   101,   104,    67,    68,    69,   106,    33,   224,     9,
     272,   219,    76,    -1,    -1,   298,    16,    81,    82,   279,
      -1,    85,    86,    87,    -1,    -1,    90,    27,    28,    -1,
      -1,    -1,    -1,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    41,    -1,    -1,    -1,   109,    -1,    -1,    48,    -1,
      -1,    51,    -1,    53,    -1,    -1,    56,    -1,    -1,    -1,
      95,    96,    97,    98,    99,   100,   101,    67,    68,    69,
      -1,    -1,    -1,     9,    -1,    -1,    76,    -1,    -1,    -1,
      16,    81,    82,    -1,    -1,    85,    86,    87,    -1,    -1,
      90,    27,    28,    -1,    -1,    -1,    -1,    33,    34,    35,
      -1,    -1,    -1,    10,    11,    41,    13,    14,    15,   109,
      -1,    -1,    48,    -1,    -1,    51,    -1,    53,    -1,    -1,
      56,    -1,    -1,    -1,    -1,    32,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    -1,    -1,    -1,     9,    -1,    -1,
      76,    -1,    -1,    -1,    16,    81,    82,    -1,    -1,    85,
      86,    87,    -1,    -1,    90,    27,    28,    -1,    -1,    -1,
      -1,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    41,
      -1,    -1,    -1,   109,    -1,    -1,    48,    -1,    -1,    51,
      -1,    53,    -1,    -1,    56,    -1,    -1,    -1,    95,    96,
      97,    98,    99,   100,   101,    67,    68,    69,    10,    11,
       9,    13,    14,    15,    76,    -1,    -1,    16,    -1,    81,
      82,    -1,    -1,    85,    86,    87,    -1,    -1,    90,    28,
      13,    14,    15,    -1,    33,    34,    35,    -1,    -1,    -1,
      42,    -1,    41,    -1,    -1,    -1,    -1,   109,    -1,    48,
      -1,    -1,    51,    -1,    53,    10,    11,    56,    13,    14,
      15,    -1,    10,    11,    -1,    13,    14,    15,    67,    68,
      69,    -1,    -1,    -1,    29,    10,    11,    76,    13,    14,
      15,    -1,    81,    82,    32,    -1,    85,    86,    87,    -1,
      -1,    90,    -1,    95,    96,    97,    98,    99,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     109,    -1,    95,    96,    97,    98,    99,   100,   101,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    97,    98,    99,   100,   101,    95,    96,    97,
      98,    99,   100,   101,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    97,    98,    99,   100,   101
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     9,    16,    28,    33,    34,    35,    41,    48,    51,
      53,    56,    67,    68,    69,    76,    81,    82,    85,    86,
      87,    90,   109,   112,   117,   118,   119,   132,   133,   134,
     138,   139,   140,   141,   142,   143,   144,   145,   149,   150,
     151,   152,   153,   154,   155,   156,   163,   167,   168,   169,
     179,    63,     9,     3,     4,     5,     6,     7,     8,     9,
      12,    66,    71,    72,    73,    74,    75,    77,    78,    79,
      80,    83,    84,    98,   105,   114,   115,   114,    36,    37,
     100,   114,   124,   125,   126,     9,    49,     9,    39,    57,
      59,   165,     9,     9,     9,   132,    88,    88,     9,     0,
     104,   105,   104,    50,   132,   104,    40,   127,   137,   104,
     104,   127,   137,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     114,   105,   103,   114,   103,   114,   114,    10,    11,    13,
      14,    15,    29,    95,    96,    97,    98,    99,   100,   101,
     105,    32,   105,   105,    49,   108,    39,    17,     9,    52,
       9,     9,    60,   166,    49,   105,   114,   123,   105,    54,
     114,     9,   171,   172,    70,    70,   106,   114,   114,   114,
     114,   114,   112,   113,   114,   114,   114,   114,   114,   114,
     114,   123,   113,   114,    38,   100,     9,   122,   124,     9,
     121,   114,     9,   135,   136,   105,    58,     9,   120,   122,
     107,   116,   106,   108,   123,    55,    17,    18,   106,   108,
      30,    31,   112,   146,   147,   148,   106,    27,   106,     9,
     106,   108,    40,   108,   127,    42,    95,   108,     9,   157,
     158,     9,   105,   106,   108,   114,   106,     9,    19,    20,
      21,    22,    24,   170,   170,    25,   171,   113,   114,   146,
      27,    32,   106,     9,     9,    41,   128,   114,   114,   135,
     170,   106,   108,    62,   106,   110,   107,     9,   173,   174,
      29,    28,    51,    92,   129,    32,   105,   159,    61,   162,
     157,     9,   170,    64,   173,   175,   176,   177,   178,   113,
      17,    44,   131,   113,     3,    89,   160,   105,   104,    65,
      91,    26,   177,    93,    45,    27,   106,    12,   161,     9,
     164,     9,     9,   113,    94,     9,    32,     8,   106,   108,
      25,   104,    27,    46,    47,   130,     9,   132,   104
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
#line 164 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 26:
#line 166 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 27:
#line 170 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 28:
#line 172 "pars0grm.y"
    { yyval = pars_func(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 29:
#line 173 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 30:
#line 174 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 31:
#line 175 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 32:
#line 176 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 33:
#line 177 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 34:
#line 178 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 35:
#line 179 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 36:
#line 180 "pars0grm.y"
    { yyval = pars_op('+', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 37:
#line 181 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 38:
#line 182 "pars0grm.y"
    { yyval = pars_op('*', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 39:
#line 183 "pars0grm.y"
    { yyval = pars_op('/', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 40:
#line 184 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[0], NULL); ;}
    break;

  case 41:
#line 185 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 42:
#line 186 "pars0grm.y"
    { yyval = pars_op('=', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 43:
#line 187 "pars0grm.y"
    { yyval = pars_op('<', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 44:
#line 188 "pars0grm.y"
    { yyval = pars_op('>', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 45:
#line 189 "pars0grm.y"
    { yyval = pars_op(PARS_GE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 46:
#line 190 "pars0grm.y"
    { yyval = pars_op(PARS_LE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 47:
#line 191 "pars0grm.y"
    { yyval = pars_op(PARS_NE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 48:
#line 192 "pars0grm.y"
    { yyval = pars_op(PARS_AND_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 49:
#line 193 "pars0grm.y"
    { yyval = pars_op(PARS_OR_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 50:
#line 194 "pars0grm.y"
    { yyval = pars_op(PARS_NOT_TOKEN, yyvsp[0], NULL); ;}
    break;

  case 51:
#line 196 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 52:
#line 198 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 53:
#line 202 "pars0grm.y"
    { yyval = &pars_to_char_token; ;}
    break;

  case 54:
#line 203 "pars0grm.y"
    { yyval = &pars_to_number_token; ;}
    break;

  case 55:
#line 204 "pars0grm.y"
    { yyval = &pars_to_binary_token; ;}
    break;

  case 56:
#line 206 "pars0grm.y"
    { yyval = &pars_binary_to_number_token; ;}
    break;

  case 57:
#line 207 "pars0grm.y"
    { yyval = &pars_substr_token; ;}
    break;

  case 58:
#line 208 "pars0grm.y"
    { yyval = &pars_concat_token; ;}
    break;

  case 59:
#line 209 "pars0grm.y"
    { yyval = &pars_instr_token; ;}
    break;

  case 60:
#line 210 "pars0grm.y"
    { yyval = &pars_length_token; ;}
    break;

  case 61:
#line 211 "pars0grm.y"
    { yyval = &pars_sysdate_token; ;}
    break;

  case 62:
#line 212 "pars0grm.y"
    { yyval = &pars_rnd_token; ;}
    break;

  case 63:
#line 213 "pars0grm.y"
    { yyval = &pars_rnd_str_token; ;}
    break;

  case 67:
#line 224 "pars0grm.y"
    { yyval = pars_stored_procedure_call(yyvsp[-4]); ;}
    break;

  case 68:
#line 229 "pars0grm.y"
    { yyval = pars_procedure_call(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 69:
#line 233 "pars0grm.y"
    { yyval = &pars_replstr_token; ;}
    break;

  case 70:
#line 234 "pars0grm.y"
    { yyval = &pars_printf_token; ;}
    break;

  case 71:
#line 235 "pars0grm.y"
    { yyval = &pars_assert_token; ;}
    break;

  case 72:
#line 239 "pars0grm.y"
    { yyval = yyvsp[-2]; ;}
    break;

  case 73:
#line 243 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 74:
#line 245 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 75:
#line 249 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 76:
#line 250 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 77:
#line 252 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 78:
#line 256 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 79:
#line 257 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]);;}
    break;

  case 80:
#line 258 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 81:
#line 262 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 82:
#line 264 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); ;}
    break;

  case 83:
#line 269 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, yyvsp[-1])))); ;}
    break;

  case 84:
#line 275 "pars0grm.y"
    { yyval = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									yyvsp[-1])); ;}
    break;

  case 85:
#line 281 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 86:
#line 282 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 87:
#line 284 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 88:
#line 288 "pars0grm.y"
    { yyval = pars_select_list(&pars_star_denoter,
								NULL); ;}
    break;

  case 89:
#line 291 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 90:
#line 292 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[0], NULL); ;}
    break;

  case 91:
#line 296 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 92:
#line 297 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 93:
#line 301 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 94:
#line 303 "pars0grm.y"
    { yyval = &pars_update_token; ;}
    break;

  case 95:
#line 307 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 96:
#line 309 "pars0grm.y"
    { yyval = &pars_share_token; ;}
    break;

  case 97:
#line 313 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 98:
#line 314 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 99:
#line 315 "pars0grm.y"
    { yyval = &pars_desc_token; ;}
    break;

  case 100:
#line 319 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 101:
#line 321 "pars0grm.y"
    { yyval = pars_order_by(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 102:
#line 330 "pars0grm.y"
    { yyval = pars_select_statement(yyvsp[-6], yyvsp[-4], yyvsp[-3],
								yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 103:
#line 336 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 104:
#line 341 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-4], yyvsp[-1], NULL); ;}
    break;

  case 105:
#line 343 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 106:
#line 347 "pars0grm.y"
    { yyval = pars_column_assignment(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 107:
#line 351 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 108:
#line 353 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 109:
#line 359 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 110:
#line 365 "pars0grm.y"
    { yyval = pars_update_statement_start(FALSE,
								yyvsp[-2], yyvsp[0]); ;}
    break;

  case 111:
#line 371 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 112:
#line 376 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 113:
#line 381 "pars0grm.y"
    { yyval = pars_update_statement_start(TRUE,
								yyvsp[0], NULL); ;}
    break;

  case 114:
#line 387 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 115:
#line 392 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 116:
#line 397 "pars0grm.y"
    { yyval = pars_row_printf_statement(yyvsp[0]); ;}
    break;

  case 117:
#line 402 "pars0grm.y"
    { yyval = pars_assignment_statement(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 118:
#line 408 "pars0grm.y"
    { yyval = pars_elsif_element(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 119:
#line 412 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 120:
#line 414 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 121:
#line 418 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 122:
#line 420 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 123:
#line 421 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 124:
#line 428 "pars0grm.y"
    { yyval = pars_if_statement(yyvsp[-5], yyvsp[-3], yyvsp[-2]); ;}
    break;

  case 125:
#line 434 "pars0grm.y"
    { yyval = pars_while_statement(yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 126:
#line 442 "pars0grm.y"
    { yyval = pars_for_statement(yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 127:
#line 446 "pars0grm.y"
    { yyval = pars_exit_statement(); ;}
    break;

  case 128:
#line 450 "pars0grm.y"
    { yyval = pars_return_statement(); ;}
    break;

  case 129:
#line 455 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_OPEN_CURSOR, yyvsp[0]); ;}
    break;

  case 130:
#line 461 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR, yyvsp[0]); ;}
    break;

  case 131:
#line 467 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], yyvsp[0], NULL); ;}
    break;

  case 132:
#line 469 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], NULL, yyvsp[0]); ;}
    break;

  case 133:
#line 474 "pars0grm.y"
    { yyval = pars_column_def(yyvsp[-4], yyvsp[-3], yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 134:
#line 478 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 135:
#line 480 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 136:
#line 484 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 137:
#line 486 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 138:
#line 490 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 139:
#line 492 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 140:
#line 497 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 141:
#line 499 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 142:
#line 504 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 143:
#line 506 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 144:
#line 513 "pars0grm.y"
    { yyval = pars_create_table(yyvsp[-4], yyvsp[-2], yyvsp[0]); ;}
    break;

  case 145:
#line 517 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 146:
#line 519 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 147:
#line 523 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 148:
#line 524 "pars0grm.y"
    { yyval = &pars_unique_token; ;}
    break;

  case 149:
#line 528 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 150:
#line 529 "pars0grm.y"
    { yyval = &pars_clustered_token; ;}
    break;

  case 151:
#line 537 "pars0grm.y"
    { yyval = pars_create_index(yyvsp[-8], yyvsp[-7], yyvsp[-5], yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 152:
#line 542 "pars0grm.y"
    { yyval = pars_commit_statement(); ;}
    break;

  case 153:
#line 547 "pars0grm.y"
    { yyval = pars_rollback_statement(); ;}
    break;

  case 154:
#line 551 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 155:
#line 552 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 156:
#line 553 "pars0grm.y"
    { yyval = &pars_char_token; ;}
    break;

  case 157:
#line 554 "pars0grm.y"
    { yyval = &pars_binary_token; ;}
    break;

  case 158:
#line 555 "pars0grm.y"
    { yyval = &pars_blob_token; ;}
    break;

  case 159:
#line 560 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_INPUT, yyvsp[0]); ;}
    break;

  case 160:
#line 563 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_OUTPUT, yyvsp[0]); ;}
    break;

  case 161:
#line 568 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 162:
#line 569 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 163:
#line 571 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 164:
#line 576 "pars0grm.y"
    { yyval = pars_variable_declaration(yyvsp[-2], yyvsp[-1]); ;}
    break;

  case 168:
#line 588 "pars0grm.y"
    { yyval = pars_cursor_declaration(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 169:
#line 593 "pars0grm.y"
    { yyval = pars_function_declaration(yyvsp[-1]); ;}
    break;

  case 175:
#line 614 "pars0grm.y"
    { yyval = pars_procedure_definition(yyvsp[-9], yyvsp[-7],
								yyvsp[-1]); ;}
    break;


    }

/* Line 1010 of yacc.c.  */
#line 2334 "pars0grm.tab.c"

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


#line 618 "pars0grm.y"


