
/*  A Bison parser, made from pars0grm.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	PARS_INT_LIT	258
#define	PARS_FLOAT_LIT	259
#define	PARS_STR_LIT	260
#define	PARS_NULL_LIT	261
#define	PARS_ID_TOKEN	262
#define	PARS_AND_TOKEN	263
#define	PARS_OR_TOKEN	264
#define	PARS_NOT_TOKEN	265
#define	PARS_GE_TOKEN	266
#define	PARS_LE_TOKEN	267
#define	PARS_NE_TOKEN	268
#define	PARS_PROCEDURE_TOKEN	269
#define	PARS_IN_TOKEN	270
#define	PARS_OUT_TOKEN	271
#define	PARS_INT_TOKEN	272
#define	PARS_INTEGER_TOKEN	273
#define	PARS_FLOAT_TOKEN	274
#define	PARS_CHAR_TOKEN	275
#define	PARS_IS_TOKEN	276
#define	PARS_BEGIN_TOKEN	277
#define	PARS_END_TOKEN	278
#define	PARS_IF_TOKEN	279
#define	PARS_THEN_TOKEN	280
#define	PARS_ELSE_TOKEN	281
#define	PARS_ELSIF_TOKEN	282
#define	PARS_LOOP_TOKEN	283
#define	PARS_WHILE_TOKEN	284
#define	PARS_RETURN_TOKEN	285
#define	PARS_SELECT_TOKEN	286
#define	PARS_SUM_TOKEN	287
#define	PARS_COUNT_TOKEN	288
#define	PARS_DISTINCT_TOKEN	289
#define	PARS_FROM_TOKEN	290
#define	PARS_WHERE_TOKEN	291
#define	PARS_FOR_TOKEN	292
#define	PARS_DDOT_TOKEN	293
#define	PARS_CONSISTENT_TOKEN	294
#define	PARS_READ_TOKEN	295
#define	PARS_ORDER_TOKEN	296
#define	PARS_BY_TOKEN	297
#define	PARS_ASC_TOKEN	298
#define	PARS_DESC_TOKEN	299
#define	PARS_INSERT_TOKEN	300
#define	PARS_INTO_TOKEN	301
#define	PARS_VALUES_TOKEN	302
#define	PARS_UPDATE_TOKEN	303
#define	PARS_SET_TOKEN	304
#define	PARS_DELETE_TOKEN	305
#define	PARS_CURRENT_TOKEN	306
#define	PARS_OF_TOKEN	307
#define	PARS_CREATE_TOKEN	308
#define	PARS_TABLE_TOKEN	309
#define	PARS_INDEX_TOKEN	310
#define	PARS_UNIQUE_TOKEN	311
#define	PARS_CLUSTERED_TOKEN	312
#define	PARS_DOES_NOT_FIT_IN_MEM_TOKEN	313
#define	PARS_ON_TOKEN	314
#define	PARS_ASSIGN_TOKEN	315
#define	PARS_DECLARE_TOKEN	316
#define	PARS_CURSOR_TOKEN	317
#define	PARS_SQL_TOKEN	318
#define	PARS_OPEN_TOKEN	319
#define	PARS_FETCH_TOKEN	320
#define	PARS_CLOSE_TOKEN	321
#define	PARS_NOTFOUND_TOKEN	322
#define	PARS_TO_CHAR_TOKEN	323
#define	PARS_TO_NUMBER_TOKEN	324
#define	PARS_TO_BINARY_TOKEN	325
#define	PARS_BINARY_TO_NUMBER_TOKEN	326
#define	PARS_SUBSTR_TOKEN	327
#define	PARS_REPLSTR_TOKEN	328
#define	PARS_CONCAT_TOKEN	329
#define	PARS_INSTR_TOKEN	330
#define	PARS_LENGTH_TOKEN	331
#define	PARS_SYSDATE_TOKEN	332
#define	PARS_PRINTF_TOKEN	333
#define	PARS_ASSERT_TOKEN	334
#define	PARS_RND_TOKEN	335
#define	PARS_RND_STR_TOKEN	336
#define	PARS_ROW_PRINTF_TOKEN	337
#define	PARS_COMMIT_TOKEN	338
#define	PARS_ROLLBACK_TOKEN	339
#define	PARS_WORK_TOKEN	340
#define	NEG	341

#line 9 "pars0grm.y"

/* The value of the semantic attribute is a pointer to a query tree node
que_node_t */
#define YYSTYPE que_node_t*
#define alloca	mem_alloc

#include <math.h>

#include "univ.i"
#include "pars0pars.h"
#include "mem0mem.h"
#include "que0types.h"
#include "que0que.h"
#include "row0sel.h"

/* #define __STDC__ */

int
yylex(void);
#ifndef YYSTYPE
#define YYSTYPE int
#endif
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		311
#define	YYFLAG		-32768
#define	YYNTBASE	102

#define YYTRANSLATE(x) ((unsigned)(x) <= 341 ? yytranslate[x] : 163)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,    94,     2,     2,    96,
    97,    91,    90,    99,    89,     2,    92,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,    95,    87,
    86,    88,    98,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,   100,     2,   101,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
    66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
    93
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     3,     5,     8,    11,    14,    17,    20,    23,    26,
    29,    32,    35,    38,    41,    44,    47,    50,    53,    56,
    59,    62,    65,    67,    70,    72,    77,    79,    81,    83,
    85,    87,    91,    95,    99,   103,   106,   110,   114,   118,
   122,   126,   130,   134,   138,   142,   145,   149,   153,   155,
   157,   159,   161,   163,   165,   167,   169,   171,   173,   175,
   176,   178,   182,   189,   194,   196,   198,   200,   202,   206,
   207,   209,   213,   214,   216,   220,   222,   227,   233,   238,
   239,   241,   245,   247,   251,   253,   254,   257,   258,   261,
   262,   265,   266,   268,   270,   271,   276,   285,   289,   295,
   298,   302,   304,   308,   313,   318,   321,   324,   328,   331,
   334,   337,   341,   346,   348,   351,   352,   355,   357,   365,
   372,   383,   385,   388,   391,   396,   399,   401,   405,   406,
   408,   416,   418,   422,   423,   425,   426,   428,   439,   442,
   445,   447,   449,   453,   457,   458,   460,   464,   468,   469,
   471,   474,   481,   482,   484,   487
};

