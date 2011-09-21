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

static volatile int number_of_calls; /* for SHOW STATUS, see below */
static volatile int number_of_calls_general_log;
static volatile int number_of_calls_general_error;
static volatile int number_of_calls_general_result;


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
  number_of_calls= 0;
  number_of_calls_general_log= 0;
  number_of_calls_general_error= 0;
  number_of_calls_general_result= 0;
  return(0);
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
  return(0);
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
  number_of_calls++;
  if (event_class == MYSQL_AUDIT_GENERAL_CLASS)
  {
    const struct mysql_event_general *event_general=
      (const struct mysql_event_general *) event;
    switch (event_general->event_subclass)
    {
    case MYSQL_AUDIT_GENERAL_LOG:
      number_of_calls_general_log++;
      break;
    case MYSQL_AUDIT_GENERAL_ERROR:
      number_of_calls_general_error++;
      break;
    case MYSQL_AUDIT_GENERAL_RESULT:
      number_of_calls_general_result++;
      break;
    default:
      break;
    }
  }
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_audit audit_null_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION,                    /* interface version    */
  NULL,                                             /* release_thd function */
  audit_null_notify,                                /* notify function      */
  { (unsigned long) MYSQL_AUDIT_GENERAL_CLASSMASK } /* class mask           */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var simple_status[]=
{
  { "Audit_null_called", (char *) &number_of_calls, SHOW_INT },
  { "Audit_null_general_log", (char *) &number_of_calls_general_log, SHOW_INT },
  { "Audit_null_general_error", (char *) &number_of_calls_general_error,
    SHOW_INT },
  { "Audit_null_general_result", (char *) &number_of_calls_general_result,
    SHOW_INT },
  { 0, 0, 0}
};


/*
  Plugin library descriptor
*/

mysql_declare_plugin(audit_null)
{
  MYSQL_AUDIT_PLUGIN,         /* type                            */
  &audit_null_descriptor,     /* descriptor                      */
  "NULL_AUDIT",               /* name                            */
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

