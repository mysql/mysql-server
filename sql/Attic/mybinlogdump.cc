
#undef MYSQL_SERVER
#include "log_event.h"
#include <getopt.h>
#include <config.h>

static const char* default_dbug_option = "d:t:o,/tmp/mybinlogdump.trace";

static struct option long_options[] =
{
  {"short-form", no_argument, 0, 's'},
  {"offset", required_argument,0, 'o'},
  {"help", no_argument, 0, 'h'},
#ifndef DBUG_OFF
  {"debug", required_argument, 0, '#'}
#endif
};

static bool short_form = 0;
static int offset = 0;

static int parse_args(int argc, char** argv);
static void dump_log_entries();
static void die(char* fmt, ...);

static void die(char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

static void usage()
{
  fprintf(stderr, "Usage: mybinlogdump [options] log-files\n");
  fprintf(stderr, "Options:\n\
   -s,--short-form - just show the queries, no extra info\n\
   -o,--offset=N - skip the first N entries\n\
   -h,--help - this message\n");
}

static int parse_args(int *argc, char*** argv)
{
  int c, opt_index = 0;

  while((c = getopt_long(*argc, *argv, "so:#:h", long_options,
			 &opt_index)) != EOF)
	{
	  switch(c)
	    {
#ifndef DBUG_OFF
	    case '#':
	      DBUG_PUSH(optarg ? optarg : default_dbug_option);
	      break;
#endif
	    case 's':
	      short_form = 1;
	      break;

	    case 'o':
	      offset = atoi(optarg);
	      break;

	    case 'h':
	    default:
	      usage();
	      exit(0);

	    }
	}

  (*argc)-=optind;
  (*argv)+=optind;
	  
  
  return 0;
}


static void dump_log_entries(const char* logname)
{
 FILE* file;
 int rec_count = 0;
 
 if(logname && logname[0] != '-')
   file = my_fopen(logname, O_RDONLY, MYF(MY_WME));
 else
   file = stdin;
 
 if(!file)
   die("Could not open log file %s", logname);
 while(1)
   {
     Log_event* ev = Log_event::read_log_event(file);
     if(!ev)
       if(!feof(file))
        die("Could not read entry at offset %ld : Error in log format or \
read error",
	   my_ftell(file, MYF(MY_WME)));
       else
	 break;

     if(rec_count >= offset)
       ev->print(stdout, short_form);
     rec_count++;
     delete ev;
   }
 
 my_fclose(file, MYF(MY_WME));
}

int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(&argc, (char***)&argv);
    
  if(!argc)
    {
      usage();
      return -1;
    }

  while(--argc >= 0)
    {
      dump_log_entries(*(argv++));
    }
  
  return 0;
}