static const short yyrhs[] = {   162,
    95,     0,   107,     0,   108,    95,     0,   139,    95,     0,
   140,    95,     0,   138,    95,     0,   141,    95,     0,   134,
    95,     0,   121,    95,     0,   123,    95,     0,   133,    95,
     0,   131,    95,     0,   132,    95,     0,   128,    95,     0,
   129,    95,     0,   142,    95,     0,   144,    95,     0,   143,
    95,     0,   153,    95,     0,   154,    95,     0,   148,    95,
     0,   152,    95,     0,   102,     0,   103,   102,     0,     7,
     0,   105,    96,   112,    97,     0,     3,     0,     4,     0,
     5,     0,     6,     0,    63,     0,   104,    90,   104,     0,
   104,    89,   104,     0,   104,    91,   104,     0,   104,    92,
   104,     0,    89,   104,     0,    96,   104,    97,     0,   104,
    86,   104,     0,   104,    87,   104,     0,   104,    88,   104,
     0,   104,    11,   104,     0,   104,    12,   104,     0,   104,
    13,   104,     0,   104,     8,   104,     0,   104,     9,   104,
     0,    10,   104,     0,     7,    94,    67,     0,    63,    94,
    67,     0,    68,     0,    69,     0,    70,     0,    71,     0,
    72,     0,    74,     0,    75,     0,    76,     0,    77,     0,
    80,     0,    81,     0,     0,    98,     0,   106,    99,    98,
     0,   100,     7,    96,   106,    97,   101,     0,   109,    96,
   112,    97,     0,    73,     0,    78,     0,    79,     0,     7,
     0,   110,    99,     7,     0,     0,     7,     0,   111,    99,
     7,     0,     0,   104,     0,   112,    99,   104,     0,   104,
     0,    33,    96,    91,    97,     0,    33,    96,    34,     7,
    97,     0,    32,    96,   104,    97,     0,     0,   113,     0,
   114,    99,   113,     0,    91,     0,   114,    46,   111,     0,
   114,     0,     0,    36,   104,     0,     0,    37,    48,     0,
     0,    39,    40,     0,     0,    43,     0,    44,     0,     0,
    41,    42,     7,   119,     0,    31,   115,    35,   110,   116,
   117,   118,   120,     0,    45,    46,     7,     0,   122,    47,
    96,   112,    97,     0,   122,   121,     0,     7,    86,   104,
     0,   124,     0,   125,    99,   124,     0,    36,    51,    52,
     7,     0,    48,     7,    49,   125,     0,   127,   116,     0,
   127,   126,     0,    50,    35,     7,     0,   130,   116,     0,
   130,   126,     0,    82,   121,     0,     7,    60,   104,     0,
    27,   104,    25,   103,     0,   135,     0,   136,   135,     0,
     0,    26,   103,     0,   136,     0,    24,   104,    25,   103,
   137,    23,    24,     0,    29,   104,    28,   103,    23,    28,
     0,    37,     7,    15,   104,    38,   104,    28,   103,    23,
    28,     0,    30,     0,    64,     7,     0,    66,     7,     0,
    65,     7,    46,   111,     0,     7,   155,     0,   145,     0,
   146,    99,   145,     0,     0,    58,     0,    53,    54,     7,
    96,   146,    97,   147,     0,     7,     0,   149,    99,     7,
     0,     0,    56,     0,     0,    57,     0,    53,   150,   151,
    55,     7,    59,     7,    96,   149,    97,     0,    83,    85,
     0,    84,    85,     0,    17,     0,    20,     0,     7,    15,
   155,     0,     7,    16,   155,     0,     0,   156,     0,   157,
    99,   156,     0,     7,   155,    95,     0,     0,   158,     0,
   159,   158,     0,    61,    62,     7,    21,   121,    95,     0,
     0,   160,     0,   161,   160,     0,    14,     7,    96,   157,
    97,    21,   159,   161,    22,   103,    23,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   125,   127,   128,   129,   130,   131,   132,   133,   134,   135,
   136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
   146,   147,   150,   152,   156,   158,   160,   161,   162,   163,
   164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
   174,   175,   176,   177,   178,   179,   180,   182,   186,   188,
   189,   190,   192,   193,   194,   195,   196,   197,   198,   201,
   203,   204,   207,   212,   217,   219,   220,   223,   225,   229,
   231,   232,   236,   238,   239,   242,   244,   249,   255,   261,
   263,   264,   268,   271,   273,   276,   278,   281,   283,   287,
   289,   293,   295,   296,   299,   301,   305,   315,   320,   323,
   327,   331,   333,   337,   343,   350,   355,   360,   366,   371,
   376,   381,   386,   392,   394,   398,   400,   402,   405,   412,
   418,   426,   430,   436,   442,   447,   451,   453,   457,   459,
   464,   470,   472,   476,   478,   481,   483,   486,   494,   499,
   504,   506,   509,   513,   518,   520,   521,   525,   530,   532,
   533,   536,   542,   544,   545,   548
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","PARS_INT_LIT",
"PARS_FLOAT_LIT","PARS_STR_LIT","PARS_NULL_LIT","PARS_ID_TOKEN","PARS_AND_TOKEN",
"PARS_OR_TOKEN","PARS_NOT_TOKEN","PARS_GE_TOKEN","PARS_LE_TOKEN","PARS_NE_TOKEN",
"PARS_PROCEDURE_TOKEN","PARS_IN_TOKEN","PARS_OUT_TOKEN","PARS_INT_TOKEN","PARS_INTEGER_TOKEN",
"PARS_FLOAT_TOKEN","PARS_CHAR_TOKEN","PARS_IS_TOKEN","PARS_BEGIN_TOKEN","PARS_END_TOKEN",
"PARS_IF_TOKEN","PARS_THEN_TOKEN","PARS_ELSE_TOKEN","PARS_ELSIF_TOKEN","PARS_LOOP_TOKEN",
"PARS_WHILE_TOKEN","PARS_RETURN_TOKEN","PARS_SELECT_TOKEN","PARS_SUM_TOKEN",
"PARS_COUNT_TOKEN","PARS_DISTINCT_TOKEN","PARS_FROM_TOKEN","PARS_WHERE_TOKEN",
"PARS_FOR_TOKEN","PARS_DDOT_TOKEN","PARS_CONSISTENT_TOKEN","PARS_READ_TOKEN",
"PARS_ORDER_TOKEN","PARS_BY_TOKEN","PARS_ASC_TOKEN","PARS_DESC_TOKEN","PARS_INSERT_TOKEN",
"PARS_INTO_TOKEN","PARS_VALUES_TOKEN","PARS_UPDATE_TOKEN","PARS_SET_TOKEN","PARS_DELETE_TOKEN",
"PARS_CURRENT_TOKEN","PARS_OF_TOKEN","PARS_CREATE_TOKEN","PARS_TABLE_TOKEN",
"PARS_INDEX_TOKEN","PARS_UNIQUE_TOKEN","PARS_CLUSTERED_TOKEN","PARS_DOES_NOT_FIT_IN_MEM_TOKEN",
"PARS_ON_TOKEN","PARS_ASSIGN_TOKEN","PARS_DECLARE_TOKEN","PARS_CURSOR_TOKEN",
"PARS_SQL_TOKEN","PARS_OPEN_TOKEN","PARS_FETCH_TOKEN","PARS_CLOSE_TOKEN","PARS_NOTFOUND_TOKEN",
"PARS_TO_CHAR_TOKEN","PARS_TO_NUMBER_TOKEN","PARS_TO_BINARY_TOKEN","PARS_BINARY_TO_NUMBER_TOKEN",
"PARS_SUBSTR_TOKEN","PARS_REPLSTR_TOKEN","PARS_CONCAT_TOKEN","PARS_INSTR_TOKEN",
"PARS_LENGTH_TOKEN","PARS_SYSDATE_TOKEN","PARS_PRINTF_TOKEN","PARS_ASSERT_TOKEN",
"PARS_RND_TOKEN","PARS_RND_STR_TOKEN","PARS_ROW_PRINTF_TOKEN","PARS_COMMIT_TOKEN",
"PARS_ROLLBACK_TOKEN","PARS_WORK_TOKEN","'='","'<'","'>'","'-'","'+'","'*'",
"'/'","NEG","'%'","';'","'('","')'","'?'","','","'{'","'}'","statement","statement_list",
"exp","function_name","question_mark_list","stored_procedure_call","predefined_procedure_call",
"predefined_procedure_name","table_list","variable_list","exp_list","select_item",
"select_item_list","select_list","search_condition","for_update_clause","consistent_read_clause",
"order_direction","order_by_clause","select_statement","insert_statement_start",
"insert_statement","column_assignment","column_assignment_list","cursor_positioned",
"update_statement_start","update_statement_searched","update_statement_positioned",
"delete_statement_start","delete_statement_searched","delete_statement_positioned",
"row_printf_statement","assignment_statement","elsif_element","elsif_list","else_part",
"if_statement","while_statement","for_statement","return_statement","open_cursor_statement",
"close_cursor_statement","fetch_statement","column_def","column_def_list","not_fit_in_memory",
"create_table","column_list","unique_def","clustered_def","create_index","commit_statement",
"rollback_statement","type_name","parameter_declaration","parameter_declaration_list",
"variable_declaration","variable_declaration_list","cursor_declaration","declaration_list",
"procedure_definition", NULL
};
#endif

