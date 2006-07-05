#ifndef _EVENT_PRIV_H_
#define _EVENT_PRIV_H_
/* Copyright (C) 2004-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define EVENT_EXEC_STARTED      0
#define EVENT_EXEC_ALREADY_EXEC 1
#define EVENT_EXEC_CANT_FORK    2

#define EVEX_DB_FIELD_LEN 64
#define EVEX_NAME_FIELD_LEN 64
#define EVEX_MAX_INTERVAL_VALUE 2147483647L

class Event_timed;

int
evex_db_find_event_by_name(THD *thd, const LEX_STRING dbname,
                          const LEX_STRING ev_name,
                          TABLE *table);

int
db_drop_event(THD *thd, Event_timed *et, bool drop_if_exists,
              uint *rows_affected);
int
db_find_event(THD *thd, sp_name *name, Event_timed **ett, TABLE *tbl,
              MEM_ROOT *root);

int
db_create_event(THD *thd, Event_timed *et, my_bool create_if_not,
                uint *rows_affected);

int
db_drop_events_from_table(THD *thd, LEX_STRING *db);

int
sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs);

/* Compares only the name part of the identifier */
bool
event_timed_name_equal(Event_timed *et, LEX_STRING *name);

/* Compares only the schema part of the identifier */
bool
event_timed_db_equal(Event_timed *et, LEX_STRING *db);

/*
  Compares only the definer part of the identifier. Use during DROP USER
  to drop user's events. (Still not implemented)
*/
bool
event_timed_definer_equal(Event_timed *et, LEX_STRING *definer);

/* Compares the whole identifier*/
bool
event_timed_identifier_equal(Event_timed *a, Event_timed *b);


bool
change_security_context(THD *thd, LEX_STRING user, LEX_STRING host,
                        LEX_STRING db, Security_context *s_ctx,
                        Security_context **backup);

void
restore_security_context(THD *thd, Security_context *backup);

#endif /* _EVENT_PRIV_H_ */
