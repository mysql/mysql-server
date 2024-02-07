/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <NdbTCP.h>
#include <inttypes.h>
#include <mgmapi.h>
#include <NdbAutoPtr.hpp>
#include <NdbOut.hpp>
#include "../src/mgmapi/mgmapi_configuration.hpp"
#include "NdbToolsProgramExitCodes.hpp"
#include "mgmcommon/Config.hpp"
#include "mgmcommon/ConfigInfo.hpp"
#include "mgmcommon/ConfigRetriever.hpp"
#include "mgmcommon/InitConfigFileParser.hpp"
#include "portlib/ssl_applink.h"
#include "storage/ndb/include/mgmcommon/NdbMgm.hpp"
#include "util/cstrbuf.h"

#include "my_alloc.h"

static int g_verbose = 0;

static int g_nodes, g_connections, g_system, g_section;
static const char *g_query = 0;
static int g_query_all = 0;
static int g_diff_default = 0;

static int g_nodeid = 0;
static const char *g_type = 0;
static const char *g_host = 0;
static const char *g_field_delimiter = ",";
static const char *g_row_delimiter = " ";
static const char *g_config_binary_file = nullptr;
static const char *g_config_file = 0;
static int g_mycnf = 0;
static int g_configinfo = 0;
static int g_xml = 0;
static int g_config_from_node = 0;
static const char *g_cluster_config_suffix = nullptr;

const char *load_default_groups[] = {"mysql_cluster", 0};

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    NdbStdOpt::ndb_connectstring,
    NdbStdOpt::mgmd_host,
    NdbStdOpt::connectstring,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::connect_retries,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    NDB_STD_OPT_DEBUG{"nodes", NDB_OPT_NOSHORT, "Print nodes", &g_nodes,
                      nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
                      nullptr},
    {"connections", NDB_OPT_NOSHORT, "Print connections", &g_connections,
     nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"system", NDB_OPT_NOSHORT, "Print system", &g_system, nullptr, nullptr,
     GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"query", 'q', "Query option(s)", &g_query, nullptr, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"host", NDB_OPT_NOSHORT, "Host", &g_host, nullptr, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"type", NDB_OPT_NOSHORT, "Type of node/connection", &g_type, nullptr,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"nodeid", NDB_OPT_NOSHORT, "Nodeid", &g_nodeid, nullptr, nullptr, GET_INT,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"fields", 'f', "Field separator", &g_field_delimiter, nullptr, nullptr,
     GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"rows", 'r', "Row separator", &g_row_delimiter, nullptr, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"config-binary-file", NDB_OPT_NOSHORT,
     "Binary config file (i.e. ndb_1_config.bin.2)", &g_config_binary_file,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"config-file", NDB_OPT_NOSHORT, "Path to config.ini", &g_config_file,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"mycnf", NDB_OPT_NOSHORT, "Read config from my.cnf", &g_mycnf, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"cluster-config-suffix", NDB_OPT_NOSHORT,
     "Override defaults-group-suffix when reading cluster configuration in "
     "my.cnf.",
     &g_cluster_config_suffix, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"configinfo", NDB_OPT_NOSHORT, "Print configinfo", &g_configinfo, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"xml", NDB_OPT_NOSHORT, "Print configinfo in xml format", &g_xml, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"config_from_node", NDB_OPT_NOSHORT,
     "Use current config from node with given nodeid", &g_config_from_node,
     nullptr, nullptr, GET_INT, REQUIRED_ARG, INT_MIN, INT_MIN, 0, nullptr, 0,
     nullptr},
    {"query_all", 'a', "Query all the options", &g_query_all, nullptr, nullptr,
     GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"diff_default", NDB_OPT_NOSHORT,
     "print parameters that are different from default", &g_diff_default,
     nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    NdbStdOpt::end_of_options};

static void short_usage_sub(void) { ndb_short_usage_sub(NULL); }

static void usage_extra() {
  char desc[] = "This program will retreive config options for a ndb cluster\n";
  puts(desc);
}

