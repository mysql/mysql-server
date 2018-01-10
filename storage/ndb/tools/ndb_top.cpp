/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define _XOPEN_SOURCE_EXTENDED
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <ndb_config.h>
#ifdef HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#elif HAVE_NCURSESW_H
#include <ncursesw.h>
#elif HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <unistd.h>
#include <mysql.h>
#include "my_getopt.h"
#include "mysql/service_mysql_alloc.h"
#include <signal.h>
#include "../../../client/client_priv.h"
#include "welcome_copyright_notice.h" /* ORACLE_WELCOME_COPYRIGHT_NOTICE */


#define BLUE_COLOR 1
#define GREEN_COLOR 2
#define RED_COLOR 3
#define YELLOW_COLOR 4
#define BLACK_COLOR 5
#define DEFAULT_COLOR 6

struct thread_result_type
{
  unsigned int thr_no;
  char         thr_name[32];
  unsigned int OS_user;
  unsigned int OS_system;
  unsigned int OS_idle;
  unsigned int thread_exec;
  unsigned int thread_send;
  unsigned int thread_buffer_full;
  unsigned int thread_sleeping;
  unsigned int elapsed_time;
};
typedef struct thread_result_type THREAD_RESULT;

static THREAD_RESULT *thread_result = NULL;
static unsigned int ndb_threads = 0;

static MYSQL *con = NULL;
unsigned int opt_port_number = 0;
char *opt_host = (char*)"localhost";
char *opt_user = (char*)"root";
char *opt_password = 0;
bool tty_password = 0;
char *db_name = (char *)"ndbinfo";
unsigned int opt_node_id = 0;
unsigned int opt_sleep_time = 0;
bool opt_measured_load = 0;
bool opt_os_load = 0;
bool opt_color = 0;
bool opt_text = 0;
bool opt_graph = 0;
bool opt_sort = 0;
bool opt_help = 0;

static char percentage_sign = '%';

void
handle_error()
{
  printf("%s\n\r", mysql_error(con));
}

void
cleanup(bool in_screen)
{
  if (in_screen)
  {
    endwin();
  }
  if (con != NULL)
  {
    mysql_close(con);
  }
  if (thread_result)
  {
    free(thread_result);
  }
  if (opt_password)
  {
    my_free(opt_password);
  }
}

int connect_mysql()
{
  enum mysql_protocol_type prot_type= MYSQL_PROTOCOL_TCP;
  mysql_options(con, MYSQL_OPT_PROTOCOL, (void*)&prot_type);

  MYSQL *loc = mysql_real_connect(con,
                                  opt_host,
                                  opt_user,
                                  opt_password,
                                  db_name,
                                  opt_port_number,
                                  NULL,
                                  0);
  return loc == NULL ? 1 : 0;
}

int
query_mysql()
{
  char buf[512];
  char *query_str = &buf[0];
  snprintf(
      buf,
      sizeof(buf),
      "SELECT cs.thr_no, ts.thread_name, cs.OS_user, cs.OS_system, cs.OS_idle,"
      " cs.thread_exec, cs.thread_send, cs.thread_buffer_full, cs.thread_sleeping,"
      " cs.elapsed_time FROM cpustat as cs, threads as ts WHERE"
      " cs.node_id = %u AND"
      " cs.thr_no = ts.thr_no AND"
      " cs.node_id = ts.node_id",
      opt_node_id);

  int res;
  if ((res = mysql_query(con, query_str)))
    return res;

  MYSQL_RES *result = mysql_store_result(con);
  if (result == NULL)
    return 2;

  int num_fields = mysql_num_fields(result);
  if (num_fields != 10)
  {
    return 3;
  }

  my_ulonglong num_rows = mysql_num_rows(result);

  if (thread_result != NULL)
  {
    free(thread_result);
    thread_result = NULL;
  }
  if (num_rows == 0)
  {
    return 4;
  }
  THREAD_RESULT *tr_array =
    (THREAD_RESULT*)malloc(sizeof(THREAD_RESULT) * num_rows);
  assert(tr_array != NULL);
  thread_result = tr_array;

  ndb_threads = 0;
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result)))
  {
    THREAD_RESULT *tr = &thread_result[ndb_threads];
    unsigned long *lengths;
    lengths = mysql_fetch_lengths(result);
    assert(row[0]);
    sscanf(row[0], "%u", &tr->thr_no);
    assert(row[1]);
    sscanf(row[1], "%31s", tr->thr_name);
    assert(row[2]);
    sscanf(row[2], "%u", &tr->OS_user);
    assert(row[3]);
    sscanf(row[3], "%u", &tr->OS_system);
    assert(row[4]);
    sscanf(row[4], "%u", &tr->OS_idle);
    assert(row[5]);
    sscanf(row[5], "%u", &tr->thread_exec);
    assert(row[6]);
    sscanf(row[6], "%u", &tr->thread_send);
    assert(row[7]);
    sscanf(row[7], "%u", &tr->thread_buffer_full);
    assert(row[8]);
    sscanf(row[8], "%u", &tr->thread_sleeping);
    assert(row[9]);
    sscanf(row[9], "%u", &tr->elapsed_time);
    ndb_threads++;
  }
  mysql_free_result(result);
  assert((my_ulonglong)ndb_threads == num_rows);
  return 0;
}

