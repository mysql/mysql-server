/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 28 "pars0grm.y" /* yacc.c:339  */

/* The value of the semantic attribute is a pointer to a query tree node
que_node_t */

#include "univ.i"
#include <math.h>

#include "pars0pars.h"
#include "mem0mem.h"
#include "que0types.h"
#include "que0que.h"
#include "row0sel.h"

#define YYSTYPE que_node_t*

/* #define __STDC__ */
int
yylex(void);

#line 85 "pars0grm.cc" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "pars0grm.tab.h".  */
#ifndef YY_YY_PARS0GRM_TAB_H_INCLUDED
# define YY_YY_PARS0GRM_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
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
    PARS_TO_BINARY_TOKEN = 326,
    PARS_SUBSTR_TOKEN = 327,
    PARS_CONCAT_TOKEN = 328,
    PARS_INSTR_TOKEN = 329,
    PARS_LENGTH_TOKEN = 330,
    PARS_COMMIT_TOKEN = 331,
    PARS_ROLLBACK_TOKEN = 332,
    PARS_WORK_TOKEN = 333,
    PARS_UNSIGNED_TOKEN = 334,
    PARS_EXIT_TOKEN = 335,
    PARS_FUNCTION_TOKEN = 336,
    PARS_LOCK_TOKEN = 337,
    PARS_SHARE_TOKEN = 338,
    PARS_MODE_TOKEN = 339,
    PARS_LIKE_TOKEN = 340,
    PARS_LIKE_TOKEN_EXACT = 341,
    PARS_LIKE_TOKEN_PREFIX = 342,
    PARS_LIKE_TOKEN_SUFFIX = 343,
    PARS_LIKE_TOKEN_SUBSTR = 344,
    PARS_TABLE_NAME_TOKEN = 345,
    PARS_COMPACT_TOKEN = 346,
    PARS_BLOCK_SIZE_TOKEN = 347,
    PARS_BIGINT_TOKEN = 348,
    NEG = 349
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_PARS0GRM_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 231 "pars0grm.cc" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
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
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) MY_ATTRIBUTE(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   626

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  107
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  68
/* YYNRULES -- Number of rules.  */
#define YYNRULES  165
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  321

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   349

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   102,     2,     2,
     104,   105,    99,    98,   106,    97,     2,   100,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   103,
      95,    94,    96,     2,     2,     2,     2,     2,     2,     2,
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
      85,    86,    87,    88,    89,    90,    91,    92,    93,   101
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   151,   151,   154,   155,   156,   157,   158,   159,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   176,   177,   182,   183,   185,   186,   187,   188,
     189,   190,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   201,   202,   203,   204,   205,   206,   207,   208,   209,
     211,   216,   217,   218,   220,   221,   225,   229,   230,   235,
     236,   237,   242,   243,   244,   248,   249,   254,   260,   267,
     268,   269,   274,   276,   279,   283,   284,   288,   289,   294,
     295,   300,   301,   302,   306,   307,   314,   329,   334,   337,
     345,   351,   352,   357,   363,   372,   380,   388,   395,   403,
     411,   418,   424,   425,   430,   431,   433,   437,   444,   450,
     460,   464,   468,   475,   482,   486,   494,   503,   504,   509,
     510,   515,   516,   522,   523,   529,   530,   536,   537,   542,
     543,   548,   559,   560,   565,   566,   570,   571,   575,   589,
     590,   594,   599,   604,   605,   606,   607,   608,   609,   613,
     618,   626,   627,   628,   633,   639,   641,   642,   646,   654,
     660,   661,   664,   666,   667,   671
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
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
  "PARS_TO_BINARY_TOKEN", "PARS_SUBSTR_TOKEN", "PARS_CONCAT_TOKEN",
  "PARS_INSTR_TOKEN", "PARS_LENGTH_TOKEN", "PARS_COMMIT_TOKEN",
  "PARS_ROLLBACK_TOKEN", "PARS_WORK_TOKEN", "PARS_UNSIGNED_TOKEN",
  "PARS_EXIT_TOKEN", "PARS_FUNCTION_TOKEN", "PARS_LOCK_TOKEN",
  "PARS_SHARE_TOKEN", "PARS_MODE_TOKEN", "PARS_LIKE_TOKEN",
  "PARS_LIKE_TOKEN_EXACT", "PARS_LIKE_TOKEN_PREFIX",
  "PARS_LIKE_TOKEN_SUFFIX", "PARS_LIKE_TOKEN_SUBSTR",
  "PARS_TABLE_NAME_TOKEN", "PARS_COMPACT_TOKEN", "PARS_BLOCK_SIZE_TOKEN",
  "PARS_BIGINT_TOKEN", "'='", "'<'", "'>'", "'-'", "'+'", "'*'", "'/'",
  "NEG", "'%'", "';'", "'('", "')'", "','", "$accept", "top_statement",
  "statement", "statement_list", "exp", "function_name",
  "user_function_call", "table_list", "variable_list", "exp_list",
  "select_item", "select_item_list", "select_list", "search_condition",
  "for_update_clause", "lock_shared_clause", "order_direction",
  "order_by_clause", "select_statement", "insert_statement_start",
  "insert_statement", "column_assignment", "column_assignment_list",
  "cursor_positioned", "update_statement_start",
  "update_statement_searched", "update_statement_positioned",
  "delete_statement_start", "delete_statement_searched",
  "delete_statement_positioned", "assignment_statement", "elsif_element",
  "elsif_list", "else_part", "if_statement", "while_statement",
  "for_statement", "exit_statement", "return_statement",
  "open_cursor_statement", "close_cursor_statement", "fetch_statement",
  "column_def", "column_def_list", "opt_column_len", "opt_unsigned",
  "opt_not_null", "not_fit_in_memory", "compact", "block_size",
  "create_table", "column_list", "unique_def", "clustered_def",
  "create_index", "table_name", "commit_statement", "rollback_statement",
  "type_name", "parameter_declaration", "parameter_declaration_list",
  "variable_declaration", "variable_declaration_list",
  "cursor_declaration", "function_declaration", "declaration",
  "declaration_list", "procedure_definition", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
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
     345,   346,   347,   348,    61,    60,    62,    45,    43,    42,
      47,   349,    37,    59,    40,    41,    44
};
# endif

