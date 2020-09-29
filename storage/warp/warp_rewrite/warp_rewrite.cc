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

#include <mysql/plugin_audit.h>
#include <mysql/psi/mysql_thread.h>
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

using std::string;

static MYSQL_PLUGIN plugin_info;
#define PLUGIN_NAME "warp_rewriter"

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
    //rewriter_plugin_status_vars,  /* status variables              */
    //rewriter_plugin_sys_vars,     /* system variables              */
    nullptr,                        /* status vars (none atm) */
    nullptr,                        /* system vars (none atm) */
    nullptr,                        /* reserverd                     */
    0                               /* flags                         */
} mysql_declare_plugin_end;

///@}

static int warp_rewriter_plugin_init(MYSQL_PLUGIN plugin_ref) {
  plugin_info = plugin_ref;

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
        parts += " ";
      }

      parts += std::string(ent->d_name);
    }
  }
  return parts;
}

bool is_warp_table(std::string schema, std::string table) {
  std::string path = schema + "/" + table + ".data/-part.txt";
  struct stat st;
  return !stat(path.c_str(), &st);
}

uint64_t get_warp_row_count(std::string schema, std::string table) {
  std::string path = schema + "/" + table + ".data/";
  ibis::table* base_table = ibis::mensa::create(path.c_str());
  uint64_t rows = base_table->nRows();
  delete base_table;
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
      auto cnt = sum->get_arg_count();
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
          used_fields.emplace(std::pair<std::string, uint>(arg_clause, max_used_expr_num));
          new_clause += std::string(sum->func_name()) + "(DISTINCT " + ll_alias + ")";
          coord_group += ll_alias;
        } else {
          used_fields.emplace(std::pair<std::string, uint>(orig_clause, max_used_expr_num));
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
    //cond->print(thd, &item_str, QT_ORDINARY);
    //coord_where += std::string(item_str.ptr(), item_str.length());
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
  for(int i =0; i< str.length(); ++i) {
    if(str[i] == '"') {
      retval += '\\';
    }
    retval += str[i];
  }
  return retval;
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
  DBUG_ASSERT(event_class == MYSQL_AUDIT_PARSE_CLASS);
  const struct mysql_event_parse *event_parse =
      static_cast<const struct mysql_event_parse *>(event);
  
  if (event_parse->event_subclass != MYSQL_AUDIT_PARSE_POSTPARSE) {
    // this is not a post-parse call to the plugin    
    return 0;
  }

  // currently only support SELECT statements
  if(mysql_parser_get_statement_type(thd) != STATEMENT_TYPE_SELECT) {
    return 0;
  }

  // currently prepared statements are not supported
  if(mysql_parser_get_number_params(thd) != 0) {
    return 0;
  }

  auto orig_query = mysql_parser_get_query(thd);
  
  // the query sent to the parallel workers
  std::string ll_query;

  // the query the coordinator node runs over the output of the ll_query
  std::string coord_query;

  // the GROUP BY clause for the ll_query
  std::string ll_group;

  // the GROUP BY clause for the coord_query
  std::string coord_group;

  // the HAVING clause for the coord query
  std::string coord_having;
  
  auto select_lex =thd->lex->select_lex;
  auto field_list = select_lex->fields_list;
  auto tables = select_lex->table_list;

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

  for(auto field_it = field_list.begin(); field_it != field_list.end(); ++field_it,++expr_num) {
    used_fields.emplace(
      std::pair<std::string, uint>(std::string(field_it->full_name()), expr_num)
    );
    used_fields.emplace(
      std::pair<std::string, uint>(std::string(field_it->item_name.ptr()), expr_num)
    );
    field_str.set("", 0, default_charset_info);
    field_it->print(thd, &field_str, QT_ORDINARY);
    used_fields.emplace(
      std::pair<std::string, uint>(std::string(field_str.ptr(), field_str.length()), expr_num)
    );

    field_str.set("",0,default_charset_info);
    field_it->print(thd, &field_str, QT_ORDINARY);
    std::string raw_field = std::string(field_str.c_ptr(), field_str.length());
    std::string orig_alias = std::string("`") + std::string(field_it->item_name.ptr()) + std::string("`");
    std::string alias = std::string("`expr$") + std::to_string(expr_num) + std::string("`");

    if(ll_query.length() > 0) {
       ll_query += ", ";
     }
     if(coord_query.length() > 0) {
       coord_query += ", ";
     }

     switch(field_it->type()) {
       // bare field is easily handled - it gets an alias of expr$NUM where NUM is the 
       // ordinal position in the SELECT clause
       case Item::Type::FIELD_ITEM:
         ll_query += std::string(field_str.c_ptr(), field_str.length()) + " AS `expr$" + std::to_string(expr_num) + "`";
         coord_query += alias + " AS " + orig_alias;
         continue;
       break;

       // item func
       case Item::Type::FUNC_ITEM: 
         std::cout << "NON-AGGREGATE FUNCTIONS NOT YET SUPPORTED\n";
         return 0;
       break;
       
       // SUM or COUNT func
       case Item::Type::SUM_FUNC_ITEM: {
         Item_sum* sum_item = (Item_sum*)field_it->this_item(); 
         std::string func_name = std::string(sum_item->func_name());
    
         if(sum_item->has_with_distinct()) {
           //std::cout << "UNSUPPORTED SUM_FUNC_TYPE HAS DISTINCT: " << func_name << "\n";
           ll_query += raw_field.substr(func_name.length(), raw_field.length() - func_name.length()) + " AS " + alias;
           coord_query += func_name + "( DISTINCT " + alias + ") AS " + orig_alias;
           if(ll_group.length() > 0) {
             ll_group += ", ";
           }
           ll_group += std::to_string(expr_num);
           continue;
         }
         
         if(func_name == "sum") {
           //ll_query += std::string(field_str.c_ptr(), field_str.length()) + " AS `expr$" + std::to_string(expr_num) + "`";
           //coord_query += std::string("`expr$") + std::to_string(expr_num) + std::string("`");
           ll_query += raw_field + " AS " + alias;
           coord_query += "SUM(" + alias + ")" + " AS " + orig_alias; 
         } else if (func_name == "count") {
           //ll_query += "COUNT" + raw_field.substr(3,raw_field.length()-3) + " AS " + alias;
           ll_query += raw_field + " AS " + alias;
           coord_query += "SUM(" + alias + ") AS " + orig_alias;
         } else {
           std::cout << "UNSUPPORTED SUM_FUNC_TYPE: " << func_name << "\n";
           return 0;  
         } 
      
         continue;
       }
       break;

       // unsupported!
       default:
         std::cout << "UNSUPPORTED ITEM TYPE: " << field_it->type() << "\n";
         return 0;
     }
  }
  
  /* handle GROUP BY */
  ORDER* group_pos = select_lex->group_list.first;
  expr_num = select_lex->fields_list.size();
  for(int i=0; i < select_lex->group_list_size(); ++i, ++expr_num) {

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
      // if not in SELECT list, need to add it to the parallel query so that it can be grouped by in the coord query
      auto alias = std::string("`expr$") + std::to_string(expr_num) + "`";
      ll_query += std::string(", ") + std::string(field_str.ptr()) + " AS " + alias;
      used_fields.emplace(std::pair<std::string, uint>(std::string(field_str.ptr()), expr_num));
      ll_group += alias;
      coord_group += alias;
    } else {
      ll_group += std::string("`expr$") + std::to_string(used_fields_it->second) + "`";
      coord_group += std::string(field_str.ptr());
    }
    
    group_pos = group_pos->next;
  }
    
  // Process the FROM clause
  std::string ll_from;
  auto tbl = tables.first;
  std::string fqtn;
  std::string fact_alias;
  uint64_t max_rows = 0;
  uint64_t rows = 0;
  std::string partition_list;
  
  for(int i = 0; i < tables.size(); ++i) {

    if(tbl->is_table_function()) {
      std::cout << "UNSUPPORTED TABLE TYPE: TABLE FUNCTIONS NOT SUPPORTED\n";
      return 0;
    }
    
    if(tbl->is_derived()) {
      std::cout << "UNSUPPORTED TABLE TYPE: DERIVED TABLES NOT SUPPORTED\n";
      return 0;
    }

    // figure out the the WARP table with the most rows
    if(is_warp_table(std::string(tbl->db), std::string(tbl->table_name))) {
      rows = get_warp_row_count(std::string(tbl->db), std::string(tbl->table_name));
      if(rows > max_rows) {
        fact_alias = std::string(tbl->alias);
        max_rows = rows;
        partition_list = get_warp_partitions(std::string(tbl->db), std::string(tbl->table_name));
      }
    }
    fqtn = "";
    fqtn = std::string("`") + std::string(tbl->db, tbl->db_length) + "`." +
           std::string("`") + std::string(tbl->table_name, tbl->table_name_length) + "` "
           " AS `" + std::string(tbl->alias) + "` ";
    
    if(ll_from.length() > 0) {
      if(tbl->is_inner_table_of_outer_join()) {
        ll_from += " OUTER ";
      }
      ll_from += " JOIN " + fqtn;
    } else {
      ll_from = std::string("FROM ") + fqtn;
    }

    // handle NATURAL JOIN / USING
    if(tbl->join_columns != nullptr) {
      std::string join_columns;
      for(auto join_col_it = tbl->join_columns->begin(); join_col_it != tbl->join_columns->end();++join_col_it) {
        if(join_columns.length() >0) {
          join_columns += ", ";
        }
        join_col_it->table_field->print(thd, &field_str, QT_ORDINARY);
        join_columns += std::string(field_str.ptr(), field_str.length());
      }
      ll_from += std::string("USING(") + join_columns + ")\n";
      continue;
    }
    
    String join_str;
    join_str.reserve(1024 * 1024);
    if(tbl->join_cond() != nullptr) {
      tbl->join_cond()->print(thd, &join_str, QT_ORDINARY);
      ll_from += "ON " + std::string(join_str.ptr(), join_str.length());
    }
    tbl = tbl->next_local;
  }

  /* Process the WHERE clause */
  std::string ll_where;
  if(thd->lex->select_lex->where_cond() != nullptr) {
    String where_str;
    where_str.reserve(1024 * 1024);
    thd->lex->select_lex->where_cond()->print(thd, &where_str, QT_ORDINARY);
    ll_where = std::string(where_str.ptr(), where_str.length());
  }

  /* Process the HAVING clause:
     The HAVING clause is transformed into a WHERE clause on the coordinator 
     table.  Any HAVING expressions that are not expressed on SELECT aliases
     have to be pushed into the ll_select clause so that they can be 
     filtered in the coord_where clause, but these columns are not projected
     by the coord_query clause!
  */
  std::string coord_where;
  if(select_lex->having_cond() != nullptr) {
    process_having(thd, select_lex->having_cond(), coord_having, ll_query, coord_group, used_fields);
  }


  std::cout << "PROCESSED QUERY ELEMENTS:\n----------------------------\n";
  std::cout << "     ORIG QUERY: " << std::string(orig_query.str, orig_query.length) << "\n";
  std::cout << "       LL_QUERY: " << ll_query << "\n";
  std::cout << "    COORD_QUERY: " << coord_query << "\n";
  std::cout << "    LL GROUP BY: " << ll_group << "\n";
  std::cout << " COORD GROUP BY: " << coord_group << "\n";
  std::cout << " LL_FROM_CLAUSE: " << ll_from << "\n";
  std::cout << "LL_WHERE_CLAUSE: " << ll_where << "\n";
  std::cout << "   COORD HAVING: " << coord_having << "\n";
  std::cout << "     FACT ALIAS: " << fact_alias << "\n";
  std::cout << "FACT PARTITIONS: " << partition_list << "\n";
  
  
  std::string call_sql = 
    "CALL warpsql.parallel_query(\n";
    call_sql +=
    '"' + escape_for_call(ll_query) + "\",\n" +
    '"' + escape_for_call(coord_query) + "\",\n" +
    '"' + escape_for_call(ll_group) + "\",\n" +
    '"' + escape_for_call(coord_group) + "\",\n" +
    '"' + escape_for_call(ll_from) + "\",\n" +
    '"' + escape_for_call(ll_where) + "\",\n" +
    '"' + escape_for_call(coord_having) + "\",\n" +
    '"' + escape_for_call(fact_alias) + ": " + partition_list + "\");\n";
  
  std::cout << "PARALLEL QUERY INTERFACE EXEC CALL:\n------------------\n";
  std::cout << call_sql << "\n";
  return 0;
}
