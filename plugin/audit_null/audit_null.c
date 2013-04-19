/* Copyright (c) 2006, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <stdio.h>
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

static volatile int ncalls; /* for SHOW STATUS, see below */
static volatile int ncalls_general_log;
static volatile int ncalls_general_error;
static volatile int ncalls_general_result;

FILE *f;

/*
  Initialize the plugin at server start or plugin installation.

  SYNOPSIS
    audit_null_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int audit_null_plugin_init(void *arg __attribute__((unused)))
{
  ncalls= 0;
  ncalls_general_log= 0;
  ncalls_general_error= 0;
  ncalls_general_result= 0;

  f = fopen("audit_null_tables.log", "w");
  if (!f)
    return 1;

  return 0;
}


/*
  Terminate the plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    audit_null_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int audit_null_plugin_deinit(void *arg __attribute__((unused)))
{
  fclose(f);
  return 0;
}


/*
  Foo

  SYNOPSIS
    audit_null_notify()
      thd                connection context

  DESCRIPTION
*/

static void audit_null_notify(MYSQL_THD thd __attribute__((unused)),
                              unsigned int event_class,
                              const void *event)
{
  /* prone to races, oh well */
  ncalls++;
  if (event_class == MYSQL_AUDIT_GENERAL_CLASS)
  {
    const struct mysql_event_general *event_general=
      (const struct mysql_event_general *) event;
    switch (event_general->event_subclass)
    {
    case MYSQL_AUDIT_GENERAL_LOG:
      ncalls_general_log++;
      fprintf(f, "%s\t>> %s\n", event_general->general_user,
              event_general->general_query);
      break;
    case MYSQL_AUDIT_GENERAL_ERROR:
      ncalls_general_error++;
      break;
    case MYSQL_AUDIT_GENERAL_RESULT:
      ncalls_general_result++;
      break;
    default:
      break;
    }
  }
  else
  if (event_class == MYSQL_AUDIT_TABLE_CLASS)
  {
    const struct mysql_event_table *event_table=
      (const struct mysql_event_table *) event;
    const char *ip= event_table->ip ? event_table->ip : "";
    const char *op= 0;
    char buf[1024];

    switch (event_table->event_subclass)
    {
    case MYSQL_AUDIT_TABLE_LOCK:
      op= event_table->read_only ? "read" : "write";
      break;
    case MYSQL_AUDIT_TABLE_CREATE:
      op= "create";
      break;
    case MYSQL_AUDIT_TABLE_DROP:
      op= "drop";
      break;
    case MYSQL_AUDIT_TABLE_ALTER:
      op= "alter";
      break;
    case MYSQL_AUDIT_TABLE_RENAME:
      snprintf(buf, sizeof(buf), "rename to %s.%s",
               event_table->new_database, event_table->new_table);
      buf[sizeof(buf)-1]= 0;
      op= buf;
      break;
    }

    fprintf(f, "%s[%s] @ %s [%s]\t%s.%s : %s\n",
            event_table->priv_user, event_table->user,
            event_table->host, ip,
            event_table->database, event_table->table, op);
  }
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_audit audit_null_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION, NULL, audit_null_notify,
  { MYSQL_AUDIT_GENERAL_CLASSMASK | MYSQL_AUDIT_TABLE_CLASSMASK }
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var simple_status[]=
{
  { "Audit_null_called", (char *) &ncalls, SHOW_INT },
  { "Audit_null_general_log", (char *) &ncalls_general_log, SHOW_INT },
  { "Audit_null_general_error", (char *) &ncalls_general_error, SHOW_INT },
  { "Audit_null_general_result", (char *) &ncalls_general_result, SHOW_INT },
  { 0, 0, 0}
};


/*
  Plugin library descriptor
*/

mysql_declare_plugin(audit_null)
{
  MYSQL_AUDIT_PLUGIN,         /* type                            */
  &audit_null_descriptor,     /* descriptor                      */
  "AUDIT_NULL",               /* name                            */
  "Oracle Corp",              /* author                          */
  "Simple NULL Audit",        /* description                     */
  PLUGIN_LICENSE_GPL,
  audit_null_plugin_init,     /* init function (when loaded)     */
  audit_null_plugin_deinit,   /* deinit function (when unloaded) */
  0x0002,                     /* version                         */
  simple_status,              /* status variables                */
  NULL,                       /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;

