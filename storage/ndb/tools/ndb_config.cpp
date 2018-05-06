 /*
   Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/**
 * Description of config variables, including their min, max, default
 * values can be printed (--configinfo). This can also be printed
 * in xml format (--xml).
 *
 * Config can be retrieved from only one of the following sources:
 ** 1) config stored at mgmd (default)
 ** 2) config stored at a data node (--config_from_node=<data node id>)
 *** (Note:
 ***  Node numbers less than 1 give error:
 ***  "Given value <node id> is not a valid node number." 
 ***  Non-data node numbers give error:
 ***  "Node <node id> is not a data node.")
 ** 3) my.cnf (--mycnf=<fullPath/mycnfFileName>)
 ** 4) config.file (--config_file=<fullPath/configFileName>
 *
 * Config variables are displayed from only one of the following
 * sections of the retrieved config:
 ** CFG_SECTION_NODE (default, or --nodes)
 ** CFG_SECTION_CONNECTION (--connections)
 ** CFG_SECTION_SYSTEM (--system)
 */

/**
 * Examples:
 * Get config from mgmd (default):
 ** Display results from section CFG_SECTION_NODE (default)
 *** ndb_config --nodes --query=nodeid --type=ndbd --host=local1
 *** ndb_config  --query=nodeid,host
 *
 ** Display results from section CFG_SECTION_SYSTEM
 *** ndb_config --system --query=ConfigGenerationNumber
 *
 ** Display results from section CFG_SECTION_CONNECTION
 *** ndb_config --connections --query=type
 *
 * Get config from eg. node 2, which is a data node:
 *
 ** ndb_config --config_from_node=2 --system --query=ConfigGenerationNumber
 ** ndb_config --config_from_node=2 --connections --query=type
 ** ndb_config --config_from_node=2 --query=id,NoOfFragmentLogFiles
 *
 **  Get config from eg. node 2 and display results for node 2 only:
 *** ndb_config --config_from_node=2 --query=id,NoOfFragmentLogFiles --nodeid=2
 */

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <mgmapi.h>
#include "../src/mgmapi/mgmapi_configuration.hpp"
#include "../src/mgmsrv/ConfigInfo.hpp"
#include <NdbAutoPtr.hpp>
#include <NdbTCP.h>

#include "my_alloc.h"

static int g_verbose = 0;

static int g_nodes, g_connections, g_system, g_section;
static const char * g_query = 0;
static int g_query_all = 0;
static int g_diff_default = 0;

static int g_nodeid = 0;
static const char * g_type = 0;
static const char * g_host = 0;
static const char * g_field_delimiter=",";
static const char * g_row_delimiter=" ";
static const char * g_config_file = 0;
static int g_mycnf = 0;
static int g_configinfo = 0;
static int g_xml = 0;
static int g_config_from_node = 0;

const char *load_default_groups[]= { "mysql_cluster",0 };

typedef ndb_mgm_configuration_iterator Iter;

