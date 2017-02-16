#define PLUGIN_CONTEXT

#include <stdio.h>

typedef void *MYSQL_THD;
struct st_mysql_const_lex_string
{
  const char *str;
  size_t length;
};
typedef struct st_mysql_const_lex_string MYSQL_LEX_CSTRING;
enum enum_sql_command{ SQLCOM_A, SQLCOM_B };
enum enum_server_command{ SERVCOM_A, SERVCOM_B };

#include "plugin_audit_v4.h"

extern void auditing(MYSQL_THD thd, unsigned int event_class, const void *ev);
extern int get_db_mysql57(MYSQL_THD thd, char **name, int *len);


struct mysql_event_general_302
{
  unsigned int event_subclass;
  int general_error_code;
  unsigned long general_thread_id;
  const char *general_user;
  unsigned int general_user_length;
  const char *general_command;
  unsigned int general_command_length;
  const char *general_query;
  unsigned int general_query_length;
  struct charset_info_st *general_charset;
  unsigned long long general_time;
  unsigned long long general_rows;
  unsigned long long query_id;
  char *database;
  int database_length;
};


static int auditing_v4(MYSQL_THD thd, mysql_event_class_t class, const void *ev)
{
  int *subclass= (int *)ev;
  struct mysql_event_general_302 ev_302;
  int subclass_v3, subclass_orig;

  if (class != MYSQL_AUDIT_GENERAL_CLASS &&
      class != MYSQL_AUDIT_CONNECTION_CLASS)
    return 0;

  subclass_orig= *subclass;

  if (class == MYSQL_AUDIT_GENERAL_CLASS)
  {
    struct mysql_event_general *event= (struct mysql_event_general *) ev;
    ev_302.general_error_code= event->general_error_code;
    ev_302.general_thread_id= event->general_thread_id;
    ev_302.general_user= event->general_user.str;
    ev_302.general_user_length= event->general_user.length;
    ev_302.general_command= event->general_command.str;
    ev_302.general_command_length= event->general_command.length;
    ev_302.general_query= event->general_query.str;
    ev_302.general_query_length= event->general_query.length;
    ev_302.general_charset= event->general_charset;
    ev_302.general_time= event->general_time;
    ev_302.general_rows= event->general_rows;
    if (get_db_mysql57(thd, &ev_302.database, &ev_302.database_length))
    {
      ev_302.database= 0;
      ev_302.database_length= 0;
    }
    ev= &ev_302;
    switch (subclass_orig)
    {
      case MYSQL_AUDIT_GENERAL_LOG:
        subclass_v3= 0;
        ev_302.event_subclass= 0;
        break;
      case MYSQL_AUDIT_GENERAL_ERROR:
        subclass_v3= 1;
        ev_302.event_subclass= 1;
        break;
      case MYSQL_AUDIT_GENERAL_RESULT:
        subclass_v3= 2;
        ev_302.event_subclass= 2;
        break;
      case MYSQL_AUDIT_GENERAL_STATUS:
      {
        subclass_v3= 3;
        ev_302.event_subclass= 3;
        break;
      }
      default:
        return 0;
    }
  }
  else /* if (class == MYSQL_AUDIT_CONNECTION_CLASS) */
  {
    switch (subclass_orig)
    {
      case MYSQL_AUDIT_CONNECTION_CONNECT:
        subclass_v3= 0;
        break;
      case MYSQL_AUDIT_CONNECTION_DISCONNECT:
        subclass_v3= 1;
        break;
      default:
        return 0;
    }
  }

  *subclass= subclass_v3;

  auditing(thd, (int) class, ev);

  *subclass= subclass_orig;
  return 0;
}


static struct st_mysql_audit mysql_descriptor =
{
  MYSQL_AUDIT_INTERFACE_VERSION,
  NULL,
  auditing_v4,
  { (unsigned long) MYSQL_AUDIT_GENERAL_ALL,
    (unsigned long) MYSQL_AUDIT_CONNECTION_ALL,
    (unsigned long) MYSQL_AUDIT_PARSE_ALL,
    0, /* This event class is currently not supported. */
    0, /* This event class is currently not supported. */
    (unsigned long) MYSQL_AUDIT_GLOBAL_VARIABLE_ALL,
    (unsigned long) MYSQL_AUDIT_SERVER_STARTUP_ALL,
    (unsigned long) MYSQL_AUDIT_SERVER_SHUTDOWN_ALL,
    (unsigned long) MYSQL_AUDIT_COMMAND_ALL,
    (unsigned long) MYSQL_AUDIT_QUERY_ALL,
    (unsigned long) MYSQL_AUDIT_STORED_PROGRAM_ALL }
#ifdef WHEN_MYSQL_BUG_FIXED
  /*
    By this moment MySQL just sends no notifications at all
    when we request only those we actually need.
    So we have to request everything and filter them inside the
    handling function.                                
  */
  { (unsigned long) MYSQL_AUDIT_GENERAL_ALL,
    (unsigned long) (MYSQL_AUDIT_CONNECTION_CONNECT |
                     MYSQL_AUDIT_CONNECTION_DISCONNECT),
    0,
    0, /* This event class is currently not supported. */
    0, /* This event class is currently not supported. */
    0,
    0,
    0,
    0,
    0,
    0
  }
#endif /*WHEN_MYSQL_BUG_FIXED*/
};


void *mysql_v4_descriptor= &mysql_descriptor;

