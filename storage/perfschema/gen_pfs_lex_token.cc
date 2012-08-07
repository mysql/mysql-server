/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* We only need the tokens here */
#define YYSTYPE_IS_DECLARED
#include <../sql/sql_yacc.h>
#include <lex.h>

#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/*
  This is a tool used during build only,
  so MY_MAX_TOKEN does not need to be exact,
  only big enough to hold:
  - 256 character terminal tokens
  - YYNTOKENS named terminal tokens
  from bison.
  See also YYMAXUTOK.
*/
#define MY_MAX_TOKEN 1000
struct gen_lex_token_string
{
  const char *m_token_string;
  int m_token_length;
};

gen_lex_token_string compiled_token_array[MY_MAX_TOKEN];
int max_token_seen= 0;

char char_tokens[256];

int tok_pfs_generic_value= 0;
int tok_pfs_generic_value_list= 0;
int tok_pfs_row_single_value= 0;
int tok_pfs_row_single_value_list= 0;
int tok_pfs_row_multiple_value= 0;
int tok_pfs_row_multiple_value_list= 0;
int tok_pfs_unused= 0;

void set_token(int tok, const char *str)
{
  if (tok <= 0)
  {
    fprintf(stderr, "Bad token found\n");
    exit(1);
  }

  if (tok > max_token_seen)
  {
    max_token_seen= tok;
  }

  if (max_token_seen >= MY_MAX_TOKEN)
  {
    fprintf(stderr, "Added that many new keywords ? Increase MY_MAX_TOKEN\n");
    exit(1);
  }

  compiled_token_array[tok].m_token_string= str;
  compiled_token_array[tok].m_token_length= strlen(str);
}

void compute_tokens()
{
  int tok;
  unsigned int i;
  char *str;

  /*
    Default value.
  */
  for (tok= 0; tok < MY_MAX_TOKEN; tok++)
  {
    compiled_token_array[tok].m_token_string= "(unknown)";
    compiled_token_array[tok].m_token_length= 9;
  }

  /*
    Tokens made of just one terminal character
  */
  for (tok=0; tok < 256; tok++)
  {
    str= & char_tokens[tok];
    str[0]= (char) tok;
    compiled_token_array[tok].m_token_string= str;
    compiled_token_array[tok].m_token_length= 1;
  }

  max_token_seen= 255;

  /*
    String terminal tokens, used in sql_yacc.yy
  */
  set_token(NEG, "~");
  set_token(TABLE_REF_PRIORITY, "TABLE_REF_PRIORITY");

  /*
    Tokens hard coded in sql_lex.cc
  */

  set_token(WITH_CUBE_SYM, "WITH CUBE");
  set_token(WITH_ROLLUP_SYM, "WITH ROLLUP");
  set_token(NOT2_SYM, "!");
  set_token(OR2_SYM, "|");
  set_token(PARAM_MARKER, "?");
  set_token(SET_VAR, ":=");
  set_token(UNDERSCORE_CHARSET, "(_charset)");
  set_token(END_OF_INPUT, "");

  /*
    Values.
    These tokens are all normalized later,
    so this strings will never be displayed.
  */
  set_token(BIN_NUM, "(bin)");
  set_token(DECIMAL_NUM, "(decimal)");
  set_token(FLOAT_NUM, "(float)");
  set_token(HEX_NUM, "(hex)");
  set_token(LEX_HOSTNAME, "(hostname)");
  set_token(LONG_NUM, "(long)");
  set_token(NUM, "(num)");
  set_token(TEXT_STRING, "(text)");
  set_token(NCHAR_STRING, "(nchar)");
  set_token(ULONGLONG_NUM, "(ulonglong)");

  /*
    Identifiers.
  */
  set_token(IDENT, "(id)");
  set_token(IDENT_QUOTED, "(id_quoted)");

  /*
    Unused tokens
  */
  set_token(LOCATOR_SYM, "LOCATOR");
  set_token(SERVER_OPTIONS, "SERVER_OPTIONS");
  set_token(UDF_RETURNS_SYM, "UDF_RETURNS");

  /*
    See symbols[] in sql/lex.h
  */
  for (i= 0; i< sizeof(symbols)/sizeof(symbols[0]); i++)
  {
    set_token(symbols[i].tok, symbols[i].name);
  }

  /*
    See sql_functions[] in sql/lex.h
  */
  for (i= 0; i< sizeof(sql_functions)/sizeof(sql_functions[0]); i++)
  {
    set_token(sql_functions[i].tok, sql_functions[i].name);
  }

  /*
    Additional FAKE tokens,
    used internally to normalize a digest text.
  */

  max_token_seen++;
  tok_pfs_generic_value= max_token_seen;
  set_token(tok_pfs_generic_value, "?");

  max_token_seen++;
  tok_pfs_generic_value_list= max_token_seen;
  set_token(tok_pfs_generic_value_list, "?, ...");

  max_token_seen++;
  tok_pfs_row_single_value= max_token_seen;
  set_token(tok_pfs_row_single_value, "(?)");

  max_token_seen++;
  tok_pfs_row_single_value_list= max_token_seen;
  set_token(tok_pfs_row_single_value_list, "(?) /* , ... */");

  max_token_seen++;
  tok_pfs_row_multiple_value= max_token_seen;
  set_token(tok_pfs_row_multiple_value, "(...)");

  max_token_seen++;
  tok_pfs_row_multiple_value_list= max_token_seen;
  set_token(tok_pfs_row_multiple_value_list, "(...) /* , ... */");

  max_token_seen++;
  tok_pfs_unused= max_token_seen;
  set_token(tok_pfs_unused, "UNUSED");
}

