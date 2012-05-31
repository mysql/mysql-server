/* Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_acl.h"                            /* GLOBAL_ACLS */

class Comp_creator;
class Item;
class Object_creation_ctx;
class Parser_state;
struct TABLE_LIST;
class THD;
class Table_ident;
struct LEX;

enum enum_mysql_completiontype {
  ROLLBACK_RELEASE=-2, ROLLBACK=1,  ROLLBACK_AND_CHAIN=7,
  COMMIT_RELEASE=-1,   COMMIT=0,    COMMIT_AND_CHAIN=6
};

extern "C" int test_if_data_home_dir(const char *dir);

bool stmt_causes_implicit_commit(const THD *thd, uint mask);

bool select_precheck(THD *thd, LEX *lex, TABLE_LIST *tables,
                     TABLE_LIST *first_table);
bool multi_update_precheck(THD *thd, TABLE_LIST *tables);
bool multi_delete_precheck(THD *thd, TABLE_LIST *tables);
int mysql_multi_update_prepare(THD *thd);
int mysql_multi_delete_prepare(THD *thd, uint *table_count);
bool mysql_insert_select_prepare(THD *thd);
bool update_precheck(THD *thd, TABLE_LIST *tables);
bool delete_precheck(THD *thd, TABLE_LIST *tables);
bool insert_precheck(THD *thd, TABLE_LIST *tables);
bool create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table);

bool parse_sql(THD *thd,
               Parser_state *parser_state,
               Object_creation_ctx *creation_ctx);

uint kill_one_thread(THD *thd, ulong id, bool only_kill_query);

void free_items(Item *item);
void cleanup_items(Item *item);

Comp_creator *comp_eq_creator(bool invert);
Comp_creator *comp_ge_creator(bool invert);
Comp_creator *comp_gt_creator(bool invert);
Comp_creator *comp_le_creator(bool invert);
Comp_creator *comp_lt_creator(bool invert);
Comp_creator *comp_ne_creator(bool invert);

int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx);
void get_default_definer(THD *thd, LEX_USER *definer);
LEX_USER *create_default_definer(THD *thd);
LEX_USER *create_definer(THD *thd, LEX_STRING *user_name, LEX_STRING *host_name);
LEX_USER *get_current_user(THD *thd, LEX_USER *user);
bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length);
bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, const CHARSET_INFO *cs,
                              bool no_error);
const CHARSET_INFO* merge_charset_and_collation(const CHARSET_INFO *cs,
                                                const CHARSET_INFO *cl);
bool check_host_name(LEX_STRING *str);
bool check_identifier_name(LEX_STRING *str, uint max_char_length,
                           uint err_code, const char *param_for_err_msg);
bool mysql_test_parse_for_slave(THD *thd,char *inBuf,uint length);
bool is_update_query(enum enum_sql_command command);
bool is_explainable_query(enum enum_sql_command command);
bool is_log_table_write_query(enum enum_sql_command command);
bool alloc_query(THD *thd, const char *packet, uint packet_length);
void mysql_init_select(LEX *lex);
void mysql_parse(THD *thd, char *rawbuf, uint length,
                 Parser_state *parser_state);
void mysql_reset_thd_for_next_command(THD *thd);
bool mysql_new_select(LEX *lex, bool move_down);
void create_select_for_variable(const char *var_name);
void create_table_set_open_action_and_adjust_tables(LEX *lex);
void mysql_init_multi_delete(LEX *lex);
bool multi_delete_set_locks_and_link_aux_tables(LEX *lex);
void create_table_set_open_action_and_adjust_tables(LEX *lex);
pthread_handler_t handle_bootstrap(void *arg);
int mysql_execute_command(THD *thd);
bool do_command(THD *thd);
void do_handle_bootstrap(THD *thd);
bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length);
void log_slow_statement(THD *thd);
bool log_slow_applicable(THD *thd);
void log_slow_do(THD *thd);
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
		       uint uint_geom_type);
bool add_to_list(THD *thd, SQL_I_List<ORDER> &list, Item *group, bool asc);
void add_join_on(TABLE_LIST *b,Item *expr);
void add_join_natural(TABLE_LIST *a,TABLE_LIST *b,List<String> *using_fields,
                      SELECT_LEX *lex);
bool push_new_name_resolution_context(THD *thd,
                                      TABLE_LIST *left_op,
                                      TABLE_LIST *right_op);
void store_position_for_column(const char *name);
void init_update_queries(void);
bool check_simple_select();
Item *negate_expression(THD *thd, Item *expr);
bool check_stack_overrun(THD *thd, long margin, uchar *dummy);

/* Variables */

extern const char* any_db;
extern uint sql_command_flags[];
extern uint server_command_flags[];
extern const LEX_STRING command_name[];
extern uint server_command_flags[];

/* Inline functions */
inline bool check_identifier_name(LEX_STRING *str, uint err_code)
{
  return check_identifier_name(str, NAME_CHAR_LEN, err_code, "");
}

inline bool check_identifier_name(LEX_STRING *str)
{
  return check_identifier_name(str, NAME_CHAR_LEN, 0, "");
}

#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *tables);
bool check_single_table_access(THD *thd, ulong privilege,
			   TABLE_LIST *tables, bool no_errors);
bool check_routine_access(THD *thd,ulong want_access,char *db,char *name,
			  bool is_proc, bool no_errors);
bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table);
bool check_some_routine_access(THD *thd, const char *db, const char *name, bool is_proc);
bool check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
                  GRANT_INTERNAL_INFO *grant_internal_info,
                  bool dont_check_global_grants, bool no_errors);
bool check_table_access(THD *thd, ulong requirements,TABLE_LIST *tables,
                        bool any_combination_of_privileges_will_do,
                        uint number,
                        bool no_errors);
#else
inline bool check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *tables)
{ return false; }
inline bool check_single_table_access(THD *thd, ulong privilege,
			   TABLE_LIST *tables, bool no_errors)
{ return false; }
inline bool check_routine_access(THD *thd,ulong want_access,char *db,
                                 char *name, bool is_proc, bool no_errors)
{ return false; }
inline bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table)
{
  table->grant.privilege= want_access;
  return false;
}
inline bool check_some_routine_access(THD *thd, const char *db,
                                      const char *name, bool is_proc)
{ return false; }
inline bool check_access(THD *, ulong, const char *, ulong *save_priv,
                         GRANT_INTERNAL_INFO *, bool, bool)
{
  if (save_priv)
    *save_priv= GLOBAL_ACLS;
  return false;
}
inline bool
check_table_access(THD *thd, ulong requirements,TABLE_LIST *tables,
                   bool any_combination_of_privileges_will_do,
                   uint number,
                   bool no_errors)
{ return false; }
#endif /*NO_EMBEDDED_ACCESS_CHECKS*/

/* These were under the INNODB_COMPATIBILITY_HOOKS */

bool check_global_access(THD *thd, ulong want_access);

inline bool is_supported_parser_charset(const CHARSET_INFO *cs)
{
  return (cs->mbminlen == 1);
}

extern "C" bool sqlcom_can_generate_row_events(const THD *thd);


#endif /* SQL_PARSE_INCLUDED */
