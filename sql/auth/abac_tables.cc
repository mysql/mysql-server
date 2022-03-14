#include "sql/auth/abac_tables.h"

#include <string.h>
#include <memory>

#include "../sql_update.h"  // compare_records()
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/mysql_lex_string.h"
#include "mysqld_error.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/auth/sql_user_table.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/records.h"
#include "sql/row_iterator.h"
#include "sql/sql_base.h"
#include "sql/sql_const.h"
#include "sql/table.h"
#include "sql_string.h"
#include "thr_lock.h"
#include "sql/auth/auth_acls.h"

using namespace std;
class THD;

#define MYSQL_POLICY_FIELD_RULE_NAME 0
#define MYSQL_POLICY_FIELD_SELECT_PRIV 1
#define MYSQL_POLICY_FIELD_INSERT_PRIV 2
#define MYSQL_POLICY_FIELD_UPDATE_PRIV 3
#define MYSQL_POLICY_FIELD_DELETE_PRIV 4

#define MYSQL_POLICY_USER_AVAL_FIELD_RULE_NAME 0
#define MYSQL_POLICY_USER_AVAL_FIELD_ATTRIB_NAME 1
#define MYSQL_POLICY_USER_AVAL_FIELD_VALUE 2

#define MYSQL_POLICY_OBJECT_AVAL_FIELD_RULE_NAME 0
#define MYSQL_POLICY_OBJECT_AVAL_FIELD_ATTRIB_NAME 1
#define MYSQL_POLICY_OBJECT_AVAL_FIELD_VALUE 2

bool modify_rule_in_table(THD *thd, TABLE *table, string rule_name, 
												int privs, bool delete_option) {
  DBUG_TRACE;
  int ret = 0;

  Acl_table_intact table_intact(thd);

  if (table_intact.check(table, ACL_TABLES::TABLE_POLICY)) return true;

  table->use_all_columns();

  table->field[MYSQL_POLICY_FIELD_RULE_NAME]->store(
								rule_name.c_str(), rule_name.size(), system_charset_info);
	char select_field = (privs & SELECT_ACL) ? 'Y' : 'N';
	char insert_field = (privs & INSERT_ACL) ? 'Y' : 'N';
	char update_field = (privs & UPDATE_ACL) ? 'Y' : 'N';
	char delete_field = (privs & DELETE_ACL) ? 'Y' : 'N';

  table->field[MYSQL_POLICY_FIELD_SELECT_PRIV]->store(
								&select_field, 1, system_charset_info, CHECK_FIELD_IGNORE);
  table->field[MYSQL_POLICY_FIELD_INSERT_PRIV]->store(
								&insert_field, 1, system_charset_info, CHECK_FIELD_IGNORE);
  table->field[MYSQL_POLICY_FIELD_UPDATE_PRIV]->store(
								&update_field, 1, system_charset_info, CHECK_FIELD_IGNORE);
  table->field[MYSQL_POLICY_FIELD_DELETE_PRIV]->store(
								&delete_field, 1, system_charset_info, CHECK_FIELD_IGNORE); 

	if (!delete_option)
		ret = table->file->ha_write_row(table->record[0]);
	else {
		uchar user_key[MAX_KEY_LENGTH];
		key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
		ret = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT);
		if (ret != HA_ERR_KEY_NOT_FOUND) {
			ret = table->file->ha_delete_row(table->record[0]);
		}
	}
	return ret != 0;
}

bool modify_policy_user_aval_in_table(THD *thd, TABLE *table, string rule_name, 
									string attrib, string value, bool delete_option) {
	DBUG_TRACE;
  int ret = 0;

  Acl_table_intact table_intact(thd);

  if (table_intact.check(table, ACL_TABLES::TABLE_POLICY_USER_AVAL)) return true;

  table->use_all_columns();

	table->field[MYSQL_POLICY_USER_AVAL_FIELD_RULE_NAME]->store(
							rule_name.c_str(), rule_name.size(), system_charset_info);
	table->field[MYSQL_POLICY_USER_AVAL_FIELD_ATTRIB_NAME]->store(
							attrib.c_str(), attrib.size(), system_charset_info);
	table->field[MYSQL_POLICY_USER_AVAL_FIELD_VALUE]->store(
							value.c_str(), value.size(), system_charset_info);
	if (!delete_option)
		ret = table->file->ha_write_row(table->record[0]);
	else {
		uchar user_key[MAX_KEY_LENGTH];
		key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
		ret = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT);
		if (ret != HA_ERR_KEY_NOT_FOUND) {
			ret = table->file->ha_delete_row(table->record[0]);
		}
	}
	return ret != 0;
}

bool modify_policy_object_aval_in_table(THD *thd, TABLE *table, string rule_name,
									string attrib, string value, bool delete_option) {
	DBUG_TRACE;
  int ret = 0;

  Acl_table_intact table_intact(thd);

  if (table_intact.check(table, ACL_TABLES::TABLE_POLICY_OBJECT_AVAL)) return true;

  table->use_all_columns();

	table->field[MYSQL_POLICY_OBJECT_AVAL_FIELD_RULE_NAME]->store(
							rule_name.c_str(), rule_name.size(), system_charset_info);
	table->field[MYSQL_POLICY_OBJECT_AVAL_FIELD_ATTRIB_NAME]->store(
							attrib.c_str(), attrib.size(), system_charset_info);
	table->field[MYSQL_POLICY_OBJECT_AVAL_FIELD_VALUE]->store(
							value.c_str(), value.size(), system_charset_info);

	if (!delete_option)
		ret = table->file->ha_write_row(table->record[0]);
	else {
		uchar user_key[MAX_KEY_LENGTH];
		key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
		ret = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT);
		if (ret != HA_ERR_KEY_NOT_FOUND) {
			ret = table->file->ha_delete_row(table->record[0]);
		}
	}
	return ret != 0;
}