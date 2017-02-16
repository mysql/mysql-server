/* Copyright (c) 2016, MariaDB

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

//#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <tap.h>

DYNAMIC_STRING str1;

static void check(const char *res)
{
  ok(strcmp(str1.str, res) == 0, "strcmp: %s", str1.str);
  str1.length= 0;
}

int main(void)
{
  plan(23);

  IF_WIN(skip_all("Test of POSIX shell escaping rules, not for CMD.EXE\n"), );

  ok(init_dynamic_string(&str1, NULL, 0, 32) == 0, "init");

  ok(dynstr_append_os_quoted(&str1, "test1", NULL) == 0, "append");
  check("'test1'");

  ok(dynstr_append_os_quoted(&str1, "con", "cat", NULL) == 0, "append");
  check("'concat'");

  ok(dynstr_append_os_quoted(&str1, "", NULL) == 0, "append");
  check("''");

  ok(dynstr_append_os_quoted(&str1, "space inside", NULL) == 0, "append");
  check("'space inside'");

  ok(dynstr_append_os_quoted(&str1, "single'quote", NULL) == 0, "append");
  check("'single'\"'\"'quote'");

  ok(dynstr_append_os_quoted(&str1, "many'single'quotes", NULL) == 0, "append");
  check("'many'\"'\"'single'\"'\"'quotes'");

  ok(dynstr_append_os_quoted(&str1, "'single quoted'", NULL) == 0, "append");
  check("''\"'\"'single quoted'\"'\"''");

  ok(dynstr_append_os_quoted(&str1, "double\"quote", NULL) == 0, "append");
  check("'double\"quote'");

  ok(dynstr_append_os_quoted(&str1, "mixed\"single'and\"double'quotes", NULL) == 0, "append");
  check("'mixed\"single'\"'\"'and\"double'\"'\"'quotes'");

  ok(dynstr_append_os_quoted(&str1, "back\\space", NULL) == 0, "append");
  check("'back\\space'");

  ok(dynstr_append_os_quoted(&str1, "backspace\\'and\\\"quote", NULL) == 0, "append");
  check("'backspace\\'\"'\"'and\\\"quote'");

  dynstr_free(&str1);

  return exit_status();
}