static char* tombs(const wchar_t* wc, const char* c)
{
  char* p;
  size_t n = wcstombs(NULL, wc, 0);
  if (n == (size_t)-1) {
    p = strdup(c);
  }
  else {
    p = (char*)malloc(n+1);
    wcstombs(p, wc, n+1);
  }
  return p;
}

int print_black_block()
{
  static char *mbs=tombs(L"\u2588", "#");
  return addstr(mbs);
}

int print_dark_shade()
{
  static char *mbs=tombs(L"\u2593", "@");
  return addstr(mbs);
}

int print_medium_shade()
{
  static char *mbs=tombs(L"\u2592", "X");
  return addstr(mbs);
}

int print_light_shade()
{
  static char *mbs=tombs(L"\u2591", "o");
  return addstr(mbs);
}

int print_space()
{
  return addstr(" ");
}

static volatile sig_atomic_t g_resize_window = 0;
void resize_window(int dummy)
{
  g_resize_window = 1;
}

static struct my_option
my_long_options[] =
{
  {"host", 'h',
   "Hostname of MySQL Server",
   (uchar**) &opt_host, (uchar**) &opt_host, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 't',
   "Port of MySQL Server",
   &opt_port_number, &opt_port_number, 0, GET_UINT,
   OPT_ARG, 3306, 0, 0, 0, 0, 0},
  {"user", 'u',
   "Username to log into MySQL Server",
   (uchar**) &opt_user, (uchar**) &opt_user, 0, GET_STR,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to log into MySQL Server (default is NULL)",
   0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"node_id", 'n',
   "Node id of data node to watch",
   &opt_node_id, &opt_node_id, 0, GET_UINT,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"sleep_time", 's',
   "Sleep time between each refresh of statistics",
   &opt_sleep_time, &opt_sleep_time, 0, GET_UINT,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"measured_load", 'm',
   "Show measured load by thread",
   &opt_measured_load, &opt_measured_load, 0, GET_BOOL,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"os_load", 'o',
   "Show load measured by OS",
   &opt_os_load, &opt_os_load, 0, GET_BOOL,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"color", 'c',
   "Use color in ASCII graphs",
   &opt_color, &opt_color, 0, GET_BOOL,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"text", 'x',
   "Use text to represent data",
   &opt_text, &opt_text, 0, GET_BOOL,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"graph", 'g',
   "Use ASCII graphs to represent data",
   &opt_graph, &opt_graph, 0, GET_BOOL,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"sort", 'r',
   "Sort threads after highest measured usage",
   &opt_sort, &opt_sort, 0, GET_BOOL,
   OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"help", '?',
   "Print usage",
   &opt_help, &opt_help, 0, GET_BOOL,
   OPT_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}

};

static void short_usage_sub()
{
  printf("Usage: %s [OPTIONS]\n", my_progname);
}

#define NDB_TOP_VERSION "1.0"
static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",
         my_progname,
         NDB_TOP_VERSION,
         MYSQL_SERVER_VERSION,
         SYSTEM_TYPE,MACHINE_TYPE);
} /* print_version */

