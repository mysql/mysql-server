/*  Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#include "storage/warp/warp_rewrite/warp_rewrite.h"
#include "my_config.h"
#include "mysql.h"
#include <mysql/plugin_audit.h>
#include <mysql/psi/mysql_thread.h>
#include <mysql/psi/mysql_memory.h>

#include <stddef.h>
#include <algorithm>
#include <atomic>
#include <new>
#include <iostream>
#include <unordered_map>
#include <dirent.h>

// MySQL SQL includes
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql_string.h"
#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/log.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/item_sum.h"
#include "sql/mysqld.h"
#include "sql/sql_parse.h"
#include "plugin/rewriter/services.h"
#include "sql/protocol_classic.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysqld_error.h"
#include "storage/warp/warp_rewrite/services.h"
#include "template_utils.h"

#include "../include/fastbit/ibis.h"
#include "../include/fastbit/mensa.h"
#include "../include/fastbit/resource.h"
#include "../include/fastbit/util.h"

#include <vector>
#include <map>
#include <list>
#include <regex>

using std::string;

static MYSQL_PLUGIN plugin_info;
#define PLUGIN_NAME "warp_rewriter"

static PSI_memory_key key_memory_warp_rewrite;

static PSI_memory_info all_rewrite_memory[] = {
    {&key_memory_warp_rewrite, "warp_rewriter", 0, 0, PSI_DOCUMENT_ME}};

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

MYSQL_PLUGIN get_rewriter_plugin_info() { return plugin_info; }

static int warp_rewrite_query_notify(MYSQL_THD thd, mysql_event_class_t event_class,
                                const void *event);
static int warp_rewriter_plugin_init(MYSQL_PLUGIN plugin_ref);
static int warp_rewriter_plugin_deinit(void *);

/* Audit plugin descriptor */
static struct st_mysql_audit warp_rewrite_query_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION, /* interface version */
    nullptr,                       /* release_thd()     */
    warp_rewrite_query_notify,          /* event_notify()    */
    {
        0,
        0,
        (unsigned long)MYSQL_AUDIT_PARSE_ALL,
    } /* class mask        */
};

static MYSQL_THDVAR_BOOL(parallel_query, PLUGIN_VAR_RQCMDARG,
                          "Use parallel query optimization",
                          nullptr, nullptr, false);

static MYSQL_THDVAR_BOOL(reorder_outer, PLUGIN_VAR_RQCMDARG,
                          "Reorder joins with OUTER joins",
                          nullptr, nullptr, true);
static MYSQL_THDVAR_BOOL(extended_syntax, PLUGIN_VAR_RQCMDARG,
                          "Materialized view DDL enhancements",
                          nullptr, nullptr, true);
static MYSQL_THDVAR_ULONG(remote_signal_id, PLUGIN_VAR_RQCMDARG,
                          "Signal ID returned from last remote query execution",
                          nullptr, nullptr, 0, 0, LONG_LONG_MAX, 0);   
static MYSQL_THDVAR_ULONG(remote_server_id, PLUGIN_VAR_RQCMDARG,
                          "Server id of server used in last remote query execution",
                          nullptr, nullptr, 0, 0, LONG_LONG_MAX, 0);                         
static MYSQL_THDVAR_ULONG(remote_query_timeout, PLUGIN_VAR_RQCMDARG,
                          "Timeout value for remote query execution",
                          nullptr, nullptr, 86400, 0, LONG_LONG_MAX, 0);   

SYS_VAR* plugin_system_variables[] = {
  MYSQL_SYSVAR(parallel_query),
  MYSQL_SYSVAR(reorder_outer),
  MYSQL_SYSVAR(extended_syntax),
  MYSQL_SYSVAR(remote_signal_id),
  MYSQL_SYSVAR(remote_server_id),
  MYSQL_SYSVAR(remote_query_timeout),
  NULL
};


/* Plugin descriptor */
mysql_declare_plugin(audit_log){
    MYSQL_AUDIT_PLUGIN,             /* plugin type                   */
    &warp_rewrite_query_descriptor, /* type specific descriptor      */
    PLUGIN_NAME,                    /* plugin name                   */
    "Justin Swanhart",              /* author                        */
    "WarpSQL optimizer enhancements and parallel query plugin",
    PLUGIN_LICENSE_GPL,             /* license                       */
    warp_rewriter_plugin_init,      /* plugin initializer            */
    nullptr,                        /* plugin check uninstall        */
    warp_rewriter_plugin_deinit,    /* plugin deinitializer          */
    0x8021,                         /* version                       */
    nullptr,                        /* status vars (none atm) */
    plugin_system_variables,        /* system vars (none atm) */
    nullptr,                        /* reserverd                     */
    0                               /* flags                         */
} mysql_declare_plugin_end;

std::string strtolower(std::string in) {
  std::string out;
  for(size_t i=0;i<in.length();++i) {
    out += tolower(in[i]);
  }
  return out;
}

std::vector<std::string> custom_lex(std::string sql, char escape_char = '\\') {
  char enclosure_type = 0;
  bool force_capture_next = false;
  std::string token = "";
  std::vector<std::string> tokens;
  bool in_comment = false;
  bool in_line_comment = false;

  for(size_t char_idx = 0; char_idx < sql.length(); ++char_idx) {
    
    /* this block of statements handles SQL commenting */
    if( (sql[char_idx] == '\t' || sql[char_idx] == ' ' || sql[char_idx] == '\r' || sql[char_idx] == '\n') && sql.substr(char_idx+2, 2) == "--" ) {
      in_line_comment = true;
      continue;
    }

    if( (in_line_comment && ((sql[char_idx] == '\r') || (sql[char_idx] == '\n'))) ) {
      in_line_comment = false;
      continue;
    }

    if( (!in_comment) && (sql.substr(char_idx,2) == "/*") ) {
      char_idx+=1;
      in_comment = true;
      continue;
    }
    
    if( in_comment && (sql.substr(char_idx, 2) == "*/") ) {
      char_idx+=1;
      in_comment = false;
      continue;
    }

    /* do not do anything if this is in a comment */
    if( in_comment ) { 
      continue;
    }

    /* Last was escape character so force capture this character
       and move on
    */
    if( force_capture_next == true ) {
      token += sql[char_idx];
      force_capture_next = false;
      continue;
    }

    /* If this is the escape character then the next character has to be
       attached to the current token even if it is an enclosure character.
    */ 
    if( sql[char_idx] == escape_char) {
      force_capture_next = true;
      continue;
    }

    /* If we are in an enclosure and the current character is the enclosure
       character then the enclosure is over.
    */
    if( enclosure_type != 0 && sql[char_idx] == enclosure_type ) {
      token += sql[char_idx];
      tokens.push_back(token);
      enclosure_type = 0;
      token = "";
      continue;
    } 

    /* these are the enclosure characters */
    if( enclosure_type == 0 && (sql[char_idx] == '`' || sql[char_idx] == '\'' || sql[char_idx] == '"') ) {
      enclosure_type = sql[char_idx];
      if(token != "") {
        tokens.push_back(token);
      }

      token = sql[char_idx];
      continue;
    } 

    switch( sql[char_idx] ) {
      /* the dot operator is the schema resolution operator and has to be handled specially */
      case '.':
        if(token != "") {
          tokens.push_back(token);
          
        }
        token="";
        tokens.push_back(".");
        continue;
      break;
      case '\n':
      case ' ':
      case '\t':
      case '\r':
      case ';':
        if(token != "") {
          tokens.push_back(token);
        }
        token = "";
        continue;
      break;  
    }

    token += sql[char_idx];
  }
  
  if( token != "" ) {
    tokens.push_back(token);
  }

  for(size_t i=0;i<tokens.size();++i) {
    if(tokens.size() > i+2) {
      if(tokens[i+1] == ".") {
        tokens[i] += tokens[i+1] + tokens[i+2];
        tokens[i+1]="";
        tokens[i+2]="";
      }
    }
  }
  std::vector<std::string> final_tokens;
  for(size_t i=0;i<tokens.size();++i) {
    if(tokens[i] == "") continue;
    final_tokens.push_back(tokens[i]);
  }

  return final_tokens;
  
}