/*
 * Sort all parameters in ConfigInfo::m_NoOfParams by name (_fname) and
 * section (_section).  Put mandatory parameters first and experimental
 * or internal parameters (name starting with _) last.
 */

static std::vector<const ConfigInfo::ParamInfo *> SortedParamInfo(
    ConfigInfo::m_NoOfParams);

static bool paraminfo_order(const ConfigInfo::ParamInfo *x,
                            const ConfigInfo::ParamInfo *y) {
  // Move mandatory parameters first
  int cmp = int(x->_default != MANDATORY) - int(y->_default != MANDATORY);
  if (cmp != 0) return (cmp < 0);

  // Move parameters with names starting with _ last
  cmp = int(x->_fname[0] == '_') - int(y->_fname[0] == '_');
  if (cmp != 0) return (cmp < 0);

  cmp = native_strcasecmp(x->_fname, y->_fname);
  if (cmp != 0) return (cmp < 0);

  cmp = native_strcasecmp(x->_section, y->_section);
  return (cmp < 0);
}

static void sort_paraminfo() {
  for (int i = 0; i < ConfigInfo::m_NoOfParams; i++)
    SortedParamInfo[i] = &ConfigInfo::m_ParamInfo[i];
  std::sort(SortedParamInfo.begin(), SortedParamInfo.end(), paraminfo_order);
}

/**
 * Match/Apply framework
 */
struct Match {
  int m_key;
  BaseString m_value;
  Match() {}
  virtual int eval(const ndb_mgm_configuration_iterator &);
  virtual ~Match() = default;
  Match(const Match &) = default;
};

struct HostMatch : public Match {
  HostMatch() {}
  int eval(const ndb_mgm_configuration_iterator &) override;
};

struct Apply {
  Apply() {}
  Apply(const char *s) : m_name(s) {}
  BaseString m_name;
  virtual int apply(const ndb_mgm_configuration_iterator &) = 0;
  virtual ~Apply() {}
};

struct ParamApply : public Apply {
  ParamApply(int val, const char *s) : Apply(s), m_key(val) {}
  int m_key;
  int apply(const ndb_mgm_configuration_iterator &) override;
};

struct NodeTypeApply : public Apply {
  NodeTypeApply(const char *s) : Apply(s) {}
  int apply(const ndb_mgm_configuration_iterator &) override;
};

struct ConnectionTypeApply : public Apply {
  ConnectionTypeApply(const char *s) : Apply(s) {}
  int apply(const ndb_mgm_configuration_iterator &) override;
};

static int parse_query(Vector<Apply *> &, int &argc, char **&argv);
static int parse_where(Vector<Match *> &, int &argc, char **&argv);
static int eval(const ndb_mgm_configuration_iterator &,
                const Vector<Match *> &);
static int apply(const ndb_mgm_configuration_iterator &,
                 const Vector<Apply *> &);
static int print_diff(int section, const ndb_mgm_configuration_iterator &);
static ndb_mgm_configuration *fetch_configuration(int from_node);
static ndb_mgm_configuration *load_configuration();

static ndb_mgm_configuration *load_binary_config(const char *config_name) {
  BaseString err;
  ndb_mgm::config_ptr config = ConfigRetriever::getConfig(config_name, err);
  if (config) return config.release();
  fprintf(stderr, "Error: %s\n", err.c_str());
  return nullptr;
}

static ndb_mgm::config_ptr get_config() {
  ndb_mgm_configuration *conf;
  if (g_config_file || g_mycnf)
    conf = load_configuration();
  else if (g_config_binary_file)
    conf = load_binary_config(g_config_binary_file);
  else
    conf = fetch_configuration(g_config_from_node);
  return ndb_mgm::config_ptr(conf);
}

