
#ifndef SQL_SHOW_H
#define SQL_SHOW_H

/* Forward declarations */
class String;
class THD;
struct st_ha_create_information;
struct st_table_list;
typedef st_ha_create_information HA_CREATE_INFO;
typedef st_table_list TABLE_LIST;

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO  *create_info_arg);
int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

int copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table);

#endif /* SQL_SHOW_H */
