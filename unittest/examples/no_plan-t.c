/* Copyright (C) 2006 MySQL AB

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

#include <my_global.h>

#include <stdlib.h>
#include <tap.h>

/*
  Sometimes, the number of tests is not known beforehand. In those
  cases, the plan can be omitted and will instead be written at the
  end of the test (inside exit_status()).

  Use this sparingly, it is a last resort: planning how many tests you
  are going to run will help you catch that offending case when some
  tests are skipped for an unknown reason.
*/
int main() {
  ok(1, " ");
  ok(1, " ");
  ok(1, " ");
  return exit_status();
}
