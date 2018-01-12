#ifndef BIN_STREAM_H
#define BIN_STREAM_H
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <iostream>
using namespace std;
static volatile int number_of_calls;
static void bin_log_finalize(); //function that will run when quitting mysql right now does nothing 
static void bin_log_notify(MYSQL_THD thd, mysql_event_class_t event_class, const void * event); /* core function  handler for a query on server mysql calls this function (registered function where all my logic will go */
static struct st_mysql_show_var simple_status[] =
{
  { "Audit_null_called",
    (char *)&number_of_calls,
     SHOW_INT,  SHOW_SCOPE_GLOBAL },
    #define AUDIT_NULL_VAR(x) { "bin_log_stream_" #x, (char *) &number_of_calls_ ## x, \
                            SHOW_INT,  SHOW_SCOPE_GLOBAL },
    //#include "Bin_Log_Stream_Variables.h"
    #undef AUDIT_NULL_VAR
    { 0, 0, SHOW_INT, SHOW_SCOPE_GLOBAL }
};
/* get user input to control behavior of program.
static struct st_mysql_sys_var* system_variables[] = {
  MYSQL_SYSVAR(abort_message),
  MYSQL_SYSVAR(abort_value),
  MYSQL_SYSVAR(event_order_check),
  MYSQL_SYSVAR(event_order_check_consume_ignore_count),
  MYSQL_SYSVAR(event_order_started),
  MYSQL_SYSVAR(event_order_check_exact),
  MYSQL_SYSVAR(event_record_def),
  MYSQL_SYSVAR(event_record),
  NULL
};*/

static struct st_mysql_audit bin_log_stream_descriptor = {
   MYSQL_AUDIT_INTERFACE_VERSION,                    /* interface version    */
   bin_log_finalize,                                             /* release_thd function */
   bin_log_notify,                                /* notify function for descriptor      */
   { (unsigned long) MYSQL_AUDIT_GENERAL_CLASSMASK }

   /*
  { (unsigned long) MYSQL_AUDIT_GENERAL_ALL,
    (unsigned long) MYSQL_AUDIT_CONNECTION_ALL,
    (unsigned long) MYSQL_AUDIT_PARSE_ALL,
    (unsigned long) 0, // This event class is currently not supported. 
    (unsigned long) MYSQL_AUDIT_TABLE_ACCESS_ALL,
    (unsigned long) MYSQL_AUDIT_GLOBAL_VARIABLE_ALL,
    (unsigned long) MYSQL_AUDIT_SERVER_STARTUP_ALL,
    (unsigned long) MYSQL_AUDIT_SERVER_SHUTDOWN_ALL,
    (unsigned long) MYSQL_AUDIT_COMMAND_ALL,
    (unsigned long) MYSQL_AUDIT_QUERY_ALL,
    (unsigned long) MYSQL_AUDIT_STORED_PROGRAM_ALL }*/
};
extern "C" int  simple_streamer_plugin_deinit();
extern "C" int  simple_streamer_plugin_init();

#endif
