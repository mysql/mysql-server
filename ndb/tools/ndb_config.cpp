/* Copyright (C) 2003 MySQL AB

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

/**
 * ndb_config --nodes --query=nodeid --type=ndbd --host=local1
 */

#include <ndb_global.h>
#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_version.h>

#include <NdbOut.hpp>
#include <mgmapi.h>
#include <mgmapi_configuration.hpp>
#include <ConfigInfo.hpp>

static int g_verbose = 0;
static int try_reconnect = 3;

static int g_nodes = 1;
static const char * g_connectstring = 0;
static const char * g_query = 0;

static int g_nodeid = 0;
static const char * g_type = 0;
static const char * g_host = 0;
static const char * g_field_delimiter=",";
static const char * g_row_delimiter=" ";
static const char * g_config_file = 0;

int g_print_full_config, opt_ndb_shm;
my_bool opt_core;

typedef ndb_mgm_configuration_iterator Iter;

static void ndb_std_print_version()
{
  printf("MySQL distrib %s, for %s (%s)\n",
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static struct my_option my_long_options[] =
{
  { "usage", '?', "Display this help and exit.", 
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "help", '?', "Display this help and exit.", 
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "version", 'V', "Output version information and exit.", 0, 0, 0, 
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { "ndb-connectstring", 256,
    "Set connect string for connecting to ndb_mgmd. " 
    "Syntax: \"[nodeid=<id>;][host=]<hostname>[:<port>]\". " 
    "Overides specifying entries in NDB_CONNECTSTRING and Ndb.cfg", 
    (gptr*) &g_connectstring, (gptr*) &g_connectstring, 
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "nodes", 256, "Print nodes",
    (gptr*) &g_nodes, (gptr*) &g_nodes,
    0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  { "query", 'q', "Query option(s)",
    (gptr*) &g_query, (gptr*) &g_query,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "host", 257, "Host",
    (gptr*) &g_host, (gptr*) &g_host,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "type", 258, "Type of node/connection",
    (gptr*) &g_type, (gptr*) &g_type,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "id", 258, "Nodeid",
    (gptr*) &g_nodeid, (gptr*) &g_nodeid,
    0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "nodeid", 258, "Nodeid",
    (gptr*) &g_nodeid, (gptr*) &g_nodeid,
    0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "fields", 'f', "Field separator",
    (gptr*) &g_field_delimiter, (gptr*) &g_field_delimiter,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "rows", 'r', "Row separator",
    (gptr*) &g_row_delimiter, (gptr*) &g_row_delimiter,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "config-file", 256, "Path to config.ini",
    (gptr*) &g_config_file, (gptr*) &g_config_file,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage()
{
  char desc[] = 
    "This program will retreive config options for a ndb cluster\n";
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}
static my_bool
ndb_std_get_one_option(int optid,
		       const struct my_option *opt __attribute__((unused)),
		       char *argument)
{
  switch (optid) {
  case 'V':
    ndb_std_print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

/**
 * Match/Apply framework
 */
struct Match
{
  int m_key;
  BaseString m_value;
  virtual int eval(const Iter&);
};

struct Apply
{
  Apply() {}
  Apply(int val) { m_key = val;}
  int m_key;
  virtual int apply(const Iter&);
};

struct NodeTypeApply : public Apply
{
  virtual int apply(const Iter&);
};

static int parse_query(Vector<Apply*>&, int &argc, char**& argv);
static int parse_where(Vector<Match*>&, int &argc, char**& argv);
static int eval(const Iter&, const Vector<Match*>&);
static int apply(const Iter&, const Vector<Apply*>&);
static ndb_mgm_configuration* fetch_configuration();
static ndb_mgm_configuration* load_configuration();

int
main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
  if ((ho_error=handle_options(&argc, &argv, my_long_options,
			       ndb_std_get_one_option)))
    return -1;

  ndb_mgm_configuration * conf = 0;

  if (g_config_file)
    conf = load_configuration();
  else
    conf = fetch_configuration();

  if (conf == 0)
  {
    return -1;
  }
  
  Vector<Apply*> select_list;
  Vector<Match*> where_clause;

  if(strcmp(g_row_delimiter, "\\n") == 0)
    g_row_delimiter = "\n";
  if(strcmp(g_field_delimiter, "\\n") == 0)
    g_field_delimiter = "\n";

  if(parse_query(select_list, argc, argv))
  {
    exit(0);
  }

  if(parse_where(where_clause, argc, argv))
  {
    exit(0);
  }

  Iter iter(* conf, CFG_SECTION_NODE);
  bool prev= false;
  iter.first();
  for(iter.first(); iter.valid(); iter.next())
  {
    if(eval(iter, where_clause))
    {
      if(prev)
	printf("%s", g_row_delimiter);
      prev= true;
      apply(iter, select_list);
    }
  }
  printf("\n");
  return 0;
}

static
int
parse_query(Vector<Apply*>& select, int &argc, char**& argv)
{
  if(g_query)
  {
    BaseString q(g_query);
    Vector<BaseString> list;
    q.split(list, ",");
    for(unsigned i = 0; i<list.size(); i++)
    {
      const char * str= list[i].c_str();
      if(strcasecmp(str, "id") == 0 || strcasecmp(str, "nodeid") == 0)
	select.push_back(new Apply(CFG_NODE_ID));
      else if(strncasecmp(str, "host", 4) == 0)
	select.push_back(new Apply(CFG_NODE_HOST));
      else if(strcasecmp(str, "type") == 0)
	select.push_back(new NodeTypeApply());
      else if(g_nodes)
      {
	bool found = false;
	for(int p = 0; p<ConfigInfo::m_NoOfParams; p++)
	{
	  if(0)ndbout_c("%s %s",
			ConfigInfo::m_ParamInfo[p]._section,
			ConfigInfo::m_ParamInfo[p]._fname);
	  if(strcmp(ConfigInfo::m_ParamInfo[p]._section, "DB") == 0 ||
	     strcmp(ConfigInfo::m_ParamInfo[p]._section, "API") == 0 ||
	     strcmp(ConfigInfo::m_ParamInfo[p]._section, "MGM") == 0)
	  {
	    if(strcasecmp(ConfigInfo::m_ParamInfo[p]._fname, str) == 0)
	    {
	      select.push_back(new Apply(ConfigInfo::m_ParamInfo[p]._paramId));
	      found = true;
	      break;
	    }
	  }
	}
	if(!found)
	{
	  fprintf(stderr, "Unknown query option: %s\n", str);
	  return 1;
	}
      }
      else
      {
	fprintf(stderr, "Unknown query option: %s\n", str);
	return 1;
      }
    }
  }
  return 0;
}

static 
int 
parse_where(Vector<Match*>& where, int &argc, char**& argv)
{
  Match m;
  if(g_host)
  {
    m.m_key = CFG_NODE_HOST;
    m.m_value.assfmt("%s", g_host);
    where.push_back(new Match(m));
  }
  
  if(g_type)
  {
    m.m_key = CFG_TYPE_OF_SECTION;
    m.m_value.assfmt("%d", ndb_mgm_match_node_type(g_type));
    where.push_back(new Match(m));
  }

  if(g_nodeid)
  {
    m.m_key = CFG_NODE_ID;
    m.m_value.assfmt("%d", g_nodeid);
    where.push_back(new Match(m));
  }
  return 0;
}

template class Vector<Apply*>;
template class Vector<Match*>;

static 
int
eval(const Iter& iter, const Vector<Match*>& where)
{
  for(unsigned i = 0; i<where.size(); i++)
  {
    if(where[i]->eval(iter) == 0)
      return 0;
  }
  
  return 1;
}

static 
int 
apply(const Iter& iter, const Vector<Apply*>& list)
{
  for(unsigned i = 0; i<list.size(); i++)
  {
    list[i]->apply(iter);
    if(i + 1 != list.size())
      printf("%s", g_field_delimiter);
  }
  return 0;
}

int
Match::eval(const Iter& iter)
{
  Uint32 val32;
  Uint64 val64;
  const char* valc;
  if (iter.get(m_key, &val32) == 0)
  {
    if(atoi(m_value.c_str()) != (int)val32)
      return 0;
  } 
  else if(iter.get(m_key, &val64) == 0)
  {
    if(strtoll(m_value.c_str(), (char **)NULL, 10) != (long long)val64)
      return 0;
  }
  else if(iter.get(m_key, &valc) == 0)
  {
    if(strcmp(m_value.c_str(), valc) != 0)
      return 0;
  }
  else
  {
    return 0;
  }
  return 1;
}

int
Apply::apply(const Iter& iter)
{
  Uint32 val32;
  Uint64 val64;
  const char* valc;
  if (iter.get(m_key, &val32) == 0)
  {
    printf("%u", val32);
  } 
  else if(iter.get(m_key, &val64) == 0)
  {
    printf("%llu", val64);
  }
  else if(iter.get(m_key, &valc) == 0)
  {
    printf("%s", valc);
  }
  return 0;
}

int
NodeTypeApply::apply(const Iter& iter)
{
  Uint32 val32;
  if (iter.get(CFG_TYPE_OF_SECTION, &val32) == 0)
  {
    printf("%s", ndb_mgm_get_node_type_alias_string((ndb_mgm_node_type)val32, 0));
  } 
  return 0;
}

ndb_mgm_configuration*
fetch_configuration()
{  
  ndb_mgm_configuration* conf = 0;
  NdbMgmHandle mgm = ndb_mgm_create_handle();
  if(mgm == NULL) {
    fprintf(stderr, "Cannot create handle to management server.\n");
    return 0;
  }

  ndb_mgm_set_error_stream(mgm, stderr);
  
  if (ndb_mgm_set_connectstring(mgm, g_connectstring))
  {
    fprintf(stderr, "* %5d: %s\n", 
	    ndb_mgm_get_latest_error(mgm),
	    ndb_mgm_get_latest_error_msg(mgm));
    fprintf(stderr, 
	    "*        %s", ndb_mgm_get_latest_error_desc(mgm));
    goto noconnect;
  }

  if(ndb_mgm_connect(mgm, try_reconnect-1, 5, 1))
  {
    fprintf(stderr, "Connect failed");
    fprintf(stderr, " code: %d, msg: %s\n",
	    ndb_mgm_get_latest_error(mgm),
	    ndb_mgm_get_latest_error_msg(mgm));
    goto noconnect;
  }
  else if(g_verbose)
  {
    fprintf(stderr, "Connected to %s:%d\n", 
	    ndb_mgm_get_connected_host(mgm),
	    ndb_mgm_get_connected_port(mgm));
  }
	  
  conf = ndb_mgm_get_configuration(mgm, 0);
  if(conf == 0)
  {
    fprintf(stderr, "Could not get configuration");
    fprintf(stderr, "code: %d, msg: %s\n",
	    ndb_mgm_get_latest_error(mgm),
	    ndb_mgm_get_latest_error_msg(mgm));
  }
  else if(g_verbose)
  {
    fprintf(stderr, "Fetched configuration\n");
  }

  ndb_mgm_disconnect(mgm);
noconnect:
  ndb_mgm_destroy_handle(&mgm);
  
  return conf;
}

#include <Config.hpp>

ndb_mgm_configuration*
load_configuration()
{  
  InitConfigFileParser parser(stderr);
  if (g_verbose)
    fprintf(stderr, "Using config.ini : %s", g_config_file);
  
  Config* conf = parser.parseConfig(g_config_file);
  if (conf)
    return conf->m_configValues;

  return 0;
}
