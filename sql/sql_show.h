
#ifndef SQL_SHOW_H
#define SQL_SHOW_H

/* Forward declarations */
class String;
class THD;
struct st_ha_create_information;
struct st_table_list;
typedef st_ha_create_information HA_CREATE_INFO;
typedef st_table_list TABLE_LIST;

enum find_files_result {
  FIND_FILES_OK,
  FIND_FILES_OOM,
  FIND_FILES_DIR
};

find_files_result find_files(THD *thd, List<char> *files, const char *db,
                             const char *path, const char *wild, bool dir);

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO  *create_info_arg);
int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

int copy_event_to_schema_table(THD *thd, TABLE *sch_table, TABLE *event_table);

#endif /* SQL_SHOW_H */