#define YYPACT_NINF -141

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-141)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      21,    30,    44,   -46,   -54,  -141,  -141,    60,    15,  -141,
     -64,     7,     7,    45,    60,  -141,  -141,  -141,  -141,  -141,
    -141,  -141,  -141,    62,  -141,     7,  -141,     3,   -29,   -35,
    -141,  -141,  -141,  -141,    -4,  -141,    66,    67,   546,  -141,
      53,   -24,    18,   208,   208,  -141,    11,    79,    40,    -3,
      68,   -21,   100,   102,   103,    35,    36,  -141,  -141,   454,
      13,    -1,    14,    78,    22,    31,    78,    42,    43,    46,
      47,    48,    49,    50,    52,    54,    56,    57,    70,    71,
      72,    80,   107,  -141,   208,  -141,  -141,  -141,  -141,  -141,
    -141,    82,   208,    84,  -141,  -141,  -141,  -141,  -141,   208,
     208,   258,    65,   278,    85,    86,  -141,   351,  -141,   -38,
     108,   171,    -3,  -141,  -141,   139,    -3,    -3,  -141,   133,
    -141,   146,  -141,  -141,  -141,  -141,  -141,  -141,    92,  -141,
    -141,    90,  -141,  -141,  -141,  -141,  -141,  -141,  -141,  -141,
    -141,  -141,  -141,  -141,  -141,  -141,  -141,  -141,  -141,  -141,
    -141,  -141,  -141,    95,   351,   130,   311,   131,    -5,   157,
     208,   208,   208,   208,   208,   546,   197,   208,   208,   208,
     208,   208,   208,   208,   208,   546,   208,     2,   195,   173,
      -3,   208,  -141,   196,  -141,   104,  -141,   160,   210,   208,
     166,   351,  -141,  -141,  -141,  -141,   311,   311,     6,     6,
     351,   424,  -141,     6,     6,     6,    39,    39,    -5,    -5,
     351,   -53,   485,   222,   213,   118,  -141,   119,  -141,   -33,
    -141,   286,   132,  -141,   121,   215,   219,   125,  -141,   119,
     -50,   231,   546,   208,  -141,   200,   214,  -141,   208,   211,
    -141,   144,  -141,   241,   208,    -3,   217,   208,   208,   196,
       7,  -141,   -44,   198,   154,  -141,  -141,   546,   319,  -141,
     233,   351,  -141,  -141,  -141,  -141,   212,   182,   327,   351,
    -141,   161,   205,   215,    -3,  -141,   546,  -141,  -141,   250,
     232,   546,   272,   199,  -141,   193,  -141,   181,   546,   203,
     245,  -141,   515,   189,  -141,   283,  -141,   206,   293,   220,
     294,   274,  -141,   300,  -141,   221,  -141,  -141,   -42,  -141,
      19,  -141,  -141,   306,  -141,   302,  -141,  -141,  -141,  -141,
    -141
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,     1,     2,   151,     0,   152,
       0,     0,     0,     0,     0,   147,   148,   143,   144,   146,
     145,   149,   150,   155,   153,     0,   156,   162,     0,     0,
     157,   160,   161,   163,     0,   154,     0,     0,     0,   164,
       0,     0,     0,     0,     0,   111,    69,     0,     0,     0,
       0,   134,     0,     0,     0,     0,     0,   110,    22,     0,
       0,     0,     0,    75,     0,     0,    75,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   159,     0,    26,    27,    28,    29,    30,
      31,    24,     0,    32,    54,    51,    52,    53,    55,     0,
       0,     0,     0,     0,     0,     0,    72,    65,    70,    74,
       0,     0,     0,   139,   140,     0,     0,     0,   135,   136,
     112,     0,   113,   141,   142,   165,    23,     9,     0,    89,
      10,     0,    95,    96,    13,    14,    98,    99,    11,    12,
       8,     6,     3,     4,     5,     7,    15,    17,    16,    20,
      21,    18,    19,     0,   100,     0,    48,     0,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    62,     0,     0,     0,    59,     0,
       0,     0,    87,     0,    97,     0,   137,     0,    59,    62,
       0,    76,   158,    49,    50,    38,    46,    47,    43,    44,
      45,   104,    40,    39,    41,    42,    34,    33,    35,    36,
      63,     0,     0,     0,     0,     0,    60,    73,    71,    75,
      57,     0,     0,    91,    94,     0,     0,    60,   115,   114,
       0,     0,     0,     0,   102,   106,     0,    25,     0,     0,
      68,     0,    66,     0,     0,     0,    77,     0,     0,     0,
       0,   117,     0,     0,     0,    88,    93,   105,     0,   103,
       0,    64,   108,    67,    61,    58,     0,    79,     0,    90,
      92,   119,   125,     0,     0,    56,     0,   107,    78,     0,
      84,     0,     0,   121,   126,   127,   118,     0,   101,     0,
       0,    86,     0,     0,   122,   123,   128,   129,     0,     0,
       0,     0,   120,     0,   116,     0,   131,   132,     0,    80,
      81,   109,   124,     0,   138,     0,    82,    83,    85,   130,
     133
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -141,  -141,   -58,  -140,   -41,  -141,  -141,  -141,   126,   124,
     152,  -141,  -141,   -61,  -141,  -141,  -141,  -141,   -37,  -141,
    -141,    74,  -141,   269,  -141,  -141,  -141,  -141,  -141,  -141,
    -141,   101,  -141,  -141,  -141,  -141,  -141,  -141,  -141,  -141,
    -141,  -141,    73,  -141,  -141,  -141,  -141,  -141,  -141,  -141,
    -141,  -141,  -141,  -141,  -141,  -108,  -141,  -141,   -12,   325,
    -141,   317,  -141,  -141,  -141,   313,  -141,  -141
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,    58,    59,   107,   102,   228,   219,   217,   211,
     108,   109,   110,   132,   267,   280,   318,   291,    60,    61,
      62,   223,   224,   133,    63,    64,    65,    66,    67,    68,
      69,   234,   235,   236,    70,    71,    72,    73,    74,    75,
      76,    77,   251,   252,   283,   295,   304,   285,   297,   306,
      78,   308,   119,   187,    79,   115,    80,    81,    21,     9,
      10,    26,    27,    31,    32,    33,    34,     3
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint16 yytable[] =
{
      22,   126,   101,   103,   182,   136,   113,   244,   184,   185,
     164,   178,    25,    28,    85,    86,    87,    88,    89,    90,
      91,   164,    38,    92,   129,   201,    15,    16,    17,    18,
      36,    19,    11,    12,    46,   212,   117,     1,   118,     4,
     214,    13,    14,   154,     5,   153,    37,   104,   105,   128,
       7,   156,   237,   238,   164,   255,   238,     6,   158,   159,
      29,   272,   273,   314,   315,   316,   317,    29,   179,     8,
      23,    25,   220,   245,    35,    40,    41,    93,    82,    83,
     166,    84,    94,    95,    96,    97,    98,   114,   111,   112,
     191,   166,   257,    85,    86,    87,    88,    89,    90,    91,
      20,   215,    92,   170,   171,   172,   173,   116,    99,   120,
     106,   121,   122,   123,   124,   100,   127,   130,   131,   196,
     197,   198,   199,   200,   166,   134,   203,   204,   205,   206,
     207,   208,   209,   210,   135,   213,   288,   265,   172,   173,
     221,   292,    46,   126,   190,   138,   139,   180,   210,   140,
     141,   142,   143,   144,   126,   145,    93,   146,   246,   147,
     148,    94,    95,    96,    97,    98,   287,   160,   161,   174,
     162,   163,   164,   149,   150,   151,    85,    86,    87,    88,
      89,    90,    91,   152,   155,    92,   157,    99,   181,   176,
     177,   183,   258,   186,   100,   188,   189,   261,   192,   126,
     193,   194,   202,   191,   216,   222,   268,   269,   225,   104,
     105,    85,    86,    87,    88,    89,    90,    91,   226,   227,
      92,   231,   241,   242,   250,   243,   248,   249,   253,   254,
     126,   233,   160,   161,   126,   162,   163,   164,   271,    93,
     256,   260,   166,   262,    94,    95,    96,    97,    98,   263,
     264,   167,   168,   169,   170,   171,   172,   173,   266,   275,
     274,   277,   195,   278,   279,   282,   284,   289,   160,   161,
      99,   162,   163,   164,    93,   293,   290,   100,   294,    94,
      95,    96,    97,    98,   296,   298,   299,   165,   160,   161,
     300,   162,   163,   164,   302,   303,   160,   161,   305,   162,
     163,   164,   307,   310,   309,    99,   311,   166,   312,   319,
     175,   320,   100,   230,   229,   313,   167,   168,   169,   170,
     171,   172,   173,   270,   162,   163,   164,   240,   247,   160,
     161,   218,   162,   163,   164,   137,   259,   160,   161,    24,
     162,   163,   164,   166,    30,     0,   286,    39,   276,     0,
       0,     0,   167,   168,   169,   170,   171,   172,   173,   281,
       0,   160,   161,   166,   162,   163,   164,     0,     0,     0,
       0,   166,   167,   168,   169,   170,   171,   172,   173,     0,
     167,   168,   169,   170,   171,   172,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   166,     0,     0,     0,
       0,     0,     0,     0,   166,   167,   168,   169,   170,   171,
     172,   173,   166,   167,   168,   169,   170,   171,   172,   173,
       0,   167,   168,   169,   170,   171,   172,   173,     0,     0,
       0,     0,     0,    42,     0,     0,   166,     0,     0,     0,
       0,     0,     0,     0,     0,   167,   168,   169,   170,   171,
     172,   173,    43,     0,   232,   233,     0,    44,    45,    46,
       0,     0,     0,    42,     0,    47,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,     0,    50,     0,     0,
      51,   125,    43,     0,     0,     0,     0,    44,    45,    46,
       0,    52,    53,    54,    42,    47,     0,     0,     0,     0,
      55,    56,    48,     0,    57,    49,     0,    50,     0,     0,
      51,     0,   239,    43,     0,     0,     0,     0,    44,    45,
      46,    52,    53,    54,    42,     0,    47,     0,     0,     0,
      55,    56,     0,    48,    57,     0,    49,     0,    50,     0,
       0,    51,   301,    43,     0,     0,     0,     0,    44,    45,
      46,     0,    52,    53,    54,    42,    47,     0,     0,     0,
       0,    55,    56,    48,     0,    57,    49,     0,    50,     0,
       0,    51,     0,     0,    43,     0,     0,     0,     0,    44,
      45,    46,    52,    53,    54,     0,     0,    47,     0,     0,
       0,    55,    56,     0,    48,    57,     0,    49,     0,    50,
       0,     0,    51,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    52,    53,    54,     0,     0,     0,     0,
       0,     0,    55,    56,     0,     0,    57
};

