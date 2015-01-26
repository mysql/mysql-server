/*  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
    02110-1301  USA */

#include <ctype.h>
#include <string.h>

#include <mysql/plugin.h>
#include <mysql/plugin_query_rewrite.h>

static MYSQL_PLUGIN plugin_info_ptr;

static int rewrite_plugin_init(MYSQL_PLUGIN plugin_ref)
{
  plugin_info_ptr= plugin_ref;
  return 0;
}

static int rewrite_lower(Mysql_rewrite_pre_parse_param *param)
{
  size_t query_length= param->query_length;
  char *rewritten_query= new char[query_length + 1];

  for (unsigned i= 0; i < param->query_length + 1; i++)
    rewritten_query[i]= tolower(param->query[i]);

  param->rewritten_query= rewritten_query;
  param->rewritten_query_length= param->query_length;
  param->flags|= FLAG_REWRITE_PLUGIN_QUERY_REWRITTEN;
  return 0;
}

static int free_rewritten_query(Mysql_rewrite_pre_parse_param *param)
{
  delete [] param->rewritten_query;
  param->rewritten_query= NULL;
  param->rewritten_query_length= 0;
  return 0;
}

static st_mysql_rewrite_pre_parse rewrite_example_descriptor= {
  MYSQL_REWRITE_PRE_PARSE_INTERFACE_VERSION,    /* interface version          */
  rewrite_lower,                                /* rewrite raw query function */
  free_rewritten_query,                         /* free allocated query       */
};

mysql_declare_plugin(rewrite_example)
{
  MYSQL_REWRITE_PRE_PARSE_PLUGIN,
  &rewrite_example_descriptor,
  "rewrite_example",
  "Padraig O'Sullivan",
  "An example of a query rewrite plugin that rewrites all queries to lower "
  "case",
  PLUGIN_LICENSE_GPL,
  rewrite_plugin_init,
  NULL,
  0x0001,                                       /* version 0.0.1      */
  NULL,                                         /* status variables   */
  NULL,                                         /* system variables   */
  NULL,                                         /* config options     */
  0,                                            /* flags              */
}
mysql_declare_plugin_end;