static const short yyr1[] = {     0,
   102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
   102,   102,   102,   102,   102,   102,   102,   102,   102,   102,
   102,   102,   103,   103,   104,   104,   104,   104,   104,   104,
   104,   104,   104,   104,   104,   104,   104,   104,   104,   104,
   104,   104,   104,   104,   104,   104,   104,   104,   105,   105,
   105,   105,   105,   105,   105,   105,   105,   105,   105,   106,
   106,   106,   107,   108,   109,   109,   109,   110,   110,   111,
   111,   111,   112,   112,   112,   113,   113,   113,   113,   114,
   114,   114,   115,   115,   115,   116,   116,   117,   117,   118,
   118,   119,   119,   119,   120,   120,   121,   122,   123,   123,
   124,   125,   125,   126,   127,   128,   129,   130,   131,   132,
   133,   134,   135,   136,   136,   137,   137,   137,   138,   139,
   140,   141,   142,   143,   144,   145,   146,   146,   147,   147,
   148,   149,   149,   150,   150,   151,   151,   152,   153,   154,
   155,   155,   156,   156,   157,   157,   157,   158,   159,   159,
   159,   160,   161,   161,   161,   162
};

static const short yyr2[] = {     0,
     2,     1,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     1,     2,     1,     4,     1,     1,     1,     1,
     1,     3,     3,     3,     3,     2,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     2,     3,     3,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
     1,     3,     6,     4,     1,     1,     1,     1,     3,     0,
     1,     3,     0,     1,     3,     1,     4,     5,     4,     0,
     1,     3,     1,     3,     1,     0,     2,     0,     2,     0,
     2,     0,     1,     1,     0,     4,     8,     3,     5,     2,
     3,     1,     3,     4,     4,     2,     2,     3,     2,     2,
     2,     3,     4,     1,     2,     0,     2,     1,     7,     6,
    10,     1,     2,     2,     4,     2,     1,     3,     0,     1,
     7,     1,     3,     0,     1,     0,     1,    10,     2,     2,
     1,     1,     3,     3,     0,     1,     3,     3,     0,     1,
     2,     6,     0,     1,     2,    11
};

