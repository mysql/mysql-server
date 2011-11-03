/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql_version.h>

#include <stdio.h>

static void _print_array(uint8 *data, uint size)
{
  uint i;
  for (i = 0; i < size; ++i)
  {
    if (i == 0 || i % 16 == size % 16) printf("  ");
    printf(" %02x", data[i]);
    if ((i+1) % 16 == size % 16) printf("\n");
  }
}

static void _print_csinfo(CHARSET_INFO *cs)
{
  printf("%s #%d\n", cs->name, cs->number);
  printf("ctype:\n"); _print_array(cs->ctype, 257);
  printf("to_lower:\n"); _print_array(cs->to_lower, 256);
  printf("to_upper:\n"); _print_array(cs->to_upper, 256);
  printf("sort_order:\n"); _print_array(cs->sort_order, 256);
  printf("collate:    %3s (%d, %p, %p, %p)\n",
         cs->strxfrm_multiply ? "yes" : "no",
         cs->strxfrm_multiply,
         cs->strnncoll,
         cs->strnxfrm,
         cs->like_range);
  printf("multi-byte: %3s (%d, %p, %p, %p)\n",
         cs->mbmaxlen > 1 ? "yes" : "no",
         cs->mbmaxlen,
         cs->ismbchar,
         cs->ismbhead,
         cs->mbcharlen);
}


int main(int argc, char **argv) {
  const char *the_set = MYSQL_CHARSET;
  char *cs_list;
  int argcnt = 1;
  CHARSET_INFO *cs;

  my_init();

  if (argc > argcnt && argv[argcnt][0] == '-' && argv[argcnt][1] == '#')
    DBUG_PUSH(argv[argcnt++]+2);

  if (argc > argcnt)
    the_set = argv[argcnt++];

  if (argc > argcnt)
    charsets_dir = argv[argcnt++];

  if (!(cs= get_charset_by_name(the_set, MYF(MY_WME))))
    return 1;

  puts("CHARSET INFO:");
  _print_csinfo(cs);
  fflush(stdout);

  return 0;
}
