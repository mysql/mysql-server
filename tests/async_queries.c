/*
  Copyright 2011 Kristian Nielsen and Monty Program Ab.

  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
  Run a set of queries in parallel against a server using the non-blocking
  API, and compare to running same queries with the normal blocking API.
*/

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>

#include <event.h>


#define SL(s) (s), sizeof(s)
static const char *my_groups[]= { "client", NULL };

/* Maintaining a list of queries to run. */
struct query_entry {
  struct query_entry *next;
  char *query;
  int index;
};
static struct query_entry *query_list;
static struct query_entry **tail_ptr= &query_list;
static int query_counter= 0;


/* State kept for each connection. */
struct state_data {
  int ST;                                    /* State machine current state */
  struct event ev_mysql;
  MYSQL mysql;
  MYSQL_RES *result;
  MYSQL *ret;
  int err;
  MYSQL_ROW row;
  struct query_entry *query_element;
  int index;
};


static const char *opt_db= NULL;
static const char *opt_user= NULL;
static const char *opt_password= NULL;
static int tty_password= 0;
static const char *opt_host= NULL;
static const char *opt_socket= NULL;
static unsigned int opt_port= 0;
static unsigned int opt_connections= 5;
static const char *opt_query_file= NULL;

static struct my_option options[] =
{
  {"database", 'D', "Database to use", &opt_db, &opt_db,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", &opt_host, &opt_host,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.",
   &opt_port, &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection",
   &opt_socket, &opt_socket, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user", &opt_user,
   &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"connections", 'n', "Number of simultaneous connections/queries.",
   &opt_connections, &opt_connections, 0, GET_UINT, REQUIRED_ARG,
   5, 0, 0, 0, 0, 0},
  {"queryfile", 'q', "Name of file containing extra queries to run",
   &opt_query_file, &opt_query_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void
fatal(struct state_data *sd, const char *msg)
{
  fprintf(stderr, "%s: %s\n", msg, (sd ? mysql_error(&sd->mysql) : ""));
  exit(1);
}


static void state_machine_handler(int fd, short event, void *arg);

static void
next_event(int new_st, int status, struct state_data *sd)
{
  short wait_event= 0;
  struct timeval tv, *ptv;
  int fd;

  if (status & MYSQL_WAIT_READ)
    wait_event|= EV_READ;
  if (status & MYSQL_WAIT_WRITE)
    wait_event|= EV_WRITE;
  if (wait_event)
    fd= mysql_get_socket(&sd->mysql);
  else
    fd= -1;
  if (status & MYSQL_WAIT_TIMEOUT)
  {
    tv.tv_sec= mysql_get_timeout_value(&sd->mysql);
    tv.tv_usec= 0;
    ptv= &tv;
  }
  else
    ptv= NULL;
  event_set(&sd->ev_mysql, fd, wait_event, state_machine_handler, sd);
  event_add(&sd->ev_mysql, ptv);
  sd->ST= new_st;
}

static int
mysql_status(short event)
{
  int status= 0;
  if (event & EV_READ)
    status|= MYSQL_WAIT_READ;
  if (event & EV_WRITE)
    status|= MYSQL_WAIT_WRITE;
  if (event & EV_TIMEOUT)
    status|= MYSQL_WAIT_TIMEOUT;
  return status;
}


static int num_active_connections;

/* Shortcut for going to new state immediately without waiting. */
#define NEXT_IMMEDIATE(sd_, new_st) do { sd_->ST= new_st; goto again; } while (0)

static void
state_machine_handler(int fd __attribute__((unused)), short event, void *arg)
{
  struct state_data *sd= arg;
  int status;

again:
  switch(sd->ST)
  {
  case 0:
    /* Initial state, start making the connection. */
    status= mysql_real_connect_start(&sd->ret, &sd->mysql, opt_host, opt_user, opt_password, opt_db, opt_port, opt_socket, 0);
    if (status)
      /* Wait for connect to complete. */
      next_event(1, status, sd);
    else
      NEXT_IMMEDIATE(sd, 9);
    break;

  case 1:
    status= mysql_real_connect_cont(&sd->ret, &sd->mysql, mysql_status(event));
    if (status)
      next_event(1, status, sd);
    else
      NEXT_IMMEDIATE(sd, 9);
    break;

  case 9:
    if (!sd->ret)
      fatal(sd, "Failed to mysql_real_connect()");
    NEXT_IMMEDIATE(sd, 10);
    break;

  case 10:
    /* Now run the next query. */
    sd->query_element= query_list;
    if (!sd->query_element)
    {
      /* No more queries, end the connection. */
      NEXT_IMMEDIATE(sd, 40);
    }
    query_list= query_list->next;

    sd->index= sd->query_element->index;
    printf("%d ! %s\n", sd->index, sd->query_element->query);
    status= mysql_real_query_start(&sd->err, &sd->mysql, sd->query_element->query,
                                   strlen(sd->query_element->query));
    if (status)
      next_event(11, status, sd);
    else
      NEXT_IMMEDIATE(sd, 20);
    break;

  case 11:
    status= mysql_real_query_cont(&sd->err, &sd->mysql, mysql_status(event));
    if (status)
      next_event(11, status, sd);
    else
      NEXT_IMMEDIATE(sd, 20);
    break;

  case 20:
    my_free(sd->query_element->query);
    my_free(sd->query_element);
    if (sd->err)
    {
      printf("%d | Error: %s\n", sd->index, mysql_error(&sd->mysql));
      NEXT_IMMEDIATE(sd, 10);
    }
    else
    {
      sd->result= mysql_use_result(&sd->mysql);
      if (!sd->result)
        fatal(sd, "mysql_use_result() returns error");
      NEXT_IMMEDIATE(sd, 30);
    }
    break;

  case 30:
    status= mysql_fetch_row_start(&sd->row, sd->result);
    if (status)
      next_event(31, status, sd);
    else
      NEXT_IMMEDIATE(sd, 39);
    break;

  case 31:
    status= mysql_fetch_row_cont(&sd->row, sd->result, mysql_status(event));
    if (status)
      next_event(31, status, sd);
    else
      NEXT_IMMEDIATE(sd, 39);
    break;

  case 39:
    if (sd->row)
    {
      /* Got a row. */
      unsigned int i;
      printf("%d - ", sd->index);
      for (i= 0; i < mysql_num_fields(sd->result); i++)
        printf("%s%s", (i ? "\t" : ""), (sd->row[i] ? sd->row[i] : "(null)"));
      printf ("\n");
      NEXT_IMMEDIATE(sd, 30);
    }
    else
    {
      if (mysql_errno(&sd->mysql))
      {
        /* An error occured. */
        printf("%d | Error: %s\n", sd->index, mysql_error(&sd->mysql));
      }
      else
      {
        /* EOF. */
        printf("%d | EOF\n", sd->index);
      }
      mysql_free_result(sd->result);
      NEXT_IMMEDIATE(sd, 10);
    }
    break;

  case 40:
    status= mysql_close_start(&sd->mysql);
    if (status)
      next_event(41, status, sd);
    else
      NEXT_IMMEDIATE(sd, 50);
    break;

  case 41:
    status= mysql_close_cont(&sd->mysql, mysql_status(event));
    if (status)
      next_event(41, status, sd);
    else
      NEXT_IMMEDIATE(sd, 50);
    break;

  case 50:
    /* We are done! */
    num_active_connections--;
    if (num_active_connections == 0)
      event_loopbreak();
    break;

  default:
    abort();
  }
}


void
add_query(const char *q)
{
  struct query_entry *e;
  char *q2;
  size_t len;

  e= my_malloc(sizeof(*e), MYF(0));
  q2= my_strdup(q, MYF(0));
  if (!e || !q2)
    fatal(NULL, "Out of memory");

  /* Remove any trailing newline. */
  len= strlen(q2);
  if (q2[len] == '\n')
    q2[len--]= '\0';
  if (q2[len] == '\r')
    q2[len--]= '\0';

  e->next= NULL;
  e->query= q2;
  e->index= query_counter++;
  *tail_ptr= e;
  tail_ptr= &e->next;
}


static my_bool
handle_option(int optid, const struct my_option *opt __attribute__((unused)),
              char *arg)
{
  switch (optid)
  {
  case '?':
    printf("Usage: async_queries [OPTIONS] query ...\n");
    my_print_help(options);
    my_print_variables(options);
    exit(0);
    break;

  case 'p':
    if (arg)
      opt_password= arg;
    else
      tty_password= 1;
    break;
  }

  return 0;
}


int
main(int argc, char *argv[])
{
  struct state_data *sds;
  unsigned int i;
  int err;
  struct event_base *libevent_base;

  err= handle_options(&argc, &argv, options, handle_option);
  if (err)
    exit(err);
  if (tty_password)
    opt_password= get_tty_password(NullS);

  if (opt_query_file)
  {
    FILE *f= fopen(opt_query_file, "r");
    char buf[65536];
    if (!f)
      fatal(NULL, "Cannot open query file");
    while (!feof(f))
    {
      if (!fgets(buf, sizeof(buf), f))
        break;
      add_query(buf);
    }
    fclose(f);
  }
  /* Add extra queries directly on command line. */
  while (argc > 0)
  {
    --argc;
    add_query(*argv++);
  }

  sds= my_malloc(opt_connections * sizeof(*sds), MYF(0));
  if (!sds)
    fatal(NULL, "Out of memory");

  libevent_base= event_init();

  err= mysql_library_init(argc, argv, (char **)my_groups);
  if (err)
  {
    fprintf(stderr, "Fatal: mysql_library_init() returns error: %d\n", err);
    exit(1);
  }

  num_active_connections= 0;
  for (i= 0; i < opt_connections; i++)
  {
    mysql_init(&sds[i].mysql);
    mysql_options(&sds[i].mysql, MYSQL_OPT_NONBLOCK, 0);
    mysql_options(&sds[i].mysql, MYSQL_READ_DEFAULT_GROUP, "async_queries");

    /*
      We put the initial connect call in the first state 0 of the state machine
      and run that manually, just to have everything in one place.
    */
    sds[i].ST= 0;
    num_active_connections++;
    state_machine_handler(-1, -1, &sds[i]);
  }

  event_dispatch();

  free(sds);

  mysql_library_end();

  event_base_free(libevent_base);

  return 0;
}
