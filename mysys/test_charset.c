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
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql_version.h>

#include <stdio.h>

extern void _print_csinfo(CHARSET_INFO *cs);

int main(int argc, char **argv) {
  const char *the_set = MYSQL_CHARSET;
  char *cs_list;
  int argcnt = 1;

  my_init();

  if (argc > argcnt && argv[argcnt][0] == '-' && argv[argcnt][1] == '#')
    DBUG_PUSH(argv[argcnt++]+2);

  if (argc > argcnt)
    the_set = argv[argcnt++];

  if (argc > argcnt)
    charsets_dir = argv[argcnt++];

  if (set_default_charset_by_name(the_set, MYF(MY_WME)))
    return 1;

  puts("CHARSET INFO:");
  _print_csinfo(default_charset_info);
  fflush(stdout);

  cs_list = list_charsets(MYF(MY_COMPILED_SETS | MY_CONFIG_SETS));
  printf("LIST OF CHARSETS (compiled + *.conf):\n%s\n", cs_list);
  free(cs_list);

  cs_list = list_charsets(MYF(MY_INDEX_SETS | MY_LOADED_SETS));
  printf("LIST OF CHARSETS (index + loaded):\n%s\n", cs_list);
  free(cs_list);

  return 0;
}
