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


#define NO_YACC_SYMBOLS
#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skipp warnings in getopt.h
#endif
#include <getopt.h>
#include "mysql_version.h"
#include "lex.h"

bool opt_search=0,opt_verbose=0;
ulong opt_count=100000;

#define max_allowed_array  8000	// Don't generate bigger arrays than this
#define max_symbol	  32767	// Use this for 'not found'
#define how_much_for_plus  8	// 2-8
#define type_count	   1	// 1-5
#define char_table_count   5
#define total_symbols  (sizeof(symbols)/sizeof(SYMBOL) +\
			sizeof(sql_functions)/sizeof(SYMBOL))

#define how_much_and INT_MAX24

/*
  The following only have to work with characters in the set
  used by SQL commands
*/

#undef tolower
#define tolower(a) ((a) >= 'A' && (a) <= 'Z') ? ((a)- 'A' + 'a') : (a)

static uint how_long_symbols,function_plus,function_mod,function_type;
static uint char_table[256];
static uchar unique_length[256];
static uchar bits[how_much_and/8+1];
static uint primes[max_allowed_array+1];
static ulong hash_results[type_count][how_much_for_plus+1][total_symbols];
static ulong start_value=0;

struct rand_struct {
  unsigned long seed1,seed2,max_value;
  double max_value_dbl;
};

void randominit(struct rand_struct *rand_st,ulong seed1, ulong seed2)
{						/* For mysql 3.21.# */
  rand_st->max_value= 0x3FFFFFFFL;
  rand_st->max_value_dbl=(double) rand_st->max_value;
  rand_st->seed1=seed1%rand_st->max_value ;
  rand_st->seed2=seed2%rand_st->max_value;
}

double rnd(struct rand_struct *rand_st)
{
  rand_st->seed1=(rand_st->seed1*3+rand_st->seed2) % rand_st->max_value;
  rand_st->seed2=(rand_st->seed1+rand_st->seed2+33) % rand_st->max_value;
  return (((double) rand_st->seed1)/rand_st->max_value_dbl);
}


static void make_char_table(ulong t1,ulong t2,int type)
{
  uint i;
  struct rand_struct rand_st;
  randominit(&rand_st,t1,t2);

  for (i=0 ; i < 256 ; i++)
  {
    switch (type) {
    case 0: char_table[i]= i + (i << 8);		break;
    case 1: char_table[i]= i + ((i ^255 ) << 8);	break;
    case 2: char_table[i]= i;				break;
    case 3: char_table[i]= i + ((uint) (rnd(&rand_st)*255) << 8); break;
    case 4: char_table[i]= (uint) (rnd(&rand_st)*255) + (i << 8); break;
    }
  }
  char_table[0]|=1+257;				// Avoid problems with 0
  for (i=0 ; i < 256 ; i++)
  {
    uint tmp=(uint) (rnd(&rand_st)*255);
    swap(uint,char_table[i],char_table[tmp]);
  }
  /* lower characters should be mapped to upper */
  for (i= 'a' ; i <= 'z' ; i++)
  {
    /* This loop is coded with extra variables to avoid a bug in gcc 2.96 */
    uchar tmp= (uchar) (i - 'a');	// Assume ascii
    tmp+='A';
    char_table[i]=char_table[tmp];
  }
}

/* Fill array primes with primes between start and 'max_allowed_array' */

static void make_prime_array(uint start)
{
  uint i,j,*to;
  uint max_index=(uint) sqrt((double) max_allowed_array);

  bzero((char*) primes,sizeof(primes[0])*max_allowed_array);

  i=2;
  while (i < max_index)
  {
    for (j=i+i ; j <= max_allowed_array ; j+=i)
      primes[j]=1;
    while (primes[++i]) ;
  }

  to=primes;
  for (i=start ; i <= max_allowed_array ; i++)
    if (!primes[i])
      *to++=i;
  *to=0;					// end marker
}

#define USE_char_table

static ulong tab_index_function(const char *s,uint add, uint type)
{
  register ulong nr=start_value+char_table[(uchar) *s]; // Nice value
  ulong pos=3;
  uint tmp_length=unique_length[(uchar) *s]-1;
  while (*++s && tmp_length-- > 0)
  {
    switch (type) {
    case 0:
      nr= (nr ^ (char_table[(uchar) *s] + (nr << add)));
      break;
    case 1:
      nr= (nr + (char_table[(uchar) *s] + (nr << add)));
      break;
    case 2:
      nr= (nr ^ (char_table[(uchar) *s] ^ (nr << add)));
      break;
    case 3:
      nr= (char_table[(uchar) *s] ^ (nr << add));
      break;
    case 4:
      nr+= nr+nr+((nr & 63)+pos)*((ulong) char_table[(uchar) *s]);
      pos+=add;
      break;
    }
  }
  return nr & INT_MAX24;
}

