/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef NDB_DAEMON_H
#define NDB_DAEMON_H

typedef int (*ndb_daemon_run_t)(int, char**);
typedef void (*ndb_daemon_stop_t)(void);

/*
  Used from "mini" main to run an application as a
  service on windows, where the "run" function will be run
  as a service if first arg is --service=<service_name>.

  Defaults to "normal" behaviour which is to call "run" directly
  with argc/argv

*/
int ndb_daemon_init(int argc, char** argv,
                    ndb_daemon_run_t run, ndb_daemon_stop_t stop,
                    const char* name, const char* display_name);

/*
  To be called at the point where an application needs to daemonize
  itself.

  The daemonizing will on most platforms do a fork itself as a daemon.
  I.e fork, setsid, create pidfile, and redirect all output to logfile.

  On windows, only create pidfile and redirect.
*/
int ndb_daemonize(const char* pidfile_name, const char *logfile_name);


/*
  To be called when application should exit.

  Performs an ordered shutdown of service if running as a serevice.
 */
void ndb_daemon_exit(int status);


/*
   if any of the functions in ndb_daemon return non-zero (failure)
   then ndb_daemon_error contains the error message
*/

extern char ndb_daemon_error[];

/*
  Print the additional arguments available for service
*/
void ndb_service_print_options(const char* name);

/*
  Utility function to make the program wait for debugger at
  a given location. Very useful for debugging a program
  started as a service.
*/
void ndb_service_wait_for_debugger(int timeout_sec);

#endif
