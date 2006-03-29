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
     PARS_NULL_LIT = 261,
     PARS_ID_TOKEN = 262,
     PARS_AND_TOKEN = 263,
     PARS_OR_TOKEN = 264,
     PARS_NOT_TOKEN = 265,
     PARS_GE_TOKEN = 266,
     PARS_LE_TOKEN = 267,
     PARS_NE_TOKEN = 268,
     PARS_PROCEDURE_TOKEN = 269,
     PARS_IN_TOKEN = 270,
     PARS_OUT_TOKEN = 271,
     PARS_BINARY_TOKEN = 272,
     PARS_BLOB_TOKEN = 273,
     PARS_INT_TOKEN = 274,
     PARS_INTEGER_TOKEN = 275,
     PARS_FLOAT_TOKEN = 276,
     PARS_CHAR_TOKEN = 277,
     PARS_IS_TOKEN = 278,
     PARS_BEGIN_TOKEN = 279,
     PARS_END_TOKEN = 280,
     PARS_IF_TOKEN = 281,
     PARS_THEN_TOKEN = 282,
     PARS_ELSE_TOKEN = 283,
     PARS_ELSIF_TOKEN = 284,
     PARS_LOOP_TOKEN = 285,
     PARS_WHILE_TOKEN = 286,
     PARS_RETURN_TOKEN = 287,
     PARS_SELECT_TOKEN = 288,
     PARS_SUM_TOKEN = 289,
     PARS_COUNT_TOKEN = 290,
     PARS_DISTINCT_TOKEN = 291,
     PARS_FROM_TOKEN = 292,
     PARS_WHERE_TOKEN = 293,
     PARS_FOR_TOKEN = 294,
     PARS_DDOT_TOKEN = 295,
     PARS_CONSISTENT_TOKEN = 296,
     PARS_READ_TOKEN = 297,
     PARS_ORDER_TOKEN = 298,
     PARS_BY_TOKEN = 299,
     PARS_ASC_TOKEN = 300,
     PARS_DESC_TOKEN = 301,
     PARS_INSERT_TOKEN = 302,
     PARS_INTO_TOKEN = 303,
     PARS_VALUES_TOKEN = 304,
     PARS_UPDATE_TOKEN = 305,
     PARS_SET_TOKEN = 306,
     PARS_DELETE_TOKEN = 307,
     PARS_CURRENT_TOKEN = 308,
     PARS_OF_TOKEN = 309,
     PARS_CREATE_TOKEN = 310,
     PARS_TABLE_TOKEN = 311,
     PARS_INDEX_TOKEN = 312,
     PARS_UNIQUE_TOKEN = 313,
     PARS_CLUSTERED_TOKEN = 314,
     PARS_DOES_NOT_FIT_IN_MEM_TOKEN = 315,
     PARS_ON_TOKEN = 316,
     PARS_ASSIGN_TOKEN = 317,
     PARS_DECLARE_TOKEN = 318,
     PARS_CURSOR_TOKEN = 319,
     PARS_SQL_TOKEN = 320,
     PARS_OPEN_TOKEN = 321,
     PARS_FETCH_TOKEN = 322,
     PARS_CLOSE_TOKEN = 323,
     PARS_NOTFOUND_TOKEN = 324,
     PARS_TO_CHAR_TOKEN = 325,
     PARS_TO_NUMBER_TOKEN = 326,
     PARS_TO_BINARY_TOKEN = 327,
     PARS_BINARY_TO_NUMBER_TOKEN = 328,
     PARS_SUBSTR_TOKEN = 329,
     PARS_REPLSTR_TOKEN = 330,
     PARS_CONCAT_TOKEN = 331,
     PARS_INSTR_TOKEN = 332,
     PARS_LENGTH_TOKEN = 333,
     PARS_SYSDATE_TOKEN = 334,
     PARS_PRINTF_TOKEN = 335,
     PARS_ASSERT_TOKEN = 336,
     PARS_RND_TOKEN = 337,
     PARS_RND_STR_TOKEN = 338,
     PARS_ROW_PRINTF_TOKEN = 339,
     PARS_COMMIT_TOKEN = 340,
     PARS_ROLLBACK_TOKEN = 341,
     PARS_WORK_TOKEN = 342,
     PARS_UNSIGNED_TOKEN = 343,
     PARS_EXIT_TOKEN = 344,
     PARS_FUNCTION_TOKEN = 345,
     NEG = 346
   };