static const yytype_int16 yycheck[] =
{
      12,    59,    43,    44,   112,    66,     9,    40,   116,   117,
      15,    49,     9,    25,     3,     4,     5,     6,     7,     8,
       9,    15,    26,    12,    61,   165,    19,    20,    21,    22,
      65,    24,    17,    18,    35,   175,    57,    16,    59,     9,
      38,   105,   106,    84,     0,    82,    81,    36,    37,    50,
     104,    92,   105,   106,    15,   105,   106,   103,    99,   100,
      64,   105,   106,   105,   106,    46,    47,    64,   106,     9,
      25,     9,   180,   106,   103,     9,     9,    66,    25,   103,
      85,    63,    71,    72,    73,    74,    75,    90,     9,    49,
     131,    85,   232,     3,     4,     5,     6,     7,     8,     9,
      93,    99,    12,    97,    98,    99,   100,    39,    97,     9,
      99,     9,     9,    78,    78,   104,   103,   103,    40,   160,
     161,   162,   163,   164,    85,   103,   167,   168,   169,   170,
     171,   172,   173,   174,   103,   176,   276,   245,    99,   100,
     181,   281,    35,   201,    54,   103,   103,    39,   189,   103,
     103,   103,   103,   103,   212,   103,    66,   103,   219,   103,
     103,    71,    72,    73,    74,    75,   274,    10,    11,   104,
      13,    14,    15,   103,   103,   103,     3,     4,     5,     6,
       7,     8,     9,   103,   102,    12,   102,    97,    17,   104,
     104,    52,   233,    60,   104,    49,   104,   238,   103,   257,
      70,    70,     5,   244,     9,     9,   247,   248,   104,    36,
      37,     3,     4,     5,     6,     7,     8,     9,    58,     9,
      12,    55,     9,   105,     9,   106,    94,   106,     9,   104,
     288,    31,    10,    11,   292,    13,    14,    15,   250,    66,
       9,    27,    85,    32,    71,    72,    73,    74,    75,   105,
       9,    94,    95,    96,    97,    98,    99,   100,    41,   105,
      62,    28,   105,    51,    82,   104,    61,    17,    10,    11,
      97,    13,    14,    15,    66,     3,    44,   104,    79,    71,
      72,    73,    74,    75,    91,   104,    83,    29,    10,    11,
      45,    13,    14,    15,   105,    12,    10,    11,    92,    13,
      14,    15,     9,     9,    84,    97,    32,    85,     8,     3,
      32,     9,   104,   189,   188,    94,    94,    95,    96,    97,
      98,    99,   100,   249,    13,    14,    15,   105,    42,    10,
      11,   179,    13,    14,    15,    66,   235,    10,    11,    14,
      13,    14,    15,    85,    27,    -1,   273,    34,    29,    -1,
      -1,    -1,    94,    95,    96,    97,    98,    99,   100,    32,
      -1,    10,    11,    85,    13,    14,    15,    -1,    -1,    -1,
      -1,    85,    94,    95,    96,    97,    98,    99,   100,    -1,
      94,    95,    96,    97,    98,    99,   100,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    85,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    85,    94,    95,    96,    97,    98,
      99,   100,    85,    94,    95,    96,    97,    98,    99,   100,
      -1,    94,    95,    96,    97,    98,    99,   100,    -1,    -1,
      -1,    -1,    -1,     9,    -1,    -1,    85,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    94,    95,    96,    97,    98,
      99,   100,    28,    -1,    30,    31,    -1,    33,    34,    35,
      -1,    -1,    -1,     9,    -1,    41,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    -1,    -1,    51,    -1,    53,    -1,    -1,
      56,    27,    28,    -1,    -1,    -1,    -1,    33,    34,    35,
      -1,    67,    68,    69,     9,    41,    -1,    -1,    -1,    -1,
      76,    77,    48,    -1,    80,    51,    -1,    53,    -1,    -1,
      56,    -1,    27,    28,    -1,    -1,    -1,    -1,    33,    34,
      35,    67,    68,    69,     9,    -1,    41,    -1,    -1,    -1,
      76,    77,    -1,    48,    80,    -1,    51,    -1,    53,    -1,
      -1,    56,    27,    28,    -1,    -1,    -1,    -1,    33,    34,
      35,    -1,    67,    68,    69,     9,    41,    -1,    -1,    -1,
      -1,    76,    77,    48,    -1,    80,    51,    -1,    53,    -1,
      -1,    56,    -1,    -1,    28,    -1,    -1,    -1,    -1,    33,
      34,    35,    67,    68,    69,    -1,    -1,    41,    -1,    -1,
      -1,    76,    77,    -1,    48,    80,    -1,    51,    -1,    53,
      -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    67,    68,    69,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    77,    -1,    -1,    80
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    16,   108,   174,     9,     0,   103,   104,     9,   166,
     167,    17,    18,   105,   106,    19,    20,    21,    22,    24,
      93,   165,   165,    25,   166,     9,   168,   169,   165,    64,
     168,   170,   171,   172,   173,   103,    65,    81,    26,   172,
       9,     9,     9,    28,    33,    34,    35,    41,    48,    51,
      53,    56,    67,    68,    69,    76,    77,    80,   109,   110,
     125,   126,   127,   131,   132,   133,   134,   135,   136,   137,
     141,   142,   143,   144,   145,   146,   147,   148,   157,   161,
     163,   164,    25,   103,    63,     3,     4,     5,     6,     7,
       8,     9,    12,    66,    71,    72,    73,    74,    75,    97,
     104,   111,   112,   111,    36,    37,    99,   111,   117,   118,
     119,     9,    49,     9,    90,   162,    39,    57,    59,   159,
       9,     9,     9,    78,    78,    27,   109,   103,    50,   125,
     103,    40,   120,   130,   103,   103,   120,   130,   103,   103,
     103,   103,   103,   103,   103,   103,   103,   103,   103,   103,
     103,   103,   103,   125,   111,   102,   111,   102,   111,   111,
      10,    11,    13,    14,    15,    29,    85,    94,    95,    96,
      97,    98,    99,   100,   104,    32,   104,   104,    49,   106,
      39,    17,   162,    52,   162,   162,    60,   160,    49,   104,
      54,   111,   103,    70,    70,   105,   111,   111,   111,   111,
     111,   110,     5,   111,   111,   111,   111,   111,   111,   111,
     111,   116,   110,   111,    38,    99,     9,   115,   117,   114,
     162,   111,     9,   128,   129,   104,    58,     9,   113,   115,
     116,    55,    30,    31,   138,   139,   140,   105,   106,    27,
     105,     9,   105,   106,    40,   106,   120,    42,    94,   106,
       9,   149,   150,     9,   104,   105,     9,   110,   111,   138,
      27,   111,    32,   105,     9,   162,    41,   121,   111,   111,
     128,   165,   105,   106,    62,   105,    29,    28,    51,    82,
     122,    32,   104,   151,    61,   154,   149,   162,   110,    17,
      44,   124,   110,     3,    79,   152,    91,   155,   104,    83,
      45,    27,   105,    12,   153,    92,   156,     9,   158,    84,
       9,    32,     8,    94,   105,   106,    46,    47,   123,     3,
       9
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   107,   108,   109,   109,   109,   109,   109,   109,   109,
     109,   109,   109,   109,   109,   109,   109,   109,   109,   109,
     109,   109,   110,   110,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   111,   111,
     111,   112,   112,   112,   112,   112,   113,   114,   114,   115,
     115,   115,   116,   116,   116,   117,   117,   117,   117,   118,
     118,   118,   119,   119,   119,   120,   120,   121,   121,   122,
     122,   123,   123,   123,   124,   124,   125,   126,   127,   127,
     128,   129,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   139,   140,   140,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   148,   149,   150,   150,   151,
     151,   152,   152,   153,   153,   154,   154,   155,   155,   156,
     156,   157,   158,   158,   159,   159,   160,   160,   161,   162,
     162,   163,   164,   165,   165,   165,   165,   165,   165,   166,
     166,   167,   167,   167,   168,   169,   169,   169,   170,   171,
     172,   172,   173,   173,   173,   174
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     1,     2,     1,     4,     1,     1,     1,     1,
       1,     1,     1,     3,     3,     3,     3,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     3,
       3,     1,     1,     1,     1,     1,     3,     1,     3,     0,
       1,     3,     0,     1,     3,     1,     4,     5,     4,     0,
       1,     3,     1,     3,     1,     0,     2,     0,     2,     0,
       4,     0,     1,     1,     0,     4,     8,     3,     5,     2,
       3,     1,     3,     4,     4,     2,     2,     3,     2,     2,
       3,     4,     1,     2,     0,     2,     1,     7,     6,    10,
       1,     1,     2,     2,     4,     4,     5,     1,     3,     0,
       3,     0,     1,     0,     2,     0,     1,     0,     1,     0,
       3,     9,     1,     3,     0,     1,     0,     1,    10,     1,
       1,     2,     2,     1,     1,     1,     1,     1,     1,     3,
       3,     0,     1,     3,     3,     0,     1,     2,     6,     4,
       1,     1,     0,     1,     2,    11
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
static int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
static int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 22:
#line 176 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 1646 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 23:
#line 178 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-1]), (yyvsp[0])); }
#line 1652 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 24:
#line 182 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1658 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 25:
#line 184 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_func((yyvsp[-3]), (yyvsp[-1])); }
#line 1664 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 26:
#line 185 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1670 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 27:
#line 186 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1676 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 28:
#line 187 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1682 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 29:
#line 188 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1688 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 30:
#line 189 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1694 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 31:
#line 190 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1700 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 32:
#line 191 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]);}
#line 1706 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 33:
#line 192 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('+', (yyvsp[-2]), (yyvsp[0])); }
#line 1712 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 34:
#line 193 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('-', (yyvsp[-2]), (yyvsp[0])); }
#line 1718 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 35:
#line 194 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('*', (yyvsp[-2]), (yyvsp[0])); }
#line 1724 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 36:
#line 195 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('/', (yyvsp[-2]), (yyvsp[0])); }
#line 1730 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 37:
#line 196 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('-', (yyvsp[0]), NULL); }
#line 1736 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 38:
#line 197 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]); }
#line 1742 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 39:
#line 198 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('=', (yyvsp[-2]), (yyvsp[0])); }
#line 1748 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 40:
#line 200 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_LIKE_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1754 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 41:
#line 201 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('<', (yyvsp[-2]), (yyvsp[0])); }
#line 1760 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 42:
#line 202 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op('>', (yyvsp[-2]), (yyvsp[0])); }
#line 1766 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 43:
#line 203 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_GE_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1772 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 44:
#line 204 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_LE_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1778 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 45:
#line 205 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_NE_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1784 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 46:
#line 206 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_AND_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1790 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 47:
#line 207 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_OR_TOKEN, (yyvsp[-2]), (yyvsp[0])); }
#line 1796 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 48:
#line 208 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_NOT_TOKEN, (yyvsp[0]), NULL); }
#line 1802 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 49:
#line 210 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_NOTFOUND_TOKEN, (yyvsp[-2]), NULL); }
#line 1808 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 50:
#line 212 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_op(PARS_NOTFOUND_TOKEN, (yyvsp[-2]), NULL); }
#line 1814 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 51:
#line 216 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_substr_token; }
#line 1820 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 52:
#line 217 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_concat_token; }
#line 1826 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 53:
#line 218 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_instr_token; }
#line 1832 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 54:
#line 220 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_to_binary_token; }
#line 1838 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 55:
#line 221 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_length_token; }
#line 1844 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 56:
#line 225 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-2]); }
#line 1850 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 57:
#line 229 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 1856 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 58:
#line 231 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 1862 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 59:
#line 235 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1868 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 60:
#line 236 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 1874 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 61:
#line 238 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 1880 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 62:
#line 242 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1886 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 63:
#line 243 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0]));}
#line 1892 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 64:
#line 244 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 1898 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 65:
#line 248 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 1904 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 66:
#line 250 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); }
#line 1913 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 67:
#line 255 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, (yyvsp[-1]))))); }
#line 1923 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 68:
#line 261 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									(yyvsp[-1]))); }
#line 1931 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 69:
#line 267 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1937 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 70:
#line 268 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 1943 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 71:
#line 270 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 1949 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 72:
#line 274 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_select_list(&pars_star_denoter,
								NULL); }