static const short yydefact[] = {     0,
     0,     0,     0,     0,   122,    80,     0,     0,     0,     0,
   134,     0,     0,     0,    65,    66,    67,     0,     0,     0,
     0,     2,     0,     0,     0,     0,     0,    86,     0,     0,
    86,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,    27,
    28,    29,    30,    25,     0,    31,    49,    50,    51,    52,
    53,    54,    55,    56,    57,    58,    59,     0,     0,     0,
     0,     0,     0,     0,    83,    76,    81,    85,     0,     0,
     0,     0,     0,     0,   135,   136,   123,     0,   124,   111,
   139,   140,     0,     3,    73,     9,     0,   100,    10,     0,
   106,   107,    14,    15,   109,   110,    12,    13,    11,     8,
     6,     4,     5,     7,    16,    18,    17,    21,    22,    19,
    20,     1,   112,   145,     0,    46,     0,    36,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    73,     0,     0,     0,    70,     0,     0,     0,
    98,     0,   108,     0,   137,     0,    70,    60,    74,     0,
    73,     0,    87,     0,   146,     0,    47,    48,    37,    44,
    45,    41,    42,    43,    23,   116,    38,    39,    40,    33,
    32,    34,    35,     0,     0,     0,     0,     0,    71,    84,
    82,    68,    86,     0,     0,   102,   105,     0,     0,   125,
    61,     0,    64,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    24,   114,   118,     0,    26,     0,    79,     0,
    77,     0,     0,     0,    88,     0,     0,     0,     0,   127,
     0,     0,     0,     0,    75,    99,   104,   141,   142,   143,
   144,   149,   147,   117,     0,   115,     0,   120,    78,    72,
    69,     0,    90,     0,   101,   103,   126,   129,     0,     0,
    63,    62,     0,   150,   153,     0,   119,    89,     0,    95,
     0,   130,   131,   128,     0,     0,     0,   151,   154,     0,
   113,    91,     0,    97,     0,     0,   148,     0,     0,   155,
     0,     0,   132,     0,     0,     0,    92,   121,   138,     0,
     0,   156,    93,    94,    96,   133,     0,   152,     0,     0,
     0
};

static const short yydefgoto[] = {   175,
   176,   159,    71,   202,    22,    23,    24,   193,   190,   160,
    77,    78,    79,   101,   253,   270,   305,   284,    25,    26,
    27,   196,   197,   102,    28,    29,    30,    31,    32,    33,
    34,    35,   214,   215,   216,    36,    37,    38,    39,    40,
    41,    42,   230,   231,   273,    43,   294,    86,   156,    44,
    45,    46,   240,   165,   166,   264,   265,   279,   280,    47
};

static const short yypact[] = {   443,
   -36,    39,   479,   479,-32768,     7,    45,    10,    54,    28,
   -28,    57,    59,    66,-32768,-32768,-32768,    49,    12,    15,
    88,-32768,    16,     6,    21,     3,    22,    84,    26,    27,
    84,    29,    30,    31,    33,    47,    48,    51,    53,    56,
    58,    60,    62,    64,    65,    67,    68,   479,    71,-32768,
-32768,-32768,-32768,    70,   479,    75,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,   479,   479,   293,
    74,   502,    76,    77,-32768,   356,-32768,   -25,   117,   108,
   147,   107,   154,   164,-32768,   122,-32768,   128,-32768,-32768,
-32768,-32768,    87,-32768,   479,-32768,    90,-32768,-32768,    38,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   356,   177,   120,   550,   131,   176,   234,   479,
   479,   479,   479,   479,   443,   479,   479,   479,   479,   479,
   479,   479,   479,   443,   479,   -26,   188,   187,   193,   479,
-32768,   195,-32768,   109,-32768,   152,   188,   110,   356,   -70,
   479,   157,   356,    20,-32768,   -67,-32768,-32768,-32768,   550,
   550,     2,     2,   356,-32768,   151,     2,     2,     2,    -6,
    -6,   176,   176,   -66,   263,   490,   199,   113,-32768,   114,
-32768,-32768,   -30,   520,   126,-32768,   115,   211,   214,   114,
-32768,   -48,-32768,   479,   -44,   216,     5,     5,   206,   177,
   443,   479,-32768,-32768,   201,   208,-32768,   204,-32768,   139,
-32768,   230,   479,   231,   202,   479,   479,   195,     5,-32768,
   -40,   181,   140,   150,   356,-32768,-32768,-32768,-32768,-32768,
-32768,   242,-32768,   443,   527,-32768,   228,-32768,-32768,-32768,
-32768,   205,   215,   558,   356,-32768,-32768,   207,   211,   253,
-32768,-32768,     5,-32768,    11,   443,-32768,-32768,   226,   232,
   443,-32768,-32768,-32768,   173,   179,   209,-32768,-32768,    -3,
   443,-32768,   233,-32768,   325,   265,-32768,   271,   443,-32768,
   272,   252,-32768,   -37,   261,   387,    61,-32768,-32768,   281,
    49,-32768,-32768,-32768,-32768,-32768,   194,-32768,   290,   291,
-32768
};

static const short yypgoto[] = {     0,
  -121,    -1,-32768,-32768,-32768,-32768,-32768,-32768,   138,  -123,
   149,-32768,-32768,   -27,-32768,-32768,-32768,-32768,   -17,-32768,
-32768,    79,-32768,   267,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    94,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    40,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,  -192,    93,-32768,    50,-32768,    32,-32768,-32768
};