int main(int argc, char **argv) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options);
  opts.set_usage_funcs(short_usage_sub, usage_extra);
  bool print_headers = false;

  if (opts.handle_options()) exit(255);

  if (g_configinfo) {
    ConfigInfo info;
    if (g_xml)
      info.print_xml();
    else
      info.print();
    exit(0);
  }

  if ((g_nodes && g_connections) || (g_system && (g_nodes || g_connections))) {
    fprintf(stderr,
            "Error: Only one of the section-options: --nodes, --connections, "
            "--system is allowed.\n");
    exit(255);
  }

  /* There is no explicit option for the user to set
   * 'retrieving config from mgmd', but this is the default.
   * Therefore will not contradict with other sources.
   */

  if ((g_config_file && g_mycnf) ||
      ((g_config_from_node != INT_MIN) && (g_config_file || g_mycnf))) {
    fprintf(stderr,
            "Error: Config should be retrieved from only one of the following "
            "sources:\n");
    fprintf(stderr, "\tconfig stored at mgmd (default),\n");
    fprintf(stderr,
            "\tconfig stored at a data node (--config_from_node=<nodeid>), \n");
    fprintf(stderr, "\tmy.cnf(--mycnf=<my.cnf file>),\n");
    fprintf(stderr, "\tconfig.file (--config_file=<config file>).\n");
    exit(255);
  }

  if (!g_nodes && !g_connections && !g_system && !g_diff_default) {
    // Default to --nodes, unless --diff-default
    g_nodes = true;
  }

  if (g_type != nullptr) {
    // Normalize node type to one of DB, API, MGM.
    ndb_mgm_node_type type = ndb_mgm_match_node_type(g_type);
    switch (type) {
      case NDB_MGM_NODE_TYPE_NDB:
        g_type = "DB";
        break;
      case NDB_MGM_NODE_TYPE_API:
        g_type = "API";
        break;
      case NDB_MGM_NODE_TYPE_MGM:
        g_type = "MGM";
        break;
      default:
        if (g_nodes) {
          fprintf(stderr, "Unknown node type '%s'.\n", g_type);
          return NdbToolsProgramExitCode::WRONG_ARGS;
        }
    }
  }

  if (strcmp(g_row_delimiter, "\\n") == 0) g_row_delimiter = "\n";
  if (strcmp(g_field_delimiter, "\\n") == 0) g_field_delimiter = "\n";

  const ndb_mgm::config_ptr conf = get_config();
  if (conf == nullptr) {
    exit(255);
  }

  if (g_diff_default) {
    sort_paraminfo();

    if (!g_nodes && !g_connections && !g_system) {
      g_nodes = g_connections = g_system = true;
    }

    std::vector<int> sections;
    if (g_system) sections.push_back(CFG_SECTION_SYSTEM);
    if (g_nodes) sections.push_back(CFG_SECTION_NODE);
    if (g_connections) sections.push_back(CFG_SECTION_CONNECTION);

    for (auto section : sections) {
      ndb_mgm_configuration_iterator iter(conf.get(), section);
      for (iter.first(); iter.valid(); iter.next()) {
        print_diff(section, iter);
      }
    }
    return NdbToolsProgramExitCode::OK;
  }

  g_section = CFG_SECTION_NODE;  // default
  if (g_connections)
    g_section = CFG_SECTION_CONNECTION;
  else if (g_system)
    g_section = CFG_SECTION_SYSTEM;

  Vector<Apply *> select_list;
  Vector<Match *> where_clause;

  if (g_query_all) {
    if (g_query) {
      fprintf(
          stderr,
          "Error: Only one of the options: --query_all, --query is allowed.\n");
      exit(0);
    }
    print_headers = true;
  }

  if (parse_query(select_list, argc, argv)) {
    exit(0);
  }

  if (parse_where(where_clause, argc, argv)) {
    exit(0);
  }

  if (print_headers) {
    printf("%s", select_list[0]->m_name.c_str());
    for (unsigned i = 1; i < select_list.size(); i++) {
      printf("%s", g_field_delimiter);
      printf("%s", select_list[i]->m_name.c_str());
    }
    printf("%s", g_row_delimiter);
  }

  ndb_mgm_configuration_iterator iter(conf.get(), g_section);

  bool prev = false;
  for (iter.first(); iter.valid(); iter.next()) {
    if (eval(iter, where_clause)) {
      if (prev) printf("%s", g_row_delimiter);
      prev = true;
      apply(iter, select_list);
    }
  }
  printf("\n");
  for (unsigned i = 0; i < select_list.size(); i++) {
    delete select_list[i];
  }

  for (unsigned i = 0; i < where_clause.size(); i++) {
    delete where_clause[i];
  }
  return 0;
}

