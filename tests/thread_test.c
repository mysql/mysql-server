/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>

#include <my_sys.h>
#include <my_pthread.h>
#include "mysql.h"
#include <my_getopt.h>

static my_bool version, verbose, tty_password= 0;
static uint thread_count,number_of_tests=1000,number_of_threads=2;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;

static char *database,*host,*user,*password,*unix_socket,*query;
uint tcp_port;

#ifndef __WIN__
void *test_thread(void *arg __attribute__((unused)))
#else
unsigned __stdcall test_thread(void *arg __attribute__((unused)))
#endif
{
  MYSQL *mysql;
  uint count;

  mysql=mysql_init(NULL);
  if (!mysql_real_connect(mysql,host,user,password,database,tcp_port,
			  unix_socket,0))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n\n",mysql_error(mysql));
    perror("");
    goto end;
  }
  mysql.reconnect= 1;
  if (verbose) { putchar('*'); fflush(stdout); }
  for (count=0 ; count < number_of_tests ; count++)
  {
    MYSQL_RES *res;
    if (mysql_query(mysql,query))
    {
      fprintf(stderr,"Query failed (%s)\n",mysql_error(mysql));
      goto end;
    }
    if (!(res=mysql_store_result(mysql)))
    {
      fprintf(stderr,"Couldn't get result from %s\n", mysql_error(mysql));
      goto end;
    }
    mysql_free_result(res);
    if (verbose) { putchar('.'); fflush(stdout); }
  }
end:
  if (verbose) { putchar('#'); fflush(stdout); }
  mysql_close(mysql);
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  pthread_cond_signal(&COND_thread_count); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_exit(0);
  return 0;
}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", &database, &database,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", &host, &host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user", &user,
   &user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write some progress indicators", &verbose,
   &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"query", 'Q', "Query to execute in each threads", &query,
   &query, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &tcp_port,
   &tcp_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection", &unix_socket,
   &unix_socket, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"test-count", 'c', "Run test count times (default %d)",
   &number_of_tests, &number_of_tests, 0, GET_UINT,
   REQUIRED_ARG, 1000, 0, 0, 0, 0, 0},
  {"thread-count", 't', "Number of threads to start",
   &number_of_threads, &number_of_threads, 0, GET_UINT,
   REQUIRED_ARG, 2, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static const char *load_default_groups[]= { "client",0 };

static void usage()
{
  printf("Connection to a mysql server with multiple threads\n");
  if (version)
    return;
  puts("This software comes with ABSOLUTELY NO WARRANTY.\n");
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);

  my_print_help(my_long_options);
  print_defaults("my",load_default_groups);
  my_print_variables(my_long_options);
  printf("\nExample usage:\n\n\
%s -Q 'select * from mysql.user' -c %d -t %d\n",
	 my_progname, number_of_tests, number_of_threads);
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'p':
    if (argument)
    {
      my_free(password);
      password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
    }
    else
      tty_password= 1;
    break;
  case 'V':
    version= 1;
    usage();
    exit(0);
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(1);
    break;
  }
  return 0;
}


static void get_options(int argc, char **argv)
{
  int ho_error;

  if ((ho_error= load_defaults("my",load_default_groups,&argc,&argv)) ||
      (ho_error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  free_defaults(argv);
  if (tty_password)
    password=get_tty_password(NullS);
  return;
}


int main(int argc, char **argv)
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  uint i;
  int error;
  MY_INIT(argv[0]);
  get_options(argc,argv);

  if ((error=pthread_cond_init(&COND_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  pthread_mutex_init(&LOCK_thread_count,MY_MUTEX_INIT_FAST);

  if ((error=pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED)))
  {
    fprintf(stderr,
	    "Got error: %d from pthread_attr_setdetachstate (errno: %d)",
	    error,errno);
    exit(1);
  }

  printf("Init ok. Creating %d threads\n",number_of_threads);
  for (i=1 ; i <= number_of_threads ; i++)
  {
    int *param= &i;

    if (verbose) { putchar('+'); fflush(stdout); }
    pthread_mutex_lock(&LOCK_thread_count);
    if ((error=pthread_create(&tid,&thr_attr,test_thread,(void*) param)))
    {
      fprintf(stderr,"\nGot error: %d from pthread_create (errno: %d) when creating thread: %i\n",
	      error,errno,i);
      pthread_mutex_unlock(&LOCK_thread_count);
      exit(1);
    }
    thread_count++;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  printf("Waiting for threads to finnish\n");
  error=pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    if ((error=pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"\nGot error: %d from pthread_cond_wait\n",error);
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_attr_destroy(&thr_attr);
  printf("\nend\n");

  my_end(0);
  return 0;

  exit(0);
  return 0;					/* Keep some compilers happy */
}