static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_config"),
  { "nodes", NDB_OPT_NOSHORT, "Print nodes",
    (uchar**) &g_nodes, (uchar**) &g_nodes,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "connections", NDB_OPT_NOSHORT, "Print connections",
    (uchar**) &g_connections, (uchar**) &g_connections,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "system", NDB_OPT_NOSHORT, "Print system",
    (uchar**) &g_system, (uchar**) &g_system,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "query", 'q', "Query option(s)",
    (uchar**) &g_query, (uchar**) &g_query,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "host", NDB_OPT_NOSHORT, "Host",
    (uchar**) &g_host, (uchar**) &g_host,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "type", NDB_OPT_NOSHORT, "Type of node/connection",
    (uchar**) &g_type, (uchar**) &g_type,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "nodeid", NDB_OPT_NOSHORT, "Nodeid",
    (uchar**) &g_nodeid, (uchar**) &g_nodeid,
    0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "fields", 'f', "Field separator",
    (uchar**) &g_field_delimiter, (uchar**) &g_field_delimiter,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "rows", 'r', "Row separator",
    (uchar**) &g_row_delimiter, (uchar**) &g_row_delimiter,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "config-file", NDB_OPT_NOSHORT, "Path to config.ini",
    (uchar**) &g_config_file, (uchar**) &g_config_file,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { "mycnf", NDB_OPT_NOSHORT, "Read config from my.cnf",
    (uchar**) &g_mycnf, (uchar**) &g_mycnf,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "configinfo", NDB_OPT_NOSHORT, "Print configinfo",
    (uchar**) &g_configinfo, (uchar**) &g_configinfo,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "xml", NDB_OPT_NOSHORT, "Print configinfo in xml format",
    (uchar**) &g_xml, (uchar**) &g_xml,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { "config_from_node", NDB_OPT_NOSHORT, "Use current config from node with given nodeid",
    (uchar**) &g_config_from_node, (uchar**) &g_config_from_node,
    0, GET_INT, REQUIRED_ARG, INT_MIN, INT_MIN, 0, 0, 0, 0},
  { "query_all", 'a', "Query all the options",
    (uchar**)&g_query_all, (uchar**)&g_query_all,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "diff_default", NDB_OPT_NOSHORT, "print parameters that are different from default",
    (uchar**)&g_diff_default, (uchar**)&g_diff_default,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void short_usage_sub(void)
{
  ndb_short_usage_sub(NULL);
}

static void usage_extra()
{
  char desc[] =
    "This program will retreive config options for a ndb cluster\n";
  puts(desc);
}

/**
 * Match/Apply framework
 */
struct Match
{
  int m_key;
  BaseString m_value;
  Match() {}
  virtual int eval(const Iter&);
  virtual ~Match() {}
};

struct HostMatch : public Match
{
  HostMatch() {}
  virtual int eval(const Iter&);
};

struct Apply
{
  Apply() {}
  Apply(const char *s):m_name(s) {}
  BaseString m_name;
  virtual int apply(const Iter&) = 0;
  virtual ~Apply() {}
};


struct ParamApply : public Apply
{
  ParamApply(int val,const char *s) :Apply(s), m_key(val) {}
  int m_key;
  virtual int apply(const Iter&);
};

struct NodeTypeApply : public Apply
{
  NodeTypeApply(const char *s) :Apply(s) {}
  virtual int apply(const Iter&);
};

struct ConnectionTypeApply : public Apply
{
  ConnectionTypeApply(const char *s) :Apply(s) {}
  virtual int apply(const Iter&);
};

static int parse_query(Vector<Apply*>&, int &argc, char**& argv);
static int parse_where(Vector<Match*>&, int &argc, char**& argv);
static int eval(const Iter&, const Vector<Match*>&);
static int apply(const Iter&, const Vector<Apply*>&);
static int print_diff(const Iter&);
static ndb_mgm_configuration* fetch_configuration(int from_node);
static ndb_mgm_configuration* load_configuration();


int
main(int argc, char** argv){
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage_extra);
  bool print_headers = false;
  if (opts.handle_options())
    exit(255);

  if (g_configinfo)
  {
    ConfigInfo info;
    if (g_xml)
      info.print_xml();
    else
      info.print();
    exit(0);
  }

  if ((g_nodes && g_connections) ||
      (g_system && (g_nodes || g_connections)))
  {
    fprintf(stderr,
        "Error: Only one of the section-options: --nodes, --connections, --system is allowed.\n");
    exit(255);
  }

  /* There is no explicit option for the user to set
   * 'retrieving config from mgmd', but this is the default.
   * Therefore will not contradict with other sources.
   */

  if ((g_config_file && g_mycnf) ||
      ((g_config_from_node != INT_MIN) && (g_config_file || g_mycnf)))
  {
    fprintf(stderr,
	    "Error: Config should be retrieved from only one of the following sources:\n");
    fprintf(stderr,
            "\tconfig stored at mgmd (default),\n");
    fprintf(stderr,
            "\tconfig stored at a data node (--config_from_node=<nodeid>), \n");
    fprintf(stderr,
            "\tmy.cnf(--mycnf=<my.cnf file>),\n");
    fprintf(stderr,
             "\tconfig.file (--config_file=<config file>).\n");
    exit(255);
  }

  g_section = CFG_SECTION_NODE; //default
  if (g_connections)
    g_section = CFG_SECTION_CONNECTION;
  else if (g_system)
    g_section = CFG_SECTION_SYSTEM;

  ndb_mgm_configuration * conf = 0;

  if (g_config_file || g_mycnf)
    conf = load_configuration();
  else
    conf = fetch_configuration(g_config_from_node);

  if (conf == 0)
  {
    exit(255);
  }

  Vector<Apply*> select_list;
  Vector<Match*> where_clause;

  if(strcmp(g_row_delimiter, "\\n") == 0)
    g_row_delimiter = "\n";
  if(strcmp(g_field_delimiter, "\\n") == 0)
    g_field_delimiter = "\n";

  if (g_query_all)
  {
    if (g_query)
    {
      fprintf(stderr,
          "Error: Only one of the options: --query_all, --query is allowed.\n");
      exit(0);
    }
    print_headers = true;
  }

  if(parse_query(select_list, argc, argv))
  {
    exit(0);
  }

  if(parse_where(where_clause, argc, argv))
  {
    exit(0);
  }

  if (print_headers)
  {
    printf("%s", select_list[0]->m_name.c_str());
    for (unsigned i = 1; i < select_list.size(); i++)
    {
      printf("%s", g_field_delimiter);
      printf("%s", select_list[i]->m_name.c_str());
    }
    printf("%s", g_row_delimiter);
  }

  Iter iter(* conf, g_section);
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
      if (g_diff_default)
      {
        print_diff(iter);
      }
    }
  }
  printf("\n");
  return 0;
}

