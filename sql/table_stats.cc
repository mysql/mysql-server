#include "item.h" /* Item */
#include "log.h" /* sql_print_error() */
#include "hash.h" /* HASH */
#include "sql_show.h" /* schema_table_store_record() */
#include "table.h" /* TABLE */

HASH global_table_stats;
static pthread_mutex_t LOCK_global_table_stats;

/*
  Update global table statistics for this table and optionally tables
  linked via TABLE::next.

  SYNOPSIS
    update_table_stats()
      tablep - the table for which global table stats are updated
      follow_next - when TRUE, update global stats for tables linked
                    via TABLE::next
 */
void update_table_stats(TABLE *tablep, bool follow_next)
{
  for (; tablep; tablep= tablep->next)
  {
    if (tablep->file)
      tablep->file->update_global_table_stats();

    if (!follow_next)
      return;
  }
}

static void
clear_table_stats_counters(TABLE_STATS* table_stats)
{
	memset(&table_stats->comp_stat, 0, sizeof(table_stats->comp_stat));
}


static TABLE_STATS*
get_table_stats_by_name(const char *db_name,
                        const char *table_name,
                        const char *cache_key,
                        uint cache_key_length,
                        TABLE *tbl)
{
  TABLE_STATS* table_stats;
  char local_cache_key[NAME_LEN * 2 + 2];

  DBUG_ASSERT(db_name && table_name);
  DBUG_ASSERT(cache_key_length <= (NAME_LEN * 2 + 2));

  if (!db_name || !table_name)
  {
    sql_print_error("No key for table stats.");
    return NULL;
  }

  if (cache_key_length > (NAME_LEN * 2 + 2))
  {
    sql_print_error("Cache key length too long for table stats.");
    return NULL;
  }

  if (!cache_key)
  {
    size_t db_name_len= strlen(db_name);
    size_t table_name_len= strlen(table_name);

    if (db_name_len > NAME_LEN || table_name_len > NAME_LEN)
    {
      sql_print_error("Db or table name too long for table stats :%s:%s:\n",
                      db_name, table_name);
      return NULL;
    }

    cache_key = local_cache_key;
    cache_key_length = db_name_len + table_name_len + 2;

    strcpy(local_cache_key, db_name);
    strcpy(local_cache_key + db_name_len + 1, table_name);
  }

  pthread_mutex_lock(&LOCK_global_table_stats);

  // Get or create the TABLE_STATS object for this table.
  if (!(table_stats= (TABLE_STATS*)my_hash_search(&global_table_stats,
                                               (uchar*)cache_key,
                                               cache_key_length)))
  {
    if (!(table_stats= ((TABLE_STATS*)my_malloc(sizeof(TABLE_STATS),
                                                MYF(MY_WME)))))
    {
      sql_print_error("Cannot allocate memory for TABLE_STATS.");
      pthread_mutex_unlock(&LOCK_global_table_stats);
      return NULL;
    }

    memcpy(table_stats->hash_key, cache_key, cache_key_length);
    table_stats->hash_key_len= cache_key_length;

    if (snprintf(table_stats->db, NAME_LEN+1, "%s", db_name) < 0 ||
        snprintf(table_stats->table, NAME_LEN+1, "%s",
                 table_name) < 0)
    {
      sql_print_error("Cannot generate name for table stats.");
      my_free((char*)table_stats);
      pthread_mutex_unlock(&LOCK_global_table_stats);
      return NULL;
    }

    clear_table_stats_counters(table_stats);

    if (my_hash_insert(&global_table_stats, (uchar*)table_stats))
    {
      // Out of memory.
      sql_print_error("Inserting table stats failed.");
      my_free((char*)table_stats);
      pthread_mutex_unlock(&LOCK_global_table_stats);
      return NULL;
    }
  }

  pthread_mutex_unlock(&LOCK_global_table_stats);

  return table_stats;
}

/*
  Return the global TABLE_STATS object for a table.

  SYNOPSIS
    get_table_stats()
    table          in: table for which an object is returned
    type_of_db     in: storage engine type

  RETURN VALUE
    TABLE_STATS structure for the requested table
    NULL on failure
*/
TABLE_STATS*
get_table_stats(TABLE *table)
{
  DBUG_ASSERT(table->s);

  if (!table->s)
  {
    sql_print_error("No key for table stats.");
    return NULL;
  }

  return get_table_stats_by_name(table->s->db.str,
                                 table->s->table_name.str,
                                 table->s->table_cache_key.str,
                                 table->s->table_cache_key.length,
                                 table);
}
  