static int search(bool write_warning)
{
  uint size_symbols = sizeof(symbols)/sizeof(SYMBOL);
  uint size_functions = sizeof(sql_functions)/sizeof(SYMBOL);
  uint size=size_symbols + size_functions;
  uint i=0,found,*prime,type;
  int igra[max_allowed_array],test_count=INT_MAX;
  uint possible_plus[how_much_for_plus*type_count+type_count];

  how_long_symbols = sizeof(symbols)/sizeof(SYMBOL);

  bzero((char*) possible_plus,sizeof(possible_plus));
  found=0;

  /* Check first which function_plus are possible */
  for (type=0 ; type < type_count ; type ++)
  {
    for (function_plus = 1;
	 function_plus <= how_much_for_plus;
	 function_plus++)
    {
      bzero((char*) bits,sizeof(bits));
      for (i=0; i < size; i++)
      {
	ulong order= tab_index_function ((i < how_long_symbols) ?
					 symbols[i].name :
					 sql_functions[i-how_long_symbols].name,
					 function_plus, type);
	hash_results[type][function_plus][i]=order;
	uint pos=order/8;
	uint bit=order & 7;
	if (bits[pos] & (1 << bit))
	  break;
	bits[pos]|=1 << bit;
      }
      if (i == size)
      {
	possible_plus[found++]=function_plus;
      }
    }
    possible_plus[found++]=0;			// End marker
  }
  if (found == type_count)
  {
    if (write_warning)
      fprintf(stderr,"\
The hash function didn't return a unique value for any parameter\n\
You have to change gen_lex_code.cc, function 'tab_index_function' to\n\
generate unique values for some parameter.  When you have succeeded in this,\n\
you have to change 'main' to print out the new function\n");
    return(1);
  }

  if (opt_verbose)
    fprintf (stderr,"Info: Possible add values: %d\n",found-type_count);

  for (prime=primes; (function_mod=*prime) ; prime++)
  {
    uint *plus_ptr=possible_plus;
    for (type=0 ; type < type_count ; type++ )
    {
      while ((function_plus= *plus_ptr++))
      {
	ulong *order_pos= &hash_results[type][function_plus][0];
	if (test_count++ == INT_MAX)
	{
	  test_count=1;
	  bzero((char*) igra,sizeof(igra));
	}
	for (i=0; i<size ;i++)
	{
	  ulong order;
	  order = *order_pos++ % function_mod;
	  if (igra[order] == test_count)
	    break;
	  igra[order] = test_count;
	}
	if (i == size)
	{
	  *prime=0;				// Mark this used
	  function_type=type;
	  return 0;				// Found ok value
	}
      }
    }
  }

  function_mod=max_allowed_array;
  if (write_warning)
    fprintf (stderr,"Fatal error when generating hash for symbols\n\
Didn't find suitable values for perfect hashing:\n\
You have to edit gen_lex_hase.cc to generate a new hashing function.\n\
You can try running gen_lex_hash with --search to find a suitable value\n\
Symbol array size = %d\n",function_mod);
  return -1;
}


void print_arrays()
{
  uint size_symbols = sizeof(symbols)/sizeof(SYMBOL);
  uint size_functions = sizeof(sql_functions)/sizeof(SYMBOL);
  uint size=size_symbols + size_functions;
  uint i;

  fprintf(stderr,"Symbols: %d  Functions: %d;  Total: %d\nShifts per char: %d,  Array size: %d\n",
	  size_symbols,size_functions,size_symbols+size_functions,
	  function_plus,function_mod);

  int *prva= (int*) my_alloca(sizeof(int)*function_mod);
  for (i=0 ; i <= function_mod; i++)
    prva[i]= max_symbol;

  for (i=0;i<size;i++)
  {
    ulong order = tab_index_function ((i < how_long_symbols) ? symbols[i].name : sql_functions[i - how_long_symbols].name,function_plus,function_type);
    order %= function_mod;
    prva [order] = i;
  }

#ifdef USE_char_table
  printf("static uint16 char_table[] = {\n");
  for (i=0; i < 255 ;i++)			// < 255 is correct
  {
    printf("%u,",char_table[i]);
    if (((i+1) & 15) == 0)
      puts("");
  }
  printf("%d\n};\n\n\n",char_table[i]);
#endif

  printf("static uchar unique_length[] = {\n");
  for (i=0; i < 255 ;i++)			// < 255 is correct
  {
    printf("%u,",unique_length[i]);
    if (((i+1) & 15) == 0)
      puts("");
  }
  printf("%d\n};\n\n\n",unique_length[i]);

  printf("static uint16 my_function_table[] = {\n");
  for (i=0; i < function_mod-1 ;i++)
  {
    printf("%d,",prva[i]);
    if (((i+1) % 12) == 0)
      puts("");
  }
  printf("%d\n};\n\n\n",prva[i]);
  my_afree((gptr) prva);
}