#define	YYLAST		650


static const short yytable[] = {   309,
    90,    70,    72,   105,    76,   223,   134,   187,    98,    50,
    51,    52,    53,    54,   134,   241,    55,   263,   289,   184,
   147,   238,   185,    48,   239,    84,   203,    85,   204,   209,
   217,   210,   204,     6,   207,   208,   257,   205,    73,    74,
    50,    51,    52,    53,    54,    49,   123,    55,   233,    97,
   234,    80,   236,   126,   204,    81,   258,   277,   259,   299,
    82,   300,    83,    87,   188,    88,   128,   129,   224,    56,
   276,   277,    89,   148,    57,    58,    59,    60,    61,     6,
    62,    63,    64,    65,   141,   142,    66,    67,   162,   244,
   139,   140,   141,   142,    93,    68,    91,    75,   163,    92,
    56,    95,    69,   303,   304,    57,    58,    59,    60,    61,
    94,    62,    63,    64,    65,    96,    99,    66,    67,   100,
   103,   104,   150,   107,   108,   109,    68,   110,   170,   171,
   172,   173,   174,    69,   177,   178,   179,   180,   181,   182,
   183,   111,   112,   186,   281,   113,    76,   114,   194,   285,
   115,   149,   116,   151,   117,   152,   118,     1,   119,   120,
   153,   121,   122,   125,     2,   225,   124,   296,   127,   143,
   154,   145,   146,   157,     3,   213,   211,   212,   155,     4,
     5,     6,   158,   164,   213,   161,   167,     7,   134,    50,
    51,    52,    53,    54,   189,     8,    55,   168,     9,   192,
    10,   195,   235,    11,   198,   220,   199,   201,   206,   221,
   245,   227,   222,   228,    12,    13,    14,   229,    73,    74,
   232,   163,   237,    15,   254,   255,   242,   212,    16,    17,
   247,   248,    18,    19,    20,   249,   250,   251,   252,   260,
   261,   130,   131,   213,   132,   133,   134,   262,   263,    56,
    21,   267,   268,   269,    57,    58,    59,    60,    61,   275,
    62,    63,    64,    65,   272,   282,    66,    67,   286,     1,
   288,   293,   283,   287,   291,    68,     2,   295,   297,   298,
   213,   301,    69,   307,   213,   218,     3,   306,   308,   310,
   311,     4,     5,     6,   200,   213,   191,   106,   274,     7,
   130,   131,   243,   132,   133,   134,   256,     8,   246,     0,
     9,   290,    10,     0,   278,    11,     0,   135,     0,   136,
   137,   138,   139,   140,   141,   142,    12,    13,    14,     0,
   169,     1,     0,     0,     0,    15,     0,     0,     2,     0,
    16,    17,     0,     0,    18,    19,    20,   292,     3,     0,
     0,     0,     0,     4,     5,     6,     0,     0,     0,     0,
     0,     7,    21,   130,   131,     0,   132,   133,   134,     8,
     0,     0,     9,     0,    10,     0,     0,    11,   136,   137,
   138,   139,   140,   141,   142,     0,     0,     0,    12,    13,
    14,     0,     0,     1,     0,     0,     0,    15,     0,     0,
     2,     0,    16,    17,     0,     0,    18,    19,    20,   302,
     3,     0,     0,     0,     0,     4,     5,     6,     0,     0,
     0,     0,     0,     7,    21,     0,     0,     0,     0,     0,
     0,     8,     0,     0,     9,     0,    10,     0,     0,    11,
     0,   136,   137,   138,   139,   140,   141,   142,     0,     1,
    12,    13,    14,     0,     0,     0,     2,     0,     0,    15,
     0,     0,     0,     0,    16,    17,     3,     0,    18,    19,
    20,     4,     5,     6,     0,     0,     0,     0,     0,     7,
     0,    50,    51,    52,    53,    54,    21,     8,    55,     0,
     9,     0,    10,     0,     0,    11,     0,   130,   131,     0,
   132,   133,   134,     0,     0,     0,    12,    13,    14,   130,
   131,     0,   132,   133,   134,    15,     0,     0,     0,     0,
    16,    17,     0,     0,    18,    19,    20,   130,   131,   144,
   132,   133,   134,     0,   130,   131,     0,   132,   133,   134,
     0,    56,    21,     0,     0,     0,    57,    58,    59,    60,
    61,   266,    62,    63,    64,    65,     0,   226,    66,    67,
   132,   133,   134,     0,     0,   130,   131,    68,   132,   133,
   134,     0,     0,     0,    69,   136,   137,   138,   139,   140,
   141,   142,     0,     0,     0,   271,   219,   136,   137,   138,
   139,   140,   141,   142,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,   136,   137,   138,   139,   140,
   141,   142,   136,   137,   138,   139,   140,   141,   142,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,   136,   137,   138,   139,   140,
   141,   142,     0,   136,   137,   138,   139,   140,   141,   142
};

