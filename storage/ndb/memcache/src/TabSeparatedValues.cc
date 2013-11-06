/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

#include <assert.h>

#include "TabSeparatedValues.h"

TabSeparatedValues::TabSeparatedValues(const char *string, Uint32 max_parts, size_t length) :
  index(0), parts(0)
{
  size_t parsed_len = 0;

  while(parsed_len <= length && parts < max_parts && parts < MAX_VAL_COLUMNS) {
    const char *s = string + parsed_len;
    pointers[parts] = s;
    lengths[parts] = find_tab(s, length - parsed_len);
    parsed_len += lengths[parts] + 1;
    parts++;
  }
}


int TabSeparatedValues::find_tab(const char *s, int remaining) const {
  int r;
  for(r = 0; r < remaining && *(s+r) != '\t' && *(s+r) != '\0'; r++);
  return r;
}
