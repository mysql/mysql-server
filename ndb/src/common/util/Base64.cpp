/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdio.h>
#include <string.h>
#include <Base64.hpp>

static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";

int
base64_encode(UtilBuffer &src, BaseString &dst) {
  char *s = (char *)src.get_data();
  int i = 0;

  while(i < src.length()) {
    int c;
    c = s[i++];
    c <<= 8;

    if(i < src.length())
      c += s[i];
    c <<= 8;
    i++;
    
    if(i < src.length())
      c += s[i];
    i++;

    dst.append(base64_table[(c >> 18) & 0x3f]);
    dst.append(base64_table[(c >> 12) & 0x3f]);

    if(i > (src.length() + 1))
      dst.append('=');
    else
      dst.append(base64_table[(c >> 6) & 0x3f]);

    if(i > src.length())
      dst.append('=');
    else
      dst.append(base64_table[(c >> 0) & 0x3f]);
  }
  return 0;
}

static inline int
pos(char c) {
  return strchr(base64_table, c) - base64_table;
}


int
base64_decode(BaseString &src, UtilBuffer &dst) {
  size_t size;
  size = (src.length() * 3) / 4;
  size_t i = 0;
  const char *s = src.c_str();
  while(i < size) {
    int c = 0;
    int mark = 0;
    c += pos(*s++);
    c <<= 6;
    i++;

    c += pos(*s++);
    c <<= 6;
    i++;

    if(*s != '=')
      c += pos(*s++);
    else {
      size--;
      mark++;
    }
    c <<= 6;
    i++;

    if(*s != '=')
      c += pos(*s++);
    else {
      size--;
      mark++;
    }
    /*    c <<= 6; */
    i++;

    char b[3];


    b[0] = (c >> 16) & 0xff;
    b[1] = (c >>  8) & 0xff;
    b[2] = (c >>  0) & 0xff;

    dst.append((void *)b, 3-mark);
  }
  return 0;
}
