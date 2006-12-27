/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

bool convert(const char* s, int& val) {

  if (s == NULL) {
    return false;
  }

  if (strlen(s) == 0) {
    return false;
  }

  errno = 0;
  char* p;
  long v = strtol(s, &p, 10);
  if (errno != 0) {
    return false;
  }
  if (p != &s[strlen(s)]) {
    return false;
  }

  val = v;
  return true;
}


