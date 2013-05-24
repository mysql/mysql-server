/*
  Copyright (c) 2008, Roland Bouman
  http://rpbouman.blogspot.com/
  roland.bouman@gmail.com
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the Roland Bouman nor the
        names of the contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * TODO: report query cache flags
 */
#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <sql_cache.h>
#include <sql_parse.h>          // check_global_access
#include <sql_acl.h>            // PROCESS_ACL
#include <sql_class.h>          // THD
#include <table.h>              // ST_SCHEMA_TABLE
#include <mysql/plugin.h>

class Accessible_Query_Cache : public Query_cache {
public:
  HASH *get_queries()
  {
    return &this->queries;
  }
} *qc;

bool schema_table_store_record(THD *thd, TABLE *table);

#define MAX_STATEMENT_TEXT_LENGTH 32767
#define COLUMN_STATEMENT_SCHEMA 0
#define COLUMN_STATEMENT_TEXT 1
#define COLUMN_RESULT_BLOCKS_COUNT 2
#define COLUMN_RESULT_BLOCKS_SIZE 3
#define COLUMN_RESULT_BLOCKS_SIZE_USED 4

/* ST_FIELD_INFO is defined in table.h */
static ST_FIELD_INFO qc_info_fields[]=
{
  {"STATEMENT_SCHEMA", NAME_LEN, MYSQL_TYPE_STRING, 0, 0, 0, 0},
  {"STATEMENT_TEXT", MAX_STATEMENT_TEXT_LENGTH, MYSQL_TYPE_STRING, 0, 0, 0, 0},
  {"RESULT_BLOCKS_COUNT", MY_INT32_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONG, 0, 0, 0, 0},
  {"RESULT_BLOCKS_SIZE", MY_INT32_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {"RESULT_BLOCKS_SIZE_USED", MY_INT32_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 0, 0, 0},
  {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}
};

static int qc_info_fill_table(THD *thd, TABLE_LIST *tables,
                                              COND *cond)
{
  int status= 1;
  CHARSET_INFO *scs= system_charset_info;
  TABLE *table= tables->table;
  HASH *queries = qc->get_queries();

  /* one must have PROCESS privilege to see others' queries */
  if (check_global_access(thd, PROCESS_ACL, true))
    return 0;

  if (qc->try_lock(thd))
    return 0; // QC is or is being disabled

  /* loop through all queries in the query cache */
  for (uint i= 0; i < queries->records; i++)
  {
    const uchar *query_cache_block_raw;
    Query_cache_block* query_cache_block;
    Query_cache_query* query_cache_query;
    uint result_blocks_count;
    ulonglong result_blocks_size;
    ulonglong result_blocks_size_used;
    Query_cache_block *first_result_block;
    Query_cache_block *result_block;
    const char *statement_text;
    size_t statement_text_length;
    const char *key, *db;
    size_t key_length, db_length;

    query_cache_block_raw = my_hash_element(queries, i);
    query_cache_block = (Query_cache_block*)query_cache_block_raw;
    if (query_cache_block->type != Query_cache_block::QUERY)
      continue;

    query_cache_query = query_cache_block->query();

    /* Get the actual SQL statement for this query cache query */
    statement_text = (const char*)query_cache_query->query();
    statement_text_length = strlen(statement_text);
    /* We truncate SQL statements up to MAX_STATEMENT_TEXT_LENGTH in our I_S table */
    table->field[COLUMN_STATEMENT_TEXT]->store((char*)statement_text,
           min(statement_text_length, MAX_STATEMENT_TEXT_LENGTH), scs);

    /* get the entire key that identifies this query cache query */
    key = (const char*)query_cache_query_get_key(query_cache_block_raw,
                                                 &key_length, 0);
    /* The database against which the statement is executed is part of the
       query cache query key
     */
    compile_time_assert(QUERY_CACHE_DB_LENGTH_SIZE == 2); 
    db= key + statement_text_length + 1 + QUERY_CACHE_DB_LENGTH_SIZE;
    db_length= uint2korr(db - QUERY_CACHE_DB_LENGTH_SIZE);

    table->field[COLUMN_STATEMENT_SCHEMA]->store(db, db_length, scs);

    /* If we have result blocks, process them */
    first_result_block= query_cache_query->result();
    if(first_result_block)
    {
      /* initialize so we can loop over the result blocks*/
      result_block= first_result_block;
      result_blocks_count = 1;
      result_blocks_size = result_block->length;
      result_blocks_size_used = result_block->used;

      /* loop over the result blocks*/
      while((result_block= result_block->next)!=first_result_block)
      {
        /* calculate total number of result blocks */
        result_blocks_count++;
        /* calculate total size of result blocks */
        result_blocks_size += result_block->length;
        /* calculate total of used size of result blocks */
        result_blocks_size_used += result_block->used;
      }
    }
    else
    {
      result_blocks_count = 0;
      result_blocks_size = 0;
      result_blocks_size_used = 0;
    }
    table->field[COLUMN_RESULT_BLOCKS_COUNT]->store(result_blocks_count, 0);
    table->field[COLUMN_RESULT_BLOCKS_SIZE]->store(result_blocks_size, 0);
    table->field[COLUMN_RESULT_BLOCKS_SIZE_USED]->store(result_blocks_size_used, 0);

    if (schema_table_store_record(thd, table))
      goto cleanup;
  }
  status = 0;

cleanup:
  qc->unlock();
  return status;
}

static int qc_info_plugin_init(void *p)
{
  ST_SCHEMA_TABLE *schema= (ST_SCHEMA_TABLE *)p;

  schema->fields_info= qc_info_fields;
  schema->fill_table= qc_info_fill_table;

#ifdef _WIN32
  qc = (Accessible_Query_Cache *)
    GetProcAddress(GetModuleHandle(NULL), "?query_cache@@3VQuery_cache@@A");
#else
  qc = (Accessible_Query_Cache *)&query_cache;
#endif

  return qc == 0;
}


static struct st_mysql_information_schema qc_info_plugin=
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

/*
  Plugin library descriptor
*/

maria_declare_plugin(query_cache_info)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &qc_info_plugin,
  "QUERY_CACHE_INFO",
  "Roland Bouman",
  "Lists all queries in the query cache.",
  PLUGIN_LICENSE_BSD,
  qc_info_plugin_init, /* Plugin Init */
  0,                          /* Plugin Deinit        */
  0x0100,                     /* version, hex         */
  NULL,                       /* status variables     */
  NULL,                       /* system variables     */
  "1.0",                      /* version as a string  */
  MariaDB_PLUGIN_MATURITY_ALPHA
}
maria_declare_plugin_end;

