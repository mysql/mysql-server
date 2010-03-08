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

int has_feature() {
  return 0;
}

/*
  In some cases, an entire test file does not make sense because there
  some feature is missing.  In that case, the entire test case can be
  skipped in the following manner.
 */
int main() {
  if (!has_feature())
    skip_all("Example of skipping an entire test");
  plan(4);
  ok(1, NULL);
  ok(1, NULL);
  ok(1, NULL);
  ok(1, NULL);
  return exit_status();
}
