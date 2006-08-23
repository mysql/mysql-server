/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*

The idea of presented algorithm see in 
"The Art of Computer Programming" by Donald E. Knuth
Volume 3 "Sorting and searching"
(chapter 6.3 "Digital searching" - name and number of chapter 
   is back translation from Russian edition :))

as illustration of data structures, imagine next table:

static SYMBOL symbols[] = {
  { "ADD",              SYM(ADD),0,0},
  { "AND",              SYM(AND),0,0},
  { "DAY",              SYM(DAY_SYM),0,0},
};

for this structure, presented program generate next searching-structure:

+-----------+-+-+-+
|       len |1|2|3|
+-----------+-+-+-+
|first_char |0|0|a|
|last_char  |0|0|d|
|link       |0|0|+|
                 |
                 V
       +----------+-+-+-+--+
       |    1 char|a|b|c|d |
       +----------+-+-+-+--+
       |first_char|b|0|0|0 |
       |last_char |n|0|0|-1|
       |link      |+|0|0|+ |
                   |     |
                   |     V
                   |  symbols[2] ( "DAY" )
                   V
+----------+--+-+-+-+-+-+-+-+-+-+--+
|    2 char|d |e|f|j|h|i|j|k|l|m|n |
+----------+--+-+-+-+-+-+-+-+-+-+--+
|first_char|0 |0|0|0|0|0|0|0|0|0|0 |
|last_char |-1|0|0|0|0|0|0|0|0|0|-1|
|link      |+ |0|0|0|0|0|0|0|0|0|+ |
            |                    |
            V                    V
         symbols[0] ( "ADD" )  symbols[1] ( "AND" )

for optimization, link is the 16-bit index in 'symbols' or 'sql_functions'
or search-array..

So, we can read full search-structure as 32-bit word

TODO:
1. use instead to_upper_lex, special array 
   (substitute chars) without skip codes..
2. try use reverse order of comparing..
       
*/

#define NO_YACC_SYMBOLS
#include "my_global.h"
#include "my_sys.h"
#include "m_string.h"
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skip warnings in getopt.h
#endif
#include <my_getopt.h>
#include "mysql_version.h"
#include "lex.h"

const char *default_dbug_option="d:t:o,/tmp/gen_lex_hash.trace";