static struct option long_options[] =
{
  {"count",	    required_argument,	   0, 'c'},
  {"search",	    no_argument,	   0, 'S'},
  {"verbose",	    no_argument,	   0, 'v'},
  {"version",	    no_argument,	   0, 'V'},
  {"rnd1",	    required_argument,	   0, 'r'},
  {"rnd2",	    required_argument,	   0, 'R'},
  {"type",	    required_argument,	   0, 't'},
  {0, 0, 0, 0}
};


static void usage(int version)
{
  printf("%s  Ver 3.2 Distrib %s, for %s (%s)\n",
	 my_progname, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  if (version)
    return;
  puts("Copyright (C) 2000 MySQL AB & MySQL Finland AB, by Sinisa and Monty");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("This program generates a perfect hashing function for the sql_lex.cc");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  printf("\n\
-c, --count=#		Try count times to find a optimal hash table\n\
-r, --rnd1=#		Set 1 part of rnd value for hash generator\n\
-R, --rnd2=#		Set 2 part of rnd value for hash generator\n\
-t, --type=#		Set type of char table to generate\n\
-S, --search		Search after good rnd1 and rnd2 values\n\
-v, --verbose		Write some information while the program executes\n\
-V, --version		Output version information and exit\n");

}

static uint best_type;
static ulong best_t1,best_t2, best_start_value;

static int get_options(int argc, char **argv)
{
  int c,option_index=0;

  while ((c=getopt_long(argc,argv,"?SvVc:r:R:t:",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'c':
      opt_count=atol(optarg);
      break;
    case 'r':
      best_t1=atol(optarg);
      break;
    case 'R':
      best_t2=atol(optarg);
      break;
    case 't':
      best_type=atoi(optarg);
      break;
    case 'S':
      opt_search=1;
      break;
    case 'v':
      opt_verbose=1;
      break;
    case 'V': usage(1); exit(0);
    case 'I':
    case '?':
      usage(0);
      exit(0);
    default:
      fprintf(stderr,"illegal option: -%c\n",opterr);
      usage(0);
      exit(1);
    }
  }
  argc-=optind;
  argv+=optind;
  if (argc >= 1)
  {
    usage(0);
     exit(1);
  }
  return(0);
}

static uint max_prefix(const char *name)
{
  uint i;
  uint max_length=1;
  for (i=0 ; i < sizeof(symbols)/sizeof(SYMBOL) ; i++)
  {
    const char *str=symbols[i].name;
    if (str != name)
    {
      const char *str2=name;
      uint length;
      while (*str && *str == *str2)
      {
	str++;
	str2++;
      }
      length=(uint) (str2 - name)+1;
      if (length > max_length)
	max_length=length;
    }
  }
  for (i=0 ; i < sizeof(sql_functions)/sizeof(SYMBOL) ; i++)
  {
    const char *str=sql_functions[i].name;
    if (str != name)
    {
      const char *str2=name;
      uint length;
      while (*str && *str == *str2)
      {
	str++;
	str2++;
      }
      length=(uint) (str2 - name)+1;
      if (length > max_length)
	max_length=length;
    }
  }
  return max_length;
}


static void make_max_length_table(void)
{
  uint i;
  for (i=0 ; i < sizeof(symbols)/sizeof(SYMBOL) ; i++)
  {
    uint length=max_prefix(symbols[i].name);
    if (length > unique_length[(uchar) symbols[i].name[0]])
    {
      unique_length[(uchar) symbols[i].name[0]]=length;
      unique_length[(uchar) tolower(symbols[i].name[0])]=length;
    }
  }
  for (i=0 ; i < sizeof(sql_functions)/sizeof(SYMBOL) ; i++)
  {
    uint length=max_prefix(sql_functions[i].name);
    if (length > unique_length[(uchar) sql_functions[i].name[0]])
    {
      unique_length[(uchar) sql_functions[i].name[0]]=length;
      unique_length[(uchar) tolower(sql_functions[i].name[0])]=length;
    }
  }
}


int main(int argc,char **argv)
{
  struct rand_struct rand_st;
  static uint best_mod,best_add,best_functype;
  int error;

  MY_INIT(argv[0]);
  start_value=5307411L; best_t1=4597287L;  best_t2=3375760L;  best_type=1; /* mode=4783  add=5  func_type: 0 */
  if (get_options(argc,(char **) argv))
    exit(1);

  make_max_length_table();
  make_char_table(best_t1,best_t2,best_type);
  make_prime_array(sizeof(symbols)/sizeof(SYMBOL) +
		   sizeof(sql_functions)/sizeof(SYMBOL));

  if ((error=search(1)) > 0 || error && !opt_search)
    exit(1);					// This should work
  best_mod=function_mod; best_add=function_plus; best_functype=function_type;

  if (opt_search)
  {
    time_t start_time=time((time_t*) 0);
    randominit(&rand_st,start_time,start_time/2); // Some random values
    printf("start_value=%ldL;  best_t1=%ldL;  best_t2=%ldL;  best_type=%d; /* mode=%d  add=%d type: %d */\n",
	   start_value, best_t1,best_t2,best_type,best_mod,best_add,
	   best_functype);

    for (uint i=1 ; i <= opt_count ; i++)
    {
      if (i % 10 == 0)
      {
	putchar('.');
	fflush(stdout);
      }
      ulong t1=(ulong) (rnd(&rand_st)*INT_MAX24);
      ulong t2=(ulong) (rnd(&rand_st)*INT_MAX24);
      uint type=(int) (rnd(&rand_st)*char_table_count);
      start_value=(ulong) (rnd(&rand_st)*INT_MAX24);
      make_char_table(t1,t2,type);
      if (!search(0))
      {
	best_mod=function_mod; best_add=function_plus;
	best_functype=function_type;
	best_t1=t1; best_t2=t2; best_type=type;
	best_start_value=start_value;
	printf("\nstart_value=%ldL; best_t1=%ldL;  best_t2=%ldL;  best_type=%d; /* mode=%d  add=%d  type: %d */\n",
	       best_start_value,best_t1,best_t2,best_type,best_mod,best_add,
	       best_functype);
      }
    }
  }

  function_mod=best_mod; function_plus=best_add;
  make_char_table(best_t1,best_t2,best_type);

  printf("/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB\n\
   This program is free software; you can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 2 of the License, or\n\
   (at your option) any later version.\n\n\
   This program is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\n\
   You should have received a copy of the GNU General Public License\n\
   along with this program; if not, write to the Free Software\n\
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */\n\n");

printf("/* This code is generated by gen_lex_hash.cc that seeks for a perfect\nhash function */\n\n");
  printf("#include \"lex.h\"\n\n");

  print_arrays();

  printf("/* start_value=%ldL;  best_t1=%ldL;  best_t2=%ldL;  best_type=%d; */ /* mode=%d  add=%d t ype: %d */\n\n",
	 best_start_value, best_t1, best_t2, best_type,
	 best_mod, best_add, best_functype);

  printf("inline SYMBOL *get_hash_symbol(const char *s,unsigned int length,bool function)\n\
{\n\
  ulong idx = %lu+char_table[(uchar) *s];\n\
  SYMBOL *sim;\n\
  const char *start=s;\n\
  int i=unique_length[(uchar) *s++];\n\
  if (i > (int) length) i=(int) length;\n\
  while (--i > 0)\n\
    idx= (idx ^ (char_table[(uchar) *s++] + (idx << %d)));\n\
  idx=my_function_table[(idx & %d) %% %d];\n\
  if (idx >= %d)\n\
  {\n\
    if (!function || idx >= %d) return (SYMBOL*) 0;\n\
    sim=sql_functions + (idx - %d);\n\
  }\n\
  else\n\
    sim=symbols + idx;\n\
  if ((length != sim->length) || lex_casecmp(start,sim->name,length))\n\
    return  (SYMBOL *)0;\n\
  return sim;\n\
}\n",(ulong) start_value,(int) function_plus,(int) how_much_and,function_mod,how_long_symbols,max_symbol,how_long_symbols);
  exit(0);
  return 0;
}
