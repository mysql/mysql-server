/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Resolve numeric stack dump produced by mysqld 3.23.30 and later
   versions into symbolic names. By Sasha Pachev <sasha@mysql.com>
 */

#define DONT_USE_RAID
#include <global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <getopt.h>

#define INIT_SYM_TABLE  4096
#define INC_SYM_TABLE  4096
#define MAX_SYM_SIZE   128
#define DUMP_VERSION "1.2"
#define HEX_INVALID  (uchar)255

typedef ulong my_long_addr_t ; /* at some point, we need to fix configure
				* to define this for us  
				*/

typedef struct sym_entry
{
  char symbol[MAX_SYM_SIZE];
  uchar* addr;
} SYM_ENTRY;


static char* dump_fname = 0, *sym_fname = 0;
static DYNAMIC_ARRAY sym_table; /* how do you like this , static DYNAMIC ? */
static FILE* fp_dump, *fp_sym = 0, *fp_out; 

struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {"symbols-file", required_argument, 0, 's'},
  {"numeric-dump-file", required_argument, 0, 'n'},
  {0, 0,0,0}
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
  printf("MySQL AB, by Sasha Pachev\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Resolve numeric stack strace dump into symbols.\n\n");
  printf("Usage: %s [OPTIONS] symbols-file [numeric-dump-file]\n", my_progname);
  printf("\n\
  -?, --help               Display this help and exit.\n\
  -h, --host=...           Connect to host.\n\
  -V, --version            Output version information and exit.\n");
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


static int parse_args(int argc, char **argv)
{
  int c, option_index = 0;

  while((c = getopt_long(argc, argv, "?Vn:s:",
			 long_options, &option_index)) != EOF)
    {
      switch(c)
	{
	case 'n':
	  dump_fname = optarg;
	  break;
	case 's':
	  sym_fname = optarg;
	  break;
	case 'V':
	  print_version();
	  exit(0);
	case '?':
	  usage();
	  exit(0);
	default:
	  usage();
	  exit(1);
	}
    }

  argc-=optind;
  argv+=optind;

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
  
  if(dump_fname && !(fp_dump = my_fopen(dump_fname, O_RDONLY, MYF(MY_WME))))
      die("Could not open %s", dump_fname);
  /* if name not given, assume stdin*/

  if(!sym_fname)
    die("Please run nm --numeric-sort on mysqld binary that produced stack \
trace dump and specify the path to it with -s or --symbols-file");
  if(!(fp_sym = my_fopen(sym_fname, O_RDONLY, MYF(MY_WME))))
    die("Could not open %s", sym_fname);
    
}

static uchar hex_val(char c)
{
  uchar l;
  if(isdigit(c))
    return c - '0';
  l = tolower(c);
  if(l < 'a' || l > 'f')
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
  char* p, *p_end;
  se->addr = (uchar*)read_addr(&buf);

  if(!se->addr)
    return -1;
  while(isspace(*buf++))
    /* empty */;
  
  while(isspace(*buf++))
    /* empty - skip more space */;
  --buf;
  /* now we are on the symbol */
  for(p = se->symbol, p_end = se->symbol + sizeof(se->symbol) - 1;
      *buf != '\n' && *buf; ++buf,++p )
    {
      if(p < p_end)
	*p = *buf;
      else
	break;
    }
  *p = 0;
  if(!strcmp(se->symbol, "gcc2_compiled."))
    return -1;
  return 0;
}

static void init_sym_table()
{
  char buf[512];
  if(init_dynamic_array(&sym_table, sizeof(SYM_ENTRY), INIT_SYM_TABLE,
		     INC_SYM_TABLE))
    die("Failed in init_dynamic_array() -- looks like out of memory problem");

  while(fgets(buf, sizeof(buf), fp_sym))
    {
      SYM_ENTRY se;
      if(init_sym_entry(&se, buf))
	continue;
      if(insert_dynamic(&sym_table, (gptr)&se))
	die("insert_dynamic() failed - looks like we are out of memory");
    }

  verify_sort();
}

static void clean_up()
{
  delete_dynamic(&sym_table);
}

static void verify_sort()
{
  uint i;
  uchar* last = 0;
  
  for(i = 0; i < sym_table.elements; i++)
    {
      SYM_ENTRY se;
      get_dynamic(&sym_table, (gptr)&se, i);
      if(se.addr < last)
	die("sym table does not appear to be sorted, did you forget \
--numeric-sort arg to nm? trouble addr = %p, last = %p", se.addr, last);
      last = se.addr;
    }
}

static SYM_ENTRY* resolve_addr(uchar* addr, SYM_ENTRY* se)
{
  uint i;
  get_dynamic(&sym_table, (gptr)se, 0);
  if(addr < se->addr)
    return 0;
  
  for(i = 1; i < sym_table.elements; i++)
    {
      get_dynamic(&sym_table, (gptr)se, i);
      if(addr < se->addr)
	{
	  get_dynamic(&sym_table, (gptr)se, i - 1);
	  return se;
	}
    }

  return se;
}

static void do_resolve()
{
  char buf[1024], *p;
  while(fgets(buf, sizeof(buf), fp_dump))
    {
      p = buf;
      while(isspace(*p))
	++p;
	/* skip space */;
      
      if(*p++ == '0' && *p++ == 'x')
	{
	  SYM_ENTRY se ;
	  uchar* addr = (uchar*)read_addr(&p);
	  if(resolve_addr(addr, &se))
	    fprintf(fp_out, "%p %s + %d\n", addr, se.symbol,
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
     
