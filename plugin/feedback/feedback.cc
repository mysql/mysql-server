/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "feedback.h"

/* MySQL functions/variables not declared in mysql_priv.h */
int fill_variables(THD *thd, TABLE_LIST *tables, COND *cond);
int fill_status(THD *thd, TABLE_LIST *tables, COND *cond);
extern ST_SCHEMA_TABLE schema_tables[];

namespace feedback {

#ifndef DBUG_OFF
ulong debug_startup_interval, debug_first_interval, debug_interval;
#endif

char server_uid_buf[SERVER_UID_SIZE+1]; ///< server uid will be written here

/* backing store for system variables */
static char *server_uid= server_uid_buf, *url, *http_proxy;
char *user_info;
ulong send_timeout, send_retry_wait;

/**
  these three are used to communicate the shutdown signal to the
  background thread
*/
mysql_mutex_t sleep_mutex;
mysql_cond_t sleep_condition;
volatile bool shutdown_plugin;
static pthread_t sender_thread;

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_sleep_mutex;
static PSI_mutex_info mutex_list[]=
{{ &key_sleep_mutex, "sleep_mutex", PSI_FLAG_GLOBAL}};

static PSI_cond_key key_sleep_cond;
static PSI_cond_info cond_list[]=
{{ &key_sleep_cond, "sleep_condition", PSI_FLAG_GLOBAL}};

static PSI_thread_key key_sender_thread;
static PSI_thread_info	thread_list[] =
{{&key_sender_thread, "sender_thread", 0}};
#endif

Url **urls;             ///< list of urls to send the report to
uint url_count;

ST_SCHEMA_TABLE *i_s_feedback; ///< table descriptor for our I_S table

/*
  the column names *must* match column names in GLOBAL_VARIABLES and
  GLOBAL_STATUS tables otherwise condition pushdown below will not work
*/
static ST_FIELD_INFO feedback_fields[] =
{
  {"VARIABLE_NAME",   255, MYSQL_TYPE_STRING, 0, 0, 0, 0},
  {"VARIABLE_VALUE", 1024, MYSQL_TYPE_STRING, 0, 0, 0, 0},
  {0, 0, MYSQL_TYPE_NULL, 0, 0, 0, 0}
};

static COND * const OOM= (COND*)1;

/**
  Generate the COND tree for the condition pushdown

  This function takes a list of strings and generates an Item tree
  corresponding to the following expression:

    field LIKE str1 OR field LIKE str2 OR field LIKE str3 OR ...

  where 'field' is the first field in the table - VARIABLE_NAME field -
  and str1, str2... are strings from the list.

  This condition is used to filter the selected rows, emulating

    SELECT * FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE ...
*/
static COND* make_cond(THD *thd, TABLE_LIST *tables, LEX_STRING *filter)
{
  Item_cond_or *res= NULL;
  Name_resolution_context nrc;
  const char *db= tables->db, *table= tables->alias,
             *field= tables->table->field[0]->field_name;
  CHARSET_INFO *cs= &my_charset_latin1;

  if (!filter->str)
    return 0;

  nrc.init();
  nrc.resolve_in_table_list_only(tables);

  res= new Item_cond_or();
  if (!res)
    return OOM;

  for (; filter->str; filter++)
  {
    Item_field  *fld= new Item_field(&nrc, db, table, field);
    Item_string *pattern= new Item_string(filter->str, filter->length, cs);
    Item_string *escape= new Item_string("\\", 1, cs);

    if (!fld || !pattern || !escape)
      return OOM;

    Item_func_like *like= new Item_func_like(fld, pattern, escape, 0);

    if (!like)
      return OOM;

    res->add(like);
  }

  if (res->fix_fields(thd, (Item**)&res))
    return OOM;

  return res;
}

/**
  System variables that we want to see in the feedback report
*/
static LEX_STRING vars_filter[]= {
  {C_STRING_WITH_LEN("auto\\_increment%")},
  {C_STRING_WITH_LEN("binlog\\_format")},
  {C_STRING_WITH_LEN("character\\_set\\_%")},
  {C_STRING_WITH_LEN("collation%")},
  {C_STRING_WITH_LEN("engine\\_condition\\_pushdown")},
  {C_STRING_WITH_LEN("event\\_scheduler")},
  {C_STRING_WITH_LEN("feedback\\_%")},
  {C_STRING_WITH_LEN("ft\\_m%")},
  {C_STRING_WITH_LEN("have\\_%")},
  {C_STRING_WITH_LEN("%\\_size")},
  {C_STRING_WITH_LEN("innodb_f%")},
  {C_STRING_WITH_LEN("%\\_length%")},
  {C_STRING_WITH_LEN("%\\_timeout")},
  {C_STRING_WITH_LEN("large\\_%")},
  {C_STRING_WITH_LEN("lc_time_names")},
  {C_STRING_WITH_LEN("log")},
  {C_STRING_WITH_LEN("log_bin")},
  {C_STRING_WITH_LEN("log_output")},
  {C_STRING_WITH_LEN("log_slow_queries")},
  {C_STRING_WITH_LEN("log_slow_time")},
  {C_STRING_WITH_LEN("lower_case%")},
  {C_STRING_WITH_LEN("max_allowed_packet")},
  {C_STRING_WITH_LEN("max_connections")},
  {C_STRING_WITH_LEN("max_prepared_stmt_count")},
  {C_STRING_WITH_LEN("max_sp_recursion_depth")},
  {C_STRING_WITH_LEN("max_user_connections")},
  {C_STRING_WITH_LEN("max_write_lock_count")},
  {C_STRING_WITH_LEN("myisam_recover_options")},
  {C_STRING_WITH_LEN("myisam_repair_threads")},
  {C_STRING_WITH_LEN("myisam_stats_method")},
  {C_STRING_WITH_LEN("myisam_use_mmap")},
  {C_STRING_WITH_LEN("net\\_%")},
  {C_STRING_WITH_LEN("new")},
  {C_STRING_WITH_LEN("old%")},
  {C_STRING_WITH_LEN("optimizer%")},
  {C_STRING_WITH_LEN("profiling")},
  {C_STRING_WITH_LEN("query_cache%")},
  {C_STRING_WITH_LEN("secure_auth")},
  {C_STRING_WITH_LEN("slow_launch_time")},
  {C_STRING_WITH_LEN("sql%")},
  {C_STRING_WITH_LEN("storage_engine")},
  {C_STRING_WITH_LEN("sync_binlog")},
  {C_STRING_WITH_LEN("table_definition_cache")},
  {C_STRING_WITH_LEN("table_open_cache")},
  {C_STRING_WITH_LEN("thread_handling")},
  {C_STRING_WITH_LEN("time_zone")},
  {C_STRING_WITH_LEN("timed_mutexes")},
  {C_STRING_WITH_LEN("version%")},
  {0, 0}
};

/**
  Status variables that we want to see in the feedback report

  (empty list = no WHERE condition)
*/
static LEX_STRING status_filter[]= {{0, 0}};

/**
  Fill our I_S table with data

  This function works by invoking fill_variables() and
  fill_status() of the corresponding I_S tables - to have
  their data UNION-ed in the same target table.
  After that it invokes our own fill_* functions
  from the utils.cc - to get the data that aren't available in the
  I_S.GLOBAL_VARIABLES and I_S.GLOBAL_STATUS.
*/
int fill_feedback(THD *thd, TABLE_LIST *tables, COND *unused)
{
  int res;
  COND *cond;

  tables->schema_table= schema_tables + SCH_GLOBAL_VARIABLES;
  cond= make_cond(thd, tables, vars_filter);
  res= (cond == OOM) ? 1 : fill_variables(thd, tables, cond);

  tables->schema_table= schema_tables + SCH_GLOBAL_STATUS;
  if (!res)
  {
    cond= make_cond(thd, tables, status_filter);
    res= (cond == OOM) ? 1 : fill_status(thd, tables, cond);
  }

  tables->schema_table= i_s_feedback;
  res= res || fill_plugin_version(thd, tables)
           || fill_misc_data(thd, tables)
           || fill_linux_info(thd, tables);

  return res;
}

/**
   plugin initialization function
*/
static int init(void *p)
{
  i_s_feedback= (ST_SCHEMA_TABLE*) p;
  /* initialize the I_S descriptor structure */
  i_s_feedback->fields_info= feedback_fields; ///< field descriptor
  i_s_feedback->fill_table= fill_feedback;    ///< how to fill the I_S table
  i_s_feedback->idx_field1 = 0;               ///< virtual index on the 1st col

#ifdef HAVE_PSI_INTERFACE
#define PSI_register(X) \
  if(PSI_server) PSI_server->register_ ## X("feedback", X ## _list, array_elements(X ## _list))
#else
#define PSI_register(X) /* no-op */
#endif

  PSI_register(mutex);
  PSI_register(cond);
  PSI_register(thread);

  if (calculate_server_uid(server_uid_buf))
    return 1;

  prepare_linux_info();

#ifndef DBUG_OFF
  if (startup_interval != debug_startup_interval ||
      first_interval != debug_first_interval ||
      interval != debug_interval)
  {
    startup_interval= debug_startup_interval;
    first_interval= debug_first_interval;
    interval= debug_interval;
    user_info= const_cast<char*>("mysql-test");
  }
#endif

  url_count= 0;
  if (*url)
  {
    // now we split url on spaces and store them in Url objects
    int slot;
    char *s, *e;

    for (s= url, url_count= 1; *s; s++)
      if (*s == ' ')
        url_count++;

    urls= (Url **)my_malloc(url_count*sizeof(Url*), MYF(MY_WME));
    if (!urls)
      return 1;

    for (s= url, e = url+1, slot= 0; e[-1]; e++)
      if (*e == 0 || *e == ' ')
      {
        if (e > s && (urls[slot]= Url::create(s, e - s)))
        {
          if (urls[slot]->set_proxy(http_proxy,
                                    http_proxy ? strlen(http_proxy) : 0))
            sql_print_error("feedback plugin: invalid proxy '%s'",
                            http_proxy ? http_proxy : "");
          slot++;
        }
        else
        {
          if (e > s)
            sql_print_error("feedback plugin: invalid url '%.*s'", (int)(e-s), s);
          url_count--;
        }
        s= e + 1;
      }

    // create a background thread to handle urls, if any
    if (url_count)
    {
      mysql_mutex_init(0, &sleep_mutex, 0);
      mysql_cond_init(0, &sleep_condition, 0);
      shutdown_plugin= false;

      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
      if (pthread_create(&sender_thread, &attr, background_thread, 0) != 0)
      {
        sql_print_error("feedback plugin: failed to start a background thread");
        return 1;
      }
    }
    else
      my_free(urls);
  }

  return 0;
}

/**
   plugin deinitialization function
*/
static int free(void *p)
{
  if (url_count)
  {
    mysql_mutex_lock(&sleep_mutex);
    shutdown_plugin= true;
    mysql_cond_signal(&sleep_condition);
    mysql_mutex_unlock(&sleep_mutex);
    pthread_join(sender_thread, NULL);

    mysql_mutex_destroy(&sleep_mutex);
    mysql_cond_destroy(&sleep_condition);

    for (uint i= 0; i < url_count; i++)
      delete urls[i];
    my_free(urls);
  }
  return 0;
}

#ifdef HAVE_OPENSSL
#define DEFAULT_PROTO "https://"
#else
#define DEFAULT_PROTO "http://"
#endif

static MYSQL_SYSVAR_STR(server_uid, server_uid,
       PLUGIN_VAR_READONLY | PLUGIN_VAR_NOCMDOPT,
       "Automatically calculated server unique id hash.", NULL, NULL, 0);
static MYSQL_SYSVAR_STR(user_info, user_info,
       PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
       "User specified string that will be included in the feedback report.",
       NULL, NULL, "");
static MYSQL_SYSVAR_STR(url, url, PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
       "Space separated URLs to send the feedback report to.", NULL, NULL,
       DEFAULT_PROTO "mariadb.org/feedback_plugin/post");
static MYSQL_SYSVAR_ULONG(send_timeout, send_timeout, PLUGIN_VAR_RQCMDARG,
       "Timeout (in seconds) for the sending the report.",
       NULL, NULL, 60, 1, 60*60*24, 1);
static MYSQL_SYSVAR_ULONG(send_retry_wait, send_retry_wait, PLUGIN_VAR_RQCMDARG,
       "Wait this many seconds before retrying a failed send.",
       NULL, NULL, 60, 1, 60*60*24, 1);
static MYSQL_SYSVAR_STR(http_proxy, http_proxy,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
       "Proxy server host:port.", NULL, NULL,0);

#ifndef DBUG_OFF
static MYSQL_SYSVAR_ULONG(debug_startup_interval, debug_startup_interval,
       PLUGIN_VAR_RQCMDARG, "for debugging only",
       NULL, NULL, startup_interval, 1, INT_MAX32, 1);
static MYSQL_SYSVAR_ULONG(debug_first_interval, debug_first_interval,
       PLUGIN_VAR_RQCMDARG, "for debugging only",
       NULL, NULL, first_interval, 1, INT_MAX32, 1);
static MYSQL_SYSVAR_ULONG(debug_interval, debug_interval,
       PLUGIN_VAR_RQCMDARG, "for debugging only",
       NULL, NULL, interval, 1, INT_MAX32, 1);
#endif

static struct st_mysql_sys_var* settings[] = {
  MYSQL_SYSVAR(server_uid),
  MYSQL_SYSVAR(user_info),
  MYSQL_SYSVAR(url),
  MYSQL_SYSVAR(send_timeout),
  MYSQL_SYSVAR(send_retry_wait),
  MYSQL_SYSVAR(http_proxy),
#ifndef DBUG_OFF
  MYSQL_SYSVAR(debug_startup_interval),
  MYSQL_SYSVAR(debug_first_interval),
  MYSQL_SYSVAR(debug_interval),
#endif
  NULL
};


static struct st_mysql_information_schema feedback =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

} // namespace feedback

mysql_declare_plugin(feedback)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &feedback::feedback,
  "FEEDBACK",
  "Sergei Golubchik",
  "MariaDB User Feedback Plugin",
  PLUGIN_LICENSE_GPL,
  feedback::init,
  feedback::free,
  0x0101,
  NULL,
  feedback::settings,
  NULL,
  0
}
mysql_declare_plugin_end;
#ifdef MARIA_PLUGIN_INTERFACE_VERSION
maria_declare_plugin(feedback)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &feedback::feedback,
  "FEEDBACK",
  "Sergei Golubchik",
  "MariaDB User Feedback Plugin",
  PLUGIN_LICENSE_GPL,
  feedback::init,
  feedback::free,
  0x0101,
  NULL,
  feedback::settings,
  "1.1",
  MariaDB_PLUGIN_MATURITY_BETA
}
maria_declare_plugin_end;
#endif