#line 1956 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 73:
#line 277 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_select_list(
					(yyvsp[-2]), static_cast<sym_node_t*>((yyvsp[0]))); }
#line 1963 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 74:
#line 279 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_select_list((yyvsp[0]), NULL); }
#line 1969 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 75:
#line 283 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1975 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 76:
#line 284 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 1981 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 77:
#line 288 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1987 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 78:
#line 290 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_update_token; }
#line 1993 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 79:
#line 294 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 1999 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 80:
#line 296 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_share_token; }
#line 2005 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 81:
#line 300 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_asc_token; }
#line 2011 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 82:
#line 301 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_asc_token; }
#line 2017 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 83:
#line 302 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_desc_token; }
#line 2023 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 84:
#line 306 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2029 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 85:
#line 308 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_order_by(
					static_cast<sym_node_t*>((yyvsp[-1])),
					static_cast<pars_res_word_t*>((yyvsp[0]))); }
#line 2037 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 86:
#line 319 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_select_statement(
					static_cast<sel_node_t*>((yyvsp[-6])),
					static_cast<sym_node_t*>((yyvsp[-4])),
					static_cast<que_node_t*>((yyvsp[-3])),
					static_cast<pars_res_word_t*>((yyvsp[-2])),
					static_cast<pars_res_word_t*>((yyvsp[-1])),
					static_cast<order_node_t*>((yyvsp[0]))); }