struct my_option my_long_options[] =
{
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"help", '?', "Display help and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

struct hash_lex_struct
{
  int first_char;
  char last_char;
  union{
    hash_lex_struct *char_tails;
    int iresult;
  };
  int ithis;
};

hash_lex_struct *get_hash_struct_by_len(hash_lex_struct **root_by_len,
					    int len, int *max_len)
{
  if (*max_len<len){
    *root_by_len= (hash_lex_struct *)realloc((char*)*root_by_len,
                                             sizeof(hash_lex_struct)*len);
    hash_lex_struct *cur, *end= *root_by_len + len;
    for (cur= *root_by_len + *max_len; cur<end; cur++)
      cur->first_char= 0;
    *max_len= len;
  }
  return (*root_by_len)+(len-1);
}

void insert_into_hash(hash_lex_struct *root, const char *name, 
		      int len_from_begin, int index, int function)
{
  hash_lex_struct *end, *cur, *tails;

  if (!root->first_char)
  {
    root->first_char= -1;
    root->iresult= index;
    return;
  }

  if (root->first_char == -1)
  {
    int index2= root->iresult;
    const char *name2= (index2 < 0 ? sql_functions[-index2-1] :
			symbols[index2]).name + len_from_begin;
    root->first_char= (int) (uchar) name2[0];
    root->last_char= (char) root->first_char;
    tails= (hash_lex_struct*)malloc(sizeof(hash_lex_struct));
    root->char_tails= tails;
    tails->first_char= -1;
    tails->iresult= index2;
  }

  size_t real_size= (root->last_char-root->first_char+1);

  if (root->first_char>(*name))
  {
    size_t new_size= root->last_char-(*name)+1;
    if (new_size<real_size) printf("error!!!!\n");
    tails= root->char_tails;
    tails= (hash_lex_struct*)realloc((char*)tails,
				       sizeof(hash_lex_struct)*new_size);
    root->char_tails= tails;
    memmove(tails+(new_size-real_size),tails,real_size*sizeof(hash_lex_struct));
    end= tails + new_size - real_size;
    for (cur= tails; cur<end; cur++)
      cur->first_char= 0;
    root->first_char= (int) (uchar) *name;
  }

  if (root->last_char<(*name))
  {
    size_t new_size= (*name)-root->first_char+1;
    if (new_size<real_size) printf("error!!!!\n");
    tails= root->char_tails;
    tails= (hash_lex_struct*)realloc((char*)tails,
				    sizeof(hash_lex_struct)*new_size);
    root->char_tails= tails;
    end= tails + new_size;
    for (cur= tails+real_size; cur<end; cur++)
      cur->first_char= 0;
    root->last_char= (*name);
  }

  insert_into_hash(root->char_tails+(*name)-root->first_char,
		   name+1,len_from_begin+1,index,function);
}


hash_lex_struct *root_by_len= 0;
int max_len=0;

hash_lex_struct *root_by_len2= 0;
int max_len2=0;

void insert_symbols()
{
  size_t i= 0;
  SYMBOL *cur;
  for (cur= symbols; i<array_elements(symbols); cur++, i++){
    hash_lex_struct *root= 
      get_hash_struct_by_len(&root_by_len,cur->length,&max_len);
    insert_into_hash(root,cur->name,0,i,0);
  }
}

void insert_sql_functions()
{
  size_t i= 0;
  SYMBOL *cur;
  for (cur= sql_functions; i<array_elements(sql_functions); cur++, i++){
    hash_lex_struct *root= 
      get_hash_struct_by_len(&root_by_len,cur->length,&max_len);
    insert_into_hash(root,cur->name,0,-i-1,1);
  }
}

void calc_length()
{
  SYMBOL *cur, *end= symbols + array_elements(symbols);
  for (cur= symbols; cur < end; cur++)
    cur->length=(uchar) strlen(cur->name);
  end= sql_functions + array_elements(sql_functions);
  for (cur= sql_functions; cur<end; cur++)
    cur->length=(uchar) strlen(cur->name);
}

void generate_find_structs()
{
  root_by_len= 0;
  max_len=0;

  insert_symbols();

  root_by_len2= root_by_len;
  max_len2= max_len;

  root_by_len= 0;
  max_len= 0;

  insert_symbols();
  insert_sql_functions();
}

char *hash_map= 0;
int size_hash_map= 0;

void add_struct_to_map(hash_lex_struct *st)
{
  st->ithis= size_hash_map/4;
  size_hash_map+= 4;
  hash_map= (char*)realloc((char*)hash_map,size_hash_map);
  hash_map[size_hash_map-4]= (char) (st->first_char == -1 ? 0 :
				     st->first_char);
  hash_map[size_hash_map-3]= (char) (st->first_char == -1 ||
				     st->first_char == 0 ? 0 : st->last_char);
  if (st->first_char == -1)
  {
    hash_map[size_hash_map-2]= ((unsigned int)(int16)st->iresult)&255;
    hash_map[size_hash_map-1]= ((unsigned int)(int16)st->iresult)>>8;
  }
  else if (st->first_char == 0)
  {
    hash_map[size_hash_map-2]= ((unsigned int)(int16)array_elements(symbols))&255;
    hash_map[size_hash_map-1]= ((unsigned int)(int16)array_elements(symbols))>>8;
  }
}


void add_structs_to_map(hash_lex_struct *st, int len)
{
  hash_lex_struct *cur, *end= st+len;
  for (cur= st; cur<end; cur++)
    add_struct_to_map(cur);
  for (cur= st; cur<end; cur++)
  {
    if (cur->first_char && cur->first_char != -1)
      add_structs_to_map(cur->char_tails,cur->last_char-cur->first_char+1);
  }
}

void set_links(hash_lex_struct *st, int len)
{
  hash_lex_struct *cur, *end= st+len;
  for (cur= st; cur<end; cur++)
  {
    if (cur->first_char != 0 && cur->first_char != -1)
    {
      int ilink= cur->char_tails->ithis;
      hash_map[cur->ithis*4+2]= ilink%256;
      hash_map[cur->ithis*4+3]= ilink/256;
      set_links(cur->char_tails,cur->last_char-cur->first_char+1);
    }
  }
}


void print_hash_map(const char *name)
{
  char *cur;
  int i;

  printf("static uchar %s[%d]= {\n",name,size_hash_map);
  for (i=0, cur= hash_map; i<size_hash_map; i++, cur++)
  {
    switch(i%4){
    case 0: case 1:
      if (!*cur)
	printf("0,   ");
      else
	printf("\'%c\', ",*cur);
      break;
    case 2: printf("%u, ",(uint)(uchar)*cur); break;
    case 3: printf("%u,\n",(uint)(uchar)*cur); break;
    }
  }
  printf("};\n");
}


void print_find_structs()
{
  add_structs_to_map(root_by_len,max_len);
  set_links(root_by_len,max_len);
  print_hash_map("sql_functions_map");

  hash_map= 0;
  size_hash_map= 0;

  printf("\n");

  add_structs_to_map(root_by_len2,max_len2);
  set_links(root_by_len2,max_len2);
  print_hash_map("symbols_map");
}


static void usage(int version)
{
  printf("%s  Ver 3.6 Distrib %s, for %s (%s)\n",
	 my_progname, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  if (version)
    return;
  puts("Copyright (C) 2001 MySQL AB, by VVA and Monty");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
and you are welcome to modify and redistribute it under the GPL license\n");
  puts("This program generates a perfect hashing function for the sql_lex.cc");
  printf("Usage: %s [OPTIONS]\n\n", my_progname);
  my_print_help(my_long_options);
}


extern "C" my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    usage(1);
    exit(0);
  case 'I':
  case '?':
    usage(0);
    exit(0);
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
  }
  return 0;
}