extern "C" uchar *get_key_table_stats(TABLE_STATS *table_stats, size_t *length,
                                      my_bool not_used __attribute__((unused)))
{
  *length = table_stats->hash_key_len;
  return (uchar*)table_stats->hash_key;
}

extern "C" void free_table_stats(TABLE_STATS* table_stats)
{
  my_free((char*)table_stats);
}

my_bool global_table_stats_inited = FALSE;

void init_global_table_stats(void)
{
  pthread_mutex_init(&LOCK_global_table_stats, MY_MUTEX_INIT_FAST);
  if (my_hash_init(&global_table_stats, system_charset_info, max_connections,
                0, 0, (my_hash_get_key)get_key_table_stats,
                (my_hash_free_key)free_table_stats, 0)) {
    sql_print_error("Initializing global_table_stats failed.");
    unireg_abort(1);
  }
  global_table_stats_inited = TRUE;
}

void free_global_table_stats(void)
{
  if (global_table_stats_inited) {
    my_hash_free(&global_table_stats);
    pthread_mutex_destroy(&LOCK_global_table_stats);
  }
}

void reset_global_table_stats()
{
  pthread_mutex_lock(&LOCK_global_table_stats);

  for (unsigned i = 0; i < global_table_stats.records; ++i) {
    TABLE_STATS *table_stats =
      (TABLE_STATS*)my_hash_element(&global_table_stats, i);
    clear_table_stats_counters(table_stats);
  }

  pthread_mutex_unlock(&LOCK_global_table_stats);
}

ST_FIELD_INFO table_stats_fields_info[]=
{
  {"TABLE_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
  {"TABLE_NAME", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},

  {"COMPRESSED_PAGE_SIZE", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_OPS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_OPS_OK", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_PRIMARY_OPS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_PRIMARY_OPS_OK", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_USECS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_OK_USECS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_PRIMARY_USECS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"COMPRESS_PRIMARY_OK_USECS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"UNCOMPRESS_OPS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},
  {"UNCOMPRESS_USECS", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, SKIP_OPEN_TABLE},

  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}
};

void fill_table_stats_cb(const char *db,
                         const char *table,
                         comp_stat_t *comp_stat)
{
  TABLE_STATS *stats;

  stats= get_table_stats_by_name(db, table, NULL, 0, NULL);
  if (!stats)
    return;

  stats->comp_stat = *comp_stat;
}

int fill_table_stats(THD *thd, TABLE_LIST *tables, Item *cond)
{
  DBUG_ENTER("fill_table_stats");
  TABLE* table= tables->table;

  ha_get_table_stats(fill_table_stats_cb);

  pthread_mutex_lock(&LOCK_global_table_stats);

  for (unsigned i = 0; i < global_table_stats.records; ++i) {
    int f= 0;

    TABLE_STATS *table_stats =
      (TABLE_STATS*)my_hash_element(&global_table_stats, i);

    if (table_stats->comp_stat.compressed == 0 &&
        table_stats->comp_stat.compressed_ok == 0 &&
        table_stats->comp_stat.compressed_usec == 0 &&
        table_stats->comp_stat.compressed_ok_usec == 0 &&
        table_stats->comp_stat.decompressed == 0 &&
        table_stats->comp_stat.decompressed_usec == 0)
    {
      continue;
    }

    restore_record(table, s->default_values);
    table->field[f++]->store(table_stats->db, strlen(table_stats->db),
                           system_charset_info);
    table->field[f++]->store(table_stats->table, strlen(table_stats->table),
                             system_charset_info);

    table->field[f++]->store(table_stats->comp_stat.page_size, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_ok, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_primary, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_primary_ok, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_usec, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_ok_usec, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_primary_usec, TRUE);
    table->field[f++]->store(table_stats->comp_stat.compressed_primary_ok_usec, TRUE);
    table->field[f++]->store(table_stats->comp_stat.decompressed, TRUE);
    table->field[f++]->store(table_stats->comp_stat.decompressed_usec, TRUE);

    if (schema_table_store_record(thd, table))
    {
      pthread_mutex_unlock(&LOCK_global_table_stats);
      DBUG_RETURN(-1);
    }
  }
  pthread_mutex_unlock(&LOCK_global_table_stats);

  DBUG_RETURN(0);
}