static int warp_rewriter_plugin_init(MYSQL_PLUGIN plugin_ref) {
  plugin_info = plugin_ref;

  const char *category = "sql";
  int count;

  count = static_cast<int>(array_elements(all_rewrite_memory));
  mysql_memory_register(category, all_rewrite_memory, count);

  // Initialize error logging service.
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;

  return 0;
}

static int warp_rewriter_plugin_deinit(void *) {
  plugin_info = nullptr;
  //delete rewriter;
  //mysql_rwlock_destroy(&LOCK_table);
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

std::string get_warp_partitions(std::string schema, std::string table) {
  std::string path = schema + "/" + table + ".data/";
  std::string parts = "";
    
  DIR* dir = opendir(path.c_str());
  if(dir == NULL) {
    return "";
  }

  struct dirent *ent = NULL;
  while((ent = readdir(dir)) != NULL) {
    if(ent->d_type == DT_DIR) {
      if(ent->d_name[0] != 'p') {
        continue;
      }

      if(parts != "") {
        parts += ", ";
      }

      parts += "('" + std::string(ent->d_name) +"')";
    }
  }
  return parts;
}

bool is_warp_table(std::string schema, std::string table) {
  std::string path = schema + "/" + table + ".data/-part.txt";
  struct stat st;
  auto res = !stat(path.c_str(), &st);
  return res;
}

uint64_t get_warp_row_count(std::string schema, std::string table) {
  std::string path = schema + "/" + table + ".data/";
  ibis::table* base_table = ibis::mensa::create(path.c_str());
  uint64_t rows = base_table->nRows();
  delete base_table;
  //std::cout << std::to_string(rows) << "\n";
  return rows;
}

bool process_having_item(THD* thd, 
                         Item* item, 
                         std::string &coord_having, 
                         std::string &ll_query, 
                         std::string &coord_group, 
                         std::unordered_map<std::string, uint> &used_fields
                         ) 
{ 
  
  String item_str;
  item_str.reserve(1024 * 1024);
  std::string orig_clause;
  std::string op = "";
  std::string new_clause;
  bool is_between = false, is_in = false, is_is_null = false, is_isnot_null = false;
  item_str.set("",0,default_charset_info);
  item->print(thd, &item_str, QT_ORDINARY);
  orig_clause = std::string(item_str.ptr(), item_str.length());
  
  switch(item->type()) {
    case Item::Type::SUM_FUNC_ITEM: 
    { 
      std::string arg_clause;
      Item_sum* sum = dynamic_cast<Item_sum*>(item);

      // gather the arguments            
      auto cnt = sum->arg_count;
      for(uint i = 0; i<cnt; ++i) {
        process_having_item(thd, sum->get_arg(i), arg_clause, ll_query, coord_group, used_fields);
      }
      
      uint max_used_expr_num = 0;
      for(auto it = used_fields.begin();it != used_fields.end(); ++it) {
        if(it->second > max_used_expr_num) {
          max_used_expr_num = it->second;
        }
      }
      
      auto expr_it = used_fields.find(orig_clause);
      
      if(expr_it != used_fields.end()) {
        new_clause += std::string("SUM(");
        if(sum->has_with_distinct()) {
          new_clause += "DISTINCT ";
        }  
        new_clause += std::string("`expr$") + std::to_string(expr_it->second) + "`)";
      } else {
        if(ll_query.length() > 0) {
          ll_query += ",";
        }
        max_used_expr_num++;
        std::string ll_alias = std::string("`expr$") + std::to_string(max_used_expr_num) + "`";
        
        if(sum->has_with_distinct()) {
          ll_query += arg_clause + " AS " + ll_alias;
          used_fields.emplace(std::make_pair(arg_clause, max_used_expr_num));
          new_clause += std::string(sum->func_name()) + "(DISTINCT " + ll_alias + ")";
          coord_group += ll_alias;
        } else {
          used_fields.emplace(std::make_pair(orig_clause, max_used_expr_num));
          ll_query += orig_clause + " AS " + ll_alias;
          new_clause += std::string("SUM(") + ll_alias + ")";
        }
      }
    }
    break;
  
    case Item::Type::FUNC_ITEM:
    {  
      Item_func *tmp = dynamic_cast<Item_func *>(const_cast<Item *>(item));
      

      /* There are only a small number of options currently available for
        filtering at the WARP SE level.  The basic numeric filters are presented
        here.
      */
      switch(tmp->functype()) {
        /* when op = " " there is special handling below because the
          syntax of the given function differs from the "regular"
          functions.
        */
        case Item_func::Functype::BETWEEN:
          is_between = true;
          break;

        case Item_func::Functype::IN_FUNC:
          is_in = true;
          break;

        case Item_func::Functype::ISNULL_FUNC:
          op = " IS NULL";
          is_is_null = true;
          break;

        case Item_func::Functype::ISNOTNULL_FUNC:
          op = "IS NOT NULL";
          is_isnot_null = true;
          break;

        /* normal arg0 OP arg1 type operators */
        case Item_func::Functype::EQ_FUNC:
        case Item_func::Functype::EQUAL_FUNC:
          op = " = ";
          break;

        case Item_func::Functype::LIKE_FUNC:
          op = " LIKE ";
          break;

        case Item_func::Functype::LT_FUNC:
          op = " < ";
          break;

        case Item_func::Functype::GT_FUNC:
          op = " > ";
          break;

        case Item_func::Functype::GE_FUNC:
          op = " >= ";
          break;

        case Item_func::Functype::LE_FUNC:
          op = " <= ";
          break;

        case Item_func::Functype::NE_FUNC:
          op = " != ";
          break;

        default:
          new_clause += std::string(dynamic_cast<Item_func*>(item)->func_name());
      }
      Item **arg = tmp->arguments();
      
      /* BETWEEN AND IN() need some special syntax handling */
      
      for (uint i = 0; i < tmp->arg_count; ++i, ++arg) {
        if((is_is_null || is_isnot_null) && i == tmp->arg_count-1) {
          new_clause += op;
        } else {
          if(i > 0) {
            if(!is_between && !is_in && !is_is_null && !is_isnot_null) { 
              new_clause += op;
            } 
            
            if(is_between) {
              if(i == 1) {
                new_clause += " BETWEEN ";
              } else {
                new_clause += " AND ";
              }
            } 
            if(is_in) {
              if(i == 1) {
                new_clause += " IN (";
              } else {
                new_clause += ", ";
              }
            }
          } 
        }
        process_having_item(thd, tmp->arguments()[i], new_clause, ll_query, coord_group, used_fields);
      }
    }
    break;
      
    default:
      new_clause += orig_clause;
    break;   
  }
  coord_having += new_clause;   
  return true;
}

bool process_having(THD* thd, 
                    Item* cond, 
                    std::string &coord_having, 
                    std::string &ll_query, 
                    std::string &coord_group, 
                    std::unordered_map<std::string, uint> &used_fields) 
{ 
  static int depth=0;
  std::string new_having;
  // reset the variables when called at depth 0
  String item_str;
  item_str.reserve(1024 * 1024);

  /* A simple comparison without conjuction or disjunction */
  if(cond->type() == Item::Type::FUNC_ITEM) {
    process_having_item(thd, cond, new_having, ll_query, coord_group, used_fields);
  } else if(cond->type() == Item::Type::COND_ITEM) {
    auto item_cond = (dynamic_cast<Item_cond *>(const_cast<Item *>(cond)));
    List<Item> items = *(item_cond->argument_list());
    auto cnt = items.size();
    new_having += "(";
    for (uint i = 0; i < cnt; ++i) {
      auto item = items.pop();
      if(i > 0) {
        if(item_cond->functype() == Item_func::Functype::COND_AND_FUNC) {
          new_having += " AND ";
        } else if(item_cond->functype() == Item_func::Functype::COND_OR_FUNC) {
          new_having += " OR ";
        } else {
          return true;
        }
      }
      /* recurse to print the field and other items.  This should be a
         FUNC_ITEM. if it isn't, then the item will be returned by this function
         and pushdown evaluation will be abandoned.
      */
      ++depth;
      if(!process_having(thd, item, new_having, ll_query, coord_group, used_fields) != true) {
        return true;
      }
      --depth;
    }
    new_having += ")";
  }
  coord_having += new_having;
  return false;
}

std::string escape_for_call(const std::string &str) {
  std::string retval;
  retval.reserve(str.length() * 2);
  for(long unsigned int i =0; i< str.length(); ++i) {
    if(str[i] == '"') {
      retval += '\\';
    }
    if(str[i] == '\\') {
	    retval += "\\\\";
    } else {
      retval += str[i];
    }
  }
  return retval;
}
bool warp_alloc_query(THD *thd, const char *packet, size_t packet_length) {
  /* Remove garbage at start and end of query */
  while (packet_length > 0 && my_isspace(thd->charset(), packet[0])) {
    packet++;
    packet_length--;
  }
  const char *pos = packet + packet_length;  // Point at end null
  while (packet_length > 0 &&
         (pos[-1] == ';' || my_isspace(thd->charset(), pos[-1]))) {
    pos--;
    packet_length--;
  }

  char *query = static_cast<char *>(thd->alloc(packet_length + 1));
  if (!query) return true;
  memcpy(query, packet, packet_length);
  query[packet_length] = '\0';

  thd->set_query(query, packet_length);

  return false;
}

int warp_parse_call(MYSQL_THD thd, const MYSQL_LEX_STRING query) {
  // throw away the pre-parsed query state
  thd->end_statement();
  thd->cleanup_after_query();
  
  //start a new query
  lex_start(thd);

  if (warp_alloc_query(thd, query.str, query.length)) {
    return 1;  // Fatal error flag set
  }

  // initialize new parser state for the CALL query
  Parser_state parser_state;
  if (parser_state.init(thd, query.str, query.length)) {
    return 1;
  }

  parser_state.m_input.m_compute_digest = true;
  thd->m_digest = &thd->m_digest_state;
  thd->m_digest->reset(thd->m_token_array, max_digest_length);

  int parse_status = parse_sql(thd, &parser_state, nullptr);
  
  return parse_status;
}

bool desc(std::pair<string, uint64_t>& a, std::pair<string, uint64_t>& b) { 
  return a.second < b.second; 
} 
  
// Function to sort the map according 
// to value in a (key-value) pairs 
std::vector<std::pair<std::string, uint64_t>>sort_from(std::map<string, uint64_t> M) 
{ 
  
    // Declare vector of pairs 
    std::vector<std::pair<std::string, uint64_t>> vec; 
  
    // Copy key-value pair from Map 
    // to vector of pairs 
    for (auto& it : M) { 
        vec.push_back(it); 
    } 
  
    // Sort using comparator function 
    sort(vec.begin(), vec.end(), desc); 
  
    return vec;
}

bool is_remote_query(std::vector<std::string> tokens) {
  std::unordered_map<std::string, int> table_map;
  bool in_single = false; 
  bool in_double = false;
  bool next_is_table_name = false;
  for(size_t i=0; i < tokens.size(); ++i) {
//	  std::cerr << i << ": " << tokens[i];
    std::string lower = strtolower(tokens[i]);

    if(!in_double && !in_single) {
      if(lower[0] == '\'') {
        if(lower[0] == '\'' && (lower[lower.length()-1] != '\'' && lower[lower.length()-2]!='\\')) {
	  in_single = true;
	}
      }
    } else if (in_single) {
        if(lower[lower.length()-1] == '\'' && lower[lower.length()-2]!='\\') {
	  in_single = false;
	}
    }
    if(!in_single && !in_double) {
      if(lower[0] == '\'') {
        if(lower[0] == '"' && (lower[lower.length()-1] != '"' && lower[lower.length()-2]!='\\')) {
	  in_double= true;
	}
      }
    } else if (in_double) {
        if((lower[lower.length()-1] == '"' && lower[lower.length()-2]!='\\')) {
	  in_double= false;
	}
    }


    if( (!in_single && !in_double) && (((lower == "from") || (lower == "join")))) {
      next_is_table_name = true;
      continue;
    }
    if(next_is_table_name) {
      next_is_table_name = false;
      const char* remote = strstr(tokens[i].c_str(), "@");

      if (remote != NULL) {
        std::string remote_host(remote);
	std::cerr << "FOUND REMOTE SERVER: " << remote_host << "\n";
	std::cerr << "TOKEN:" << tokens[i] << "\n";
        auto find_it = table_map.find(remote_host);
        if(find_it == table_map.end()) {
          table_map.emplace(make_pair(remote_host, 1));
        }
      } 
    }
  }
  
  return table_map.size() >0;
}
bool is_valid_remote_query(std::vector<std::string> tokens) {
  //LIXME
  std::unordered_map<std::string, int> table_map;
  int remote_server_count = 0;
  int local_server_count = 0;
  bool next_is_table_name = false;
  for(size_t i=0; i < tokens.size(); ++i) {
    std::string lower = strtolower(tokens[i]);
    
    if((lower == "from") || (lower == "join")) {
      next_is_table_name = true;
      continue;
    }
    if(next_is_table_name) {
      next_is_table_name = false;
      const char* remote = strstr(tokens[i].c_str(), "@");
      if (remote != NULL) {
        std::string remote_host(remote);
        auto find_it = table_map.find(remote_host);
        if(find_it == table_map.end()) {
          remote_server_count++;
          table_map.emplace(make_pair(remote_host, 1));
        }
      } else {
        local_server_count++;
      }
    }
  }
  
  if((remote_server_count == 1) && (local_server_count == 0)) {
    return true;
  }
  return false;
}

std::string get_remote_server(std::vector<std::string> tokens) {
  std::unordered_map<std::string, int> table_map;
  
  bool next_is_table_name = false;
  for(size_t i=0; i < tokens.size(); ++i) {
    std::string lower = strtolower(tokens[i]);
    
    if((lower == "from") || (lower == "join")) {
      next_is_table_name = true;
      continue;
    }
    if(next_is_table_name) {
      next_is_table_name = false;
      const char* remote = strstr(tokens[i].c_str(), "@");
      if (remote != NULL) {
        std::string remote_host(remote);
        auto find_it = table_map.find(remote_host);
        if(find_it == table_map.end()) {
          table_map.emplace(make_pair(remote_host, 1));
          break;
        }
      } 
    }
  }
  
  return table_map.begin()->first;
}

std::string strip_remote_server(std::vector<std::string> tokens, bool strip_ddl = true) {
  std::string out;
  bool next_is_table_name = false;
  bool found_as = false;
  bool is_ddl = false;
  
  size_t i=0;
  for(i=0;i<tokens.size();++i) {
    
    if(tokens[i] != " ") {
      break;
    }
  }
  
  /*if(tokens[i] != "select") {
    tokens[0] = "select";
  }*/
  
  if((strtolower(tokens[0]) == "create") || (strtolower(tokens[0]) == "insert")){
    is_ddl=true;
  }
  for(size_t i=0; i < tokens.size(); ++i) {
    std::string lower = strtolower(tokens[i]);
    if(is_ddl & !found_as && strip_ddl) {
      
      if((lower != "as") && (lower != "select")) {
        continue;
      }
      
      if(lower == "select") {
        out += lower + " ";
      }
      
      found_as = true;
      continue;
    }
    if((lower == "from") || (lower == "join")) {
      next_is_table_name = true;
      out += lower + " ";
      continue;
    }
    if(next_is_table_name) {
      next_is_table_name = false;
      const char* remote = strstr(tokens[i].c_str(), "@");
      if (remote != NULL) {
        std::string new_token = "";
        for(size_t z=0;z<tokens[i].length();++z) {
          if(tokens[i][z] == '@') {
            out += "/*@";
            continue;
          }
          out+=tokens[i][z];  
        }    
        out += "*/ ";
      } else {
        out += tokens[i] + " ";
      }
    } else {
      out += tokens[i] + " ";
    }
  }
  
  return out;
}

// MODIFIES TOKENS!
std::string extract_ddl(std::vector<std::string>* tokens) {
  std::string out;
  
  for(size_t i=0; i < tokens->size(); ++i) {
    std::string lower = strtolower((*tokens)[i]);
    if(lower == "select") {
      break;
    }
    out += (*tokens)[i] + " ";
    //f((*tokens)[i] != "select") {
      (*tokens)[i] = ' ';
    //}
    if(lower == "as") {
      break;
    }
  }
  return out;
}

std::string get_local_root_password() {
  FILE* fp = NULL;
  char password[65]="";
  fp = fopen("/usr/local/leapdb/config/.rootpw","r");
  if(fp == NULL) {
    sql_print_error("Could not open password file for reading.  Remote queries may not work");
    return "";
  }
  if(fgets(password, 64, fp)!=NULL) {
    int len = strlen(password);
    if(password[len-1] == '\n') {
      password[len-1] = 0;
    }
    return std::string(password);
  }
  return "";
}

std::string execute_remote_query(std::vector<std::string> tokens ) {
  std::string sqlstr = "";
  std::string remote_tmp_name = "remote_tmp" + std::to_string(std::rand());

  if(is_remote_query(tokens)){
    if (is_valid_remote_query(tokens)) {
      std::string remote_host;
      std::string remote_db;
      std::string remote_user;
      std::string remote_pw;
      uint32_t remote_port;
      
      std::string rootpw = get_local_root_password();
      std::string servername = get_remote_server(tokens);
      // get rid of the leading @
      servername = std::string(servername.c_str()+1);
      
      MYSQL *local = mysql_init(NULL);
      /*FIXME HACK
       * This is necessary in 8.0.28 on AWS outside of debug mode.  Instead of using mysql_init twice
       * (the second time it returns NULL) we just call it once and copy the structure into the second
       * connection before we establish it.  This appears to work properly, as long as the memory is
       * allocated via the MySQL allocator.  However, since remote is a copy of local, we can only mysql_close
       * the local copy later!
       */
      //MYSQL *remote = (MYSQL *)my_malloc(key_memory_warp_rewrite, sizeof(MYSQL),
	//	                                           MYF(MY_WME | MY_ZEROFILL));
      //memcpy(remote, local, sizeof(MYSQL));
      MYSQL *remote = mysql_init(NULL);
      int timeout=THDVAR(current_thd, remote_query_timeout);
      mysql_options(local, MYSQL_OPT_READ_TIMEOUT, &timeout);
      mysql_options(local, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
      mysql_options(remote, MYSQL_OPT_READ_TIMEOUT, &timeout);
      mysql_options(remote, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
      MYSQL_RES *result;
      MYSQL_ROW row = NULL;
      if (local == NULL) {
        sqlstr  = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Could not initialize local database connection'";
        return sqlstr;
      }
      if (remote == NULL) {
        sqlstr  = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Could not initialize remote database connection'";
        return sqlstr;
      }
        /* establish a connection to the local server to get the remote connection details*/
      if (mysql_real_connect(local, NULL, "root", rootpw.c_str(), NULL, 3306, "/tmp/mysql.sock", 0) == NULL) {
        sqlstr  = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Could not connect to local database connection'";
        return sqlstr;
      }
      
      std::string sql = "select * from mysql.servers where server_name=\"" + escape_for_call(servername) + "\"";
      mysql_real_query(local, sql.c_str(), sql.length());
      result = mysql_use_result(local);
      
      while((row = mysql_fetch_row(result))) {
        remote_host=std::string(row[1]);
        remote_db=std::string(row[2] != NULL ? row[2] : "");
        remote_user=std::string(row[3]);
        remote_pw=std::string(row[4]);
        
        if( row[5] != NULL ) {
          remote_port=atoll(row[5]);
        } else {
          remote_port = 3306;
        }
	if(remote_port == 0) {
	  remote_port = 3306;
	}

        //std::cerr << "remote deets\n host:" << remote_host << " db:" << remote_db << " user:" << remote_user << " pw:" << remote_pw << "\n";
      }
      mysql_free_result(result);
      result = NULL;
      //mysql_close(conn);
            
      if (mysql_real_connect(remote, remote_host.c_str(), remote_user.c_str(), remote_pw.c_str(), NULL, remote_port, NULL, 0) == NULL) {
        sqlstr  = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Could not connect to remote database connection'";
        return sqlstr;
      }

      std::string remote_sql = "START TRANSACTION";
      
      mysql_real_query(remote, remote_sql.c_str(), remote_sql.length());
      
      int myerrno = mysql_errno(remote);
      if(myerrno >0) {
        sqlstr = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error [while starting transaction]:(" + 
          std::to_string(myerrno) + ")" + std::string(mysql_error(remote)) + "';";
        mysql_close(remote);
	//because of the hack above we can't close local but we shouldn't have to the extension will be freed above
        //mysql_close(local);
        return sqlstr;
      }
      
      remote_sql = "INSERT INTO leapdb.mview_signal values (DEFAULT,NOW())";
      mysql_real_query(remote, remote_sql.c_str(), remote_sql.length());
      
      myerrno = mysql_errno(remote);
      if(myerrno >0) {
        sqlstr = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error [while starting transaction]:(" + 
          std::to_string(myerrno) + ")" + std::string(mysql_error(remote)) + "';";
        //mysql_close(remote);
        mysql_close(local);
        return sqlstr;
      }
      // capture the insert_id of the remote insertion into a local THD var
      THDVAR(current_thd, remote_signal_id) = mysql_insert_id(remote);
      remote_sql = "select @@server_id";
      mysql_real_query(remote,remote_sql.c_str(), remote_sql.size());
      result = mysql_store_result(remote);
      
      row = mysql_fetch_row(result);
      if(row == NULL || row[0] == NULL) {
        mysql_free_result(result);
        mysql_close(local);
        //mysql_close(remote);
        return "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error: could not fetch remote server_id';";
      }
      THDVAR(current_thd, remote_server_id) = atoll(row[0]);
      /* execute the actual remote SQL */
      
      //FIXME: this should no longer be needed
      if(tokens.size() > 5) {
        if(tokens[4] == ".") {
          for(size_t i=0;i<6;++i) {
            tokens[i] = "";
          }
        }
      }
      remote_sql = strip_remote_server(tokens);
      remote_sql = "CREATE TEMPORARY TABLE leapdb." + remote_tmp_name + " AS " + remote_sql;
      
      mysql_real_query(remote, remote_sql.c_str(), remote_sql.length());
      myerrno = mysql_errno(remote);
      if(myerrno >0) {
        sqlstr = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT=\"Remote query error [while creating temporary table]:(" + 
          std::to_string(myerrno) + ")" + escape_for_call(std::string(mysql_error(remote))) + "\";";
        //mysql_close(remote);
        mysql_close(local);
        return sqlstr;
      }
      mysql_real_query(remote, "commit", 6);
      std::string get_create_table = "show create table leapdb." + remote_tmp_name + ";";
      mysql_real_query(remote, get_create_table.c_str(), get_create_table.length());
      
      myerrno = mysql_errno(remote);
      if(myerrno >0) {
        sqlstr = "SIGNAL SQLSTATE \"45000\" SET MESSAGE_TEXT=\"Remote query error [unable to get remote query metadata]: " + std::string(mysql_error(remote)) + "\";";
        mysql_close(local);
        //mysql_close(remote);
        return sqlstr;
      }
      
      result = mysql_store_result(remote);
      row = mysql_fetch_row(result);
      if(row == NULL || row[0] == NULL) {
        mysql_free_result(result);
        mysql_close(local);
        //mysql_close(remote);
        return "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error: could not fetch temporary table metadata';";
      }
      sql = "drop table if exists leapdb." + remote_tmp_name + ";";
      mysql_real_query(local, sql.c_str(), sql.length());
      if(myerrno >0) {
        sqlstr = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error [unable to drop local temporary table]: " + std::to_string(myerrno) + "';";
        mysql_close(local);
        //mysql_close(remote);
        return " " + sqlstr;
      }

      std::string create_table_sql = std::string(row[1]);
      
      //CREATE TEMPORARY TABLE
      create_table_sql.erase(7, 9);
      mysql_select_db(local, "leapdb");
      mysql_real_query(local, create_table_sql.c_str(), create_table_sql.length());
      if(mysql_errno(local) > 0) {
        mysql_free_result(result);
        mysql_close(local);
        //mysql_close(remote);
        return "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error: could not create local temporary table for remote query contents';";
      }
      
      sql = "select * from leapdb." + remote_tmp_name + ";";
      mysql_real_query(remote, sql.c_str(), sql.length());
      if(mysql_errno(remote) > 0) {
        mysql_close(local);
        //mysql_close(remote);
        return "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Remote query error: could not create fetch remote temporary table contents';";
      }
      
      mysql_real_query(local, "begin", 5);
      result = mysql_store_result(remote);
      int col_cnt = mysql_num_fields(result);
      
      while((row = mysql_fetch_row(result))) {
        std::string insert_sql = "";  
        for(auto n=0;n<col_cnt;++n) {
          if(insert_sql != "") {
            insert_sql += ", ";
          }
          if( row[n] == nullptr ) {
            insert_sql += "NULL";
	  } else {
	    //std::cerr << "INSERT: " << insert_sql << "\n";
	    insert_sql += '"' + escape_for_call(std::string(row[n])) + '"';
          }
        }
        insert_sql = "INSERT INTO leapdb." + remote_tmp_name + " VALUES(" + insert_sql + ");";
        if(!mysql_real_query(local, insert_sql.c_str(), insert_sql.length())) {
		std::cerr << std::string(mysql_error(local)) << "\n";
		std::cerr << insert_sql << "\n";
	}
      }
      mysql_real_query(local, "commit", 6);
      mysql_free_result(result);   
      mysql_close(local);
      //mysql_close(remote);
      sqlstr =  "select * from leapdb." + remote_tmp_name + ";";
      return sqlstr;
    } else {
      sqlstr  = "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='A remote query may only access remote tables from a single remote server and no local tables'";
    }
  } 
  return sqlstr;
}

std::string strip_backticks(const std::string str) {
  std::string out = "";
  for(size_t i = 0; i< str.size();++i) {
    if(str[i] != '`') {
      out += str[i];
    }
  }
  return out;
}
/**
  Entry point to the plugin. The server calls this function after each parsed
  query when the plugin is active. The function extracts the digest of the
  query. If the digest matches an existing rewrite rule, it is executed.
*/
static int warp_rewrite_query_notify(
  MYSQL_THD thd, mysql_event_class_t event_class MY_ATTRIBUTE((unused)),
  const void *event
) {
  //DBUG_ASSERT(event_class == MYSQL_AUDIT_PARSE_CLASS);
  const struct mysql_event_parse *event_parse =
      static_cast<const struct mysql_event_parse *>(event);
  std::string rewrite_error = "";
  std::vector<std::string> tokens = custom_lex(std::string(event_parse->query.str, event_parse->query.length), false);
 
  if(tokens.size() == 0) {
	 return 0;
  }
  
  if (event_parse->event_subclass != MYSQL_AUDIT_PARSE_POSTPARSE) {

    
    bool is_incremental = false;
    std::string mvname;

    std::string sqlstr = "";
    bool capture_sql = false;
    bool is_create_table = false;
    if(tokens.size() < 4) {
      return 0;
    }
    if(strtolower(tokens[0]) == "prepare") {
      return 0;
    }
    
    if(strtolower(tokens[0]) == "set") {
      goto process_sql;
    }

    if ( (tokens.size() == 4) && strtolower(tokens[0])=="show" && strtolower(tokens[1])=="materialized" && strtolower(tokens[2]) == "view" && strtolower(tokens[3])== "logs") {
      sqlstr = "call leapdb.show_materialized_view_logs(database());";
    } else if ( (tokens.size() == 6) && strtolower(tokens[0]) == "rename" && strtolower(tokens[1]) == "materialized" && strtolower(tokens[2])=="view" && strtolower(tokens[4]) == "to" ) {
      std::string from_table = strip_backticks(tokens[3]);
      std::string to_table = strip_backticks(tokens[5]);
      std::string from_db = "database()";
      std::string to_db = "database()";
      const char *dot_at;
      
      if((dot_at = strstr(from_table.c_str(), ".")) != NULL) {
        if(dot_at != nullptr) {
          from_db = "'" + from_table.substr(0, dot_at - from_table.c_str() ) + "'";
          from_table = from_table.substr(dot_at - from_table.c_str()+1, -1);
        } else {
          from_db = "database()";
        }
      }
      if((dot_at = strstr(to_table.c_str(), ".")) != NULL) {
        if(dot_at != nullptr) {
          to_db = "'" + to_table.substr(0, dot_at - to_table.c_str() ) + "'";
          to_table = to_table.substr(dot_at - to_table.c_str()+1, -1);
        } else {
          to_db = "database()";
        }
      }
      from_table = "'" + from_table + "'";
      to_table = "'" + to_table + "'";
      sqlstr = "CALL leapdb.rename(leapdb.get_id(" + from_db + "," + from_table + "), " + to_db + "," + to_table + ");";
      
    } else if( tokens.size() == 6 && (strtolower(tokens[0]) == "create" || strtolower(tokens[0]) == "drop") && 
       strtolower(tokens[1]) == "materialized" && 
       strtolower(tokens[2]) == "view" && 
       strtolower(tokens[3]) == "log" && 
       strtolower(tokens[4]) == "on") 
    {
      std::string mvlog_db = "database()";
      std::string mvlog_table = "";
      const char* dot_pos = NULL;
      if( (dot_pos = strstr(tokens[5].c_str(),".")) != NULL ) {
        mvlog_db = "'" + tokens[5].substr(0, dot_pos - tokens[5].c_str() ) + "'";
        mvlog_table.assign(dot_pos+1);
        mvlog_table = "'" + mvlog_table + "'";
      } else{
        mvlog_table = "'" + tokens[5] + "'";
      }
      std::string proc_name = "create_mvlog";
      if(strtolower(tokens[0]) == "drop") {
        proc_name = "drop_mvlog";
      }
      
      sqlstr = "CALL leapdb." + proc_name + "(" + mvlog_db + ", " + escape_for_call(mvlog_table) + ");";
      
    } else if( tokens.size() == 4 && (strtolower(tokens[0]) == "drop") &&
               (strtolower(tokens[1]) == "materialized") &&
               (strtolower(tokens[2]) == "view")
      ) {
      std::string mvlog_db = "database()";
      std::string mvlog_table = "";
      const char* dot_pos = NULL;
      if( (dot_pos = strstr(tokens[3].c_str(),".")) != NULL ) {
        mvlog_db = "'" + tokens[3].substr(0, dot_pos - tokens[3].c_str() ) + "'";
        mvlog_table.assign(dot_pos+1);
        mvlog_table = "'" + mvlog_table + "'";
      } else{
        mvlog_table = "'" + tokens[3] + "'";
      }
      
      sqlstr = "CALL leapdb.drop(leapdb.get_id(" + mvlog_db + ", " + escape_for_call(mvlog_table) + "));";

    } else {
      if(strtolower(tokens[0]) == "drop") {
        return 0;
      }
      // HANDLE CREATE TABLE STATEMENTS WITH REMOTE QUERIES
      if(strstr(event_parse->query.str, "^@") != NULL || 
        (strtolower(tokens[0]) == "create" && (strtolower(tokens[1]) == "temporary" || strtolower(tokens[1]) == "table")) ||
        (strtolower(tokens[0])=="insert"))  
      { 
        if(is_remote_query(tokens) && is_valid_remote_query(tokens)) {
            std::string ddl = extract_ddl(&tokens);
            std::string tmp = strip_remote_server(tokens);
            sqlstr = ddl + " " + execute_remote_query(tokens);
            is_create_table = true;
        }
      } else {/*
	      for(auto i=0;i<tokens.size();++i) {
		      std::cerr << i << ": " << tokens[i] << "\n";
	      }*/
        // handle create [incremental] materialized view
        if(tokens.size()>4 && strtolower(tokens[0]) == "create") {
          if(strtolower(tokens[1]) == "incremental") {
            is_incremental = true;
            if(strtolower(tokens[2]) != "materialized" && strtolower(tokens[3]) != "view") {
              return -1;
            }
            mvname = tokens[4];
          } else {
            
            if(strtolower(tokens[1]) == "materialized" && strtolower(tokens[2]) == "view") {
              mvname = tokens[3];
            } else {
              //not a create [incremental] materialised view statement
              return 0;
            }
          }
        }
      }
    
      if(mvname != "") {  
        std::string prefix = "/*~cmv:";
        if(is_incremental) {
          prefix += "i";
        } else {
          prefix += "f";
        } 
        prefix += "|" + mvname ;
        if(is_remote_query(tokens) && is_valid_remote_query(tokens)) {
          prefix += "^" + get_remote_server(tokens);
          prefix += "*/";
          sqlstr = prefix + strip_remote_server(tokens);
        } else {
          for(size_t i=0;i<tokens.size();++i) {
            if(tokens[i] == mvname) {
              capture_sql = true;
              if(i+1 < tokens.size()) {
                if(strtolower(tokens[i+1]) == "as" || strtolower(tokens[i+1]) == "select") {
                  ++i;
                }
              }
              continue;
            }
            if(capture_sql) sqlstr += tokens[i] + " ";
          }
          sqlstr = prefix + "*/" + sqlstr;
        }
        
      } else if(!is_create_table) {
        sqlstr = execute_remote_query(tokens);
      }  

    }
    process_sql:
    if(sqlstr != "") {
      char *rewritten_query = static_cast<char *>(
      my_malloc(key_memory_warp_rewrite, sqlstr.length() + 1, MYF(0)));
      memcpy(rewritten_query, sqlstr.c_str(), sqlstr.length());
      rewritten_query[sqlstr.length()]=0;
      event_parse->rewritten_query->str = rewritten_query;
      event_parse->rewritten_query->length = sqlstr.length();
      
      *((int *)event_parse->flags) |=
          (int)MYSQL_AUDIT_PARSE_REWRITE_PLUGIN_QUERY_REWRITTEN;
    } 
    
    // this is not a post-parse call to the plugin    
    return 0;
  }
  
  const bool is_mv_create = (strstr(event_parse->query.str, "/*~cmv:") != NULL);
  
  // If we are building a materialized view create script, commands contains the add_table and add_expr commands
  // They will be executed by the create_from_rewriter function
  std::string commands;

  bool is_incremental = false;
  std::string mvname;
  if(is_mv_create) {
    const char* pos = strstr(event_parse->query.str, ":");
    if(pos == NULL) {
      return -1;
    }
    if(*(pos+1) == 'i') {
      is_incremental = true;
    }
    pos = strstr(event_parse->query.str, "|");
    if(pos == NULL) {
      return -1;
    }
    ++pos;
    while(*pos != '*') {
      mvname += *pos;
      ++pos;
    }
  }
  const char* remote_name=NULL;
  if(auto pos = strstr(mvname.c_str(),"^@")) {
    if(pos!=NULL)  {
      remote_name = pos+1;
    }
  }
  
  // MV creation must proceed even if parallel query is not enabled
  if(!is_mv_create) {
    if(THDVAR(thd,parallel_query) != true) {
      //std::cout << "PARALLEL QUERY IS DISABLED\n";
      return 0;
    } 
  }

  // currently only support SELECT statements
  if(mysql_parser_get_statement_type(thd) != STATEMENT_TYPE_SELECT) {
    return 0;
  }
  if(thd->lex->query_block->is_part_of_union()) {
    return -1;
  }
  // currently prepared statements are not supported
  if(mysql_parser_get_number_params(thd) != 0) {
    return 0;
  }
    //auto orig_query = mysql_parser_get_query(thd);
    ////std::cout << "WORKING ON: " << std::string(orig_query.str) << "\n";
    // the query sent to the parallel workers
    std::string ll_query;

    // the query the coordinator node runs over the output of the ll_query
    std::string coord_query;

    // the GROUP BY clause for the ll_query
    std::string ll_group;

    // the GROUP BY clause for the coord_query
    std::string coord_group;


    std::string ll_where;
    std::string ll_from;
    std::string coord_having;
    std::string coord_order;
    std::string fact_alias;
    std::string partition_list;
    
    auto select_lex =thd->lex->query_block[0];
    auto field_list = select_lex.get_fields_list();
    auto tables = select_lex.table_list;
    //bool is_straight_join = (stristr(strtolower(orig_query.str), "straight_join") != NULL);
    bool is_straight_join = 1;
  if(!is_mv_create || (is_mv_create && is_incremental)) {
    
    // query has no tables - do nothing
    if(tables.size() == 0) {
      return 0;
    }

    // Process the SELECT clause
    // number of the expression in the SELECT clause
    uint expr_num = 0;

    // String object used to hold printed field object
    String field_str;
    field_str.reserve(1024*1024);

    // List of fields used in the SELECT clause
    // This list is used in processing the GROUP BY clause and HAVING clause
    std::unordered_map<std::string, uint> used_fields;
    int star_count = 0;
    for(auto field_it = field_list->begin(); field_it != field_list->end(); ++field_it,++expr_num) {
      auto field = *field_it;
      
      used_fields.emplace(
        std::pair<std::string, uint>(std::string(field->full_name()), expr_num)
      );
      used_fields.emplace(
        std::pair<std::string, uint>(std::string(field->item_name.ptr()), expr_num)
      );
      field_str.set("", 0, default_charset_info);
      field->print(thd, &field_str, QT_ORDINARY);
      used_fields.emplace(
        std::pair<std::string, uint>(std::string(field_str.ptr(), field_str.length()), expr_num)
      );

      field_str.set("",0,default_charset_info);
      field->print(thd, &field_str, QT_ORDINARY);
      std::string raw_field = std::string(field_str.c_ptr(), field_str.length());
      std::string orig_alias = std::string("`") + std::string(field->item_name.ptr()) + std::string("`");
      std::string alias = std::string("`expr$") + std::to_string(expr_num) + std::string("`");
      std::string raw_alias = std::string("expr$") + std::to_string(expr_num);

      if(commands != "") {
        commands += ";;";
      }
      
      if(ll_query.length() > 0) {
        ll_query += ", ";
      }
      if(coord_query.length() > 0) {
        coord_query += ", ";
      }
      
      switch(field->type()) {
        // bare field is easily handled - it gets an alias of expr$NUM where NUM is the 
        // ordinal position in the SELECT clause
        case Item::Type::FIELD_ITEM:
          ll_query += std::string(field_str.c_ptr(), field_str.length()) + " AS `expr$" + std::to_string(expr_num) + "`";
          coord_query += alias + " AS " + orig_alias;

          if(select_lex.group_list_size() > 0) {
            commands += "CALL leapdb.add_expr(@mvid, 'GROUP', \"";
          } else {
            commands += "CALL leapdb.add_expr(@mvid, 'COLUMN', \"";
          }
          if(orig_alias == "`*`") {
            std::string new_alias = orig_alias;
            if(star_count > 0) {
              new_alias += std::to_string(star_count);
            }
            ++star_count;
            commands += escape_for_call(raw_field) + "\",\"" + escape_for_call(new_alias) + "\")";
          } else {
            commands += escape_for_call(raw_field) + "\",\"" + escape_for_call(orig_alias) + "\")";
          }
          continue;
        break;

        // item func
        case Item::Type::FUNC_ITEM: 
          
          { String tmp;
            tmp.reserve(1024*1024);
            tmp.set("",0,default_charset_info);
            field->this_item()->print(current_thd, &tmp, QT_ORDINARY);
            if(select_lex.group_list_size() > 0) {
              commands += "CALL leapdb.add_expr(@mvid, 'GROUP', \"";
            } else {
              commands += "CALL leapdb.add_expr(@mvid, 'COLUMN', \"";
            }
            commands+=escape_for_call(std::string(tmp.c_ptr(), tmp.length())) + "\",\"" + escape_for_call(orig_alias) + "\")";
            
          }
          continue;
          //return 0;
        break;
        
        // SUM or COUNT func
        case Item::Type::SUM_FUNC_ITEM: {
          Item_sum* sum_item = (Item_sum*)field->this_item(); 
          std::string func_name = std::string(sum_item->func_name());
          std::string inner_field = raw_field.substr(func_name.length(), raw_field.length() - func_name.length());
          if(func_name == "group_concat") {
            inner_field = inner_field.substr(1,inner_field.length()-2);
          }
          if(func_name == "std") {
            func_name = "stddev_pop";
          }
          if(func_name == "var") {
            func_name = "var_pop";
          }
          if(sum_item->has_with_distinct()) {
            ll_query += raw_field.substr(func_name.length(), raw_field.length() - func_name.length()) + " AS " + alias;
            coord_query += func_name + "( DISTINCT " + alias + ") AS " + orig_alias;
            if(ll_group.length() > 0) {
              ll_group += ", ";
            }
            ll_group += std::to_string(expr_num);
            if(is_mv_create) {
              if(func_name != "count") {
                return -1;
              }
              //remove distinct from the inner expression
              inner_field = "(" + inner_field.substr(strlen("(distinct"), inner_field.length()-strlen("(distinct ")) + ")";
              commands += "CALL leapdb.add_expr(@mvid, 'COUNT_DISTINCT', \"" + escape_for_call(inner_field) + "\", \"" + escape_for_call(orig_alias) + "\")";
            }
            continue;
          }
          
          if(func_name == "sum") {
            //ll_query += std::string(field_str.c_ptr(), field_str.length()) + " AS `expr$" + std::to_string(expr_num) + "`";
            //coord_query += std::string("`expr$") + std::to_string(expr_num) + std::string("`");
            ll_query += raw_field + " AS " + alias;
            coord_query += "SUM(" + alias + ")" + " AS " + orig_alias; 
            commands += "CALL leapdb.add_expr(@mvid, 'SUM', \"" + escape_for_call(inner_field) + "\", \"" + escape_for_call(orig_alias) + "\")";
          } else if (func_name == "count") {
            //ll_query += "COUNT" + raw_field.substr(3,raw_field.length()-3) + " AS " + alias;
            ll_query += raw_field + " AS " + alias;
            coord_query += "SUM(" + alias + ") AS " + orig_alias;
            commands += "CALL leapdb.add_expr(@mvid, 'COUNT', \"" + escape_for_call(inner_field) + "\", \"" + escape_for_call(orig_alias) + "\")";
          } else if(func_name == "avg") {
            const char* raw_field_ptr = raw_field.c_str() + 4;
            ll_query += "COUNT( " + std::string(raw_field_ptr) + " AS `" + raw_alias + "_cnt` , SUM(" + std::string(raw_field_ptr) + " AS `" + raw_alias + "_sum`";
            coord_query += "SUM(`" + raw_alias + "_cnt`) / SUM(`" + raw_alias + "_sum`)" + " AS " + orig_alias ; 
            commands += "CALL leapdb.add_expr(@mvid, 'AVG', \"" + escape_for_call(inner_field) + "\", \"" + escape_for_call(orig_alias) + "\")";
          } else {
            if(is_mv_create) {
              commands += "CALL leapdb.add_expr(@mvid, '" + func_name + "', \"" + escape_for_call(inner_field) + "\", \"" + escape_for_call(orig_alias) + "\")";
            } else { 
              std::cout << "UNSUPPORTED PARALLEL QUERY SUM_FUNC_TYPE: " << func_name << "\n";
              return 0;
            }  
          }      
        
          continue;
        }
        break;
        
        default:
          //std::cout << "UNSUPPORTED PARALLEL QUERY ITEM TYPE: " << field->type() << "\n";
          return -1;
      }
    }
    
    /* handle GROUP BY */
    ORDER* group_pos = select_lex.group_list.first;
    expr_num = select_lex.get_fields_list()->size();
    for(auto i=0; i < select_lex.group_list_size(); ++i, ++expr_num) {

      // is this group item one of the select items?
      auto group_item = *(group_pos->item);

      if(ll_group.length() > 0) {
        ll_group += ", ";
      }
      
      if(coord_group.length() > 0) {
        coord_group += ", ";
      }
      
      // extract the group by item
      field_str.set("",0,default_charset_info);
      group_item->print(thd, &field_str, QT_ORDINARY); 
      
      // determine if this field is in the SELECT list
      auto used_fields_it = used_fields.find(std::string(group_item->full_name()));
      
      if(used_fields_it == used_fields.end()) {
        std::string bare_field = "";
        const char* pos;
        
        if( (pos = strstr(group_item->full_name(), "`.`")) != NULL) {
          pos+=3;
          bare_field = std::string(pos); 
          used_fields_it = used_fields.find(bare_field); 
        }

      }
      if (used_fields_it == used_fields.end()) {
        // if not in SELECT list, need to add it to the parallel query so that it can be grouped by in the coord query
        auto alias = std::string("`expr$") + std::to_string(expr_num) + "`";
        ll_query += std::string(", ") + std::string(field_str.ptr()) + " AS " + alias;
        used_fields.emplace(std::pair<std::string, uint>(std::string(field_str.ptr()), expr_num));
        ll_group += alias;
        coord_group += alias;
        // positional group by is discarded because the column is type 'GROUP'
        bool is_numeric = true;
        for(size_t i=0;i<field_str.length();++i) {
          if(field_str[i] < '0' || field_str[i] > '9') {
            is_numeric = false;
          }
        }
        if(!is_numeric) {
          if(commands != "") {
            commands += ";;";
          }
          commands += "CALL leapdb.add_expr(@mvid, 'GROUP', \"";
          commands += escape_for_call(std::string(field_str.ptr())) + "\",\"" + escape_for_call(std::string(field_str.ptr())) + "\")";
        }
      } else {
        ll_group += std::string("`expr$") + std::to_string(used_fields_it->second) + "`";
        coord_group += std::string(field_str.ptr());
      }
      
      group_pos = group_pos->next;
    }  

    // Process the FROM clause
    auto tbl = tables.first;
    std::string fqtn;
    uint64_t max_rows = 0;
    uint64_t rows = 0;
    std::map<std::string, std::string> from_clause;
    std::string tmp_from = "";
    std::map<std::string, uint64_t> table_row_counts;
    bool has_outer_joins = false;
    bool all_warp_tables = false;
    for(long unsigned long int i = 0; i < tables.size(); ++i) {
      if(commands != "") {
        commands += ";;";
      }
      tmp_from = "";
      if(tbl->is_table_function()) {
        return -1;
      }
      
      if(tbl->is_derived()) {
        return -1;
      }
      
      // figure out the the WARP table with the most rows
      if(is_warp_table(std::string(tbl->db), std::string(tbl->table_name))) {
        rows = get_warp_row_count(std::string(tbl->db), std::string(tbl->table_name));
        table_row_counts.emplace(std::make_pair(std::string(tbl->alias), rows));
        if(rows > max_rows) {
          fact_alias = std::string(tbl->alias);
          max_rows = rows;
          partition_list = get_warp_partitions(std::string(tbl->db), std::string(tbl->table_name));
        }
      } else {
        all_warp_tables = false;
      }
      fqtn = "";
      
      fqtn = std::string("`") + std::string(tbl->db, tbl->db_length) + "`." +
             std::string("`") + std::string(tbl->table_name, tbl->table_name_length) + "` "
             " AS `" + std::string(tbl->alias) + "` ";
      
      commands += "CALL leapdb.add_table(@mvid, \"" + escape_for_call(std::string(tbl->db, tbl->db_length)) + "\",\"";
      if( std::string(tbl->table_name, tbl->table_name_length) == "dual" ) {
        return(-1);
      }
      commands += escape_for_call(std::string(tbl->table_name, tbl->table_name_length));
      if(remote_name != NULL) {
        commands += std::string(remote_name);
      }
      commands += "\",\"";
      commands += escape_for_call(std::string(tbl->alias)) + "\",";
      if(!from_clause.empty()) {
        if(tbl->is_inner_table_of_outer_join()) {
          has_outer_joins = true;
          tmp_from += "LEFT ";
          if(is_mv_create) {
            return -1;
          }
        } 
        tmp_from += "JOIN " + fqtn;
      } else {
        tmp_from = std::string("FROM ") + fqtn;
      }
      
      // handle NATURAL JOIN / USING)
      // FIXME: for some reason natural join/using are not populating
      // the pointers like I think they should and so they are not
      // supported right now, mv creation will raise an error
      if(tbl->join_columns != nullptr) {
        
        std::string join_columns;
        for(auto join_col_it = tbl->join_columns->begin(); join_col_it != tbl->join_columns->end();++join_col_it) {
          if(join_columns.length() >0) {
            join_columns += ", ";
          }
          join_col_it->table_field->print(thd, &field_str, QT_ORDINARY);
          join_columns += std::string(field_str.ptr(), field_str.length());
        }
        tmp_from += std::string("/*%TOKEN%*/USING(") + join_columns + ")\n";
        
        commands += "\"" + escape_for_call(join_columns) + "\")";
      } else {
        String join_str;
        join_str.reserve(1024 * 1024);
        if(tbl->join_cond() != nullptr) {
          tbl->join_cond()->print(thd, &join_str, QT_ORDINARY);
          tmp_from += "/*%TOKEN%*/ON " + std::string(join_str.ptr(), join_str.length());
          commands += "\" ON (" + escape_for_call(std::string(join_str.ptr(), join_str.length())) + ")\")";
        } else {
          if(is_mv_create && i>0) {
            return -1;
          }
          commands += "NULL)";
        }
      }
      ll_from += tmp_from;
      from_clause.emplace(std::make_pair(std::string(tbl->alias), tmp_from));
      tbl = tbl->next_local;
    }
    
    if(all_warp_tables) {
      /* this puts the largest table first, then the tables in ascending row count order */
      auto sorted_from_cnts = sort_from(table_row_counts);
      std::vector<std::string> sorted_from;
      auto t = sorted_from_cnts.back();
      auto find_it = from_clause.find(t.first);
      sorted_from.push_back(find_it->second);
      for(auto sort_it=sorted_from_cnts.begin();sort_it<sorted_from_cnts.end()-1;++sort_it) {
        auto find_it = from_clause.find(sort_it->first);
        sorted_from.push_back(find_it->second);
      }

      /* select * from dim join fact on fact.dim_id = dim.id */
      tmp_from ="";
      resort:
      if(!has_outer_joins || (has_outer_joins && THDVAR(thd, reorder_outer))) {
        for(auto it = sorted_from.begin();it != sorted_from.end();++it) {

          if(*it == "") continue;

          if((*it).substr(0,1) == "F") {
            tmp_from += *it;
            *it = "";
            goto resort;
          } else {

            if(tmp_from != "") {
              tmp_from += " " + *it;
              *it = "";
              goto resort;
            }

            std::string swap_table = *it;
            // find the first JOIN (not OUTER JOIN) table and swap the JOIN clauses
            *it="";
            ++it;

            //it = JOIN t2 as t2 /*%TOKEN%/ON ...
            //it2 = FROM t1 as t1
            for(;it != sorted_from.end();++it) {
              if((*it).substr(0,1) != "F") {
                continue;
              }
              char *tmpstr = strdup(swap_table.c_str());
              char* token_at = strstr(tmpstr, "/*%TOKEN%*/");
              uint64_t token_pos = token_at - tmpstr-5;
            
              /* should never happen but just in case error out*/
              if(token_at == NULL) {
                free(tmpstr);
                tmp_from = "";
                goto after_sort;
              }

              /* get t1 as t1*/
              std::string first_table_name_and_alias = std::string((*it).c_str() + 5);
              /* get t2 as t2*/
              std::string second_table_name_and_alias = " " + swap_table.substr(5, token_pos);

              token_at += 11;
              if(swap_table.substr(0,1) != "L") {
                tmp_from = "FROM " + second_table_name_and_alias;
                tmp_from += " JOIN " + first_table_name_and_alias + " " + std::string(token_at);
              } else {
                tmp_from = "FROM " + first_table_name_and_alias;
                tmp_from += " LEFT JOIN " + second_table_name_and_alias.substr(5) + " " + std::string(token_at);
              }
              *it = "";
              free(tmpstr);
              tmpstr=nullptr;
              goto resort;
            }
          
            if(it == sorted_from.end()) {
              tmp_from == "";
              // query only contains OUTER JOIN joins so it can not be reordered
              goto after_sort;
            }
          }
        }
      }
      after_sort:
      if(tmp_from != "") ll_from = tmp_from;
      if(partition_list == "") {
        return 0;
      }
    }
    
    /* Process the WHERE clause */
    if(select_lex.where_cond() != nullptr) {
      String where_str;
      where_str.reserve(1024 * 1024);
      select_lex.where_cond()->print(thd, &where_str, QT_ORDINARY);
      ll_where = std::string(where_str.ptr(), where_str.length());
      
      if(commands != "") {
        commands += ";;";
      }
      ll_where = std::regex_replace(ll_where, std::regex(" '"), "'"); 

      auto cmd = "CALL leapdb.add_expr(@mvid,'WHERE',\"" + escape_for_call(ll_where) + "\",'WHERE_CLAUSE')";
      //std::cout << "unescaped where: " << ll_where << "\n escaped where: " << escape_for_call(ll_where) << "\n" << "command: " << cmd << "\n";
      commands += cmd;
      
    }

    /* Process the HAVING clause:
      The HAVING clause is transformed into a WHERE clause on the coordinator 
      table.  Any HAVING expressions that are not expressed on SELECT aliases
      have to be pushed into the ll_select clause so that they can be 
      filtered in the coord_where clause, but these columns are not projected
      by the coord_query clause!
    */
    std::string coord_where;
    if(select_lex.having_cond() != nullptr) {
      if(is_mv_create) {
        return -1;
      }
      process_having(thd, select_lex.having_cond(), coord_having, ll_query, coord_group, used_fields);
    }

    auto orderby = select_lex.order_list;
    String tmp_ob;
    tmp_ob.reserve(1024*1024);  
    if(orderby.size() > 0) {
      if(is_mv_create) {
        return -1;
      }
      auto ob = orderby.first;
      for(uint i = 0;i<orderby.size();++i) {    
        tmp_ob.set("", 0, &my_charset_bin);  
        (*(ob->item))->print_for_order(thd, &tmp_ob,QT_ORDINARY,ob->used_alias);
        if(coord_order != "") {
          coord_order += ",";
        }
        coord_order += std::string(tmp_ob.c_ptr(), tmp_ob.length());
        ob = ob->next;
      } 
    }
  } 
  
  std::string call_sql = "";
  if(!is_mv_create) { 
    call_sql = 
      "CALL warpsql.parallel_query(\n";
      call_sql +=
      '"' + escape_for_call(ll_query) + "\",\n" +
      '"' + escape_for_call(coord_query) + "\",\n" +
      '"' + escape_for_call(ll_group) + "\",\n" +
      '"' + escape_for_call(coord_group) + "\",\n" +
      '"' + escape_for_call(ll_from) + "\",\n" +
      '"' + escape_for_call(ll_where) + "\",\n" +
      '"' + escape_for_call(coord_having) + "\",\n" +
      '"' + escape_for_call(coord_order) + "\",\n" +
      (partition_list != "" ? 
        '"' + escape_for_call(fact_alias) + ":" + partition_list + "\",\n" :
        "'',") +
      (is_straight_join ? "1" : "0") + ")";
  } else {
    call_sql = "CALL leapdb.create_from_rewriter('";
    if(is_incremental) {
      call_sql += "i";
    } else {
      call_sql += "c";
      commands = std::string(event_parse->query.str,event_parse->query.length);
    }

    call_sql += "','" + escape_for_call(mvname) + "', (select database()), \"" + escape_for_call(commands) + "\");";
    
  }
  
  MYSQL_LEX_STRING call_sql_str;
  call_sql_str.length = call_sql.size();
  call_sql_str.str = strdup(call_sql.c_str());
  //call_sql.assign(call_sql.c_str(), call_sql.size());
  
  if(warp_parse_call(thd, call_sql_str)) {
    return 1;
  }
  free(call_sql_str.str);
  return 0;
}