static void usage(void)
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2017"));
  puts("ndb_top");
  puts("");
  puts("ndb_top uses the ndbinfo table cpustat to view CPU stats of NDB threads");
  puts("");
  puts("Each thread can be represented by two rows, the first one shows the OS stats,");
  puts("the second row shows the measured stats in the thread (also affected by");
  puts("the OS descheduling the thread.");
  puts("");
  puts("The graph display shows OS user time as filled blue boxes, OS system time as");
  puts("shady green boxes and idle time as space, for measured load we use filled");
  puts("blue boxes for execution time, yellow shady boxes for send time and red");
  puts("filled boxes for time spent in send buffer full waits and space for idle.");
  puts("");
  puts("The percentage shown in graph display is the sum of all non-idle percentages.");
  puts("The text display shows the same information as the graph display but in text");
  puts("representation. It is possible to use text and graph at the same time.");
  puts("");
  puts("The sorted view is based on the maximum of the measured load and the load");
  puts("reported by the OS.");
  puts("");
  puts("The view will adjust itself to the height and width of the terminal window.");
  puts("The minimum width required is 76 characters wide.");
  puts("");
  puts("By default it shows the CPU usage in node 1.");
  puts("Quit program by using Ctrl-C.");
  puts("");
  short_usage_sub();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */

static bool
get_one_option(int optid,
               const struct my_option *opt MY_ATTRIBUTE((unused)),
               char *argument)
{
  switch (optid) {
  case 'p':
  {
    if (argument == disabled_my_option)
    {
      argument = (char*)"";
    }
    if (argument)
    {
      char *start=argument;
      my_free(opt_password);
      opt_password=my_strdup(PSI_NOT_INSTRUMENTED,
                             argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';       /* Destroy argument */
      if (*start)
        start[1]=0;                             /* Cut length of argument */
      tty_password= 0;
    }
    else
      tty_password=1;
    break;
  }
  case '?':
  {
    usage();
    exit(0);
    break;
  }
  case 'V':
  {
    print_version();
    exit(0);
  }
  case 't':
  case 'n':
  case 'u':
  case 'h':
  case 's':
  case 'm':
  case 'o':
  case 'c':
  case 'x':
  case 'g':
  case 'r':
  {
    break;
  }
  default:
  {
    usage();
    exit(0);
    break;
  }
  }
  return FALSE;
}

#define SORT_ORDER_ENTRIES 128
static void init_sort_order(unsigned int *sort_order,
                            THREAD_RESULT *tr,
                            unsigned int num_threads)
{
  unsigned int measured_load[SORT_ORDER_ENTRIES];
  if (!opt_sort)
  {
    for (unsigned int i = 0; i < num_threads; i++)
    {
      sort_order[i] = i;
    }
    return;
  }
  for (unsigned int i = 0; i < num_threads; i++)
  {
    unsigned int meas_load = tr[i].thread_exec +
                             tr[i].thread_send +
                             tr[i].thread_buffer_full;
    unsigned int os_load = tr[i].OS_user +
                           tr[i].OS_system;
    unsigned int load = meas_load;
    if (os_load > meas_load)
    {
      load = os_load;
    }
    measured_load[i] = load;
    sort_order[i] = i;
  }
  for (unsigned int i = 0; i < num_threads; i++)
  {
    unsigned int max_val = measured_load[i];
    unsigned int max_index = i;
    for (unsigned int j = i + 1; j < num_threads; j++)
    {
      if (measured_load[j] > max_val)
      {
        max_val = measured_load[j];
        max_index = j;
      }
    }
    measured_load[max_index] = measured_load[i];
    measured_load[i] = max_val;
    unsigned int save = sort_order[i];
    sort_order[i] = sort_order[max_index];
    sort_order[max_index] = save;
  }
  return;
}

int main(int argc, char **argv)
{
  int ret;
  int exit_code = 0;
  bool use_color = false;
  bool in_screen = false;
  WINDOW *win;
  unsigned int sort_order[SORT_ORDER_ENTRIES];
  MY_INIT("ndb_top");

  ret = handle_options(&argc, &argv, my_long_options, get_one_option);
  if (ret != 0)
  {
    printf("Wrong options\n");
    exit_code = 1;
    goto exit_handling;
  }
  if (tty_password)
  {
    opt_password = get_tty_password(NullS);
  }
  if (opt_os_load == 0 &&
      opt_measured_load == 0)
  {
    usage();
    printf("\n\rError message:\n\rAt least one load need to be shown\n\r");
    exit_code = 1;
    goto exit_handling;
  }
  if (opt_text == 0 &&
      opt_graph == 0)
  {
    usage();
    printf("\n\rError message:\n\rAt least one of text and graph is needed\n\r");
    exit_code = 1;
    goto exit_handling;
  }
  con = mysql_init(NULL);
  if (con == NULL)
  {
    usage();
    printf("\n\rError message:\n\rmysql_init failed\n");
    exit_code = 1;
    goto exit_handling;
  }
  if (connect_mysql() != 0)
  {
    usage();
    printf("\n\rError message:\n\r");
    printf("Connect to ndbinfo database in MySQL Server failed\n");
    handle_error();
    exit_code = 1;
    goto exit_handling;
  }

  signal(SIGWINCH, resize_window);
  setlocale(LC_ALL,"");
  win = initscr();
  use_default_colors();
  if (opt_color && has_colors())
  {
    int ret_code = start_color();
    if (ret_code != ERR)
    {
      use_color = true;
      init_pair(BLUE_COLOR, COLOR_BLUE, -1);
      init_pair(GREEN_COLOR, COLOR_GREEN, -1);
      init_pair(RED_COLOR, COLOR_RED, -1);
      init_pair(YELLOW_COLOR, COLOR_YELLOW, -1);
      init_pair(BLACK_COLOR, COLOR_BLACK, -1);
      init_pair(DEFAULT_COLOR, -1, -1);
    }
  }
  while (1)
  {
    int res;
    if ((res = query_mysql() != 0))
    {
      refresh();
      endwin();
      usage();
      switch (res)
      {
      case 1:
        printf("\n\rFailed in mysql_query, empty result set, check node_id\n\r");
        break;
      case 2:
        printf("\n\rFailed in mysql_store_results:\n\r");
        break;
      case 3:
        printf("\n\rFailed in mysql_num_fields:\n\r");
        break;
      case 4:
        printf("\n\rFailed in mysql_num_rows:\n\r");
        break;
      default:
        if (res == CR_SERVER_LOST)
          printf("\n\rFailed in mysql_query: Server lost\n\r");
        else if (res == CR_SERVER_GONE_ERROR)
          printf("\n\rFailed in mysql_query: Server gone\n\r");
        else if (res == CR_UNKNOWN_ERROR)
          printf("\n\rFailed in mysql_query: MySQL unknown error\n\r");
        else if (res == CR_COMMANDS_OUT_OF_SYNC)
          printf("\n\rFailed in mysql_query: Commands out of sync\n\r");
        else
          printf("\n\rFailed in mysql_query: Error code not documented\n\r");

        break;
      }
      printf("\n\rError message:\n\rFailed to query MySQL\n\r");
      handle_error();
      exit_code = 1;
      goto exit_handling;
    }

    int width = 0;
    int height = 0;
    getmaxyx(win, height, width);

    if (width < 76)
    {
      endwin();
      printf("Width of screen is %d, smaller than 76, height is %d,"
             " no use in proceeding\n",
             width, height);
      handle_error();
      exit_code = 1;
      goto exit_handling;
    }

    in_screen = true;
    move(0,0);
    int lines_used = 0;
    unsigned int total_dots = width - 33;
    init_sort_order(&sort_order[0], &thread_result[0], ndb_threads);
    for (unsigned int k = 0; k < ndb_threads; k++)
    {
      THREAD_RESULT *tr = &thread_result[sort_order[k]];
      if (opt_os_load)
      {
        unsigned int blue_dots = (tr->OS_user * total_dots) / 100;
        unsigned int green_dots = (tr->OS_system * total_dots) / 100;
        assert(total_dots >= (blue_dots + green_dots));
        unsigned int white_dots = total_dots - (blue_dots + green_dots);
        unsigned int percentage = tr->OS_user + tr->OS_system;

        if (opt_text)
        {
          if (lines_used++ >= height)
          {
            break;
          }
          printw("%4s thr_no %2u   OS view [",
                 tr->thr_name,
                 tr->thr_no);

          unsigned int idle;
          if ((tr->OS_user + tr->OS_system) > 100)
          {
            idle = 0;
          }
          else
          {
            idle = (100 - (tr->OS_user + tr->OS_system));
          }
          printw("user: %3u%c, system: %3u%c, idle: %3u%c] %3u%c\n\r",
                 tr->OS_user,
                 percentage_sign,
                 tr->OS_system,
                 percentage_sign,
                 idle,
                 percentage_sign,
                 (tr->OS_user + tr->OS_system),
                 percentage_sign);
        }
        if (opt_graph)
        {
          if (lines_used++ >= height)
          {
            break;
          }
          printw("%4s thr_no %2u   OS view [",
                 tr->thr_name,
                 tr->thr_no);

          if (use_color)
            attron(COLOR_PAIR(BLUE_COLOR));
          for (unsigned int j = 0; j < blue_dots; j++)
            print_black_block();

          if (use_color)
            attron(COLOR_PAIR(GREEN_COLOR));
          for (unsigned int j = 0; j < green_dots; j++)
            print_medium_shade();

          if (use_color)
            attron(COLOR_PAIR(BLACK_COLOR));
          for (unsigned int j = 0; j < white_dots; j++)
            print_space();

          if (use_color)
            attron(COLOR_PAIR(DEFAULT_COLOR));
          printw("] %3u%c\n\r", percentage, percentage_sign);
        }
      }
      if (opt_measured_load)
      {

        unsigned int blue_dots = (tr->thread_exec * total_dots) / 100;
        unsigned int yellow_dots = (tr->thread_send * total_dots) / 100;
        unsigned int red_dots = (tr->thread_buffer_full * total_dots) / 100;
        assert(total_dots >= (blue_dots + yellow_dots + red_dots));
        unsigned int white_dots = total_dots - (blue_dots + yellow_dots + red_dots);
        unsigned int percentage = tr->thread_exec +
                                    tr->thread_send +
                                    tr->thread_buffer_full;
        if (opt_text)
        {
          if (lines_used++ >= height)
          {
            break;
          }
          printw("%4s thr_no %2u user view [",
                 tr->thr_name,
                 tr->thr_no);
          unsigned int idle;
          if ((tr->thread_exec + tr->thread_send + tr->thread_buffer_full) >
               100)
          {
            idle = 0;
          }
          else
          {
            idle = (100 -
              (tr->thread_exec + tr->thread_send + tr->thread_buffer_full));
          }
          printw("exec: %3u%c, send: %3u%c, full: %3u%c idle: %3u%c] %3u%c\n\r",
                 tr->thread_exec,
                 percentage_sign,
                 tr->thread_send,
                 percentage_sign,
                 tr->thread_buffer_full,
                 percentage_sign,
                 idle,
                 percentage_sign,
                 (tr->thread_exec + tr->thread_send + tr->thread_buffer_full),
                 percentage_sign);
        }
        if (opt_graph)
        {
          if (lines_used++ >= height)
          {
            break;
          }
          printw("%4s thr_no %2u user view [",
                 tr->thr_name,
                 tr->thr_no);
          if (use_color)
            attron(COLOR_PAIR(BLUE_COLOR));
          for (unsigned int j = 0; j < blue_dots; j++)
            print_black_block();

          if (use_color)
            attron(COLOR_PAIR(YELLOW_COLOR));
          for (unsigned int j = 0; j < yellow_dots; j++)
            print_dark_shade();
          if (use_color)
            attron(COLOR_PAIR(RED_COLOR));
          for (unsigned int j = 0; j < red_dots; j++)
            print_medium_shade();

          if (use_color)
            attron(COLOR_PAIR(DEFAULT_COLOR));
          for (unsigned int j = 0; j < white_dots; j++)
            print_space();
          printw("] %3u%c\n\r", percentage, percentage_sign);
        }
      }
    }
    if (g_resize_window)
    {
      endwin();
      refresh();
      g_resize_window = 0;
    }
    wrefresh(win);
    sleep(opt_sleep_time);
    if (g_resize_window)
    {
      endwin();
      refresh();
      g_resize_window = 0;
    }
    in_screen = false;
  }
  endwin();

exit_handling:
  cleanup(in_screen);
  exit(exit_code);
}