#line 2049 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 87:
#line 330 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2055 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 88:
#line 335 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_insert_statement(
					static_cast<sym_node_t*>((yyvsp[-4])), (yyvsp[-1]), NULL); }
#line 2062 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 89:
#line 338 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_insert_statement(
					static_cast<sym_node_t*>((yyvsp[-1])),
					NULL,
					static_cast<sel_node_t*>((yyvsp[0]))); }
#line 2071 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 90:
#line 345 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_column_assignment(
					static_cast<sym_node_t*>((yyvsp[-2])),
					static_cast<que_node_t*>((yyvsp[0]))); }
#line 2079 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 91:
#line 351 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 2085 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 92:
#line 353 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 2091 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 93:
#line 359 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2097 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 94:
#line 365 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement_start(
					FALSE,
					static_cast<sym_node_t*>((yyvsp[-2])),
					static_cast<col_assign_node_t*>((yyvsp[0]))); }
#line 2106 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 95:
#line 373 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[-1])),
					NULL,
					static_cast<que_node_t*>((yyvsp[0]))); }
#line 2115 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 96:
#line 381 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[-1])),
					static_cast<sym_node_t*>((yyvsp[0])),
					NULL); }
#line 2124 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 97:
#line 389 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement_start(
					TRUE,
					static_cast<sym_node_t*>((yyvsp[0])), NULL); }