static int print_diff(int section, const ndb_mgm_configuration_iterator &iter) {
  Uint32 val32;
  Uint64 val64;
  const char *config_value;
  const char *section_name = nullptr;

  require(iter.get(CFG_TYPE_OF_SECTION, &val32) == 0);
  if (section == CFG_SECTION_NODE) {
    if (val32 == NODE_TYPE_DB)
      section_name = "DB";
    else if (val32 == NODE_TYPE_API)
      section_name = "API";
    else if (val32 == NODE_TYPE_MGM)
      section_name = "MGM";
    else
      assert(val32 < 3);
  } else if (section == CFG_SECTION_CONNECTION) {
    if (val32 == CONNECTION_TYPE_TCP)
      section_name = "TCP";
    else if (val32 == CONNECTION_TYPE_SHM)
      section_name = "SHM";
    else if (val32 == CONNECTION_TYPE_SCI)
      section_name = "SCI";
    else if (val32 == CONNECTION_TYPE_OSE)
      section_name = "OSE";
    else
      assert(val32 < 4);
  } else if (section == CFG_SECTION_SYSTEM) {
    assert(val32 == CFG_SECTION_SYSTEM);
    section_name = "SYSTEM";
  }
  require(section_name != nullptr);

  if (g_type && native_strcasecmp(g_type, section_name) != 0) {
    // Ignore this section
    return 0;
  }

  if (section == CFG_SECTION_NODE) {
    if (iter.get(CFG_NODE_ID, &val32) != 0) {
      val32 = 0;
    }
    printf("config of [%s] node id %d that is different from default",
           section_name, val32);
  } else if (section == CFG_SECTION_CONNECTION) {
    Uint32 node1, node2;
    if (iter.get(CFG_CONNECTION_NODE_1, &node1) != 0) {
      node1 = 0;
    }
    if (iter.get(CFG_CONNECTION_NODE_2, &node2) != 0) {
      node2 = 0;
    }
    printf(
        "config of [%s] connection between node id %d and %d that is "
        "different from default",
        section_name, node1, node2);
  } else if (section == CFG_SECTION_SYSTEM) {
    printf("config of [%s] system", section_name);
  }
  printf("%s", g_row_delimiter);
  printf("CONFIG_PARAMETER");
  printf("%s", g_field_delimiter);
  printf("ACTUAL_VALUE");
  printf("%s", g_field_delimiter);
  printf("DEFAULT_VALUE");
  printf("%s", g_row_delimiter);

  for (auto p : SortedParamInfo) {
    if (p->_type == ConfigInfo::CI_SECTION) continue;
    if (native_strcasecmp(section_name, p->_section) != 0) continue;

    cstrbuf<20 + 1> str_buf;  // enough for 64-bit decimal number
    const char *str = nullptr;
    if (iter.get(p->_paramId, &val32) == 0) {
      require(str_buf.appendf("%u", val32) == 0);
      str = str_buf.c_str();
    } else if (iter.get(p->_paramId, &val64) == 0) {
      require(str_buf.appendf("%ju", uintmax_t{val64}) == 0);
      str = str_buf.c_str();
    } else if (iter.get(p->_paramId, &config_value) == 0) {
      str = config_value;
    } else {
      continue;
    }
    require(str != nullptr);

    auto normalized_str = ConfigInfo::normalizeParamValue(*p, str);
    require(normalized_str.has_value());
    if (p->_default != MANDATORY && p->_default != nullptr) {
      auto normalized_def = ConfigInfo::normalizeParamValue(*p, p->_default);
      require(normalized_def.has_value());
      if (normalized_str == normalized_def)
        continue;  // default value in config
    }

    printf("%s", p->_fname);
    printf("%s", g_field_delimiter);
    printf("%s", normalized_str.value().c_str());
    printf("%s", g_field_delimiter);
    if (MANDATORY == p->_default)
      printf("(mandatory)");
    else if (nullptr == p->_default)
      printf("(null)");
    else
      printf("%s", p->_default);
    printf("%s", g_row_delimiter);
  }
  printf("\n");
  return 0;
}

