/* Copyright (c) 2008, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  The actual probe names in DTrace scripts will replace '__' by '-'. Thus
  insert__row__start will be insert-row-start.

  Recommendations for adding new probes:

  - each probe should have the minimal set of arguments required to
  unambiguously identify the context in which the probe fires. Redundant
  arguments (i.e. the ones that can be obtained in user scripts from previous
  probes' arguments or otherwise) may be added for convenience.

  - try to avoid computationally expensive probe arguments. If impossible,
  use *_ENABLED() macros to check if the probe is activated before
  performing expensive calculations for a probe argument.

  - all *-done probes should have a status argument wherever applicable to make
  it possible for user scripts to figure out whether the completed operation
  was successful or not.
  
  - for all status arguments, a non-zero value should be returned on error or
  failure, 0 should be returned on success.
*/

provider mysql {
  
  /* The following ones fire when creating or closing a client connection */
  probe connection__start(unsigned long conn_id, char *user, char *host);
  probe connection__done(int status, unsigned long conn_id);

  /*
    Fire at the start/end of any client command processing (including SQL
    queries).
  */
  probe command__start(unsigned long conn_id, int command,
                       char *user, char *host);
  probe command__done(int status);
  
  /*
    The following probes fire at the start/end of any SQL query processing,
    respectively.

    query_start() has a lot of parameters that can be used to pick up
    parameters for a lot of other probes here.  For simplicity reasons we also
    add the query string to most other DTrace probes as well. Hostname is
    either the hostname or the IP address of the MySQL Client.
  */
  probe query__start(char *query,
                     unsigned long conn_id,
                     char *db_name,
                     char *user,
                     char *host);
  probe query__done(int status); 

  /* Fire at the start/end of SQL query parsing */
  probe query__parse__start(char *query);
  probe query__parse__done(int status);

  /* Track whether the query hits the query cache or not */
  probe query__cache__hit(char *query, unsigned long rows);
  probe query__cache__miss(char *query);

  /*
    This probe fires when the actual query execution starts, i.e. after
    parsing and checking the query cache, but before privilege checks,
    optimizing, etc.

    Query means also all independent queries of a stored procedure and prepared
    statements. Also the stored procedure itself is a query.

    exec_type is:
    0:           Executed query from sql_parse, top-level query (sql_parse.cc)
    1:           Executed prepared statement (sql_prepare.cc)
    2:           Executed cursor statement (sql_cursor.cc)
    3:           Executed query in stored procedure (sp_head.cc)
  */
  probe query__exec__start(char *query,
                           unsigned long connid,
                           char *db_name,
                           char *user,
                           char *host,
                           int exec_type);
  probe query__exec__done(int status);

  /* These probes fire when performing row operations towards any handler */
  probe insert__row__start(char *db, char *table);
  probe insert__row__done(int status);
  probe update__row__start(char *db, char *table);
  probe update__row__done(int status);
  probe delete__row__start(char *db, char *table);
  probe delete__row__done(int status);
  probe read__row__start(char *db, char *table, int scan_flag);
  probe read__row__done(int status);
  probe index__read__row__start(char *db, char *table);
  probe index__read__row__done(int status);
  
  /*
    These probes fire when calling external_lock for any handler
    depending on the lock type being acquired or released.
  */
  probe handler__rdlock__start(char *db, char *table);
  probe handler__wrlock__start(char *db, char *table);
  probe handler__unlock__start(char *db, char *table);
  probe handler__rdlock__done(int status);
  probe handler__wrlock__done(int status);
  probe handler__unlock__done(int status);
  
  /*
    These probes fire when a filesort activity happens in a query.
  */
  probe filesort__start(char *db, char *table);
  probe filesort__done(int status, unsigned long rows);
  /*
    The query types SELECT, INSERT, INSERT AS SELECT, UPDATE, UPDATE with
    multiple tables, DELETE, DELETE with multiple tables are all probed.
    The start probe always contains the query text.
  */
  probe select__start(char *query);
  probe select__done(int status, unsigned long rows);
  probe insert__start(char *query);
  probe insert__done(int status, unsigned long rows);
  probe insert__select__start(char *query);
  probe insert__select__done(int status, unsigned long rows);
  probe update__start(char *query);
  probe update__done(int status,
                     unsigned long rowsmatches, unsigned long rowschanged);
  probe multi__update__start(char *query);
  probe multi__update__done(int status,
                            unsigned long rowsmatches, 
                            unsigned long rowschanged);
  probe delete__start(char *query);
  probe delete__done(int status, unsigned long rows);
  probe multi__delete__start(char *query);
  probe multi__delete__done(int status, unsigned long rows);

  /*
    These probes can be used to measure the time waiting for network traffic
    or identify network-related problems.
  */
  probe net__read__start();
  probe net__read__done(int status, unsigned long bytes);
  probe net__write__start(unsigned long bytes);
  probe net__write__done(int status);

  /* MyISAM Key cache probes */
  probe keycache__read__start(char *filepath, unsigned long  bytes,
                              unsigned long mem_used, unsigned long mem_free);
  probe keycache__read__block(unsigned long bytes);
  probe keycache__read__hit();
  probe keycache__read__miss();
  probe keycache__read__done(unsigned long mem_used, unsigned long mem_free);
  probe keycache__write__start(char *filepath, unsigned long bytes,
                               unsigned long mem_used, unsigned long mem_free);
  probe keycache__write__block(unsigned long bytes);
  probe keycache__write__done(unsigned long mem_used, unsigned long mem_free);
};

#pragma D attributes Evolving/Evolving/Common provider mysql provider
#pragma D attributes Evolving/Evolving/Common provider mysql module
#pragma D attributes Evolving/Evolving/Common provider mysql function
#pragma D attributes Evolving/Evolving/Common provider mysql name
#pragma D attributes Evolving/Evolving/Common provider mysql args