static int get_options(int argc, char **argv)
{
  int ho_error;

  if ((ho_error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (argc >= 1)
  {
    usage(0);
     exit(1);
  }
  return(0);
}


int check_dup_symbols(SYMBOL *s1, SYMBOL *s2)
{
  if (s1->length!=s2->length || strncmp(s1->name,s2->name,s1->length))
    return 0;

  const char *err_tmpl= "\ngen_lex_hash fatal error : \
Unfortunately gen_lex_hash can not generate a hash,\n since \
your lex.h has duplicate definition for a symbol \"%s\"\n\n";
  printf (err_tmpl,s1->name);
  fprintf (stderr,err_tmpl,s1->name);

  return 1;
}


int check_duplicates()
{
  SYMBOL *cur1, *cur2, *s_end, *f_end;

  s_end= symbols + array_elements(symbols);
  f_end= sql_functions + array_elements(sql_functions);

  for (cur1= symbols; cur1<s_end; cur1++)
  {
    for (cur2= cur1+1; cur2<s_end; cur2++)
    {
      if (check_dup_symbols(cur1,cur2))
	return 1;
    }
    for (cur2= sql_functions; cur2<f_end; cur2++)
    {
      if (check_dup_symbols(cur1,cur2))
	return 1;
    }
  }

  for (cur1= sql_functions; cur1<f_end; cur1++)
  {
    for (cur2= cur1+1; cur2< f_end; cur2++)
    {
      if (check_dup_symbols(cur1,cur2))
	return 1;
    }
  }
  return 0;
}


int main(int argc,char **argv)
{
  MY_INIT(argv[0]);
  DBUG_PROCESS(argv[0]);

  if (get_options(argc,(char **) argv))
    exit(1);

  printf("/* Copyright (C) 2001-2004 MySQL AB\n\
   This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n\
   and you are welcome to modify and redistribute it under the GPL license\n\
   \n*/\n\n");

  /* Broken up to indicate that it's not advice to you, gentle reader. */
  printf("/* Do " "not " "edit " "this " "file!  This is generated by "
         "gen_lex_hash.cc\nthat seeks for a perfect hash function */\n\n");
  printf("#include \"lex.h\"\n\n");

  calc_length();

  if (check_duplicates())
    exit(1);

  generate_find_structs();
  print_find_structs();

  printf("\nstatic unsigned int sql_functions_max_len=%d;\n", max_len);
  printf("\nstatic unsigned int symbols_max_len=%d;\n\n", max_len2);

  printf("\
static inline SYMBOL *get_hash_symbol(const char *s,\n\
                                    unsigned int len,bool function)\n\
{\n\
  register uchar *hash_map;\n\
  register const char *cur_str= s;\n\
\n\
  if (len == 0) {\n\
    DBUG_PRINT(\"warning\", (\"get_hash_symbol() received a request for a zero-length symbol, which is probably a mistake.\"));\
    return(NULL);\n\
  }\n"
);

  printf("\
  if (function){\n\
    if (len>sql_functions_max_len) return 0;\n\
    hash_map= sql_functions_map;\n\
    register uint32 cur_struct= uint4korr(hash_map+((len-1)*4));\n\
\n\
    for (;;){\n\
      register uchar first_char= (uchar)cur_struct;\n\
\n\
      if (first_char == 0)\n\
      {\n\
        register int16 ires= (int16)(cur_struct>>16);\n\
        if (ires==array_elements(symbols)) return 0;\n\
        register SYMBOL *res;\n\
        if (ires>=0) \n\
          res= symbols+ires;\n\
        else\n\
          res= sql_functions-ires-1;\n\
        register uint count= cur_str-s;\n\
        return lex_casecmp(cur_str,res->name+count,len-count) ? 0 : res;\n\
      }\n\
\n\
      register uchar cur_char= (uchar)to_upper_lex[(uchar)*cur_str];\n\
      if (cur_char<first_char) return 0;\n\
      cur_struct>>=8;\n\
      if (cur_char>(uchar)cur_struct) return 0;\n\
\n\
      cur_struct>>=8;\n\
      cur_struct= uint4korr(hash_map+\n\
                        (((uint16)cur_struct + cur_char - first_char)*4));\n\
      cur_str++;\n\
    }\n"
);

  printf("\
  }else{\n\
    if (len>symbols_max_len) return 0;\n\
    hash_map= symbols_map;\n\
    register uint32 cur_struct= uint4korr(hash_map+((len-1)*4));\n\
\n\
    for (;;){\n\
      register uchar first_char= (uchar)cur_struct;\n\
\n\
      if (first_char==0){\n\
        register int16 ires= (int16)(cur_struct>>16);\n\
        if (ires==array_elements(symbols)) return 0;\n\
        register SYMBOL *res= symbols+ires;\n\
        register uint count= cur_str-s;\n\
        return lex_casecmp(cur_str,res->name+count,len-count)!=0 ? 0 : res;\n\
      }\n\
\n\
      register uchar cur_char= (uchar)to_upper_lex[(uchar)*cur_str];\n\
      if (cur_char<first_char) return 0;\n\
      cur_struct>>=8;\n\
      if (cur_char>(uchar)cur_struct) return 0;\n\
\n\
      cur_struct>>=8;\n\
      cur_struct= uint4korr(hash_map+\n\
                        (((uint16)cur_struct + cur_char - first_char)*4));\n\
      cur_str++;\n\
    }\n\
  }\n\
}\n"
);
  my_end(0);
  exit(0);
}