#line 2132 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 98:
#line 396 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[-1])),
					NULL,
					static_cast<que_node_t*>((yyvsp[0]))); }
#line 2141 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 99:
#line 404 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_update_statement(
					static_cast<upd_node_t*>((yyvsp[-1])),
					static_cast<sym_node_t*>((yyvsp[0])),
					NULL); }
#line 2150 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 100:
#line 412 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_assignment_statement(
					static_cast<sym_node_t*>((yyvsp[-2])),
					static_cast<que_node_t*>((yyvsp[0]))); }
#line 2158 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 101:
#line 420 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_elsif_element((yyvsp[-2]), (yyvsp[0])); }
#line 2164 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 102:
#line 424 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 2170 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 103:
#line 426 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-1]), (yyvsp[0])); }
#line 2176 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 104:
#line 430 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2182 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 105:
#line 432 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2188 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 106:
#line 433 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2194 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 107:
#line 440 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_if_statement((yyvsp[-5]), (yyvsp[-3]), (yyvsp[-2])); }
#line 2200 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 108:
#line 446 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_while_statement((yyvsp[-4]), (yyvsp[-2])); }
#line 2206 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 109:
#line 454 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_for_statement(
					static_cast<sym_node_t*>((yyvsp[-8])),
					(yyvsp[-6]), (yyvsp[-4]), (yyvsp[-2])); }
