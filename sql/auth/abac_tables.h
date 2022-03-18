#ifndef ABAC_TABLES_H
#define ABAC_TABLES_H

#include "sql/auth/auth_common.h"
#include<string.h>
using namespace std;

class THD;
struct TABLE;

bool modify_rule_in_table(THD *thd, TABLE *table, string rule_name, int privs, bool delete_option);
bool modify_policy_user_aval_in_table(THD *thd, TABLE *table, string rule_name, 
									string attrib, string value, bool delete_option);
bool modify_policy_object_aval_in_table(THD *thd, TABLE *table, string rule_name,
									string attrib, string value, bool delete_option);
bool modify_user_attribute_in_table(THD *thd, TABLE *table, string user_attrib, 
									bool delete_option);
bool modify_object_attribute_in_table(THD *thd, TABLE *table, string object_attrib, 
									bool delete_option);			
bool modify_user_attrib_val_in_table(THD *thd, TABLE *table, LEX_USER user, 
						LEX_STRING attrib, string value, bool delete_option);	
bool modify_object_attrib_val_in_table(THD *thd, TABLE *table, LEX_CSTRING db_name, LEX_CSTRING table_name,
						LEX_STRING attrib, std::string value, bool delete_option);														
#endif