static int helper(Vector<Apply *> &select, const char *str) {
  bool all = false;
  bool retflag = false;

  if (g_query_all) {
    all = true;
  }

  if (g_section == CFG_SECTION_NODE) {
    if (all) {
      select.push_back(new ParamApply(CFG_NODE_ID, "nodeid"));
      select.push_back(new ParamApply(CFG_NODE_HOST, "host"));
      select.push_back(new NodeTypeApply("type"));
    } else if (native_strcasecmp(str, "nodeid") == 0) {
      select.push_back(new ParamApply(CFG_NODE_ID, "nodeid"));
      retflag = true;
    } else if (native_strncasecmp(str, "host", 4) == 0) {
      select.push_back(new ParamApply(CFG_NODE_HOST, "host"));
      retflag = true;
    } else if (native_strcasecmp(str, "type") == 0) {
      select.push_back(new NodeTypeApply("type"));
      retflag = true;
    }
  } else if (g_section == CFG_SECTION_CONNECTION) {
    if (all || native_strcasecmp(str, "type") == 0) {
      select.push_back(new ConnectionTypeApply("type"));
      retflag = true;
    }
  }
  if (all || !retflag) {
    bool found = false;
    for (int p = 0; p < ConfigInfo::m_NoOfParams; p++) {
      if (0)
        ndbout_c("%s %s", ConfigInfo::m_ParamInfo[p]._section,
                 ConfigInfo::m_ParamInfo[p]._fname);
      if ((g_section == CFG_SECTION_CONNECTION &&
           (strcmp(ConfigInfo::m_ParamInfo[p]._section, "TCP") == 0 ||
            strcmp(ConfigInfo::m_ParamInfo[p]._section, "SHM") == 0)) ||
          (g_section == CFG_SECTION_NODE &&
           (strcmp(ConfigInfo::m_ParamInfo[p]._section, "DB") == 0 ||
            strcmp(ConfigInfo::m_ParamInfo[p]._section, "API") == 0 ||
            strcmp(ConfigInfo::m_ParamInfo[p]._section, "MGM") == 0)) ||
          (g_section == CFG_SECTION_SYSTEM)) {
        if (all ||
            native_strcasecmp(ConfigInfo::m_ParamInfo[p]._fname, str) == 0) {
          select.push_back(new ParamApply(ConfigInfo::m_ParamInfo[p]._paramId,
                                          ConfigInfo::m_ParamInfo[p]._fname));
          if (!all) {
            found = true;
            break;
          }
        }
      }
    }
    if (!all && !found) {
      fprintf(stderr, "Unknown query option: %s\n", str);
      return 1;
    }
  }
  return 0;
}

static int parse_query(Vector<Apply *> &select, int &argc, char **&argv) {
  if (g_query) {
    BaseString q(g_query);
    Vector<BaseString> list;
    q.split(list, ",");
    for (unsigned i = 0; i < list.size(); i++) {
      const char *str = list[i].c_str();
      if (helper(select, str)) {
        return 1;
      }
    }
  }
  if (g_query_all) {
    return helper(select, NULL);
  }
  return 0;
}

