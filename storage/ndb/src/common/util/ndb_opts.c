#include <my_global.h>
#define OPTEXPORT
#include <ndb_opts.h>

static char* g_ndb_opt_progname= "ndbapi_program";

static void default_ndb_opt_short(void)
{
  ndb_short_usage_sub(g_ndb_opt_progname,NULL);
}

static void default_ndb_opt_usage()
{
  struct my_option my_long_options[] =
    {
      NDB_STD_OPTS("ndbapi_program")
    };
  const char *load_default_groups[]= { "mysql_cluster", 0 };

  ndb_usage(default_ndb_opt_short, load_default_groups, my_long_options);
}

static void (*g_ndb_opt_short_usage)(void)= default_ndb_opt_short;
static void (*g_ndb_opt_usage)(void)= default_ndb_opt_usage;

void ndb_opt_set_usage_funcs(const char* my_progname,
                             void (*short_usage)(void),
                             void (*usage)(void))
{
  if(my_progname)
    g_ndb_opt_progname= my_progname;
  if(short_usage)
    g_ndb_opt_short_usage= short_usage;
  if(usage)
    g_ndb_opt_usage= usage;
}

void ndb_short_usage_sub(const char* my_progname, char* extra)
{
  printf("Usage: %s [OPTIONS]%s%s\n", my_progname,
         (extra)?" ":"",
         (extra)?extra:"");
}

void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options)
{
  (*usagefunc)();

  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

my_bool
ndb_std_get_one_option(int optid,
                       const struct my_option *opt __attribute__((unused)),
                       char *argument)
{
  switch (optid) {
#ifndef DBUG_OFF
  case '#':
    if (opt_debug)
    {
      DBUG_PUSH(opt_debug);
    }
    else
    {
      DBUG_PUSH("d:t");
    }
    opt_endinfo= 1;
    break;
#endif
  case 'V':
    ndb_std_print_version();
    exit(0);
  case '?':
    (*g_ndb_opt_usage)();
    exit(0);
  case OPT_NDB_SHM:
    if (opt_ndb_shm)
    {
#ifndef NDB_SHM_TRANSPORTER
      printf("Warning: binary not compiled with shared memory support,\n"
             "Tcp connections will now be used instead\n");
      opt_ndb_shm= 0;
#endif
    }
    break;
  case OPT_NDB_MGMD:
  case OPT_NDB_NODEID:
  {
    int len= my_snprintf(opt_ndb_constrbuf+opt_ndb_constrbuf_len,
                         sizeof(opt_ndb_constrbuf)-opt_ndb_constrbuf_len,
                         "%s%s%s",opt_ndb_constrbuf_len > 0 ? ",":"",
                         optid == OPT_NDB_NODEID ? "nodeid=" : "",
                         argument);
    opt_ndb_constrbuf_len+= len;
  }
  /* fall through to add the connectstring to the end
   * and set opt_ndbcluster_connectstring
   */
  case OPT_NDB_CONNECTSTRING:
    if (opt_ndb_connectstring && opt_ndb_connectstring[0])
      my_snprintf(opt_ndb_constrbuf+opt_ndb_constrbuf_len,
                  sizeof(opt_ndb_constrbuf)-opt_ndb_constrbuf_len,
                  "%s%s", opt_ndb_constrbuf_len > 0 ? ",":"",
                  opt_ndb_connectstring);
    else
      opt_ndb_constrbuf[opt_ndb_constrbuf_len]= 0;
    opt_connect_str= opt_ndb_constrbuf;
    break;
  }
  return 0;
}

void ndb_std_print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",
         MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}
