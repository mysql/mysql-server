/* Copyright (C) 2015 MariaDB

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

#include <tap.h>
#include <my_getopt.h>
#include <stdarg.h>

ulonglong opt_ull;
ulong opt_ul;
int argc, res;
char **argv, *args[100];

struct my_option my_long_options[]=
{
  {"ull", 0, "ull", &opt_ull, &opt_ull,
   0, GET_ULL, REQUIRED_ARG, 1, 0, ~0ULL, 0, 0, 0},
  {"ul", 0, "ul", &opt_ul, &opt_ul,
   0, GET_ULONG, REQUIRED_ARG, 1, 0, 0xFFFFFFFF, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

void run(const char *arg, ...)
{
  va_list ap;
  va_start(ap, arg);
  argv= args;
  *argv++= (char*)"<skipped>";
  while (arg)
  {
    *argv++= (char*)arg;
    arg= va_arg(ap, char*);
  }
  va_end(ap);
  argc= argv - args;
  argv= args;
  res= handle_options(&argc, &argv, my_long_options, 0);
}

int main() {
  plan(3);

  run("--ull=100", NULL);
  ok(res==0 && argc==0 && opt_ull==100,
     "res:%d, argc:%d, opt_ull:%llu", res, argc, opt_ull);

  /*
    negative numbers are wrapped. this is kinda questionable,
    we might want to fix it eventually. but it'd be a change in behavior,
    users might've got used to "-1" meaning "max possible value"
  */
  run("--ull=-100", NULL);
  ok(res==0 && argc==0 && opt_ull==18446744073709551516ULL,
     "res:%d, argc:%d, opt_ull:%llu", res, argc, opt_ull);
  run("--ul=-100", NULL);
  ok(res==0 && argc==0 && opt_ul==4294967295UL,
     "res:%d, argc:%d, opt_ul:%lu", res, argc, opt_ul);
  return exit_status();
}

