/* Copyright (c) 2001, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Resolve numeric stack dump produced by mysqld 3.23.30 and later
   versions into symbolic names. By Sasha Pachev <sasha@mysql.com>
 */

#include <my_config.h>
#include <stdio.h>                              // Needed on SunOS 5.10
#include <vector>
#include <string>

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <my_getopt.h>
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

const int initial_symbol_table_size= 4096;

#define DUMP_VERSION "1.5"
#define HEX_INVALID  (uchar)255

typedef ulong my_long_addr_t ; /* at some point, we need to fix configure
				* to define this for us  
				*/

typedef struct sym_entry
{
  std::string symbol;
  uchar* addr;
} SYM_ENTRY;


static char* dump_fname = 0, *sym_fname = 0;
static std::vector<sym_entry> sym_table;
static FILE* fp_dump, *fp_sym = 0, *fp_out; 
static void die(const char* fmt, ...)
  MY_ATTRIBUTE((noreturn)) MY_ATTRIBUTE((format(printf, 1, 2)));

static struct my_option my_long_options[] =
{
  {"help", 'h', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"symbols-file", 's', "Use specified symbols file.", &sym_fname,
   &sym_fname, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"numeric-dump-file", 'n', "Read the dump from specified file.",
   &dump_fname, &dump_fname, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void verify_sort();


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,DUMP_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage()
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2001"));
  printf("Resolve numeric stack strace dump into symbols.\n\n");
  printf("Usage: %s [OPTIONS] symbols-file [numeric-dump-file]\n",
	 my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  printf("\n\
The symbols-file should include the output from:  'nm --numeric-sort mysqld'.\n\
The numeric-dump-file should contain a numeric stack trace from mysqld.\n\
If the numeric-dump-file is not given, the stack trace is read from stdin.\n");
}


static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}


static my_bool
get_one_option(int optid, const struct my_option *opt MY_ATTRIBUTE((unused)),
	       char *argument MY_ATTRIBUTE((unused)))
{
  switch(optid) {
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


static int parse_args(int argc, char **argv)
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  /*
    The following code is to make the command compatible with the old
    version that required one to use the -n and -s options
  */

  if (argc == 2)
  {
    sym_fname= argv[0];
    dump_fname= argv[1];
  }
  else if (argc == 1)
  {
    if (!sym_fname)
      sym_fname = argv[0];
    else if (!dump_fname)
      dump_fname = argv[0];
    else
    {
      usage();
      exit(1);
    }
  }
  else if (argc != 0 || !sym_fname)
  {
    usage();
    exit(1);
  }
  return 0;
}


static void open_files()
{
  fp_out = stdout;
  fp_dump = stdin;

  if (dump_fname && !(fp_dump = my_fopen(dump_fname, O_RDONLY, MYF(MY_WME))))
      die("Could not open %s", dump_fname);
  /* if name not given, assume stdin*/

  if (!sym_fname)
    die("Please run nm --numeric-sort on mysqld binary that produced stack \
trace dump and specify the path to it with -s or --symbols-file");
  if (!(fp_sym = my_fopen(sym_fname, O_RDONLY, MYF(MY_WME))))
    die("Could not open %s", sym_fname);

}

static uchar hex_val(char c)
{
  uchar l;
  if (my_isdigit(&my_charset_latin1,c))
    return c - '0';
  l = my_tolower(&my_charset_latin1,c);
  if (l < 'a' || l > 'f')
    return HEX_INVALID; 
  return (uchar)10 + ((uchar)c - (uchar)'a');
}

static my_long_addr_t read_addr(char** buf)
{
  uchar c;
  char* p = *buf;
  my_long_addr_t addr = 0;

  while((c = hex_val(*p++)) != HEX_INVALID)
      addr = (addr << 4) + c;

  *buf = p; 
  return addr;
}

static int init_sym_entry(SYM_ENTRY* se, char* buf)
{
  char* p;
  se->addr = (uchar*)read_addr(&buf);

  if (!se->addr)
    return -1;
  while (my_isspace(&my_charset_latin1,*buf++))
    /* empty */;

  while (my_isspace(&my_charset_latin1,*buf++))
    /* empty - skip more space */;
  --buf;
  /* now we are on the symbol */
  for (p =buf; *buf != '\n' && *buf; ++buf)
    ;
  try {
    se->symbol.assign(p, buf - p);
  }
  catch (...)
  {
    die("failed to allocate space for symbol %.*s", (int) (buf - p), p);
  }

  return 0;
}

static void init_sym_table()
{
  /*
    A buffer of 100Kb should be big enough to hold any single line output from
    'nm --demangle'
  */
  static char buf[1024 * 100];
  try {
    sym_table.reserve(initial_symbol_table_size);
  }
  catch (...)
  {
    die("Failed in std::vector.reserve() -- looks like out of memory problem");
  }
  while (fgets(buf, sizeof(buf), fp_sym))
  {
    SYM_ENTRY se;
    if (init_sym_entry(&se, buf))
      continue;
    try {
      sym_table.push_back(se);
    }
    catch (...)
    {
      die("std::vector.push_back() failed - looks like we are out of memory");
    }
  }

  verify_sort();
}

static void clean_up()
{
}

static void verify_sort()
{
  uint i;
  uchar* last = 0;

  for (i = 0; i < sym_table.size(); i++)
  {
    SYM_ENTRY se= sym_table[i];
    if (se.addr < last)
      die("sym table does not appear to be sorted, did you forget "
          "--numeric-sort arg to nm? trouble addr = %p, last = %p",
          se.addr, last);
    last = se.addr;
  }
}


static SYM_ENTRY* resolve_addr(uchar* addr, SYM_ENTRY* se)
{
  uint i;
  *se= sym_table[0];
  if (addr < se->addr)
    return 0;

  for (i = 1; i < sym_table.size(); i++)
  {
    *se= sym_table[i];
    if (addr < se->addr)
    {
      *se= sym_table[i - 1];
      return se;
    }
  }

  return se;
}


static void do_resolve()
{
  char buf[1024 * 8], *p;
  while (fgets(buf, sizeof(buf), fp_dump))
  {
    /* skip bracket */
    p= (p= strchr(buf, '[')) ? p+1 : buf;
    /* skip space */
    while (my_isspace(&my_charset_latin1,*p))
      ++p;

    if (*p++ == '0' && *p++ == 'x')
    {
      SYM_ENTRY se ;
      uchar* addr = (uchar*)read_addr(&p);
      if (resolve_addr(addr, &se))
	fprintf(fp_out, "%p %s + %d\n", addr, se.symbol.c_str(),
		(int) (addr - se.addr));
      else
	fprintf(fp_out, "%p (?)\n", addr);

    }
    else
    {
      fputs(buf, fp_out);
      continue;
    }
  }
}


int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(argc, argv);
  open_files();
  init_sym_table();
  do_resolve();
  clean_up();
  return 0;
}