static const short yycheck[] = {     0,
    18,     3,     4,    31,     6,    36,    13,    34,    26,     3,
     4,     5,     6,     7,    13,   208,    10,     7,    22,   143,
    46,    17,   144,    60,    20,    54,    97,    56,    99,    97,
    97,    99,    99,    31,    15,    16,   229,   161,    32,    33,
     3,     4,     5,     6,     7,     7,    48,    10,    97,    47,
    99,     7,    97,    55,    99,    46,    97,    61,    99,    97,
     7,    99,    35,     7,    91,     7,    68,    69,    99,    63,
   263,    61,     7,    99,    68,    69,    70,    71,    72,    31,
    74,    75,    76,    77,    91,    92,    80,    81,    51,   211,
    89,    90,    91,    92,     7,    89,    85,    91,   100,    85,
    63,    96,    96,    43,    44,    68,    69,    70,    71,    72,
    95,    74,    75,    76,    77,    95,    95,    80,    81,    36,
    95,    95,    15,    95,    95,    95,    89,    95,   130,   131,
   132,   133,   134,    96,   136,   137,   138,   139,   140,   141,
   142,    95,    95,   145,   266,    95,   148,    95,   150,   271,
    95,    35,    95,     7,    95,    49,    95,     7,    95,    95,
     7,    95,    95,    94,    14,   193,    96,   289,    94,    96,
     7,    96,    96,    46,    24,   176,    26,    27,    57,    29,
    30,    31,    96,     7,   185,    96,    67,    37,    13,     3,
     4,     5,     6,     7,     7,    45,    10,    67,    48,     7,
    50,     7,   204,    53,    96,     7,    55,    98,    52,    97,
   212,    86,    99,    99,    64,    65,    66,     7,    32,    33,
     7,   223,     7,    73,   226,   227,    21,    27,    78,    79,
    23,    28,    82,    83,    84,    97,     7,     7,    37,    59,
   101,     8,     9,   244,    11,    12,    13,    98,     7,    63,
   100,    24,    48,    39,    68,    69,    70,    71,    72,     7,
    74,    75,    76,    77,    58,    40,    80,    81,    96,     7,
    62,     7,    41,    95,    42,    89,    14,     7,     7,    28,
   281,    21,    96,   301,   285,    23,    24,     7,    95,     0,
     0,    29,    30,    31,   157,   296,   148,    31,   259,    37,
     8,     9,   210,    11,    12,    13,   228,    45,   215,    -1,
    48,   280,    50,    -1,   265,    53,    -1,    25,    -1,    86,
    87,    88,    89,    90,    91,    92,    64,    65,    66,    -1,
    97,     7,    -1,    -1,    -1,    73,    -1,    -1,    14,    -1,
    78,    79,    -1,    -1,    82,    83,    84,    23,    24,    -1,
    -1,    -1,    -1,    29,    30,    31,    -1,    -1,    -1,    -1,
    -1,    37,   100,     8,     9,    -1,    11,    12,    13,    45,
    -1,    -1,    48,    -1,    50,    -1,    -1,    53,    86,    87,
    88,    89,    90,    91,    92,    -1,    -1,    -1,    64,    65,
    66,    -1,    -1,     7,    -1,    -1,    -1,    73,    -1,    -1,
    14,    -1,    78,    79,    -1,    -1,    82,    83,    84,    23,
    24,    -1,    -1,    -1,    -1,    29,    30,    31,    -1,    -1,
    -1,    -1,    -1,    37,   100,    -1,    -1,    -1,    -1,    -1,
    -1,    45,    -1,    -1,    48,    -1,    50,    -1,    -1,    53,
    -1,    86,    87,    88,    89,    90,    91,    92,    -1,     7,
    64,    65,    66,    -1,    -1,    -1,    14,    -1,    -1,    73,
    -1,    -1,    -1,    -1,    78,    79,    24,    -1,    82,    83,
    84,    29,    30,    31,    -1,    -1,    -1,    -1,    -1,    37,
    -1,     3,     4,     5,     6,     7,   100,    45,    10,    -1,
    48,    -1,    50,    -1,    -1,    53,    -1,     8,     9,    -1,
    11,    12,    13,    -1,    -1,    -1,    64,    65,    66,     8,
     9,    -1,    11,    12,    13,    73,    -1,    -1,    -1,    -1,
    78,    79,    -1,    -1,    82,    83,    84,     8,     9,    28,
    11,    12,    13,    -1,     8,     9,    -1,    11,    12,    13,
    -1,    63,   100,    -1,    -1,    -1,    68,    69,    70,    71,
    72,    25,    74,    75,    76,    77,    -1,    38,    80,    81,
    11,    12,    13,    -1,    -1,     8,     9,    89,    11,    12,
    13,    -1,    -1,    -1,    96,    86,    87,    88,    89,    90,
    91,    92,    -1,    -1,    -1,    28,    97,    86,    87,    88,
    89,    90,    91,    92,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    86,    87,    88,    89,    90,
    91,    92,    86,    87,    88,    89,    90,    91,    92,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    86,    87,    88,    89,    90,
    91,    92,    -1,    86,    87,    88,    89,    90,    91,    92
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 23:
#line 151 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 24:
#line 153 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;
    break;}
case 25:
#line 157 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 26:
#line 159 "pars0grm.y"
{ yyval = pars_func(yyvsp[-3], yyvsp[-1]); ;
    break;}
case 27:
#line 160 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 28:
#line 161 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 29:
#line 162 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 30:
#line 163 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 31:
#line 164 "pars0grm.y"
{ yyval = yyvsp[0];;
    break;}
case 32:
#line 165 "pars0grm.y"
{ yyval = pars_op('+', yyvsp[-2], yyvsp[0]); ;
    break;}
case 33:
#line 166 "pars0grm.y"
{ yyval = pars_op('-', yyvsp[-2], yyvsp[0]); ;
    break;}
case 34:
#line 167 "pars0grm.y"
{ yyval = pars_op('*', yyvsp[-2], yyvsp[0]); ;
    break;}
case 35:
#line 168 "pars0grm.y"
{ yyval = pars_op('/', yyvsp[-2], yyvsp[0]); ;
    break;}
case 36:
#line 169 "pars0grm.y"
{ yyval = pars_op('-', yyvsp[0], NULL); ;
    break;}
case 37:
#line 170 "pars0grm.y"
{ yyval = yyvsp[-1]; ;
    break;}
case 38:
#line 171 "pars0grm.y"
{ yyval = pars_op('=', yyvsp[-2], yyvsp[0]); ;
    break;}
case 39:
#line 172 "pars0grm.y"
{ yyval = pars_op('<', yyvsp[-2], yyvsp[0]); ;
    break;}
