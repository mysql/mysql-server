/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

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
/* Tokens.  */
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
#line 28 "pars0grm.y"

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

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 316 "pars0grm.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   752

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  111
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  70
/* YYNRULES -- Number of rules.  */
#define YYNRULES  175
/* YYNRULES -- Number of states.  */
#define YYNSTATES  339

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   350

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
static const yytype_uint16 yyprhs[] =
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

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     112,     0,    -1,   180,   104,    -1,   118,    -1,   119,   104,
      -1,   151,   104,    -1,   152,   104,    -1,   153,   104,    -1,
     150,   104,    -1,   154,   104,    -1,   146,   104,    -1,   133,
     104,    -1,   135,   104,    -1,   145,   104,    -1,   143,   104,
      -1,   144,   104,    -1,   140,   104,    -1,   141,   104,    -1,
     155,   104,    -1,   157,   104,    -1,   156,   104,    -1,   169,
     104,    -1,   170,   104,    -1,   164,   104,    -1,   168,   104,
      -1,   113,    -1,   114,   113,    -1,     9,    -1,   116,   105,
     124,   106,    -1,     3,    -1,     4,    -1,     5,    -1,     6,
      -1,     7,    -1,     8,    -1,    66,    -1,   115,    99,   115,
      -1,   115,    98,   115,    -1,   115,   100,   115,    -1,   115,
     101,   115,    -1,    98,   115,    -1,   105,   115,   106,    -1,
     115,    95,   115,    -1,   115,    96,   115,    -1,   115,    97,
     115,    -1,   115,    13,   115,    -1,   115,    14,   115,    -1,
     115,    15,   115,    -1,   115,    10,   115,    -1,   115,    11,
     115,    -1,    12,   115,    -1,     9,   103,    70,    -1,    66,
     103,    70,    -1,    71,    -1,    72,    -1,    73,    -1,    74,
      -1,    75,    -1,    77,    -1,    78,    -1,    79,    -1,    80,
      -1,    83,    -1,    84,    -1,    -1,   107,    -1,   117,   108,
     107,    -1,   109,     9,   105,   117,   106,   110,    -1,   120,
     105,   124,   106,    -1,    76,    -1,    81,    -1,    82,    -1,
       9,   105,   106,    -1,     9,    -1,   122,   108,     9,    -1,
      -1,     9,    -1,   123,   108,     9,    -1,    -1,   115,    -1,
     124,   108,   115,    -1,   115,    -1,    37,   105,   100,   106,
      -1,    37,   105,    38,     9,   106,    -1,    36,   105,   115,
     106,    -1,    -1,   125,    -1,   126,   108,   125,    -1,   100,
      -1,   126,    49,   123,    -1,   126,    -1,    -1,    40,   115,
      -1,    -1,    41,    51,    -1,    -1,    92,    17,    93,    94,
      -1,    -1,    46,    -1,    47,    -1,    -1,    44,    45,     9,
     131,    -1,    35,   127,    39,   122,   128,   129,   130,   132,
      -1,    48,    49,     9,    -1,   134,    50,   105,   124,   106,
      -1,   134,   133,    -1,     9,    95,   115,    -1,   136,    -1,
     137,   108,   136,    -1,    40,    54,    55,     9,    -1,    51,
       9,    52,   137,    -1,   139,   128,    -1,   139,   138,    -1,
      53,    39,     9,    -1,   142,   128,    -1,   142,   138,    -1,
      85,   133,    -1,     9,    63,   115,    -1,    31,   115,    29,
     114,    -1,   147,    -1,   148,   147,    -1,    -1,    30,   114,
      -1,   148,    -1,    28,   115,    29,   114,   149,    27,    28,
      -1,    33,   115,    32,   114,    27,    32,    -1,    41,     9,
      17,   115,    42,   115,    32,   114,    27,    32,    -1,    90,
      -1,    34,    -1,    67,     9,    -1,    69,     9,    -1,    68,
       9,    49,   123,    -1,    68,     9,    49,   121,    -1,     9,
     171,   160,   161,   162,    -1,   158,    -1,   159,   108,   158,
      -1,    -1,   105,     3,   106,    -1,    -1,    89,    -1,    -1,
      12,     8,    -1,    -1,    61,    -1,    56,    57,     9,   105,
     159,   106,   163,    -1,     9,    -1,   165,   108,     9,    -1,
      -1,    59,    -1,    -1,    60,    -1,    56,   166,   167,    58,
       9,    62,     9,   105,   165,   106,    -1,    86,    88,    -1,
      87,    88,    -1,    21,    -1,    22,    -1,    24,    -1,    19,
      -1,    20,    -1,     9,    17,   171,    -1,     9,    18,   171,
      -1,    -1,   172,    -1,   173,   108,   172,    -1,     9,   171,
     104,    -1,    -1,   174,    -1,   175,   174,    -1,    64,    65,
       9,    25,   133,   104,    -1,    64,    91,     9,   104,    -1,
     176,    -1,   177,    -1,    -1,   178,    -1,   179,   178,    -1,
      16,     9,   105,   173,   106,    25,   175,   179,    26,   114,
      27,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   153,   153,   156,   157,   158,   159,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   181,   182,   187,   188,   190,
     191,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,   203,   204,   205,   206,   207,   208,   209,   210,
     211,   212,   214,   219,   220,   221,   222,   224,   225,   226,
     227,   228,   229,   230,   233,   235,   236,   240,   248,   253,
     254,   255,   259,   263,   264,   269,   270,   271,   276,   277,
     278,   282,   283,   288,   294,   301,   302,   303,   308,   310,
     313,   317,   318,   322,   323,   328,   329,   334,   335,   336,
     340,   341,   348,   363,   368,   372,   380,   386,   387,   392,
     398,   407,   415,   423,   431,   439,   447,   453,   460,   466,
     467,   472,   473,   475,   479,   486,   492,   502,   506,   510,
     517,   524,   528,   536,   546,   547,   552,   553,   558,   559,
     565,   566,   572,   573,   579,   588,   589,   594,   595,   599,
     600,   604,   617,   622,   627,   628,   629,   630,   631,   635,
     640,   648,   649,   650,   655,   661,   663,   664,   668,   676,
     682,   683,   686,   688,   689,   693
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
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
  "'?'", "','", "'{'", "'}'", "$accept", "top_statement", "statement",
  "statement_list", "exp", "function_name", "question_mark_list",
  "stored_procedure_call", "predefined_procedure_call",
  "predefined_procedure_name", "user_function_call", "table_list",
  "variable_list", "exp_list", "select_item", "select_item_list",
  "select_list", "search_condition", "for_update_clause",
  "lock_shared_clause", "order_direction", "order_by_clause",
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
static const yytype_uint16 yytoknum[] =
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
static const yytype_uint8 yyr1[] =
{
       0,   111,   112,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   113,   113,   113,   113,   113,
     113,   113,   113,   113,   113,   114,   114,   115,   115,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   115,   115,
     115,   115,   115,   115,   115,   115,   115,   115,   115,   115,
     115,   115,   115,   116,   116,   116,   116,   116,   116,   116,
     116,   116,   116,   116,   117,   117,   117,   118,   119,   120,
     120,   120,   121,   122,   122,   123,   123,   123,   124,   124,
     124,   125,   125,   125,   125,   126,   126,   126,   127,   127,
     127,   128,   128,   129,   129,   130,   130,   131,   131,   131,
     132,   132,   133,   134,   135,   135,   136,   137,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     148,   149,   149,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   157,   158,   159,   159,   160,   160,   161,   161,
     162,   162,   163,   163,   164,   165,   165,   166,   166,   167,
     167,   168,   169,   170,   171,   171,   171,   171,   171,   172,
     172,   173,   173,   173,   174,   175,   175,   175,   176,   177,
     178,   178,   179,   179,   179,   180
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
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
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,     1,     2,   161,     0,   162,
       0,     0,     0,     0,     0,   157,   158,   154,   155,   156,
     159,   160,   165,   163,     0,   166,   172,     0,     0,   167,
     170,   171,   173,     0,   164,     0,     0,     0,   174,     0,
       0,     0,     0,     0,   128,    85,     0,     0,     0,     0,
     147,     0,     0,     0,    69,    70,    71,     0,     0,     0,
     127,     0,    25,     0,     3,     0,     0,     0,     0,     0,
      91,     0,     0,    91,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   169,     0,    29,    30,    31,    32,    33,    34,    27,
       0,    35,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,     0,     0,     0,     0,     0,     0,     0,
      88,    81,    86,    90,     0,     0,     0,     0,     0,     0,
     148,   149,   129,     0,   130,   116,   152,   153,     0,   175,
      26,     4,    78,    11,     0,   105,    12,     0,   111,   112,
      16,    17,   114,   115,    14,    15,    13,    10,     8,     5,
       6,     7,     9,    18,    20,    19,    23,    24,    21,    22,
       0,   117,     0,    50,     0,    40,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      78,     0,     0,     0,    75,     0,     0,     0,   103,     0,
     113,     0,   150,     0,    75,    64,    79,     0,    78,     0,
      92,   168,    51,    52,    41,    48,    49,    45,    46,    47,
     121,    42,    43,    44,    37,    36,    38,    39,     0,     0,
       0,     0,     0,    76,    89,    87,    73,    91,     0,     0,
     107,   110,     0,     0,    76,   132,   131,    65,     0,    68,
       0,     0,     0,     0,     0,   119,   123,     0,    28,     0,
      84,     0,    82,     0,     0,     0,    93,     0,     0,     0,
       0,   134,     0,     0,     0,     0,     0,    80,   104,   109,
     122,     0,   120,     0,   125,    83,    77,    74,     0,    95,
       0,   106,   108,   136,   142,     0,     0,    72,    67,    66,
       0,   124,    94,     0,   100,     0,     0,   138,   143,   144,
     135,     0,   118,     0,     0,   102,     0,     0,   139,   140,
       0,     0,     0,     0,   137,     0,   133,   145,     0,    96,
      97,   126,   141,   151,     0,    98,    99,   101,   146
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    62,    63,   206,   116,   248,    64,    65,    66,
     245,   237,   234,   207,   122,   123,   124,   148,   289,   304,
     337,   315,    67,    68,    69,   240,   241,   149,    70,    71,
      72,    73,    74,    75,    76,    77,   255,   256,   257,    78,
      79,    80,    81,    82,    83,    84,    85,   271,   272,   307,
     319,   326,   309,    86,   328,   131,   203,    87,    88,    89,
      20,     9,    10,    25,    26,    30,    31,    32,    33,     3
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -177
static const yytype_int16 yypact[] =
{
      28,    38,    54,   -46,   -29,  -177,  -177,    56,    50,  -177,
     -75,     8,     8,    46,    56,  -177,  -177,  -177,  -177,  -177,
    -177,  -177,    63,  -177,     8,  -177,     2,   -26,   -51,  -177,
    -177,  -177,  -177,   -13,  -177,    71,    72,   587,  -177,    57,
     -21,    26,   272,   272,  -177,    13,    91,    55,    96,    67,
     -22,    99,   100,   103,  -177,  -177,  -177,    75,    29,    35,
    -177,   116,  -177,   396,  -177,    22,    23,    27,    -9,    30,
      87,    31,    32,    87,    47,    49,    52,    58,    59,    60,
      61,    62,    65,    66,    74,    77,    78,    86,    89,   102,
      75,  -177,   272,  -177,  -177,  -177,  -177,  -177,  -177,    39,
     272,    51,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,
    -177,  -177,  -177,   272,   272,   361,    25,   489,    45,    90,
    -177,   651,  -177,   -39,    93,   142,   124,   108,   152,   170,
    -177,   131,  -177,   143,  -177,  -177,  -177,  -177,    98,  -177,
    -177,  -177,   272,  -177,   110,  -177,  -177,   256,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,
     112,   651,   137,   101,   147,   204,    88,   272,   272,   272,
     272,   272,   587,   272,   272,   272,   272,   272,   272,   272,
     272,   587,   272,   -30,   211,   168,   212,   272,  -177,   213,
    -177,   118,  -177,   167,   217,   122,   651,   -63,   272,   175,
     651,  -177,  -177,  -177,  -177,   101,   101,    21,    21,   651,
     332,    21,    21,    21,    -6,    -6,   204,   204,   -60,   460,
     198,   222,   126,  -177,   125,  -177,  -177,   -33,   584,   140,
    -177,   128,   228,   229,   139,  -177,   125,  -177,   -53,  -177,
     272,   -49,   240,   587,   272,  -177,   224,   226,  -177,   225,
    -177,   150,  -177,   258,   272,   260,   230,   272,   272,   213,
       8,  -177,   -45,   208,   166,   164,   176,   651,  -177,  -177,
     587,   631,  -177,   254,  -177,  -177,  -177,  -177,   234,   194,
     638,   651,  -177,   182,   227,   228,   280,  -177,  -177,  -177,
     587,  -177,  -177,   273,   247,   587,   289,   214,  -177,  -177,
    -177,   195,   587,   209,   261,  -177,   524,   199,  -177,   295,
     292,   215,   299,   279,  -177,   304,  -177,  -177,   -44,  -177,
      -8,  -177,  -177,  -177,   305,  -177,  -177,  -177,  -177
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -177,  -177,   -62,  -176,   -40,  -177,  -177,  -177,  -177,  -177,
    -177,  -177,   109,  -166,   120,  -177,  -177,   -69,  -177,  -177,
    -177,  -177,   -34,  -177,  -177,    48,  -177,   243,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,  -177,    64,  -177,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,  -177,  -177,    24,  -177,  -177,
    -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,  -177,
     -12,   307,  -177,   297,  -177,  -177,  -177,   285,  -177,  -177
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint16 yytable[] =
{
      21,   140,   115,   117,   152,   121,   220,   264,   231,   181,
     194,    24,    27,    37,    35,   229,    93,    94,    95,    96,
      97,    98,    99,   135,   228,   100,    45,    15,    16,    17,
      18,    13,    19,    14,   145,   129,   181,   130,   335,   336,
      36,   144,   251,   249,     1,   250,   258,     4,   250,   118,
     119,    28,   171,   275,     5,   276,   170,   278,     6,   250,
     173,   294,   333,   295,   334,     8,    28,    11,    12,   195,
     232,    22,    24,   175,   176,   265,     7,   280,    34,   101,
      39,    40,    90,    91,   102,   103,   104,   105,   106,    92,
     107,   108,   109,   110,   188,   189,   111,   112,   177,   178,
     125,   179,   180,   181,   126,   127,   128,   210,   132,   133,
      45,   113,   134,   120,   179,   180,   181,   136,   114,   186,
     187,   188,   189,   137,   312,   138,   141,   147,   142,   316,
     190,   143,   196,   198,   146,   150,   151,   215,   216,   217,
     218,   219,   172,   221,   222,   223,   224,   225,   226,   227,
     192,   154,   230,   155,   174,   121,   156,   238,   140,   197,
     199,   200,   157,   158,   159,   160,   161,   140,   266,   162,
     163,    93,    94,    95,    96,    97,    98,    99,   164,   201,
     100,   165,   166,   183,   184,   185,   186,   187,   188,   189,
     167,   202,   204,   168,   214,   193,   183,   184,   185,   186,
     187,   188,   189,   205,   118,   119,   169,   212,   177,   178,
     277,   179,   180,   181,   281,   208,   211,   213,   140,   181,
     233,   236,   239,   242,   210,   243,   244,   290,   291,   247,
     252,   261,   262,   263,   101,   268,   269,   270,   273,   102,
     103,   104,   105,   106,   274,   107,   108,   109,   110,   279,
     140,   111,   112,   283,   140,   254,   285,   284,   293,    93,
      94,    95,    96,    97,    98,    99,   113,   286,   100,   287,
     296,   288,   297,   114,   298,    93,    94,    95,    96,    97,
      98,    99,   301,   299,   100,   302,   303,   306,   308,   311,
     313,   314,   317,   183,   184,   185,   186,   187,   188,   189,
     320,   327,   321,   318,   260,   324,   322,   325,   330,   329,
     209,   331,   332,   246,   338,   235,   153,   292,    38,   310,
     282,    23,   101,    29,     0,     0,     0,   102,   103,   104,
     105,   106,     0,   107,   108,   109,   110,     0,   101,   111,
     112,    41,     0,   102,   103,   104,   105,   106,     0,   107,
     108,   109,   110,     0,   113,   111,   112,     0,     0,     0,
      42,   114,   253,   254,     0,    43,    44,    45,     0,     0,
     113,   177,   178,    46,   179,   180,   181,   114,     0,     0,
      47,     0,     0,    48,     0,    49,     0,     0,    50,     0,
     182,     0,     0,     0,     0,     0,     0,     0,     0,    51,
      52,    53,     0,     0,     0,    41,     0,     0,    54,     0,
       0,     0,     0,    55,    56,     0,     0,    57,    58,    59,
       0,     0,    60,   139,    42,     0,     0,     0,     0,    43,
      44,    45,     0,     0,     0,     0,     0,    46,     0,     0,
       0,    61,     0,     0,    47,     0,     0,    48,     0,    49,
       0,     0,    50,     0,     0,     0,   183,   184,   185,   186,
     187,   188,   189,    51,    52,    53,     0,     0,     0,    41,
       0,     0,    54,     0,     0,     0,     0,    55,    56,     0,
       0,    57,    58,    59,     0,     0,    60,   259,    42,     0,
       0,     0,     0,    43,    44,    45,     0,     0,     0,   177,
     178,    46,   179,   180,   181,    61,     0,     0,    47,     0,
       0,    48,     0,    49,     0,     0,    50,     0,     0,     0,
       0,   191,     0,     0,     0,     0,     0,    51,    52,    53,
       0,     0,     0,    41,     0,     0,    54,     0,     0,     0,
       0,    55,    56,     0,     0,    57,    58,    59,     0,     0,
      60,   323,    42,     0,     0,     0,     0,    43,    44,    45,
       0,     0,     0,     0,     0,    46,     0,     0,     0,    61,
       0,     0,    47,     0,     0,    48,     0,    49,     0,     0,
      50,     0,     0,     0,   183,   184,   185,   186,   187,   188,
     189,    51,    52,    53,   177,   178,    41,   179,   180,   181,
      54,     0,     0,     0,     0,    55,    56,     0,     0,    57,
      58,    59,     0,     0,    60,    42,     0,     0,     0,     0,
      43,    44,    45,     0,     0,     0,   267,     0,    46,     0,
       0,     0,     0,    61,     0,    47,     0,     0,    48,     0,
      49,   177,   178,    50,   179,   180,   181,     0,   177,   178,
       0,   179,   180,   181,    51,    52,    53,     0,     0,     0,
     300,   177,   178,    54,   179,   180,   181,     0,    55,    56,
     305,     0,    57,    58,    59,     0,     0,    60,     0,   183,
     184,   185,   186,   187,   188,   189,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    61,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   183,   184,   185,   186,
     187,   188,   189,   183,   184,   185,   186,   187,   188,   189,
       0,     0,     0,     0,     0,     0,   183,   184,   185,   186,
     187,   188,   189
};

static const yytype_int16 yycheck[] =
{
      12,    63,    42,    43,    73,    45,   182,    40,    38,    15,
      49,     9,    24,    26,    65,   191,     3,     4,     5,     6,
       7,     8,     9,    57,   190,    12,    35,    19,    20,    21,
      22,   106,    24,   108,    68,    57,    15,    59,    46,    47,
      91,    50,   208,   106,    16,   108,   106,     9,   108,    36,
      37,    64,    92,   106,     0,   108,    90,   106,   104,   108,
     100,   106,   106,   108,   108,     9,    64,    17,    18,   108,
     100,    25,     9,   113,   114,   108,   105,   253,   104,    66,
       9,     9,    25,   104,    71,    72,    73,    74,    75,    63,
      77,    78,    79,    80,   100,   101,    83,    84,    10,    11,
       9,    13,    14,    15,    49,     9,    39,   147,     9,     9,
      35,    98,     9,   100,    13,    14,    15,    88,   105,    98,
      99,   100,   101,    88,   300,     9,   104,    40,   105,   305,
     105,   104,    39,     9,   104,   104,   104,   177,   178,   179,
     180,   181,   103,   183,   184,   185,   186,   187,   188,   189,
     105,   104,   192,   104,   103,   195,   104,   197,   220,    17,
      52,     9,   104,   104,   104,   104,   104,   229,   237,   104,
     104,     3,     4,     5,     6,     7,     8,     9,   104,     9,
      12,   104,   104,    95,    96,    97,    98,    99,   100,   101,
     104,    60,    49,   104,   106,   105,    95,    96,    97,    98,
      99,   100,   101,   105,    36,    37,   104,    70,    10,    11,
     250,    13,    14,    15,   254,   105,   104,    70,   280,    15,
       9,     9,     9,   105,   264,    58,     9,   267,   268,   107,
      55,     9,   106,   108,    66,    95,   108,     9,     9,    71,
      72,    73,    74,    75,   105,    77,    78,    79,    80,     9,
     312,    83,    84,    27,   316,    31,   106,    32,   270,     3,
       4,     5,     6,     7,     8,     9,    98,     9,    12,     9,
      62,    41,   106,   105,   110,     3,     4,     5,     6,     7,
       8,     9,    28,   107,    12,    51,    92,   105,    61,     9,
      17,    44,     3,    95,    96,    97,    98,    99,   100,   101,
     105,     9,    93,    89,   106,   106,    45,    12,     9,    94,
      54,    32,     8,   204,     9,   195,    73,   269,    33,   295,
     256,    14,    66,    26,    -1,    -1,    -1,    71,    72,    73,
      74,    75,    -1,    77,    78,    79,    80,    -1,    66,    83,
      84,     9,    -1,    71,    72,    73,    74,    75,    -1,    77,
      78,    79,    80,    -1,    98,    83,    84,    -1,    -1,    -1,
      28,   105,    30,    31,    -1,    33,    34,    35,    -1,    -1,
      98,    10,    11,    41,    13,    14,    15,   105,    -1,    -1,
      48,    -1,    -1,    51,    -1,    53,    -1,    -1,    56,    -1,
      29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    -1,    -1,    -1,     9,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    81,    82,    -1,    -1,    85,    86,    87,
      -1,    -1,    90,    27,    28,    -1,    -1,    -1,    -1,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    41,    -1,    -1,
      -1,   109,    -1,    -1,    48,    -1,    -1,    51,    -1,    53,
      -1,    -1,    56,    -1,    -1,    -1,    95,    96,    97,    98,
      99,   100,   101,    67,    68,    69,    -1,    -1,    -1,     9,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    81,    82,    -1,
      -1,    85,    86,    87,    -1,    -1,    90,    27,    28,    -1,
      -1,    -1,    -1,    33,    34,    35,    -1,    -1,    -1,    10,
      11,    41,    13,    14,    15,   109,    -1,    -1,    48,    -1,
      -1,    51,    -1,    53,    -1,    -1,    56,    -1,    -1,    -1,
      -1,    32,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      -1,    -1,    -1,     9,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    81,    82,    -1,    -1,    85,    86,    87,    -1,    -1,
      90,    27,    28,    -1,    -1,    -1,    -1,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    41,    -1,    -1,    -1,   109,
      -1,    -1,    48,    -1,    -1,    51,    -1,    53,    -1,    -1,
      56,    -1,    -1,    -1,    95,    96,    97,    98,    99,   100,
     101,    67,    68,    69,    10,    11,     9,    13,    14,    15,
      76,    -1,    -1,    -1,    -1,    81,    82,    -1,    -1,    85,
      86,    87,    -1,    -1,    90,    28,    -1,    -1,    -1,    -1,
      33,    34,    35,    -1,    -1,    -1,    42,    -1,    41,    -1,
      -1,    -1,    -1,   109,    -1,    48,    -1,    -1,    51,    -1,
      53,    10,    11,    56,    13,    14,    15,    -1,    10,    11,
      -1,    13,    14,    15,    67,    68,    69,    -1,    -1,    -1,
      29,    10,    11,    76,    13,    14,    15,    -1,    81,    82,
      32,    -1,    85,    86,    87,    -1,    -1,    90,    -1,    95,
      96,    97,    98,    99,   100,   101,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   109,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    97,    98,
      99,   100,   101,    95,    96,    97,    98,    99,   100,   101,
      -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    97,    98,
      99,   100,   101
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    16,   112,   180,     9,     0,   104,   105,     9,   172,
     173,    17,    18,   106,   108,    19,    20,    21,    22,    24,
     171,   171,    25,   172,     9,   174,   175,   171,    64,   174,
     176,   177,   178,   179,   104,    65,    91,    26,   178,     9,
       9,     9,    28,    33,    34,    35,    41,    48,    51,    53,
      56,    67,    68,    69,    76,    81,    82,    85,    86,    87,
      90,   109,   113,   114,   118,   119,   120,   133,   134,   135,
     139,   140,   141,   142,   143,   144,   145,   146,   150,   151,
     152,   153,   154,   155,   156,   157,   164,   168,   169,   170,
      25,   104,    63,     3,     4,     5,     6,     7,     8,     9,
      12,    66,    71,    72,    73,    74,    75,    77,    78,    79,
      80,    83,    84,    98,   105,   115,   116,   115,    36,    37,
     100,   115,   125,   126,   127,     9,    49,     9,    39,    57,
      59,   166,     9,     9,     9,   133,    88,    88,     9,    27,
     113,   104,   105,   104,    50,   133,   104,    40,   128,   138,
     104,   104,   128,   138,   104,   104,   104,   104,   104,   104,
     104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
     133,   115,   103,   115,   103,   115,   115,    10,    11,    13,
      14,    15,    29,    95,    96,    97,    98,    99,   100,   101,
     105,    32,   105,   105,    49,   108,    39,    17,     9,    52,
       9,     9,    60,   167,    49,   105,   115,   124,   105,    54,
     115,   104,    70,    70,   106,   115,   115,   115,   115,   115,
     114,   115,   115,   115,   115,   115,   115,   115,   124,   114,
     115,    38,   100,     9,   123,   125,     9,   122,   115,     9,
     136,   137,   105,    58,     9,   121,   123,   107,   117,   106,
     108,   124,    55,    30,    31,   147,   148,   149,   106,    27,
     106,     9,   106,   108,    40,   108,   128,    42,    95,   108,
       9,   158,   159,     9,   105,   106,   108,   115,   106,     9,
     114,   115,   147,    27,    32,   106,     9,     9,    41,   129,
     115,   115,   136,   171,   106,   108,    62,   106,   110,   107,
      29,    28,    51,    92,   130,    32,   105,   160,    61,   163,
     158,     9,   114,    17,    44,   132,   114,     3,    89,   161,
     105,    93,    45,    27,   106,    12,   162,     9,   165,    94,
       9,    32,     8,   106,   108,    46,    47,   131,     9
};

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
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
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
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

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
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
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

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
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
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
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

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
#line 181 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 26:
#line 183 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (2)]), (yyvsp[(2) - (2)])); ;}
    break;

  case 27:
#line 187 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 28:
#line 189 "pars0grm.y"
    { (yyval) = pars_func((yyvsp[(1) - (4)]), (yyvsp[(3) - (4)])); ;}
    break;

  case 29:
#line 190 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 30:
#line 191 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 31:
#line 192 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 32:
#line 193 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 33:
#line 194 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 34:
#line 195 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 35:
#line 196 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]);;}
    break;

  case 36:
#line 197 "pars0grm.y"
    { (yyval) = pars_op('+', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 37:
#line 198 "pars0grm.y"
    { (yyval) = pars_op('-', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 38:
#line 199 "pars0grm.y"
    { (yyval) = pars_op('*', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 39:
#line 200 "pars0grm.y"
    { (yyval) = pars_op('/', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 40:
#line 201 "pars0grm.y"
    { (yyval) = pars_op('-', (yyvsp[(2) - (2)]), NULL); ;}
    break;

  case 41:
#line 202 "pars0grm.y"
    { (yyval) = (yyvsp[(2) - (3)]); ;}
    break;

  case 42:
#line 203 "pars0grm.y"
    { (yyval) = pars_op('=', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 43:
#line 204 "pars0grm.y"
    { (yyval) = pars_op('<', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 44:
#line 205 "pars0grm.y"
    { (yyval) = pars_op('>', (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 45:
#line 206 "pars0grm.y"
    { (yyval) = pars_op(PARS_GE_TOKEN, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 46:
#line 207 "pars0grm.y"
    { (yyval) = pars_op(PARS_LE_TOKEN, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 47:
#line 208 "pars0grm.y"
    { (yyval) = pars_op(PARS_NE_TOKEN, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 48:
#line 209 "pars0grm.y"
    { (yyval) = pars_op(PARS_AND_TOKEN, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 49:
#line 210 "pars0grm.y"
    { (yyval) = pars_op(PARS_OR_TOKEN, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 50:
#line 211 "pars0grm.y"
    { (yyval) = pars_op(PARS_NOT_TOKEN, (yyvsp[(2) - (2)]), NULL); ;}
    break;

  case 51:
#line 213 "pars0grm.y"
    { (yyval) = pars_op(PARS_NOTFOUND_TOKEN, (yyvsp[(1) - (3)]), NULL); ;}
    break;

  case 52:
#line 215 "pars0grm.y"
    { (yyval) = pars_op(PARS_NOTFOUND_TOKEN, (yyvsp[(1) - (3)]), NULL); ;}
    break;

  case 53:
#line 219 "pars0grm.y"
    { (yyval) = &pars_to_char_token; ;}
    break;

  case 54:
#line 220 "pars0grm.y"
    { (yyval) = &pars_to_number_token; ;}
    break;

  case 55:
#line 221 "pars0grm.y"
    { (yyval) = &pars_to_binary_token; ;}
    break;

  case 56:
#line 223 "pars0grm.y"
    { (yyval) = &pars_binary_to_number_token; ;}
    break;

  case 57:
#line 224 "pars0grm.y"
    { (yyval) = &pars_substr_token; ;}
    break;

  case 58:
#line 225 "pars0grm.y"
    { (yyval) = &pars_concat_token; ;}
    break;

  case 59:
#line 226 "pars0grm.y"
    { (yyval) = &pars_instr_token; ;}
    break;

  case 60:
#line 227 "pars0grm.y"
    { (yyval) = &pars_length_token; ;}
    break;

  case 61:
#line 228 "pars0grm.y"
    { (yyval) = &pars_sysdate_token; ;}
    break;

  case 62:
#line 229 "pars0grm.y"
    { (yyval) = &pars_rnd_token; ;}
    break;

  case 63:
#line 230 "pars0grm.y"
    { (yyval) = &pars_rnd_str_token; ;}
    break;

  case 67:
#line 241 "pars0grm.y"
    {
				  (yyval) = pars_stored_procedure_call(
					static_cast<sym_node_t*>((yyvsp[(2) - (6)])));
				;}
    break;

  case 68:
#line 249 "pars0grm.y"
    { (yyval) = pars_procedure_call((yyvsp[(1) - (4)]), (yyvsp[(3) - (4)])); ;}
    break;

  case 69:
#line 253 "pars0grm.y"
    { (yyval) = &pars_replstr_token; ;}
    break;

  case 70:
#line 254 "pars0grm.y"
    { (yyval) = &pars_printf_token; ;}
    break;

  case 71:
#line 255 "pars0grm.y"
    { (yyval) = &pars_assert_token; ;}
    break;

  case 72:
#line 259 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (3)]); ;}
    break;

  case 73:
#line 263 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 74:
#line 265 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 75:
#line 269 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 76:
#line 270 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 77:
#line 272 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 78:
#line 276 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 79:
#line 277 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)]));;}
    break;

  case 80:
#line 278 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 81:
#line 282 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]); ;}
    break;

  case 82:
#line 284 "pars0grm.y"
    { (yyval) = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); ;}
    break;

  case 83:
#line 289 "pars0grm.y"
    { (yyval) = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, (yyvsp[(4) - (5)]))))); ;}
    break;

  case 84:
#line 295 "pars0grm.y"
    { (yyval) = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									(yyvsp[(3) - (4)]))); ;}
    break;

  case 85:
#line 301 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 86:
#line 302 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 87:
#line 304 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 88:
#line 308 "pars0grm.y"
    { (yyval) = pars_select_list(
					&pars_star_denoter, NULL); ;}
    break;

  case 89:
#line 311 "pars0grm.y"
    { (yyval) = pars_select_list(
					(yyvsp[(1) - (3)]), static_cast<sym_node_t*>((yyvsp[(3) - (3)]))); ;}
    break;

  case 90:
#line 313 "pars0grm.y"
    { (yyval) = pars_select_list((yyvsp[(1) - (1)]), NULL); ;}
    break;

  case 91:
#line 317 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 92:
#line 318 "pars0grm.y"
    { (yyval) = (yyvsp[(2) - (2)]); ;}
    break;

  case 93:
#line 322 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 94:
#line 324 "pars0grm.y"
    { (yyval) = &pars_update_token; ;}
    break;

  case 95:
#line 328 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 96:
#line 330 "pars0grm.y"
    { (yyval) = &pars_share_token; ;}
    break;

  case 97:
#line 334 "pars0grm.y"
    { (yyval) = &pars_asc_token; ;}
    break;

  case 98:
#line 335 "pars0grm.y"
    { (yyval) = &pars_asc_token; ;}
    break;

  case 99:
#line 336 "pars0grm.y"
    { (yyval) = &pars_desc_token; ;}
    break;

  case 100:
#line 340 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 101:
#line 342 "pars0grm.y"
    { (yyval) = pars_order_by(
					static_cast<sym_node_t*>((yyvsp[(3) - (4)])),
					static_cast<pars_res_word_t*>((yyvsp[(4) - (4)]))); ;}
    break;

  case 102:
#line 353 "pars0grm.y"
    { (yyval) = pars_select_statement(
					static_cast<sel_node_t*>((yyvsp[(2) - (8)])),
					static_cast<sym_node_t*>((yyvsp[(4) - (8)])),
					static_cast<que_node_t*>((yyvsp[(5) - (8)])),
				       	static_cast<pars_res_word_t*>((yyvsp[(6) - (8)])),
				       	static_cast<pars_res_word_t*>((yyvsp[(7) - (8)])),
					static_cast<order_node_t*>((yyvsp[(8) - (8)]))); ;}
    break;

  case 103:
#line 364 "pars0grm.y"
    { (yyval) = (yyvsp[(3) - (3)]); ;}
    break;

  case 104:
#line 369 "pars0grm.y"
    { (yyval) = pars_insert_statement(
					static_cast<sym_node_t*>((yyvsp[(1) - (5)])),
					(yyvsp[(4) - (5)]), NULL); ;}
    break;

  case 105:
#line 373 "pars0grm.y"
    { (yyval) = pars_insert_statement(
					static_cast<sym_node_t*>((yyvsp[(1) - (2)])),
					NULL,
					static_cast<sel_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 106:
#line 380 "pars0grm.y"
    { (yyval) = pars_column_assignment(
					static_cast<sym_node_t*>((yyvsp[(1) - (3)])),
					static_cast<que_node_t*>((yyvsp[(3) - (3)]))); ;}
    break;

  case 107:
#line 386 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 108:
#line 388 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 109:
#line 394 "pars0grm.y"
    { (yyval) = (yyvsp[(4) - (4)]); ;}
    break;

  case 110:
#line 400 "pars0grm.y"
    { (yyval) = pars_update_statement_start(
					FALSE,
					static_cast<sym_node_t*>((yyvsp[(2) - (4)])),
					static_cast<col_assign_node_t*>((yyvsp[(4) - (4)]))); ;}
    break;

  case 111:
#line 408 "pars0grm.y"
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[(1) - (2)])),
					NULL,
					static_cast<que_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 112:
#line 416 "pars0grm.y"
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[(1) - (2)])),
					static_cast<sym_node_t*>((yyvsp[(2) - (2)])),
					NULL); ;}
    break;

  case 113:
#line 424 "pars0grm.y"
    { (yyval) = pars_update_statement_start(
					TRUE,
					static_cast<sym_node_t*>((yyvsp[(3) - (3)])),
					NULL); ;}
    break;

  case 114:
#line 432 "pars0grm.y"
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[(1) - (2)])),
				       	NULL,
					static_cast<que_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 115:
#line 440 "pars0grm.y"
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[(1) - (2)])),
					static_cast<sym_node_t*>((yyvsp[(2) - (2)])),
					NULL); ;}
    break;

  case 116:
#line 448 "pars0grm.y"
    { (yyval) = pars_row_printf_statement(
					static_cast<sel_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 117:
#line 454 "pars0grm.y"
    { (yyval) = pars_assignment_statement(
					static_cast<sym_node_t*>((yyvsp[(1) - (3)])),
					static_cast<que_node_t*>((yyvsp[(3) - (3)]))); ;}
    break;

  case 118:
#line 462 "pars0grm.y"
    { (yyval) = pars_elsif_element((yyvsp[(2) - (4)]), (yyvsp[(4) - (4)])); ;}
    break;

  case 119:
#line 466 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 120:
#line 468 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (2)]), (yyvsp[(2) - (2)])); ;}
    break;

  case 121:
#line 472 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 122:
#line 474 "pars0grm.y"
    { (yyval) = (yyvsp[(2) - (2)]); ;}
    break;

  case 123:
#line 475 "pars0grm.y"
    { (yyval) = (yyvsp[(1) - (1)]); ;}
    break;

  case 124:
#line 482 "pars0grm.y"
    { (yyval) = pars_if_statement((yyvsp[(2) - (7)]), (yyvsp[(4) - (7)]), (yyvsp[(5) - (7)])); ;}
    break;

  case 125:
#line 488 "pars0grm.y"
    { (yyval) = pars_while_statement((yyvsp[(2) - (6)]), (yyvsp[(4) - (6)])); ;}
    break;

  case 126:
#line 496 "pars0grm.y"
    { (yyval) = pars_for_statement(
					static_cast<sym_node_t*>((yyvsp[(2) - (10)])),
					(yyvsp[(4) - (10)]), (yyvsp[(6) - (10)]), (yyvsp[(8) - (10)])); ;}
    break;

  case 127:
#line 502 "pars0grm.y"
    { (yyval) = pars_exit_statement(); ;}
    break;

  case 128:
#line 506 "pars0grm.y"
    { (yyval) = pars_return_statement(); ;}
    break;

  case 129:
#line 511 "pars0grm.y"
    { (yyval) = pars_open_statement(
					ROW_SEL_OPEN_CURSOR,
					static_cast<sym_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 130:
#line 518 "pars0grm.y"
    { (yyval) = pars_open_statement(
					ROW_SEL_CLOSE_CURSOR,
					static_cast<sym_node_t*>((yyvsp[(2) - (2)]))); ;}
    break;

  case 131:
#line 525 "pars0grm.y"
    { (yyval) = pars_fetch_statement(
					static_cast<sym_node_t*>((yyvsp[(2) - (4)])),
					static_cast<sym_node_t*>((yyvsp[(4) - (4)])), NULL); ;}
    break;

  case 132:
#line 529 "pars0grm.y"
    { (yyval) = pars_fetch_statement(
					static_cast<sym_node_t*>((yyvsp[(2) - (4)])),
					NULL,
					static_cast<sym_node_t*>((yyvsp[(4) - (4)]))); ;}
    break;

  case 133:
#line 537 "pars0grm.y"
    { (yyval) = pars_column_def(
					static_cast<sym_node_t*>((yyvsp[(1) - (5)])),
					static_cast<pars_res_word_t*>((yyvsp[(2) - (5)])),
					static_cast<sym_node_t*>((yyvsp[(3) - (5)])),
					(yyvsp[(4) - (5)]),
					(yyvsp[(5) - (5)])); ;}
    break;

  case 134:
#line 546 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 135:
#line 548 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 136:
#line 552 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 137:
#line 554 "pars0grm.y"
    { (yyval) = (yyvsp[(2) - (3)]); ;}
    break;

  case 138:
#line 558 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 139:
#line 560 "pars0grm.y"
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 140:
#line 565 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 141:
#line 567 "pars0grm.y"
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 142:
#line 572 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 143:
#line 574 "pars0grm.y"
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ ;}
    break;

  case 144:
#line 581 "pars0grm.y"
    { (yyval) = pars_create_table(
					static_cast<sym_node_t*>((yyvsp[(3) - (7)])),
					static_cast<sym_node_t*>((yyvsp[(5) - (7)])),
					(yyvsp[(7) - (7)])); ;}
    break;

  case 145:
#line 588 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 146:
#line 590 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 147:
#line 594 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 148:
#line 595 "pars0grm.y"
    { (yyval) = &pars_unique_token; ;}
    break;

  case 149:
#line 599 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 150:
#line 600 "pars0grm.y"
    { (yyval) = &pars_clustered_token; ;}
    break;

  case 151:
#line 608 "pars0grm.y"
    { (yyval) = pars_create_index(
					static_cast<pars_res_word_t*>((yyvsp[(2) - (10)])),
					static_cast<pars_res_word_t*>((yyvsp[(3) - (10)])),
					static_cast<sym_node_t*>((yyvsp[(5) - (10)])),
					static_cast<sym_node_t*>((yyvsp[(7) - (10)])),
					static_cast<sym_node_t*>((yyvsp[(9) - (10)]))); ;}
    break;

  case 152:
#line 618 "pars0grm.y"
    { (yyval) = pars_commit_statement(); ;}
    break;

  case 153:
#line 623 "pars0grm.y"
    { (yyval) = pars_rollback_statement(); ;}
    break;

  case 154:
#line 627 "pars0grm.y"
    { (yyval) = &pars_int_token; ;}
    break;

  case 155:
#line 628 "pars0grm.y"
    { (yyval) = &pars_int_token; ;}
    break;

  case 156:
#line 629 "pars0grm.y"
    { (yyval) = &pars_char_token; ;}
    break;

  case 157:
#line 630 "pars0grm.y"
    { (yyval) = &pars_binary_token; ;}
    break;

  case 158:
#line 631 "pars0grm.y"
    { (yyval) = &pars_blob_token; ;}
    break;

  case 159:
#line 636 "pars0grm.y"
    { (yyval) = pars_parameter_declaration(
					static_cast<sym_node_t*>((yyvsp[(1) - (3)])),
					PARS_INPUT,
					static_cast<pars_res_word_t*>((yyvsp[(3) - (3)]))); ;}
    break;

  case 160:
#line 641 "pars0grm.y"
    { (yyval) = pars_parameter_declaration(
					static_cast<sym_node_t*>((yyvsp[(1) - (3)])),
					PARS_OUTPUT,
					static_cast<pars_res_word_t*>((yyvsp[(3) - (3)]))); ;}
    break;

  case 161:
#line 648 "pars0grm.y"
    { (yyval) = NULL; ;}
    break;

  case 162:
#line 649 "pars0grm.y"
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[(1) - (1)])); ;}
    break;

  case 163:
#line 651 "pars0grm.y"
    { (yyval) = que_node_list_add_last((yyvsp[(1) - (3)]), (yyvsp[(3) - (3)])); ;}
    break;

  case 164:
#line 656 "pars0grm.y"
    { (yyval) = pars_variable_declaration(
					static_cast<sym_node_t*>((yyvsp[(1) - (3)])),
					static_cast<pars_res_word_t*>((yyvsp[(2) - (3)]))); ;}
    break;

  case 168:
#line 670 "pars0grm.y"
    { (yyval) = pars_cursor_declaration(
					static_cast<sym_node_t*>((yyvsp[(3) - (6)])),
					static_cast<sel_node_t*>((yyvsp[(5) - (6)]))); ;}
    break;

  case 169:
#line 677 "pars0grm.y"
    { (yyval) = pars_function_declaration(
					static_cast<sym_node_t*>((yyvsp[(3) - (4)]))); ;}
    break;

  case 175:
#line 699 "pars0grm.y"
    { (yyval) = pars_procedure_definition(
					static_cast<sym_node_t*>((yyvsp[(2) - (11)])),
					static_cast<sym_node_t*>((yyvsp[(4) - (11)])),
					(yyvsp[(10) - (11)])); ;}
    break;


/* Line 1267 of yacc.c.  */
#line 2744 "pars0grm.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
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
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
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


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 705 "pars0grm.y"


