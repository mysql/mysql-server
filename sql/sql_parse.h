/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_PARSE_INCLUDED
#define SQL_PARSE_INCLUDED

#include "my_global.h"
#include "handler.h"                 // enum_schema_tables
#include "mysqld_thd_manager.h"      // Find_THD_Impl
#include "sql_class.h"               // THD

class Comp_creator;
class Item;
class Object_creation_ctx;
class Parser_state;
class Table_ident;
struct LEX;
struct Parse_context;
struct TABLE_LIST;
class THD;
union COM_DATA;
typedef struct st_lex_user LEX_USER;
typedef struct st_order ORDER;
typedef class st_select_lex SELECT_LEX;

/* in sql_client.cc */
class SQLRow;
class SQLCursor;
class SQLClient;

extern "C" int test_if_data_home_dir(const char *dir);

bool stmt_causes_implicit_commit(const THD *thd, uint mask);

#ifndef DBUG_OFF
extern void turn_parser_debug_on();
#endif

bool parse_sql(THD *thd,
               Parser_state *parser_state,
               Object_creation_ctx *creation_ctx);

void free_items(Item *item);
void cleanup_items(Item *item);

Comp_creator *comp_eq_creator(bool invert);
Comp_creator *comp_equal_creator(bool invert);
Comp_creator *comp_ge_creator(bool invert);
Comp_creator *comp_gt_creator(bool invert);
Comp_creator *comp_le_creator(bool invert);
Comp_creator *comp_lt_creator(bool invert);
Comp_creator *comp_ne_creator(bool invert);

int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx);
void get_default_definer(THD *thd, LEX_USER *definer);
LEX_USER *create_default_definer(THD *thd);
LEX_USER *get_current_user(THD *thd, LEX_USER *user);
bool check_string_char_length(const LEX_CSTRING &str, const char *err_msg,
                              size_t max_char_length, const CHARSET_INFO *cs,
                              bool no_error);
const CHARSET_INFO* merge_charset_and_collation(const CHARSET_INFO *cs,
                                                const CHARSET_INFO *cl);
bool check_host_name(const LEX_CSTRING &str);
bool mysql_test_parse_for_slave(THD *thd);
bool is_update_query(enum enum_sql_command command);
bool is_explainable_query(enum enum_sql_command command);
bool is_log_table_write_query(enum enum_sql_command command);
bool alloc_query(THD *thd, const char *packet, size_t packet_length);
void mysql_parse(THD *thd, Parser_state *parser_state);
void mysql_reset_thd_for_next_command(THD *thd);
void create_select_for_variable(Parse_context *pc, const char *var_name);
void create_table_set_open_action_and_adjust_tables(LEX *lex);
void mysql_init_multi_delete(LEX *lex);
void create_table_set_open_action_and_adjust_tables(LEX *lex);
int mysql_execute_command(THD *thd, bool first_level = false);
bool do_command(THD *thd);
bool dispatch_command(THD *thd, const COM_DATA *com_data,
                      enum enum_server_command command);
bool append_file_to_dir(THD *thd, const char **filename_ptr,
                        const char *table_name);
bool append_file_to_dir(THD *thd, const char **filename_ptr,
                        const char *table_name);
void execute_init_command(THD *thd, LEX_STRING *init_command,
                          mysql_rwlock_t *var_lock);
bool add_field_to_list(THD *thd, LEX_STRING *field_name, enum enum_field_types type,
		       char *length, char *decimal,
		       uint type_modifier,
		       Item *default_value, Item *on_update_value,
		       LEX_STRING *comment,
		       char *change, List<String> *interval_list,
		       const CHARSET_INFO *cs,
		       uint uint_geom_type,
                       Generated_column *gcol_info);
void add_to_list(SQL_I_List<ORDER> &list, ORDER *order);
void add_join_on(TABLE_LIST *b,Item *expr);
void add_join_natural(TABLE_LIST *a,TABLE_LIST *b,List<String> *using_fields,
                      SELECT_LEX *lex);
bool push_new_name_resolution_context(Parse_context *pc,
                                      TABLE_LIST *left_op,
                                      TABLE_LIST *right_op);
void store_position_for_column(const char *name);
void init_update_queries(void);
Item *negate_expression(Parse_context *pc, Item *expr);
bool check_stack_overrun(THD *thd, long margin, uchar *dummy);
void killall_non_super_threads(THD *thd);
bool shutdown(THD *thd, enum mysql_enum_shutdown_level level, enum enum_server_command command);

/* Variables */

extern uint sql_command_flags[];
extern const LEX_STRING command_name[];

// Statement timeout function(s)
void reset_statement_timer(THD *thd);

inline bool is_supported_parser_charset(const CHARSET_INFO *cs)
{
  return (cs->mbminlen == 1);
}

bool sqlcom_can_generate_row_events(enum enum_sql_command command);

/**
  Callback function used by kill_one_thread and timer_notify functions
  to find "thd" based on the thread id.

  @note It acquires LOCK_thd_data mutex when it finds matching thd.
  It is the responsibility of the caller to release this mutex.
*/
class Find_thd_with_id: public Find_THD_Impl
{
public:
  Find_thd_with_id(ulong value): m_id(value) {}
  virtual bool operator()(THD *thd)
  {
    if (thd->get_command() == COM_DAEMON)
      return false;
    if (thd->thread_id() == m_id)
    {
      mysql_mutex_lock(&thd->LOCK_thd_data);
      return true;
    }
    return false;
  }
private:
  ulong m_id;
};


#ifdef HAVE_REPLICATION
bool all_tables_not_ok(THD *thd, TABLE_LIST *tables);
#endif /*HAVE_REPLICATION*/
bool some_non_temp_table_to_be_updated(THD *thd, TABLE_LIST *tables);

#endif /* SQL_PARSE_INCLUDED */