static
int
print_diff(const Iter& iter)
{
  //works better with this --diff_default --fields=" " --rows="\n"
  Uint32 val32;
  Uint64 val64;
  const char* config_value;
  const char* node_type;
  char str[300] = {0};

  if (iter.get(CFG_TYPE_OF_SECTION, &val32) == 0)
  {
    if (val32 == 0)
      node_type = "DB";
    else if (val32 == 1)
      node_type = "API";
    else if (val32 == 2)
      node_type = "MGM";
  }


  if (iter.get(3, &val32) == 0)
  {
    printf("config of node id %d that is different from default", val32);
    printf("%s", g_row_delimiter);
    printf("CONFIG_PARAMETER");
    printf("%s", g_field_delimiter);
    printf("ACTUAL_VALUE");
    printf("%s", g_field_delimiter);
    printf("DEFAULT_VALUE");
    printf("%s", g_row_delimiter);
  }

  for (int p = 0; p < ConfigInfo::m_NoOfParams; p++)
  {
    if ((g_section == CFG_SECTION_CONNECTION &&
        (strcmp(ConfigInfo::m_ParamInfo[p]._section, "TCP") == 0 ||
        strcmp(ConfigInfo::m_ParamInfo[p]._section, "SCI") == 0 ||
        strcmp(ConfigInfo::m_ParamInfo[p]._section, "SHM") == 0))
        ||
        (g_section == CFG_SECTION_NODE &&
        (strcmp(ConfigInfo::m_ParamInfo[p]._section, "DB") == 0 ||
        strcmp(ConfigInfo::m_ParamInfo[p]._section, "API") == 0 ||
        strcmp(ConfigInfo::m_ParamInfo[p]._section, "MGM") == 0))
        ||
        (g_section == CFG_SECTION_SYSTEM))
    {
      if (iter.get(ConfigInfo::m_ParamInfo[p]._paramId, &val32) == 0)
      {
        sprintf(str, "%u", val32);
      }
      else if (iter.get(ConfigInfo::m_ParamInfo[p]._paramId, &val64) == 0)
      {
        sprintf(str, "%llu", val64);
      }
      else if (iter.get(ConfigInfo::m_ParamInfo[p]._paramId, &config_value) == 0)
      {
        strncpy(str, config_value,300);
      }
      else
      {
        continue;
      }

      if ((MANDATORY != ConfigInfo::m_ParamInfo[p]._default)
          && (ConfigInfo::m_ParamInfo[p]._default)
          && !strcmp(node_type, ConfigInfo::m_ParamInfo[p]._section)
          && strcmp(str, ConfigInfo::m_ParamInfo[p]._default)          )
      {
        char parse_str[300] = {0};
        bool convert_bytes = false;
        uint64 memory_convert,def_value = 0;
        uint len = strlen(ConfigInfo::m_ParamInfo[p]._default) - 1;
        strncpy(parse_str, ConfigInfo::m_ParamInfo[p]._default,299);
        if (parse_str[len] == 'M' || parse_str[len] == 'm')
        {
          memory_convert = 1048576;
          convert_bytes = true;
        }
        if (parse_str[len] == 'K' || parse_str[len] == 'k')
        {
          memory_convert = 1024;
          convert_bytes = true;
        }
        if (parse_str[len] == 'G' || parse_str[len] == 'g')
        {
          memory_convert = 1099511627776ULL;
          convert_bytes = true;
        }

        if (convert_bytes)
        {
          parse_str[len] = '\0';
          def_value = atoi(parse_str);
          memory_convert = memory_convert * def_value;
          BaseString::snprintf(parse_str, 299, "%llu", memory_convert);
          if (!strcmp(str, parse_str))
          {
            continue;
          }
        }

        if ((!strcmp("true", ConfigInfo::m_ParamInfo[p]._default) && !strcmp(str, "1")) ||
            (!strcmp("true", ConfigInfo::m_ParamInfo[p]._default) && !strcmp(str, "0")))
        {
          continue;
        }

        printf("%s", ConfigInfo::m_ParamInfo[p]._fname);
        printf("%s", g_field_delimiter);
        printf("%s", str);
        printf("%s", g_field_delimiter);
        printf("%s", ConfigInfo::m_ParamInfo[p]._default);
        printf("%s", g_row_delimiter);
      }
    }
  }
  return 0;
}