case 40:
#line 173 "pars0grm.y"
{ yyval = pars_op('>', yyvsp[-2], yyvsp[0]); ;
    break;}
case 41:
#line 174 "pars0grm.y"
{ yyval = pars_op(PARS_GE_TOKEN, yyvsp[-2], yyvsp[0]); ;
    break;}
case 42:
#line 175 "pars0grm.y"
{ yyval = pars_op(PARS_LE_TOKEN, yyvsp[-2], yyvsp[0]); ;
    break;}
case 43:
#line 176 "pars0grm.y"
{ yyval = pars_op(PARS_NE_TOKEN, yyvsp[-2], yyvsp[0]); ;
    break;}
case 44:
#line 177 "pars0grm.y"
{ yyval = pars_op(PARS_AND_TOKEN, yyvsp[-2], yyvsp[0]); ;
    break;}
case 45:
#line 178 "pars0grm.y"
{ yyval = pars_op(PARS_OR_TOKEN, yyvsp[-2], yyvsp[0]); ;
    break;}
case 46:
#line 179 "pars0grm.y"
{ yyval = pars_op(PARS_NOT_TOKEN, yyvsp[0], NULL); ;
    break;}
case 47:
#line 181 "pars0grm.y"
{ yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;
    break;}
case 48:
#line 183 "pars0grm.y"
{ yyval = pars_op(PARS_NOTFOUND_TOKEN, yyvsp[-2], NULL); ;
    break;}
case 49:
#line 187 "pars0grm.y"
{ yyval = &pars_to_char_token; ;
    break;}
case 50:
#line 188 "pars0grm.y"
{ yyval = &pars_to_number_token; ;
    break;}
case 51:
#line 189 "pars0grm.y"
{ yyval = &pars_to_binary_token; ;
    break;}
case 52:
#line 191 "pars0grm.y"
{ yyval = &pars_binary_to_number_token; ;
    break;}
case 53:
#line 192 "pars0grm.y"
{ yyval = &pars_substr_token; ;
    break;}
case 54:
#line 193 "pars0grm.y"
{ yyval = &pars_concat_token; ;
    break;}
case 55:
#line 194 "pars0grm.y"
{ yyval = &pars_instr_token; ;
    break;}
case 56:
#line 195 "pars0grm.y"
{ yyval = &pars_length_token; ;
    break;}
case 57:
#line 196 "pars0grm.y"
{ yyval = &pars_sysdate_token; ;
    break;}
case 58:
#line 197 "pars0grm.y"
{ yyval = &pars_rnd_token; ;
    break;}
case 59:
#line 198 "pars0grm.y"
{ yyval = &pars_rnd_str_token; ;
    break;}
case 63:
#line 209 "pars0grm.y"
{ yyval = pars_stored_procedure_call(yyvsp[-4]); ;
    break;}
case 64:
#line 214 "pars0grm.y"
{ yyval = pars_procedure_call(yyvsp[-3], yyvsp[-1]); ;
    break;}
case 65:
#line 218 "pars0grm.y"
{ yyval = &pars_replstr_token; ;
    break;}
case 66:
#line 219 "pars0grm.y"
{ yyval = &pars_printf_token; ;
    break;}
case 67:
#line 220 "pars0grm.y"
{ yyval = &pars_assert_token; ;
    break;}
case 68:
#line 224 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 69:
#line 226 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 70:
#line 230 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 71:
#line 231 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 72:
#line 233 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 73:
#line 237 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 74:
#line 238 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]);;
    break;}
case 75:
#line 239 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 76:
#line 243 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 77:
#line 245 "pars0grm.y"
{ yyval = pars_func(&pars_count_token,
				          que_node_list_add_last(NULL,
					    sym_tab_add_int_lit(
						pars_sym_tab_global, 1))); ;
    break;}
case 78:
#line 250 "pars0grm.y"
{ yyval = pars_func(&pars_count_token,
					    que_node_list_add_last(NULL,
						pars_func(&pars_distinct_token,
						     que_node_list_add_last(
								NULL, yyvsp[-1])))); ;
    break;}
case 79:
#line 256 "pars0grm.y"
{ yyval = pars_func(&pars_sum_token,
						que_node_list_add_last(NULL,
									yyvsp[-1])); ;
    break;}
case 80:
#line 262 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 81:
#line 263 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 82:
#line 265 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 83:
#line 269 "pars0grm.y"
{ yyval = pars_select_list(&pars_star_denoter,
								NULL); ;
    break;}
case 84:
#line 272 "pars0grm.y"
{ yyval = pars_select_list(yyvsp[-2], yyvsp[0]); ;
    break;}
case 85:
#line 273 "pars0grm.y"
{ yyval = pars_select_list(yyvsp[0], NULL); ;
    break;}
case 86:
#line 277 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 87:
#line 278 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 88:
#line 282 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 89:
#line 284 "pars0grm.y"
{ yyval = &pars_update_token; ;
    break;}
case 90:
#line 288 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 91:
#line 290 "pars0grm.y"
{ yyval = &pars_consistent_token; ;
    break;}
case 92:
#line 294 "pars0grm.y"
{ yyval = &pars_asc_token; ;
    break;}
case 93:
#line 295 "pars0grm.y"
{ yyval = &pars_asc_token; ;
    break;}
case 94:
#line 296 "pars0grm.y"
{ yyval = &pars_desc_token; ;
    break;}
case 95:
#line 300 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 96:
#line 302 "pars0grm.y"
{ yyval = pars_order_by(yyvsp[-1], yyvsp[0]); ;
    break;}
case 97:
#line 311 "pars0grm.y"
{ yyval = pars_select_statement(yyvsp[-6], yyvsp[-4], yyvsp[-3],
								yyvsp[-2], yyvsp[-1], yyvsp[0]); ;
    break;}