#endif
#define PARS_INT_LIT 258
#define PARS_FLOAT_LIT 259
#define PARS_STR_LIT 260
#define PARS_NULL_LIT 261
#define PARS_ID_TOKEN 262
#define PARS_AND_TOKEN 263
#define PARS_OR_TOKEN 264
#define PARS_NOT_TOKEN 265
#define PARS_GE_TOKEN 266
#define PARS_LE_TOKEN 267
#define PARS_NE_TOKEN 268
#define PARS_PROCEDURE_TOKEN 269
#define PARS_IN_TOKEN 270
#define PARS_OUT_TOKEN 271
#define PARS_BINARY_TOKEN 272
#define PARS_BLOB_TOKEN 273
#define PARS_INT_TOKEN 274
#define PARS_INTEGER_TOKEN 275
#define PARS_FLOAT_TOKEN 276
#define PARS_CHAR_TOKEN 277
#define PARS_IS_TOKEN 278
#define PARS_BEGIN_TOKEN 279
#define PARS_END_TOKEN 280
#define PARS_IF_TOKEN 281
#define PARS_THEN_TOKEN 282
#define PARS_ELSE_TOKEN 283
#define PARS_ELSIF_TOKEN 284
#define PARS_LOOP_TOKEN 285
#define PARS_WHILE_TOKEN 286
#define PARS_RETURN_TOKEN 287
#define PARS_SELECT_TOKEN 288
#define PARS_SUM_TOKEN 289
#define PARS_COUNT_TOKEN 290
#define PARS_DISTINCT_TOKEN 291
#define PARS_FROM_TOKEN 292
#define PARS_WHERE_TOKEN 293
#define PARS_FOR_TOKEN 294
#define PARS_DDOT_TOKEN 295
#define PARS_CONSISTENT_TOKEN 296
#define PARS_READ_TOKEN 297
#define PARS_ORDER_TOKEN 298
#define PARS_BY_TOKEN 299
#define PARS_ASC_TOKEN 300
#define PARS_DESC_TOKEN 301
#define PARS_INSERT_TOKEN 302
#define PARS_INTO_TOKEN 303
#define PARS_VALUES_TOKEN 304
#define PARS_UPDATE_TOKEN 305
#define PARS_SET_TOKEN 306
#define PARS_DELETE_TOKEN 307
#define PARS_CURRENT_TOKEN 308
#define PARS_OF_TOKEN 309
#define PARS_CREATE_TOKEN 310
#define PARS_TABLE_TOKEN 311
#define PARS_INDEX_TOKEN 312
#define PARS_UNIQUE_TOKEN 313
#define PARS_CLUSTERED_TOKEN 314
#define PARS_DOES_NOT_FIT_IN_MEM_TOKEN 315
#define PARS_ON_TOKEN 316
#define PARS_ASSIGN_TOKEN 317
#define PARS_DECLARE_TOKEN 318
#define PARS_CURSOR_TOKEN 319
#define PARS_SQL_TOKEN 320
#define PARS_OPEN_TOKEN 321
#define PARS_FETCH_TOKEN 322
#define PARS_CLOSE_TOKEN 323
#define PARS_NOTFOUND_TOKEN 324
#define PARS_TO_CHAR_TOKEN 325
#define PARS_TO_NUMBER_TOKEN 326
#define PARS_TO_BINARY_TOKEN 327
#define PARS_BINARY_TO_NUMBER_TOKEN 328
#define PARS_SUBSTR_TOKEN 329
#define PARS_REPLSTR_TOKEN 330
#define PARS_CONCAT_TOKEN 331
#define PARS_INSTR_TOKEN 332
#define PARS_LENGTH_TOKEN 333
#define PARS_SYSDATE_TOKEN 334
#define PARS_PRINTF_TOKEN 335
#define PARS_ASSERT_TOKEN 336
#define PARS_RND_TOKEN 337
#define PARS_RND_STR_TOKEN 338
#define PARS_ROW_PRINTF_TOKEN 339
#define PARS_COMMIT_TOKEN 340
#define PARS_ROLLBACK_TOKEN 341
#define PARS_WORK_TOKEN 342
#define PARS_UNSIGNED_TOKEN 343
#define PARS_EXIT_TOKEN 344
#define PARS_FUNCTION_TOKEN 345
#define NEG 346




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
#line 289 "pars0grm.tab.c"

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
#define YYFINAL  97
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   703

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  107
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  69
/* YYNRULES -- Number of rules. */
#define YYNRULES  173
/* YYNRULES -- Number of states. */
#define YYNSTATES  335

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   346

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    99,     2,     2,
     101,   102,    96,    95,   104,    94,     2,    97,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   100,
      92,    91,    93,   103,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   105,     2,   106,     2,     2,     2,     2,
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
      85,    86,    87,    88,    89,    90,    98
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     6,     8,    11,    14,    17,    20,    23,
      26,    29,    32,    35,    38,    41,    44,    47,    50,    53,
      56,    59,    62,    65,    68,    71,    73,    76,    78,    83,
      85,    87,    89,    91,    93,    97,   101,   105,   109,   112,
     116,   120,   124,   128,   132,   136,   140,   144,   148,   151,
     155,   159,   161,   163,   165,   167,   169,   171,   173,   175,
     177,   179,   181,   182,   184,   188,   195,   200,   202,   204,
     206,   210,   212,   216,   217,   219,   223,   224,   226,   230,
     232,   237,   243,   248,   249,   251,   255,   257,   261,   263,
     264,   267,   268,   271,   272,   275,   276,   278,   280,   281,
     286,   295,   299,   305,   308,   312,   314,   318,   323,   328,
     331,   334,   338,   341,   344,   347,   351,   356,   358,   361,
     362,   365,   367,   375,   382,   393,   395,   397,   400,   403,
     408,   413,   419,   421,   425,   426,   430,   431,   433,   434,
     437,   438,   440,   448,   450,   454,   455,   457,   458,   460,
     471,   474,   477,   479,   481,   483,   485,   487,   491,   495,
     496,   498,   502,   506,   507,   509,   512,   519,   524,   526,
     528,   529,   531,   534
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     108,     0,    -1,   175,   100,    -1,   113,    -1,   114,   100,
      -1,   146,   100,    -1,   147,   100,    -1,   148,   100,    -1,
     145,   100,    -1,   149,   100,    -1,   141,   100,    -1,   128,
     100,    -1,   130,   100,    -1,   140,   100,    -1,   138,   100,
      -1,   139,   100,    -1,   135,   100,    -1,   136,   100,    -1,
     150,   100,    -1,   152,   100,    -1,   151,   100,    -1,   164,
     100,    -1,   165,   100,    -1,   159,   100,    -1,   163,   100,
      -1,   108,    -1,   109,   108,    -1,     7,    -1,   111,   101,
     119,   102,    -1,     3,    -1,     4,    -1,     5,    -1,     6,
      -1,    65,    -1,   110,    95,   110,    -1,   110,    94,   110,
      -1,   110,    96,   110,    -1,   110,    97,   110,    -1,    94,
     110,    -1,   101,   110,   102,    -1,   110,    91,   110,    -1,
     110,    92,   110,    -1,   110,    93,   110,    -1,   110,    11,
     110,    -1,   110,    12,   110,    -1,   110,    13,   110,    -1,
     110,     8,   110,    -1,   110,     9,   110,    -1,    10,   110,
      -1,     7,    99,    69,    -1,    65,    99,    69,    -1,    70,
      -1,    71,    -1,    72,    -1,    73,    -1,    74,    -1,    76,
      -1,    77,    -1,    78,    -1,    79,    -1,    82,    -1,    83,
      -1,    -1,   103,    -1,   112,   104,   103,    -1,   105,     7,
     101,   112,   102,   106,    -1,   115,   101,   119,   102,    -1,
      75,    -1,    80,    -1,    81,    -1,     7,   101,   102,    -1,
       7,    -1,   117,   104,     7,    -1,    -1,     7,    -1,   118,
     104,     7,    -1,    -1,   110,    -1,   119,   104,   110,    -1,
     110,    -1,    35,   101,    96,   102,    -1,    35,   101,    36,
       7,   102,    -1,    34,   101,   110,   102,    -1,    -1,   120,
      -1,   121,   104,   120,    -1,    96,    -1,   121,    48,   118,
      -1,   121,    -1,    -1,    38,   110,    -1,    -1,    39,    50,
      -1,    -1,    41,    42,    -1,    -1,    45,    -1,    46,    -1,
      -1,    43,    44,     7,   126,    -1,    33,   122,    37,   117,
     123,   124,   125,   127,    -1,    47,    48,     7,    -1,   129,
      49,   101,   119,   102,    -1,   129,   128,    -1,     7,    91,
     110,    -1,   131,    -1,   132,   104,   131,    -1,    38,    53,
      54,     7,    -1,    50,     7,    51,   132,    -1,   134,   123,
      -1,   134,   133,    -1,    52,    37,     7,    -1,   137,   123,
      -1,   137,   133,    -1,    84,   128,    -1,     7,    62,   110,
      -1,    29,   110,    27,   109,    -1,   142,    -1,   143,   142,
      -1,    -1,    28,   109,    -1,   143,    -1,    26,   110,    27,
     109,   144,    25,    26,    -1,    31,   110,    30,   109,    25,
      30,    -1,    39,     7,    15,   110,    40,   110,    30,   109,
      25,    30,    -1,    89,    -1,    32,    -1,    66,     7,    -1,
      68,     7,    -1,    67,     7,    48,   118,    -1,    67,     7,
      48,   116,    -1,     7,   166,   155,   156,   157,    -1,   153,
      -1,   154,   104,   153,    -1,    -1,   101,     3,   102,    -1,
      -1,    88,    -1,    -1,    10,     6,    -1,    -1,    60,    -1,
      55,    56,     7,   101,   154,   102,   158,    -1,     7,    -1,
     160,   104,     7,    -1,    -1,    58,    -1,    -1,    59,    -1,
      55,   161,   162,    57,     7,    61,     7,   101,   160,   102,
      -1,    85,    87,    -1,    86,    87,    -1,    19,    -1,    20,
      -1,    22,    -1,    17,    -1,    18,    -1,     7,    15,   166,
      -1,     7,    16,   166,    -1,    -1,   167,    -1,   168,   104,
     167,    -1,     7,   166,   100,    -1,    -1,   169,    -1,   170,
     169,    -1,    63,    64,     7,    23,   128,   100,    -1,    63,
      90,     7,   100,    -1,   171,    -1,   172,    -1,    -1,   173,
      -1,   174,   173,    -1,    14,     7,   101,   168,   102,    23,
     170,   174,    24,   109,    25,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   134,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   160,   161,   166,   167,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
     191,   196,   197,   198,   199,   201,   202,   203,   204,   205,
     206,   207,   210,   212,   213,   217,   222,   227,   228,   229,
     233,   237,   238,   243,   244,   245,   250,   251,   252,   256,
     257,   262,   268,   275,   276,   277,   282,   284,   286,   290,
     291,   295,   296,   301,   302,   307,   308,   309,   313,   314,
     319,   329,   334,   336,   341,   345,   346,   351,   357,   364,
     369,   374,   380,   385,   390,   395,   400,   406,   407,   412,
     413,   415,   419,   426,   432,   440,   444,   448,   454,   460,
     462,   467,   472,   473,   478,   479,   484,   485,   491,   492,
     498,   499,   505,   511,   512,   517,   518,   522,   523,   527,
     535,   540,   545,   546,   547,   548,   549,   553,   556,   562,
     563,   564,   569,   573,   575,   576,   580,   586,   591,   592,
     595,   597,   598,   602
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PARS_INT_LIT", "PARS_FLOAT_LIT",
  "PARS_STR_LIT", "PARS_NULL_LIT", "PARS_ID_TOKEN", "PARS_AND_TOKEN",
  "PARS_OR_TOKEN", "PARS_NOT_TOKEN", "PARS_GE_TOKEN", "PARS_LE_TOKEN",
  "PARS_NE_TOKEN", "PARS_PROCEDURE_TOKEN", "PARS_IN_TOKEN",
  "PARS_OUT_TOKEN", "PARS_BINARY_TOKEN", "PARS_BLOB_TOKEN",
  "PARS_INT_TOKEN", "PARS_INTEGER_TOKEN", "PARS_FLOAT_TOKEN",
  "PARS_CHAR_TOKEN", "PARS_IS_TOKEN", "PARS_BEGIN_TOKEN", "PARS_END_TOKEN",
  "PARS_IF_TOKEN", "PARS_THEN_TOKEN", "PARS_ELSE_TOKEN",
  "PARS_ELSIF_TOKEN", "PARS_LOOP_TOKEN", "PARS_WHILE_TOKEN",
  "PARS_RETURN_TOKEN", "PARS_SELECT_TOKEN", "PARS_SUM_TOKEN",
  "PARS_COUNT_TOKEN", "PARS_DISTINCT_TOKEN", "PARS_FROM_TOKEN",
  "PARS_WHERE_TOKEN", "PARS_FOR_TOKEN", "PARS_DDOT_TOKEN",
  "PARS_CONSISTENT_TOKEN", "PARS_READ_TOKEN", "PARS_ORDER_TOKEN",
  "PARS_BY_TOKEN", "PARS_ASC_TOKEN", "PARS_DESC_TOKEN",
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
  "PARS_UNSIGNED_TOKEN", "PARS_EXIT_TOKEN", "PARS_FUNCTION_TOKEN", "'='",
  "'<'", "'>'", "'-'", "'+'", "'*'", "'/'", "NEG", "'%'", "';'", "'('",
  "')'", "'?'", "','", "'{'", "'}'", "$accept", "statement",
  "statement_list", "exp", "function_name", "question_mark_list",
  "stored_procedure_call", "predefined_procedure_call",
  "predefined_procedure_name", "user_function_call", "table_list",
  "variable_list", "exp_list", "select_item", "select_item_list",
  "select_list", "search_condition", "for_update_clause",
  "consistent_read_clause", "order_direction", "order_by_clause",
  "select_statement", "insert_statement_start", "insert_statement",
  "column_assignment", "column_assignment_list", "cursor_positioned",
  "update_statement_start", "update_statement_searched",
  "update_statement_positioned", "delete_statement_start",
  "delete_statement_searched", "delete_statement_positioned",
  "row_printf_statement", "assignment_statement", "elsif_element",
  "elsif_list", "else_part", "if_statement", "while_statement",
  "for_statement", "exit_statement", "return_statement",
  "open_cursor_statement", "close_cursor_statement", "fetch_statement",
  "column_def", "column_def_list", "opt_column_len", "opt_unsigned",
  "opt_not_null", "not_fit_in_memory", "create_table", "column_list",
  "unique_def", "clustered_def", "create_index", "commit_statement",
  "rollback_statement", "type_name", "parameter_declaration",
  "parameter_declaration_list", "variable_declaration",
  "variable_declaration_list", "cursor_declaration",
  "function_declaration", "declaration", "declaration_list",
  "procedure_definition", 0
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
     345,    61,    60,    62,    45,    43,    42,    47,   346,    37,
      59,    40,    41,    63,    44,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   107,   108,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   108,   108,   108,   108,   108,   108,   108,   108,
     108,   108,   108,   108,   108,   109,   109,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   112,   112,   112,   113,   114,   115,   115,   115,
     116,   117,   117,   118,   118,   118,   119,   119,   119,   120,
     120,   120,   120,   121,   121,   121,   122,   122,   122,   123,
     123,   124,   124,   125,   125,   126,   126,   126,   127,   127,
     128,   129,   130,   130,   131,   132,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   143,   144,
     144,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     152,   153,   154,   154,   155,   155,   156,   156,   157,   157,
     158,   158,   159,   160,   160,   161,   161,   162,   162,   163,
     164,   165,   166,   166,   166,   166,   166,   167,   167,   168,
     168,   168,   169,   170,   170,   170,   171,   172,   173,   173,
     174,   174,   174,   175
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     2,     1,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     1,     2,     1,     4,     1,
       1,     1,     1,     1,     3,     3,     3,     3,     2,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     3,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     1,     3,     6,     4,     1,     1,     1,
       3,     1,     3,     0,     1,     3,     0,     1,     3,     1,
       4,     5,     4,     0,     1,     3,     1,     3,     1,     0,
       2,     0,     2,     0,     2,     0,     1,     1,     0,     4,
       8,     3,     5,     2,     3,     1,     3,     4,     4,     2,
       2,     3,     2,     2,     2,     3,     4,     1,     2,     0,
       2,     1,     7,     6,    10,     1,     1,     2,     2,     4,
       4,     5,     1,     3,     0,     3,     0,     1,     0,     2,
       0,     1,     7,     1,     3,     0,     1,     0,     1,    10,
       2,     2,     1,     1,     1,     1,     1,     3,     3,     0,
       1,     3,     3,     0,     1,     2,     6,     4,     1,     1,
       0,     1,     2,    11
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,     0,     0,     0,   126,    83,     0,     0,     0,
       0,   145,     0,     0,     0,    67,    68,    69,     0,     0,
       0,   125,     0,     0,     3,     0,     0,     0,     0,     0,
      89,     0,     0,    89,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,    30,    31,    32,    27,     0,    33,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,     0,     0,     0,     0,     0,     0,     0,    86,    79,
      84,    88,     0,     0,     0,     0,     0,     0,   146,   147,
     127,     0,   128,   114,   150,   151,     0,     1,     4,    76,
      11,     0,   103,    12,     0,   109,   110,    16,    17,   112,
     113,    14,    15,    13,    10,     8,     5,     6,     7,     9,
      18,    20,    19,    23,    24,    21,    22,     2,   115,   159,
       0,    48,     0,    38,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    76,     0,
       0,     0,    73,     0,     0,     0,   101,     0,   111,     0,
     148,     0,    73,    62,    77,     0,    76,     0,    90,     0,
     160,     0,    49,    50,    39,    46,    47,    43,    44,    45,
      25,   119,    40,    41,    42,    35,    34,    36,    37,     0,
       0,     0,     0,     0,    74,    87,    85,    71,    89,     0,
       0,   105,   108,     0,     0,    74,   130,   129,    63,     0,
      66,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      26,   117,   121,     0,    28,     0,    82,     0,    80,     0,
       0,     0,    91,     0,     0,     0,     0,   132,     0,     0,
       0,     0,     0,    78,   102,   107,   155,   156,   152,   153,
     154,   157,   158,   163,   161,   120,     0,   118,     0,   123,
      81,    75,    72,     0,    93,     0,   104,   106,   134,   140,
       0,     0,    70,    65,    64,     0,   164,   170,     0,   122,
      92,     0,    98,     0,     0,   136,   141,   142,   133,     0,
       0,     0,   165,   168,   169,   171,     0,   116,    94,     0,
     100,     0,     0,   137,   138,     0,   162,     0,     0,     0,
     172,     0,     0,   135,     0,   131,   143,     0,     0,     0,
       0,    95,   124,   139,   149,     0,     0,   167,   173,    96,
      97,    99,   144,     0,   166
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,   180,   181,   164,    74,   209,    24,    25,    26,   206,
     198,   195,   165,    80,    81,    82,   105,   264,   282,   331,
     300,    27,    28,    29,   201,   202,   106,    30,    31,    32,
      33,    34,    35,    36,    37,   221,   222,   223,    38,    39,
      40,    41,    42,    43,    44,    45,   237,   238,   285,   304,
     315,   287,    46,   317,    89,   161,    47,    48,    49,   251,
     170,   171,   276,   277,   293,   294,   295,   296,    50
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -196
static const short int yypact[] =
{
     384,   -31,    32,   522,   522,  -196,   121,    36,     7,    39,
      12,   -11,    52,    59,    77,  -196,  -196,  -196,    40,    42,
      46,  -196,    82,    95,  -196,    47,    38,    50,    -5,    51,
     132,    71,    72,   132,    73,    74,    75,    76,    78,    80,
      83,    84,    85,    87,    88,    89,    96,   101,   105,   106,
     108,   522,    81,  -196,  -196,  -196,  -196,   112,   522,   113,
    -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,
    -196,   522,   522,   485,   115,   555,   118,   122,  -196,   606,
    -196,   -41,   140,   192,   206,   163,   217,   218,  -196,   171,
    -196,   183,  -196,  -196,  -196,  -196,   134,  -196,  -196,   522,
    -196,   135,  -196,  -196,   471,  -196,  -196,  -196,  -196,  -196,
    -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,
    -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,   606,   230,
     169,    24,   170,   227,     5,   522,   522,   522,   522,   522,
     384,   522,   522,   522,   522,   522,   522,   522,   522,   384,
     522,   -13,   234,   437,   236,   522,  -196,   237,  -196,   145,
    -196,   191,   242,   148,   606,   -50,   522,   198,   606,   107,
    -196,   -44,  -196,  -196,  -196,    24,    24,    -3,    -3,   606,
    -196,     1,    -3,    -3,    -3,     8,     8,   227,   227,   -40,
     195,    66,   246,   152,  -196,   153,  -196,  -196,   -32,   548,
     165,  -196,   154,   252,   253,   164,  -196,   153,  -196,    10,
    -196,   522,    28,   257,    91,    91,   243,   230,   384,   522,
    -196,  -196,   239,   244,  -196,   241,  -196,   175,  -196,   265,
     522,   266,   248,   522,   522,   237,    91,  -196,    62,   221,
     176,   177,   185,   606,  -196,  -196,  -196,  -196,  -196,  -196,
    -196,  -196,  -196,   282,  -196,   384,   562,  -196,   264,  -196,
    -196,  -196,  -196,   245,   255,   598,   606,  -196,   193,   238,
     252,   295,  -196,  -196,  -196,    91,  -196,     2,   384,  -196,
    -196,   261,   262,   384,   301,   220,  -196,  -196,  -196,   205,
     211,   -52,  -196,  -196,  -196,  -196,    -2,   384,  -196,   269,
    -196,   260,   212,  -196,   306,   310,  -196,   311,   312,   384,
    -196,   314,   292,  -196,   317,  -196,  -196,    63,   302,   224,
     322,   -20,  -196,  -196,  -196,   323,    40,  -196,  -196,  -196,
    -196,  -196,  -196,   231,  -196
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -196,     0,  -130,    -1,  -196,  -196,  -196,  -196,  -196,  -196,
    -196,   172,  -124,   179,  -196,  -196,   -29,  -196,  -196,  -196,
    -196,   -17,  -196,  -196,    98,  -196,   304,  -196,  -196,  -196,
    -196,  -196,  -196,  -196,  -196,   116,  -196,  -196,  -196,  -196,
    -196,  -196,  -196,  -196,  -196,  -196,    69,  -196,  -196,  -196,
    -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -196,  -195,
     125,  -196,    79,  -196,  -196,  -196,    54,  -196,  -196
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned short int yytable[] =
{
      23,    93,    73,    75,   109,    79,   230,   152,     1,   275,
     139,   102,   307,   135,   136,     2,   137,   138,   139,   190,
     252,   139,   309,   192,   189,   329,   330,     3,     6,   218,
     219,    51,     4,     5,     6,   137,   138,   139,   308,    52,
       7,   268,   212,    83,   101,    87,    85,    88,     8,    86,
     128,     9,   210,    10,   211,    84,    11,   131,   216,    90,
     217,   291,   224,   153,   211,   291,    91,    12,    13,    14,
     133,   134,   231,     6,   135,   136,    15,   137,   138,   139,
     290,    16,    17,   193,    92,    18,    19,    20,   255,    96,
      21,   144,   145,   146,   147,    97,   141,   142,   143,   144,
     145,   146,   147,   168,   146,   147,    22,   174,   246,   247,
     248,   249,   241,   250,   242,   141,   142,   143,   144,   145,
     146,   147,   214,   215,    53,    54,    55,    56,    57,    94,
     244,    58,   211,    95,   175,   176,   177,   178,   179,    99,
     182,   183,   184,   185,   186,   187,   188,    98,   297,   191,
     100,   103,    79,   301,   199,    76,    77,   141,   142,   143,
     144,   145,   146,   147,   269,   324,   270,   325,   226,   232,
     104,   107,   108,   111,   112,   113,   114,   154,   115,   320,
     116,   220,   129,   117,   118,   119,    59,   120,   121,   122,
     220,    60,    61,    62,    63,    64,   123,    65,    66,    67,
      68,   124,     1,    69,    70,   125,   126,   155,   127,     2,
     243,   130,   132,   156,   157,    71,   148,    78,   256,   150,
     225,     3,    72,   151,   158,   159,     4,     5,     6,   168,
     160,   162,   265,   266,     7,   163,   166,   169,   172,   173,
     139,   194,     8,   197,   200,     9,   203,    10,   204,   205,
      11,   208,   213,   227,   228,   220,   234,   229,   235,   236,
     239,    12,    13,    14,   245,   240,   253,     1,   219,   258,
      15,   259,   261,   262,     2,    16,    17,   260,   272,    18,
      19,    20,   271,   273,    21,   312,     3,   263,   274,   275,
     279,     4,     5,     6,   284,   280,   281,   220,   286,     7,
      22,   220,   289,   298,   302,   299,   305,     8,   303,   333,
       9,   306,    10,   311,   313,    11,   314,   316,   318,   319,
     220,   321,   322,   323,   327,   326,    12,    13,    14,     1,
     332,   334,   196,   267,   207,    15,     2,   110,   257,   288,
      16,    17,   254,     0,    18,    19,    20,   328,     3,    21,
     310,     0,     0,     4,     5,     6,   292,     0,     0,     0,
       0,     7,     0,     0,     0,    22,     0,     0,     0,     8,
       0,     0,     9,     0,    10,     0,     0,    11,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    12,    13,
      14,     1,     0,     0,     0,     0,     0,    15,     2,     0,
       0,     0,    16,    17,     0,     0,    18,    19,    20,     0,
       3,    21,     0,     0,     0,     4,     5,     6,     0,     0,
       0,     0,     0,     7,     0,     0,     0,    22,     0,     0,
       0,     8,     0,     0,     9,     0,    10,     0,     0,    11,
      53,    54,    55,    56,    57,     0,     0,    58,     0,     0,
      12,    13,    14,     0,     0,     0,     0,     0,     0,    15,
       0,     0,     0,     0,    16,    17,     0,     0,    18,    19,
      20,    76,    77,    21,    53,    54,    55,    56,    57,     0,
       0,    58,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,   135,   136,     0,   137,   138,   139,     0,
       0,     0,    59,     0,     0,     0,     0,    60,    61,    62,
      63,    64,   140,    65,    66,    67,    68,     0,     0,    69,
      70,     0,     0,     0,   167,    53,    54,    55,    56,    57,
       0,    71,    58,     0,     0,     0,    59,     0,    72,     0,
       0,    60,    61,    62,    63,    64,     0,    65,    66,    67,
      68,     0,     0,    69,    70,     0,   135,   136,     0,   137,
     138,   139,     0,   135,   136,    71,   137,   138,   139,     0,
     135,   136,    72,   137,   138,   139,   141,   142,   143,   144,
     145,   146,   147,     0,     0,   149,     0,    59,   233,   278,
       0,     0,    60,    61,    62,    63,    64,     0,    65,    66,
      67,    68,     0,     0,    69,    70,   135,   136,     0,   137,
     138,   139,     0,     0,   135,   136,    71,   137,   138,   139,
       0,     0,     0,    72,     0,     0,     0,     0,   283,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   141,
     142,   143,   144,   145,   146,   147,   141,   142,   143,   144,
     145,   146,   147,   141,   142,   143,   144,   145,   146,   147,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   141,
     142,   143,   144,   145,   146,   147,     0,   141,   142,   143,
     144,   145,   146,   147
};

static const short int yycheck[] =
{
       0,    18,     3,     4,    33,     6,    38,    48,     7,     7,
      13,    28,    64,     8,     9,    14,    11,    12,    13,   149,
     215,    13,    24,    36,   148,    45,    46,    26,    33,    28,
      29,    62,    31,    32,    33,    11,    12,    13,    90,     7,
      39,   236,   166,     7,    49,    56,     7,    58,    47,    37,
      51,    50,   102,    52,   104,    48,    55,    58,   102,     7,
     104,    63,   102,   104,   104,    63,     7,    66,    67,    68,
      71,    72,   104,    33,     8,     9,    75,    11,    12,    13,
     275,    80,    81,    96,     7,    84,    85,    86,   218,     7,
      89,    94,    95,    96,    97,     0,    91,    92,    93,    94,
      95,    96,    97,   104,    96,    97,   105,   102,    17,    18,
      19,    20,   102,    22,   104,    91,    92,    93,    94,    95,
      96,    97,    15,    16,     3,     4,     5,     6,     7,    87,
     102,    10,   104,    87,   135,   136,   137,   138,   139,   101,
     141,   142,   143,   144,   145,   146,   147,   100,   278,   150,
     100,   100,   153,   283,   155,    34,    35,    91,    92,    93,
      94,    95,    96,    97,   102,   102,   104,   104,   102,   198,
      38,   100,   100,   100,   100,   100,   100,    37,   100,   309,
     100,   181,   101,   100,   100,   100,    65,   100,   100,   100,
     190,    70,    71,    72,    73,    74,   100,    76,    77,    78,
      79,   100,     7,    82,    83,   100,   100,    15,   100,    14,
     211,    99,    99,     7,    51,    94,   101,    96,   219,   101,
      25,    26,   101,   101,     7,     7,    31,    32,    33,   230,
      59,    48,   233,   234,    39,   101,   101,     7,    69,    69,
      13,     7,    47,     7,     7,    50,   101,    52,    57,     7,
      55,   103,    54,     7,   102,   255,    91,   104,   104,     7,
       7,    66,    67,    68,     7,   101,    23,     7,    29,    25,
      75,    30,     7,     7,    14,    80,    81,   102,   102,    84,
      85,    86,    61,   106,    89,    25,    26,    39,   103,     7,
      26,    31,    32,    33,   101,    50,    41,   297,    60,    39,
     105,   301,     7,    42,     3,    43,   101,    47,    88,   326,
      50,   100,    52,    44,   102,    55,    10,     7,     7,     7,
     320,     7,    30,     6,   100,    23,    66,    67,    68,     7,
       7,   100,   153,   235,   162,    75,    14,    33,   222,   270,
      80,    81,   217,    -1,    84,    85,    86,    25,    26,    89,
     296,    -1,    -1,    31,    32,    33,   277,    -1,    -1,    -1,
      -1,    39,    -1,    -1,    -1,   105,    -1,    -1,    -1,    47,
      -1,    -1,    50,    -1,    52,    -1,    -1,    55,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,     7,    -1,    -1,    -1,    -1,    -1,    75,    14,    -1,
      -1,    -1,    80,    81,    -1,    -1,    84,    85,    86,    -1,
      26,    89,    -1,    -1,    -1,    31,    32,    33,    -1,    -1,
      -1,    -1,    -1,    39,    -1,    -1,    -1,   105,    -1,    -1,
      -1,    47,    -1,    -1,    50,    -1,    52,    -1,    -1,    55,
       3,     4,     5,     6,     7,    -1,    -1,    10,    -1,    -1,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    75,
      -1,    -1,    -1,    -1,    80,    81,    -1,    -1,    84,    85,
      86,    34,    35,    89,     3,     4,     5,     6,     7,    -1,
      -1,    10,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   105,
      -1,    -1,    -1,     8,     9,    -1,    11,    12,    13,    -1,
      -1,    -1,    65,    -1,    -1,    -1,    -1,    70,    71,    72,
      73,    74,    27,    76,    77,    78,    79,    -1,    -1,    82,
      83,    -1,    -1,    -1,    53,     3,     4,     5,     6,     7,
      -1,    94,    10,    -1,    -1,    -1,    65,    -1,   101,    -1,
      -1,    70,    71,    72,    73,    74,    -1,    76,    77,    78,
      79,    -1,    -1,    82,    83,    -1,     8,     9,    -1,    11,
      12,    13,    -1,     8,     9,    94,    11,    12,    13,    -1,
       8,     9,   101,    11,    12,    13,    91,    92,    93,    94,
      95,    96,    97,    -1,    -1,    30,    -1,    65,    40,    27,
      -1,    -1,    70,    71,    72,    73,    74,    -1,    76,    77,
      78,    79,    -1,    -1,    82,    83,     8,     9,    -1,    11,
      12,    13,    -1,    -1,     8,     9,    94,    11,    12,    13,
      -1,    -1,    -1,   101,    -1,    -1,    -1,    -1,    30,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    93,    94,    95,    96,    97,    91,    92,    93,    94,
      95,    96,    97,    91,    92,    93,    94,    95,    96,    97,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      92,    93,    94,    95,    96,    97,    -1,    91,    92,    93,
      94,    95,    96,    97
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     7,    14,    26,    31,    32,    33,    39,    47,    50,
      52,    55,    66,    67,    68,    75,    80,    81,    84,    85,
      86,    89,   105,   108,   113,   114,   115,   128,   129,   130,
     134,   135,   136,   137,   138,   139,   140,   141,   145,   146,
     147,   148,   149,   150,   151,   152,   159,   163,   164,   165,
     175,    62,     7,     3,     4,     5,     6,     7,    10,    65,
      70,    71,    72,    73,    74,    76,    77,    78,    79,    82,
      83,    94,   101,   110,   111,   110,    34,    35,    96,   110,
     120,   121,   122,     7,    48,     7,    37,    56,    58,   161,
       7,     7,     7,   128,    87,    87,     7,     0,   100,   101,
     100,    49,   128,   100,    38,   123,   133,   100,   100,   123,
     133,   100,   100,   100,   100,   100,   100,   100,   100,   100,
     100,   100,   100,   100,   100,   100,   100,   100,   110,   101,
      99,   110,    99,   110,   110,     8,     9,    11,    12,    13,
      27,    91,    92,    93,    94,    95,    96,    97,   101,    30,
     101,   101,    48,   104,    37,    15,     7,    51,     7,     7,
      59,   162,    48,   101,   110,   119,   101,    53,   110,     7,
     167,   168,    69,    69,   102,   110,   110,   110,   110,   110,
     108,   109,   110,   110,   110,   110,   110,   110,   110,   119,
     109,   110,    36,    96,     7,   118,   120,     7,   117,   110,
       7,   131,   132,   101,    57,     7,   116,   118,   103,   112,
     102,   104,   119,    54,    15,    16,   102,   104,    28,    29,
     108,   142,   143,   144,   102,    25,   102,     7,   102,   104,
      38,   104,   123,    40,    91,   104,     7,   153,   154,     7,
     101,   102,   104,   110,   102,     7,    17,    18,    19,    20,
      22,   166,   166,    23,   167,   109,   110,   142,    25,    30,
     102,     7,     7,    39,   124,   110,   110,   131,   166,   102,
     104,    61,   102,   106,   103,     7,   169,   170,    27,    26,
      50,    41,   125,    30,   101,   155,    60,   158,   153,     7,
     166,    63,   169,   171,   172,   173,   174,   109,    42,    43,
     127,   109,     3,    88,   156,   101,   100,    64,    90,    24,
     173,    44,    25,   102,    10,   157,     7,   160,     7,     7,
     109,     7,    30,     6,   102,   104,    23,   100,    25,    45,
      46,   126,     7,   128,   100
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
#line 160 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 26:
#line 162 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 27:
#line 166 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 28:
#line 168 "pars0grm.y"
    { yyval = pars_func(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 29:
#line 169 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 30:
#line 170 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 31:
#line 171 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 32:
#line 172 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 33:
#line 173 "pars0grm.y"
    { yyval = yyvsp[0];;}
    break;

  case 34:
#line 174 "pars0grm.y"
    { yyval = pars_op('+', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 35:
#line 175 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 36:
#line 176 "pars0grm.y"
    { yyval = pars_op('*', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 37:
#line 177 "pars0grm.y"
    { yyval = pars_op('/', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 38:
#line 178 "pars0grm.y"
    { yyval = pars_op('-', yyvsp[0], NULL); ;}
    break;

  case 39:
#line 179 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 40:
#line 180 "pars0grm.y"
    { yyval = pars_op('=', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 41:
#line 181 "pars0grm.y"
    { yyval = pars_op('<', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 42:
#line 182 "pars0grm.y"
    { yyval = pars_op('>', yyvsp[-2], yyvsp[0]); ;}
    break;

  case 43:
#line 183 "pars0grm.y"
    { yyval = pars_op(PARS_GE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 44:
#line 184 "pars0grm.y"
    { yyval = pars_op(PARS_LE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 45:
#line 185 "pars0grm.y"
    { yyval = pars_op(PARS_NE_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 46:
#line 186 "pars0grm.y"
    { yyval = pars_op(PARS_AND_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 47:
#line 187 "pars0grm.y"
    { yyval = pars_op(PARS_OR_TOKEN, yyvsp[-2], yyvsp[0]); ;}
    break;

  case 48:
#line 188 "pars0grm.y"
    { yyval = pars_op(PARS_NOT_TOKEN, yyvsp[0], NULL); ;}
    break;

  case 49:
#line 190 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 50:
#line 192 "pars0grm.y"
    { yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;}
    break;

  case 51:
#line 196 "pars0grm.y"
    { yyval = &pars_to_char_token; ;}
    break;

  case 52:
#line 197 "pars0grm.y"
    { yyval = &pars_to_number_token; ;}
    break;

  case 53:
#line 198 "pars0grm.y"
    { yyval = &pars_to_binary_token; ;}
    break;

  case 54:
#line 200 "pars0grm.y"
    { yyval = &pars_binary_to_number_token; ;}
    break;

  case 55:
#line 201 "pars0grm.y"
    { yyval = &pars_substr_token; ;}
    break;

  case 56:
#line 202 "pars0grm.y"
    { yyval = &pars_concat_token; ;}
    break;

  case 57:
#line 203 "pars0grm.y"
    { yyval = &pars_instr_token; ;}
    break;

  case 58:
#line 204 "pars0grm.y"
    { yyval = &pars_length_token; ;}
    break;

  case 59:
#line 205 "pars0grm.y"
    { yyval = &pars_sysdate_token; ;}
    break;

  case 60:
#line 206 "pars0grm.y"
    { yyval = &pars_rnd_token; ;}
    break;

  case 61:
#line 207 "pars0grm.y"
    { yyval = &pars_rnd_str_token; ;}
    break;

  case 65:
#line 218 "pars0grm.y"
    { yyval = pars_stored_procedure_call(yyvsp[-4]); ;}
    break;

  case 66:
#line 223 "pars0grm.y"
    { yyval = pars_procedure_call(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 67:
#line 227 "pars0grm.y"
    { yyval = &pars_replstr_token; ;}
    break;

  case 68:
#line 228 "pars0grm.y"
    { yyval = &pars_printf_token; ;}
    break;

  case 69:
#line 229 "pars0grm.y"
    { yyval = &pars_assert_token; ;}
    break;

  case 70:
#line 233 "pars0grm.y"
    { yyval = yyvsp[-2]; ;}
    break;

  case 71:
#line 237 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 72:
#line 239 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 73:
#line 243 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 74:
#line 244 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 75:
#line 246 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 76:
#line 250 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 77:
#line 251 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]);;}
    break;

  case 78:
#line 252 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 79:
#line 256 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 80:
#line 258 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); ;}
    break;

  case 81:
#line 263 "pars0grm.y"
    { yyval = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, yyvsp[-1])))); ;}
    break;

  case 82:
#line 269 "pars0grm.y"
    { yyval = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									yyvsp[-1])); ;}
    break;

  case 83:
#line 275 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 84:
#line 276 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 85:
#line 278 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 86:
#line 282 "pars0grm.y"
    { yyval = pars_select_list(&pars_star_denoter,
								NULL); ;}
    break;

  case 87:
#line 285 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 88:
#line 286 "pars0grm.y"
    { yyval = pars_select_list(yyvsp[0], NULL); ;}
    break;

  case 89:
#line 290 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 90:
#line 291 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 91:
#line 295 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 92:
#line 297 "pars0grm.y"
    { yyval = &pars_update_token; ;}
    break;

  case 93:
#line 301 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 94:
#line 303 "pars0grm.y"
    { yyval = &pars_consistent_token; ;}
    break;

  case 95:
#line 307 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 96:
#line 308 "pars0grm.y"
    { yyval = &pars_asc_token; ;}
    break;

  case 97:
#line 309 "pars0grm.y"
    { yyval = &pars_desc_token; ;}
    break;

  case 98:
#line 313 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 99:
#line 315 "pars0grm.y"
    { yyval = pars_order_by(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 100:
#line 324 "pars0grm.y"
    { yyval = pars_select_statement(yyvsp[-6], yyvsp[-4], yyvsp[-3],
								yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 101:
#line 330 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 102:
#line 335 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-4], yyvsp[-1], NULL); ;}
    break;

  case 103:
#line 337 "pars0grm.y"
    { yyval = pars_insert_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 104:
#line 341 "pars0grm.y"
    { yyval = pars_column_assignment(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 105:
#line 345 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 106:
#line 347 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 107:
#line 353 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 108:
#line 359 "pars0grm.y"
    { yyval = pars_update_statement_start(FALSE,
								yyvsp[-2], yyvsp[0]); ;}
    break;

  case 109:
#line 365 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 110:
#line 370 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 111:
#line 375 "pars0grm.y"
    { yyval = pars_update_statement_start(TRUE,
								yyvsp[0], NULL); ;}
    break;

  case 112:
#line 381 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;}
    break;

  case 113:
#line 386 "pars0grm.y"
    { yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;}
    break;

  case 114:
#line 391 "pars0grm.y"
    { yyval = pars_row_printf_statement(yyvsp[0]); ;}
    break;

  case 115:
#line 396 "pars0grm.y"
    { yyval = pars_assignment_statement(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 116:
#line 402 "pars0grm.y"
    { yyval = pars_elsif_element(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 117:
#line 406 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 118:
#line 408 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;}
    break;

  case 119:
#line 412 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 120:
#line 414 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 121:
#line 415 "pars0grm.y"
    { yyval = yyvsp[0]; ;}
    break;

  case 122:
#line 422 "pars0grm.y"
    { yyval = pars_if_statement(yyvsp[-5], yyvsp[-3], yyvsp[-2]); ;}
    break;

  case 123:
#line 428 "pars0grm.y"
    { yyval = pars_while_statement(yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 124:
#line 436 "pars0grm.y"
    { yyval = pars_for_statement(yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2]); ;}
    break;

  case 125:
#line 440 "pars0grm.y"
    { yyval = pars_exit_statement(); ;}
    break;

  case 126:
#line 444 "pars0grm.y"
    { yyval = pars_return_statement(); ;}
    break;

  case 127:
#line 449 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_OPEN_CURSOR, yyvsp[0]); ;}
    break;

  case 128:
#line 455 "pars0grm.y"
    { yyval = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR, yyvsp[0]); ;}
    break;

  case 129:
#line 461 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], yyvsp[0], NULL); ;}
    break;

  case 130:
#line 463 "pars0grm.y"
    { yyval = pars_fetch_statement(yyvsp[-2], NULL, yyvsp[0]); ;}
    break;

  case 131:
#line 468 "pars0grm.y"
    { yyval = pars_column_def(yyvsp[-4], yyvsp[-3], yyvsp[-2], yyvsp[-1], yyvsp[0]); ;}
    break;

  case 132:
#line 472 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 133:
#line 474 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 134:
#line 478 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 135:
#line 480 "pars0grm.y"
    { yyval = yyvsp[-1]; ;}
    break;

  case 136:
#line 484 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 137:
#line 486 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 138:
#line 491 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 139:
#line 493 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 140:
#line 498 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 141:
#line 500 "pars0grm.y"
    { yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 142:
#line 507 "pars0grm.y"
    { yyval = pars_create_table(yyvsp[-4], yyvsp[-2], yyvsp[0]); ;}
    break;

  case 143:
#line 511 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 144:
#line 513 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 145:
#line 517 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 146:
#line 518 "pars0grm.y"
    { yyval = &pars_unique_token; ;}
    break;

  case 147:
#line 522 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 148:
#line 523 "pars0grm.y"
    { yyval = &pars_clustered_token; ;}
    break;

  case 149:
#line 531 "pars0grm.y"
    { yyval = pars_create_index(yyvsp[-8], yyvsp[-7], yyvsp[-5], yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 150:
#line 536 "pars0grm.y"
    { yyval = pars_commit_statement(); ;}
    break;

  case 151:
#line 541 "pars0grm.y"
    { yyval = pars_rollback_statement(); ;}
    break;

  case 152:
#line 545 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 153:
#line 546 "pars0grm.y"
    { yyval = &pars_int_token; ;}
    break;

  case 154:
#line 547 "pars0grm.y"
    { yyval = &pars_char_token; ;}
    break;

  case 155:
#line 548 "pars0grm.y"
    { yyval = &pars_binary_token; ;}
    break;

  case 156:
#line 549 "pars0grm.y"
    { yyval = &pars_blob_token; ;}
    break;

  case 157:
#line 554 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_INPUT, yyvsp[0]); ;}
    break;

  case 158:
#line 557 "pars0grm.y"
    { yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_OUTPUT, yyvsp[0]); ;}
    break;

  case 159:
#line 562 "pars0grm.y"
    { yyval = NULL; ;}
    break;

  case 160:
#line 563 "pars0grm.y"
    { yyval = que_node_list_add_last(NULL, yyvsp[0]); ;}
    break;

  case 161:
#line 565 "pars0grm.y"
    { yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;}
    break;

  case 162:
#line 570 "pars0grm.y"
    { yyval = pars_variable_declaration(yyvsp[-2], yyvsp[-1]); ;}
    break;

  case 166:
#line 582 "pars0grm.y"
    { yyval = pars_cursor_declaration(yyvsp[-3], yyvsp[-1]); ;}
    break;

  case 167:
#line 587 "pars0grm.y"
    { yyval = pars_function_declaration(yyvsp[-1]); ;}
    break;

  case 173:
#line 608 "pars0grm.y"
    { yyval = pars_procedure_definition(yyvsp[-9], yyvsp[-7],
								yyvsp[-1]); ;}
    break;


    }

/* Line 1010 of yacc.c.  */
#line 2313 "pars0grm.tab.c"

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


#line 612 "pars0grm.y"


