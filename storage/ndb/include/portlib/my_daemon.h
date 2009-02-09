/* Copyright (C) 2008 Sun Microsystems

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef MY_DAEMON_H
#define MY_DAEMON_H

#ifndef DAEMONEXPORT
#define DAEMONEXPORT extern
#endif

C_MODE_START

/*
   all functions return 0 for success so the retval can be returned from main()

   to run the daemon, you must provide a struct MY_DAEMON * to my_daemon_run().
   the two functions in that struct are start() and stop()

   int start(void*)
      . this function should do the main work of the daemon
      . on windows, it is run in a new thread.
     -- the parameter to start will always be 0.
     -- the return value of start is ignored

   int stop()
      . when called, this function must terminate the start() function
      . usually, start() runs an event loop and setting a global variable
        to 0 breaks the loop.  stop() should set this variable to 0.
     --  the return value is ignored
*/

typedef int (*daemon_start_t)(void *);
typedef int (*daemon_onstop_t)();
struct MY_DAEMON {
  daemon_start_t start;
  daemon_onstop_t stop;
};

int my_daemon_run(char *name, struct MY_DAEMON*d);
int my_daemon_files_run(char *name, struct MY_DAEMON*d, char *node_id);

/*
   if any of the functions in my_daemon return non-zero (failure)
   then my_daemon_error contains the error message
*/
extern char *my_daemon_error;

/*
   my_daemon_install() adds a service called [name] which will
   be called using the command line [cmd]
   to start a service after installing it, you can use the command line
    > net start [name]
*/
int my_daemon_install(const char *name, const char *cmd);

/*
   my_daemon_remove() deletes any service called [name]
*/
int my_daemon_remove(const char *name);

/*
   these macros are provided for convenience.  including these macros
   in your main source file give you standard options for services
*/
#define NONE
#ifdef _WIN32
#define MY_DAEMON_VARS_PASTE(prefix) \
  uchar *prefix##remove,*prefix##install,*prefix##service
#define MY_DAEMON_VARS(prefix) uchar *prefix remove,* install,*prefix service
#define MY_DAEMON_VARS0 MY_DAEMON_VARS(NONE)
#define MY_DAEMON_LONG_OPTS_PASTE(prefix) \
  {"install", 'i', "Install this program as a service.", \
   &prefix##install, &prefix##install, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0}, \
  {"remove",  'r', "Remove this program as a service.", \
   &prefix##remove,  &prefix##remove,  0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0}, \
  {"service", 's', "Internal use only (for windows service)", \
   &prefix##service, &prefix##service, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#define MY_DAEMON_LONG_OPTS(prefix) \
  {"install", 'i', "Install this service.", \
   &prefix install, &prefix install, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0}, \
  {"remove",  'r', "Remove this service.", \
   &prefix remove,  &prefix remove,  0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0}, \
  {"service", 's', "Internal use only (for windows service)", \
   &prefix service, &prefix service, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

#else
#define MY_DAEMON_VARS_PASTE(prefix)
#define MY_DAEMON_VARS(prefix)
#define MY_DAEMON_VARS0
#define MY_DAEMON_LONG_OPTS(prefix)
#define MY_DAEMON_LONG_OPTS_PASTE(prefix)
#endif
/*
   my_dlog is the log file created in daemon_files()
*/
DAEMONEXPORT FILE *my_dlog;
/*
  convenience macros -- mainly for debugging a service
*/
#define DLOG(x)    do{fprintf(my_dlog,"%s",x);fflush(my_dlog);}while(0)
#define DLOG1(x,y) do{fprintf(my_dlog,x,y);fflush(my_dlog);}while(0)

/*
   my_daemon_prefiles() checks that pidname and logname can be created
   and that pidname can be locked.
   a non-zero return usually means that the process abort -- can't create
   necessary files.
*/
int my_daemon_prefiles(const char *pidname, const char *logname);
/*
   my_daemon_files() opens the files passed in prefiles() and redirects
   stdout/stderr to the logfile.
   my_dlog points is the logfile after daemon_files()
*/
int my_daemon_files();
/*
   my_daemon_closefiles() closes files opened by my_daemon_files()
*/
int my_daemon_closefiles();

/*
   a helper function to turn a --install command line to a
   --service command line for internal use with MY_DAEMON_LONG_OPTS
*/
char *my_daemon_make_svc_cmd(int n, char **v);

C_MODE_END
#endif //MY_DAEMON_H
