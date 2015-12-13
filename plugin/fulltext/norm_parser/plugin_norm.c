/* Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_config.h"
#include <stdlib.h>
#include <ctype.h>
#include <mysql/plugin_ftparser.h>
#include <m_ctype.h>
#include "unicode/unorm2.h"
#include "unicode/ustring.h"

static int norm_parser_plugin_init(void *arg __attribute__((unused)))
{
  return(0);
}

static int norm_parser_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}

static int norm_parser_init(MYSQL_FTPARSER_PARAM *param
                              __attribute__((unused)))
{
  return(0);
}

static int norm_parser_deinit(MYSQL_FTPARSER_PARAM *param
                                __attribute__((unused)))
{
  return(0);
}

static void add_word(MYSQL_FTPARSER_PARAM *param, char *word, size_t len)
{
  const UNormalizer2 *icu_unorm;
  UChar icu_src[1000];
  UChar icu_dst[1000];
  char norm_word[1000];
  UErrorCode err = U_ZERO_ERROR;
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info=
    { FT_TOKEN_WORD, 0, 0, 0, 0, (word - param->doc), ' ', 0 };

  u_strFromUTF8(icu_src, 1000, NULL, word, (int32_t)len, &err);
  if (U_FAILURE(err)) {
    fprintf(stderr, "u_strFromUTF8() Failed! err:%s\n", u_errorName(err));
  }
  
  icu_unorm = unorm2_getNFCInstance(&err);
  if (U_FAILURE(err)) {
    fprintf(stderr, "unorm2_getNFCInstance() Failed! err:%s\n", u_errorName(err));
  } 

  unorm2_normalize(icu_unorm,icu_src,len,icu_dst,1000,&err);
  if (U_FAILURE(err)) {
    fprintf(stderr, "unorm2_normalize() Failed! err:%s\n", u_errorName(err));
  }
 
  u_strToUTF8(norm_word, 1000, NULL, icu_dst, len, &err);
  if (U_FAILURE(err)) {
    fprintf(stderr, "u_strToUTF8() Failed! err:%s\n", u_errorName(err));
  }

  if (norm_word != NULL) {
    param->mysql_add_word(param, norm_word, len, &bool_info);
  }
}

static int norm_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *end, *start, *docend= param->doc + param->length;

  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end > start)
        add_word(param, start, end - start);
      break;
    }
    else if (my_isspace(param->cs, *end))
    {
      if (end > start)
        add_word(param, start, end - start);
      start= end + 1;
    }
  }
  return(0);
}

static struct st_mysql_ftparser norm_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION,
  norm_parser_parse,
  norm_parser_init,
  norm_parser_deinit
};

mysql_declare_plugin(ftnorm)
{
  MYSQL_FTPARSER_PLUGIN,
  &norm_parser_descriptor,
  "norm_parser",
  "Daniël van Eeden",
  "Normalized Unicode Parser",
  PLUGIN_LICENSE_GPL,
  norm_parser_plugin_init,
  norm_parser_plugin_deinit,
  0x0001,
  NULL,
  NULL,
  NULL,
  0,
}
mysql_declare_plugin_end;