void print_tokens()
{
  int tok;

  printf("lex_token_string lex_token_array[]=\n");
  printf("{\n");
  printf("/* PART 1: character tokens. */\n");

  for (tok= 0; tok<256; tok++)
  {
    printf("/* %03d */  { \"\\x%02x\", 1},\n", tok, tok);
  }

  printf("/* PART 2: named tokens. */\n");

  for (tok= 256; tok<= max_token_seen; tok++)
  {
    printf("/* %03d */  { \"%s\", %d},\n",
           tok,
           compiled_token_array[tok].m_token_string,
           compiled_token_array[tok].m_token_length);
  }

  printf("/* DUMMY */ { \"\", 0}\n");
  printf("};\n");

  printf("/* PFS specific tokens. */\n");
  printf("#define TOK_PFS_GENERIC_VALUE %d\n", tok_pfs_generic_value);
  printf("#define TOK_PFS_GENERIC_VALUE_LIST %d\n", tok_pfs_generic_value_list);
  printf("#define TOK_PFS_ROW_SINGLE_VALUE %d\n", tok_pfs_row_single_value);
  printf("#define TOK_PFS_ROW_SINGLE_VALUE_LIST %d\n", tok_pfs_row_single_value_list);
  printf("#define TOK_PFS_ROW_MULTIPLE_VALUE %d\n", tok_pfs_row_multiple_value);
  printf("#define TOK_PFS_ROW_MULTIPLE_VALUE_LIST %d\n", tok_pfs_row_multiple_value_list);
  printf("#define TOK_PFS_UNUSED %d\n", tok_pfs_unused);
}

int main(int argc,char **argv)
{
  puts("/*");
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2011"));
  puts("*/");

  printf("/*\n");
  printf("  This file is generated, do not edit.\n");
  printf("  See file storage/perfschema/gen_pfs_lex_token.cc.\n");
  printf("*/\n");
  printf("struct lex_token_string\n");
  printf("{\n");
  printf("  const char *m_token_string;\n");
  printf("  int m_token_length;\n");
  printf("};\n");
  printf("typedef struct lex_token_string lex_token_string;\n");

  compute_tokens();
  print_tokens();

  return 0;
}