static int parse_where(Vector<Match *> &where, int &argc, char **&argv) {
  Match m;
  if (g_host) {
    HostMatch *tmp = new HostMatch;
    tmp->m_key = CFG_NODE_HOST;
    tmp->m_value.assfmt("%s", g_host);
    where.push_back(tmp);
  }

  if (g_type) {
    m.m_key = CFG_TYPE_OF_SECTION;
    m.m_value.assfmt("%d", ndb_mgm_match_node_type(g_type));
    Match *tmp = new Match(m);
    where.push_back(tmp);
  }

  if (g_nodeid) {
    m.m_key = CFG_NODE_ID;
    m.m_value.assfmt("%d", g_nodeid);
    Match *tmp = new Match(m);
    where.push_back(tmp);
  }
  return 0;
}

template class Vector<Apply *>;
template class Vector<Match *>;

static int eval(const ndb_mgm_configuration_iterator &iter,
                const Vector<Match *> &where) {
  for (unsigned i = 0; i < where.size(); i++) {
    if (where[i]->eval(iter) == 0) return 0;
  }

  return 1;
}

static int apply(const ndb_mgm_configuration_iterator &iter,
                 const Vector<Apply *> &list) {
  for (unsigned i = 0; i < list.size(); i++) {
    list[i]->apply(iter);
    if (i + 1 != list.size()) printf("%s", g_field_delimiter);
  }
  return 0;
}

int Match::eval(const ndb_mgm_configuration_iterator &iter) {
  Uint32 val32;
  Uint64 val64;
  const char *valc;
  if (iter.get(m_key, &val32) == 0) {
    if (atoi(m_value.c_str()) != (int)val32) return 0;
  } else if (iter.get(m_key, &val64) == 0) {
    if (my_strtoll(m_value.c_str(), (char **)NULL, 10) != (long long)val64)
      return 0;
  } else if (iter.get(m_key, &valc) == 0) {
    if (strcmp(m_value.c_str(), valc) != 0) return 0;
  } else {
    return 0;
  }
  return 1;
}

int HostMatch::eval(const ndb_mgm_configuration_iterator &iter) {
  const char *valc;

  if (iter.get(m_key, &valc) == 0) {
    struct hostent *h1, *h2, copy1;
    char *addr1;

    if (m_value.empty()) {
      return 0;
    }
    h1 = gethostbyname(m_value.c_str());
    if (h1 == NULL) {
      return 0;
    }

    // gethostbyname returns a pointer to a static structure
    // so we need to copy the results before doing the next call
    memcpy(&copy1, h1, sizeof(struct hostent));
    addr1 = (char *)malloc(copy1.h_length);
    NdbAutoPtr<char> tmp_aptr(addr1);
    memcpy(addr1, h1->h_addr, copy1.h_length);

    if (valc == NULL || strlen(valc) == 0) {
      return 0;
    }
    h2 = gethostbyname(valc);
    if (h2 == NULL) {
      return 0;
    }

    if (copy1.h_addrtype != h2->h_addrtype) {
      return 0;
    }

    if (copy1.h_length != h2->h_length) {
      return 0;
    }

    return 0 == memcmp(addr1, h2->h_addr, copy1.h_length);
  }

  return 0;
}

int ParamApply::apply(const ndb_mgm_configuration_iterator &iter) {
  Uint32 val32;
  Uint64 val64;
  const char *valc;
  if (iter.get(m_key, &val32) == 0) {
    printf("%u", val32);
  } else if (iter.get(m_key, &val64) == 0) {
    printf("%llu", val64);
  } else if (iter.get(m_key, &valc) == 0) {
    printf("%s", valc);
  }
  return 0;
}

int NodeTypeApply::apply(const ndb_mgm_configuration_iterator &iter) {
  Uint32 val32;
  if (iter.get(CFG_TYPE_OF_SECTION, &val32) == 0) {
    printf("%s",
           ndb_mgm_get_node_type_alias_string((ndb_mgm_node_type)val32, 0));
  }
  return 0;
}

