#include "hash.h" /* HASH */
#include "sql_table.h" /* TABLE */
#include "table.h" /* ST_FIELD_INFO */
#include "item.h"

/* for SHOW GLOBAL TABLE STATUS */
void update_table_stats(TABLE *table_ptr, bool follow_next);
extern HASH global_table_stats;
void init_global_table_stats(void);
void free_global_table_stats(void);
void reset_global_table_stats(void);
extern ST_FIELD_INFO table_stats_fields_info[];

int fill_table_stats(THD *thd, TABLE_LIST *tables, Item *cond);
typedef void (*table_stats_cb)(const char *db, const char *table,
                               comp_stat_t* comp_stat);
void fill_table_stats_cb(const char *db, const char *table);
