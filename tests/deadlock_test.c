/* Copyright (C) 2000-2001, 2003-2004, 2006 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <mysql.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char uchar;
static void die(char* fmt, ...);
static void safe_query(MYSQL* mysql, char* query, int read_ok);
static void run_query_batch(int* order, int num_queries);
static void permute(int *order, int num_queries);
static void permute_aux(int *order, int num_queries, int* fixed);
static void dump_result(MYSQL* mysql, char* query);

int count = 0;


struct query
{
  MYSQL* mysql;
  char* query;
  int read_ok;
  int pri;
  int dump_result;
};

MYSQL lock, sel, del_ins;

struct query queries[] =
{
  {&del_ins, "insert delayed into foo values(1)", 1, 0, 0},
  {&del_ins, "insert delayed into foo values(1)", 1, 0, 0},
  {&lock, "lock tables foo write", 1, 1, 0},
  {&lock, "unlock tables", 1,2, 0},
  {&sel, "select * from foo", 0,0, 0},
  {&del_ins, "insert  into foo values(4)", 0,3, 0},
  {0,0,0}
};

static void die(char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

static void permute(int *order, int num_queries)
{
  int *fixed;
  if(num_queries < 2) return;
  if(!(fixed = (int*)malloc(num_queries * sizeof(int))))
    die("malloc() failed");

  memset(fixed, 0, num_queries * sizeof(int));
  permute_aux(order, num_queries, fixed);

  free(fixed);
}

static order_ok(int *order, int num_queries)
{
  int i,j, pri_i, pri_j;
  for(i = 0; i < num_queries; i++)
    {
      if((pri_i = queries[order[i]].pri))
	for(j = i + 1; j < num_queries; j++)
	  {
	    pri_j = queries[order[j]].pri;
	    if(pri_j && pri_i > pri_j)
	      return 0;
	  }
    }

  return 1;
}

static void permute_aux(int *order, int num_queries, int* fixed)
{
  int *p,*p1,j,i,tmp, num_free = 0;
  p = fixed;
  for(i = 0; i < num_queries; i++, p++)
    {
      if(!*p)
	{
	  num_free++;
	  *p = 1;
	  for(j = 0, p1 = fixed ;
	      j < num_queries; j++,p1++)
	    {
	      if(!*p1)
		{
		  tmp = order[i];
		  order[i] = order[j];
		  order[j] = tmp;
		  *p1 = 1;
		  permute_aux(order, num_queries, fixed);
		  tmp = order[i];
		  order[i] = order[j];
		  order[j] = tmp;
		  *p1 = 0;
		}
	    }
	  *p = 0;
	}
    }

  /*printf("num_free = %d\n", num_free); */

  if(num_free <= 1)
    {
      count++;
      if(order_ok(order, num_queries))
        run_query_batch(order, num_queries);
    }
}

static void run_query_batch(int* order, int num_queries)
{
  int i;
  struct query* q;
  int *save_order;
  safe_query(&lock, "delete from foo", 1);
  save_order = order;
  for(i = 0; i < num_queries; i++,order++)
    {
      q = queries + *order;
      printf("query='%s'\n", q->query);
      safe_query(q->mysql, q->query, q->read_ok);
    }
  order = save_order;
  for(i = 0; i < num_queries; i++,order++)
    {
      q = queries + *order;
      if(q->dump_result)
       dump_result(q->mysql, q->query);
    }
  printf("\n");

}

static void safe_net_read(NET* net, char* query)
{
  int len;
  len = my_net_read(net); 
  if(len == packet_error || !len)
    die("Error running query '%s'", query);
  if(net->read_pos[0] == 255)
    die("Error running query '%s'", query);
}


static void safe_query(MYSQL* mysql, char* query, int read_ok)
{
  int len;
  NET* net = &mysql->net;
  net_clear(net);
  if(net_write_command(net,(uchar)COM_QUERY, query,strlen(query)))
    die("Error running query '%s': %s", query, mysql_error(mysql));
  if(read_ok)
    {
      safe_net_read(net, query);
    }
}

static void dump_result(MYSQL* mysql, char* query)
{
  MYSQL_RES* res;
  safe_net_read(&mysql->net, query);
  res = mysql_store_result(mysql);
  if(res)
   mysql_free_result(res);
}

static int* init_order(int* num_queries)
{
  struct query* q;
  int *order, *order_end, *p;
  int n,i;

  for(q = queries; q->mysql; q++)
    ;

  n = q - queries;
  if(!(order = (int*) malloc(n * sizeof(int))))
    die("malloc() failed");
  order_end = order + n;
  for(p = order,i = 0; p < order_end; p++,i++)
    *p = i;
  *num_queries = n;
  return order;
}

int main()
{
  char* user = "root", *pass = "", *host = "localhost", *db = "test";
  int *order, num_queries;
  order = init_order(&num_queries);
  if(!mysql_init(&lock) || !mysql_init(&sel) || !mysql_init(&del_ins))
    die("error in mysql_init()");

  mysql_options(&lock, MYSQL_READ_DEFAULT_GROUP, "mysql");
  mysql_options(&sel, MYSQL_READ_DEFAULT_GROUP, "mysql");
  mysql_options(&del_ins, MYSQL_READ_DEFAULT_GROUP, "mysql");

  if(!mysql_real_connect(&lock, host, user, pass, db, 0,0,0 ) ||
     !mysql_real_connect(&sel, host, user, pass, db, 0,0,0 ) ||
     !mysql_real_connect(&del_ins, host, user, pass, db, 0,0,0 ))
    die("Error in mysql_real_connect(): %s", mysql_error(&lock));
  lock.reconnect= sel.reconnect= del_ins.reconnect= 1;

  permute(order, num_queries);
  printf("count = %d\n", count);

  mysql_close(&lock);
  mysql_close(&sel);
  mysql_close(&del_ins);
  free(order);
}