int ConnectionTypeApply::apply(const ndb_mgm_configuration_iterator &iter) {
  Uint32 val32;
  if (iter.get(CFG_TYPE_OF_SECTION, &val32) == 0) {
    switch (val32) {
      case CONNECTION_TYPE_TCP:
        printf("tcp");
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

static ndb_mgm_configuration *fetch_configuration(int from_node) {
  TlsKeyManager tlsKeyManager;
  tlsKeyManager.init_mgm_client(opt_tls_search_path);
  ndb_mgm_configuration *conf = 0;
  NdbMgmHandle mgm = ndb_mgm_create_handle();
  if (mgm == NULL) {
    fprintf(stderr, "Cannot create handle to management server.\n");
    return 0;
  }

  ndb_mgm_set_error_stream(mgm, stderr);

  if (ndb_mgm_set_connectstring(mgm, opt_ndb_connectstring)) {
    fprintf(stderr, "* %5d: %s\n", ndb_mgm_get_latest_error(mgm),
            ndb_mgm_get_latest_error_msg(mgm));
    fprintf(stderr, "*        %s", ndb_mgm_get_latest_error_desc(mgm));
    goto noconnect;
  }

  ndb_mgm_set_ssl_ctx(mgm, tlsKeyManager.ctx());
  if (ndb_mgm_connect_tls(mgm, opt_connect_retries - 1, opt_connect_retry_delay,
                          1, opt_mgm_tls)) {
    fprintf(stderr, "Connect failed");
    fprintf(stderr, " code: %d, msg: %s\n", ndb_mgm_get_latest_error(mgm),
            ndb_mgm_get_latest_error_msg(mgm));
    goto noconnect;
  } else if (g_verbose) {
    fprintf(stderr, "Connected to %s:%d\n", ndb_mgm_get_connected_host(mgm),
            ndb_mgm_get_connected_port(mgm));
  }

  if (from_node == INT_MIN) {
    // from_node option is not requested.
    // Retrieve config from the default src: mgmd
    conf = ndb_mgm_get_configuration(mgm, 0);
  } else if (from_node < 1) {
    fprintf(stderr, "Invalid node number %d is given for --config_from_node.\n",
            from_node);
    goto noconnect;
  } else {
    // Retrieve config from the given data node
    conf = ndb_mgm_get_configuration_from_node(mgm, from_node);
  }

  if (conf == 0) {
    fprintf(stderr, "Could not get configuration, ");
    fprintf(stderr, "error code: %d, error msg: %s\n",
            ndb_mgm_get_latest_error(mgm), ndb_mgm_get_latest_error_msg(mgm));
  } else if (g_verbose) {
    fprintf(stderr, "Fetched configuration\n");
  }

  ndb_mgm_disconnect(mgm);
noconnect:
  ndb_mgm_destroy_handle(&mgm);

  return conf;
}

#include <EventLogger.hpp>
#include "mgmcommon/Config.hpp"

static ndb_mgm_configuration *load_configuration() {
  g_eventLogger->removeAllHandlers();
  g_eventLogger->createConsoleHandler(ndberr);
  g_eventLogger->setCategory("ndb_config");
  InitConfigFileParser parser;
  if (g_config_file) {
    if (g_verbose) fprintf(stderr, "Using config.ini : %s\n", g_config_file);

    Config *conf = parser.parseConfig(g_config_file);
    if (conf) {
      ndb_mgm_configuration *mgm_config = conf->m_configuration;
      conf->m_configuration = nullptr;
      // mgm_config is moved out of config. It has to be freed by caller.
      delete conf;

      return mgm_config;
    }
    return 0;
  }

  if (g_verbose) fprintf(stderr, "Using my.cnf\n");

  Config *conf = parser.parse_mycnf(g_cluster_config_suffix);
  if (conf) {
    ndb_mgm_configuration *mgm_config = conf->m_configuration;
    conf->m_configuration = nullptr;
    // mgm_config is moved out of config. It has to be freed by caller.
    delete conf;

    return mgm_config;
  }

  return 0;
}