#line 2214 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 110:
#line 460 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_exit_statement(); }
#line 2220 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 111:
#line 464 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_return_statement(); }
#line 2226 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 112:
#line 469 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_open_statement(
						ROW_SEL_OPEN_CURSOR,
						static_cast<sym_node_t*>((yyvsp[0]))); }
#line 2234 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 113:
#line 476 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR,
						static_cast<sym_node_t*>((yyvsp[0]))); }
#line 2242 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 114:
#line 483 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_fetch_statement(
					static_cast<sym_node_t*>((yyvsp[-2])),
					static_cast<sym_node_t*>((yyvsp[0])), NULL); }
#line 2250 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 115:
#line 487 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_fetch_statement(
					static_cast<sym_node_t*>((yyvsp[-2])),
					NULL,
					static_cast<sym_node_t*>((yyvsp[0]))); }
#line 2259 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 116:
#line 495 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_column_def(
					static_cast<sym_node_t*>((yyvsp[-4])),
					static_cast<pars_res_word_t*>((yyvsp[-3])),
					static_cast<sym_node_t*>((yyvsp[-2])),
					(yyvsp[-1]), (yyvsp[0])); }
#line 2269 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 117:
#line 503 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 2275 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 118:
#line 505 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 2281 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 119:
#line 509 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2287 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 120:
#line 511 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]); }
#line 2293 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 121:
#line 515 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2299 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 122:
#line 517 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ }
#line 2306 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 123:
#line 522 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2312 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 124:
#line 524 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ }
#line 2319 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 125:
#line 529 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2325 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 126:
#line 531 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ }
#line 2332 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 127:
#line 536 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2338 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 128:
#line 537 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token;
					/* pass any non-NULL pointer */ }
