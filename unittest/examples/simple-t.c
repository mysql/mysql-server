/* Copyright (C) 2006 MySQL AB
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <tap.h>

unsigned int gcs(unsigned int a, unsigned int b)
{
  if (b > a) {
    unsigned int t = a;
    a = b;
    b = t;
  }

  while (b != 0) {
    unsigned int m = a % b;
    a = b;
    b = m;
  }
  return a;
}

int main() {
  unsigned int a,b;
  unsigned int failed;
  plan(1);
  diag("Testing basic functions");
  failed = 0;
  for (a = 1 ; a < 2000 ; ++a)
    for (b = 1 ; b < 2000 ; ++b)
    {
      unsigned int d = gcs(a, b);
      if (a % d != 0 || b % d != 0) {
        ++failed;
        diag("Failed for gcs(%4u,%4u)", a, b);
      }
    }
  ok(failed == 0, "Testing gcs()");
  return exit_status();
}