static
int
helper(Vector<Apply*>& select, const char * str)
{
  bool all = false;
  bool retflag = false;

  if (g_query_all)
  {
    all = true;
  }

  if (g_section == CFG_SECTION_NODE)
  {
    if (all)
    {
      select.push_back(new ParamApply(CFG_NODE_ID, "nodeid"));
      select.push_back(new ParamApply(CFG_NODE_HOST, "host"));
      select.push_back(new NodeTypeApply("type"));
    }
    else if (native_strcasecmp(str, "nodeid") == 0)
    {
      select.push_back(new ParamApply(CFG_NODE_ID, "nodeid"));
      retflag = true;
    }
    else if (native_strncasecmp(str, "host", 4) == 0)
    {
      select.push_back(new ParamApply(CFG_NODE_HOST, "host"));
      retflag = true;
    }
    else if (native_strcasecmp(str, "type") == 0)
    {
      select.push_back(new NodeTypeApply("type"));
      retflag = true;
    }
  }
  else if (g_section == CFG_SECTION_CONNECTION)
  {
    if (all || native_strcasecmp(str, "type") == 0)
    {
      select.push_back(new ConnectionTypeApply("type"));
      retflag = true;
    }
  }
  if (all || !retflag)
  {
    bool found = false;
    for (int p = 0; p < ConfigInfo::m_NoOfParams; p++)
    {
      if (0)ndbout_c("%s %s",
          ConfigInfo::m_ParamInfo[p]._section,
          ConfigInfo::m_ParamInfo[p]._fname);
      if ((g_section == CFG_SECTION_CONNECTION &&
        (strcmp(ConfigInfo::m_ParamInfo[p]._section, "TCP") == 0 ||
          strcmp(ConfigInfo::m_ParamInfo[p]._section, "SCI") == 0 ||
          strcmp(ConfigInfo::m_ParamInfo[p]._section, "SHM") == 0))
        ||
        (g_section == CFG_SECTION_NODE &&
        (strcmp(ConfigInfo::m_ParamInfo[p]._section, "DB") == 0 ||
          strcmp(ConfigInfo::m_ParamInfo[p]._section, "API") == 0 ||
          strcmp(ConfigInfo::m_ParamInfo[p]._section, "MGM") == 0))
        ||
        (g_section == CFG_SECTION_SYSTEM))
      {
        if (all || native_strcasecmp(ConfigInfo::m_ParamInfo[p]._fname, str) == 0)
        {
          select.push_back(new ParamApply(ConfigInfo::m_ParamInfo[p]._paramId,
            ConfigInfo::m_ParamInfo[p]._fname));
          if (!all)
          {
            found = true;
            break;
          }
        }
      }
    }
    if (!all && !found)
    {
      fprintf(stderr, "Unknown query option: %s\n", str);
      return 1;
    }
  }
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
      if (helper(select, str))
      {
        return 1;
      }
    }
  }
  if (g_query_all)
  {
    return helper(select, NULL);
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
    HostMatch *tmp = new HostMatch;
    tmp->m_key = CFG_NODE_HOST;
    tmp->m_value.assfmt("%s", g_host);
    where.push_back(tmp);
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
    if(my_strtoll(m_value.c_str(), (char **)NULL, 10) != (long long)val64)
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
HostMatch::eval(const Iter& iter)
{
  const char* valc;
  
  if(iter.get(m_key, &valc) == 0)
  {
    struct hostent *h1, *h2, copy1;
    char *addr1;

    if (m_value.empty())
    {
      return 0;
    }
    h1 = gethostbyname(m_value.c_str());
    if (h1 == NULL)
    {
      return 0;
    }

    // gethostbyname returns a pointer to a static structure
    // so we need to copy the results before doing the next call
    memcpy(&copy1, h1, sizeof(struct hostent));
    addr1 = (char *)malloc(copy1.h_length);
    NdbAutoPtr<char> tmp_aptr(addr1);
    memcpy(addr1, h1->h_addr, copy1.h_length);

    if (valc == NULL || strlen(valc) == 0)
    {
      return 0;
    }
    h2 = gethostbyname(valc);
    if (h2 == NULL)
    {
      return 0;
    }

    if (copy1.h_addrtype != h2->h_addrtype)
    {
      return 0;
    }

    if (copy1.h_length != h2->h_length) 
    {
      return 0;
    }
 
    return 0 ==  memcmp(addr1, h2->h_addr, copy1.h_length);	  
  }

  return 0;
}

int
ParamApply::apply(const Iter& iter)
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

int
ConnectionTypeApply::apply(const Iter& iter)
{
  Uint32 val32;
  if (iter.get(CFG_TYPE_OF_SECTION, &val32) == 0)
  {
    switch (val32)
    {
    case CONNECTION_TYPE_TCP:
      printf("tcp");
      break;
    case CONNECTION_TYPE_SCI:
      printf("sci");
      break;
    case CONNECTION_TYPE_SHM:
      printf("shm");
      break;
    default:
      printf("<unknown>");
      break;
    }
  } 
  return 0;
}

static ndb_mgm_configuration*
fetch_configuration(int from_node)
{
  ndb_mgm_configuration* conf = 0;
  NdbMgmHandle mgm = ndb_mgm_create_handle();
  if(mgm == NULL) {
    fprintf(stderr, "Cannot create handle to management server.\n");
    return 0;
  }

  ndb_mgm_set_error_stream(mgm, stderr);
  
  if (ndb_mgm_set_connectstring(mgm, opt_ndb_connectstring))
  {
    fprintf(stderr, "* %5d: %s\n", 
	    ndb_mgm_get_latest_error(mgm),
	    ndb_mgm_get_latest_error_msg(mgm));
    fprintf(stderr, 
	    "*        %s", ndb_mgm_get_latest_error_desc(mgm));
    goto noconnect;
  }

  if(ndb_mgm_connect(mgm, opt_connect_retries - 1, opt_connect_retry_delay, 1))
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
	 
  if (from_node == INT_MIN)
  {
    // from_node option is not requested.
    // Retrieve config from the default src: mgmd
    conf = ndb_mgm_get_configuration(mgm, 0);
  }
  else if (from_node < 1)
  {
    fprintf(stderr, "Invalid node number %d is given for --config_from_node.\n", from_node);
    goto noconnect;
  }
  else
  {
    // Retrieve config from the given data node
     conf = ndb_mgm_get_configuration_from_node(mgm, from_node);
  }

  if(conf == 0)
  {
    fprintf(stderr, "Could not get configuration, ");
    fprintf(stderr, "error code: %d, error msg: %s\n",
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

#include "../src/mgmsrv/Config.hpp"
#include <EventLogger.hpp>

extern EventLogger *g_eventLogger;

static ndb_mgm_configuration*
load_configuration()
{
  g_eventLogger->removeAllHandlers();
  g_eventLogger->createConsoleHandler(ndberr);
  g_eventLogger->setCategory("ndb_config");
  InitConfigFileParser parser;
  if (g_config_file)
  {
    if (g_verbose)
      fprintf(stderr, "Using config.ini : %s\n", g_config_file);
    
    Config* conf = parser.parseConfig(g_config_file);
    if (conf)
      return conf->m_configValues;
    return 0;
  }
  
  if (g_verbose)
    fprintf(stderr, "Using my.cnf\n");
  
  Config* conf = parser.parse_mycnf();
  if (conf)
    return conf->m_configValues;

  return 0;
}