#line 2345 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 129:
#line 542 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2351 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 130:
#line 544 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2357 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 131:
#line 551 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_create_table(
					static_cast<sym_node_t*>((yyvsp[-6])),
					static_cast<sym_node_t*>((yyvsp[-4])),
					static_cast<sym_node_t*>((yyvsp[-1])),
					static_cast<sym_node_t*>((yyvsp[0])), (yyvsp[-2])); }
#line 2367 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 132:
#line 559 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 2373 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 133:
#line 561 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 2379 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 134:
#line 565 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2385 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 135:
#line 566 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_unique_token; }
#line 2391 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 136:
#line 570 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2397 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 137:
#line 571 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_clustered_token; }
#line 2403 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 138:
#line 580 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_create_index(
					static_cast<pars_res_word_t*>((yyvsp[-8])),
					static_cast<pars_res_word_t*>((yyvsp[-7])),
					static_cast<sym_node_t*>((yyvsp[-5])),
					static_cast<sym_node_t*>((yyvsp[-3])),
					static_cast<sym_node_t*>((yyvsp[-1]))); }
#line 2414 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 139:
#line 589 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2420 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 140:
#line 590 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 2426 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 141:
#line 595 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_commit_statement(); }
#line 2432 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 142:
#line 600 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_rollback_statement(); }
#line 2438 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 143:
#line 604 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token; }
#line 2444 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 144:
#line 605 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_int_token; }
#line 2450 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 145:
#line 606 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_bigint_token; }
#line 2456 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 146:
#line 607 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_char_token; }
#line 2462 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 147:
#line 608 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_binary_token; }
#line 2468 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 148:
#line 609 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = &pars_blob_token; }
#line 2474 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 149:
#line 614 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_parameter_declaration(
					static_cast<sym_node_t*>((yyvsp[-2])),
					PARS_INPUT,
					static_cast<pars_res_word_t*>((yyvsp[0]))); }
#line 2483 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 150:
#line 619 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_parameter_declaration(
					static_cast<sym_node_t*>((yyvsp[-2])),
					PARS_OUTPUT,
					static_cast<pars_res_word_t*>((yyvsp[0]))); }
#line 2492 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 151:
#line 626 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = NULL; }
#line 2498 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 152:
#line 627 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last(NULL, (yyvsp[0])); }
#line 2504 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 153:
#line 629 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = que_node_list_add_last((yyvsp[-2]), (yyvsp[0])); }
#line 2510 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 154:
#line 634 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_variable_declaration(
					static_cast<sym_node_t*>((yyvsp[-2])),
					static_cast<pars_res_word_t*>((yyvsp[-1]))); }
#line 2518 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 158:
#line 648 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_cursor_declaration(
					static_cast<sym_node_t*>((yyvsp[-3])),
					static_cast<sel_node_t*>((yyvsp[-1]))); }
#line 2526 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 159:
#line 655 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_function_declaration(
					static_cast<sym_node_t*>((yyvsp[-1]))); }
#line 2533 "pars0grm.cc" /* yacc.c:1646  */
    break;

  case 165:
#line 677 "pars0grm.y" /* yacc.c:1646  */
    { (yyval) = pars_procedure_definition(
					static_cast<sym_node_t*>((yyvsp[-9])),
					static_cast<sym_node_t*>((yyvsp[-7])),
					(yyvsp[-1])); }
#line 2542 "pars0grm.cc" /* yacc.c:1646  */
    break;


#line 2546 "pars0grm.cc" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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

  /* Else will try to reuse lookahead token after shifting the error
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
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
  return yyresult;
}
#line 683 "pars0grm.y" /* yacc.c:1906  */