case 98:
#line 317 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 99:
#line 322 "pars0grm.y"
{ yyval = pars_insert_statement(yyvsp[-4], yyvsp[-1], NULL); ;
    break;}
case 100:
#line 324 "pars0grm.y"
{ yyval = pars_insert_statement(yyvsp[-1], NULL, yyvsp[0]); ;
    break;}
case 101:
#line 328 "pars0grm.y"
{ yyval = pars_column_assignment(yyvsp[-2], yyvsp[0]); ;
    break;}
case 102:
#line 332 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 103:
#line 334 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 104:
#line 340 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 105:
#line 346 "pars0grm.y"
{ yyval = pars_update_statement_start(FALSE,
								yyvsp[-2], yyvsp[0]); ;
    break;}
case 106:
#line 352 "pars0grm.y"
{ yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;
    break;}
case 107:
#line 357 "pars0grm.y"
{ yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;
    break;}
case 108:
#line 362 "pars0grm.y"
{ yyval = pars_update_statement_start(TRUE,
								yyvsp[0], NULL); ;
    break;}
case 109:
#line 368 "pars0grm.y"
{ yyval = pars_update_statement(yyvsp[-1], NULL, yyvsp[0]); ;
    break;}
case 110:
#line 373 "pars0grm.y"
{ yyval = pars_update_statement(yyvsp[-1], yyvsp[0], NULL); ;
    break;}
case 111:
#line 378 "pars0grm.y"
{ yyval = pars_row_printf_statement(yyvsp[0]); ;
    break;}
case 112:
#line 383 "pars0grm.y"
{ yyval = pars_assignment_statement(yyvsp[-2], yyvsp[0]); ;
    break;}
case 113:
#line 389 "pars0grm.y"
{ yyval = pars_elsif_element(yyvsp[-2], yyvsp[0]); ;
    break;}
case 114:
#line 393 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 115:
#line 395 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-1], yyvsp[0]); ;
    break;}
case 116:
#line 399 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 117:
#line 401 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 118:
#line 402 "pars0grm.y"
{ yyval = yyvsp[0]; ;
    break;}
case 119:
#line 409 "pars0grm.y"
{ yyval = pars_if_statement(yyvsp[-5], yyvsp[-3], yyvsp[-2]); ;
    break;}
case 120:
#line 415 "pars0grm.y"
{ yyval = pars_while_statement(yyvsp[-4], yyvsp[-2]); ;
    break;}
case 121:
#line 423 "pars0grm.y"
{ yyval = pars_for_statement(yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2]); ;
    break;}
case 122:
#line 427 "pars0grm.y"
{ yyval = pars_return_statement(); ;
    break;}
case 123:
#line 432 "pars0grm.y"
{ yyval = pars_open_statement(
						ROW_SEL_OPEN_CURSOR, yyvsp[0]); ;
    break;}
case 124:
#line 438 "pars0grm.y"
{ yyval = pars_open_statement(
						ROW_SEL_CLOSE_CURSOR, yyvsp[0]); ;
    break;}
case 125:
#line 444 "pars0grm.y"
{ yyval = pars_fetch_statement(yyvsp[-2], yyvsp[0]); ;
    break;}
case 126:
#line 448 "pars0grm.y"
{ yyval = pars_column_def(yyvsp[-1], yyvsp[0]); ;
    break;}
case 127:
#line 452 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 128:
#line 454 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 129:
#line 458 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 130:
#line 460 "pars0grm.y"
{ yyval = &pars_int_token;
					/* pass any non-NULL pointer */ ;
    break;}
case 131:
#line 467 "pars0grm.y"
{ yyval = pars_create_table(yyvsp[-4], yyvsp[-2], yyvsp[0]); ;
    break;}
case 132:
#line 471 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 133:
#line 473 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 134:
#line 477 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 135:
#line 478 "pars0grm.y"
{ yyval = &pars_unique_token; ;
    break;}
case 136:
#line 482 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 137:
#line 483 "pars0grm.y"
{ yyval = &pars_clustered_token; ;
    break;}
case 138:
#line 491 "pars0grm.y"
{ yyval = pars_create_index(yyvsp[-8], yyvsp[-7], yyvsp[-5], yyvsp[-3], yyvsp[-1]); ;
    break;}
case 139:
#line 496 "pars0grm.y"
{ yyval = pars_commit_statement(); ;
    break;}
case 140:
#line 501 "pars0grm.y"
{ yyval = pars_rollback_statement(); ;
    break;}
case 141:
#line 505 "pars0grm.y"
{ yyval = &pars_int_token; ;
    break;}
case 142:
#line 506 "pars0grm.y"
{ yyval = &pars_char_token; ;
    break;}
case 143:
#line 511 "pars0grm.y"
{ yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_INPUT, yyvsp[0]); ;
    break;}
case 144:
#line 514 "pars0grm.y"
{ yyval = pars_parameter_declaration(yyvsp[-2],
							PARS_OUTPUT, yyvsp[0]); ;
    break;}
case 145:
#line 519 "pars0grm.y"
{ yyval = NULL; ;
    break;}
case 146:
#line 520 "pars0grm.y"
{ yyval = que_node_list_add_last(NULL, yyvsp[0]); ;
    break;}
case 147:
#line 522 "pars0grm.y"
{ yyval = que_node_list_add_last(yyvsp[-2], yyvsp[0]); ;
    break;}
case 148:
#line 527 "pars0grm.y"
{ yyval = pars_variable_declaration(yyvsp[-2], yyvsp[-1]); ;
    break;}
case 152:
#line 539 "pars0grm.y"
{ yyval = pars_cursor_declaration(yyvsp[-3], yyvsp[-1]); ;
    break;}
case 156:
#line 555 "pars0grm.y"
{ yyval = pars_procedure_definition(yyvsp[-9], yyvsp[-7],
								yyvsp[-1]); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 559 "pars0grm.y"

