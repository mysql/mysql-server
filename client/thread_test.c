/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include <global.h>

#ifndef THREAD

int main(int argc, char **argv)
{
  printf("This test must be compiled with multithread support to work\n");
  exit(1);
}
#else

#include <my_sys.h>
#include <my_pthread.h>
#include "mysql.h"
#include <getopt.h>

static my_bool version,verbose;
static uint thread_count,number_of_tests=1000,number_of_threads=2;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;

static char *database,*host,*user,*password,*unix_socket,*query;
uint tcp_port;

#ifndef __WIN__
void *test_thread(void *arg)
#else
unsigned __stdcall test_thread(void *arg)
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
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_exit(0);
  return 0;
}


static struct option long_options[] =
{
  {"help",	no_argument,	   0, '?'},
  {"database",  required_argument, 0, 'D'},
  {"host",	required_argument, 0, 'h'},
  {"password",	optional_argument, 0, 'p'},
  {"user",	required_argument, 0, 'u'},
  {"version",	no_argument,	   0, 'V'},
  {"verbose",	no_argument,	   0, 'v'},
  {"query",	required_argument, 0, 'Q'},
  {"port",	required_argument, 0, 'P'},
  {"socket",	required_argument, 0, 'S'},
  {"test-count",required_argument, 0, 'c'},
  {"thread-count",required_argument, 0, 't'},
  {0, 0, 0, 0}
};

static const char *load_default_groups[]= { "client",0 };

static void usage()
{
  printf("Connection to a mysql server with multiple threads\n");
  if (version)
    return;
  puts("This software comes with ABSOLUTELY NO WARRANTY.\n");
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);
  printf("\n\
  -?, --help		Display this help and exit\n\
  -c #, --test-count=#	Run test count times (default %d)\n",number_of_tests);
  printf("\
  -D, --database=..	Database to use\n\
  -h, --host=...	Connect to host\n\
  -p[password], --password[=...]\n\
			Password to use when connecting to server\n\
			If password is not given it's asked from the tty.\n");
  printf("\n\
  -P  --port=...	Port number to use for connection\n\
  -Q, --query=...	Query to execute in each threads\n\
  -S  --socket=...	Socket file to use for connection\n");
  printf("\
  -t  --thread-count=#	Number of threads to start (default: %d) \n\
  -u, --user=#		User for login if not current user\n\
  -v, --verbose		Write some progress indicators\n\
  -V, --version		Output version information and exit\n",
	 number_of_threads);

  print_defaults("my",load_default_groups);

  printf("\nExample usage:\n\n\
%s -Q 'select * from mysql.user' -c %d -t %d\n",
	 my_progname, number_of_tests, number_of_threads);
}


static void get_options(int argc, char **argv)
{
  int c,option_index=0,error=0;
  bool tty_password=0;
  load_defaults("my",load_default_groups,&argc,&argv);

  while ((c=getopt_long(argc,argv, "c:D:h:p::VQ:P:S:t:?I",
			long_options, &option_index)) != EOF)
  {
    switch (c) {
    case 'c':
      number_of_tests=atoi(optarg);
      break;
    case 'D':
      my_free(database,MYF(MY_ALLOW_ZERO_PTR));      
      database=my_strdup(optarg,MYF(MY_WME));
      break;
    case 'h':
      host = optarg;
      break;
    case 'Q':					/* Allow old 'q' option */
      query= optarg;
      break;
    case 'p':
      if (optarg)
      {
	my_free(password,MYF(MY_ALLOW_ZERO_PTR));
	password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
      }
      else
	tty_password=1;
      break;
    case 'u':
      my_free(user,MYF(MY_ALLOW_ZERO_PTR));
      user= my_strdup(optarg,MYF(0));
      break;
    case 'P':
      tcp_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      my_free(unix_socket,MYF(MY_ALLOW_ZERO_PTR));
      unix_socket= my_strdup(optarg,MYF(0));
      break;
    case 't':
      number_of_threads=atoi(optarg);
      break;
    case 'v':
      verbose=1;
      break;
    case 'V':
      version=1;
      usage();
      exit(0);
      break;
    default:
      fprintf(stderr,"Illegal option character '%c'\n",opterr);
      /* Fall through */
    case '?':
    case 'I':					/* Info */
      error++;
      break;
    }
  }
  if (error || argc != optind)
  {
    usage();
    exit(1);
  }
  free_defaults(argv);
  if (tty_password)
    password=get_tty_password(NullS);
  return;
}


int main(int argc, char **argv)
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int i,error;
  MY_INIT(argv[0]);
  get_options(argc,argv);

  if ((error=pthread_cond_init(&COND_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  pthread_mutex_init(&LOCK_thread_count,NULL);

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

#endif /* THREAD */
