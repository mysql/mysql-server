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

#include <ndb_global.h>
#include <Base64.hpp>

static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";

int
base64_encode(const UtilBuffer &src, BaseString &dst) 
{
  return base64_encode(src.get_data(), src.length(), dst);
}

int
base64_encode(const void * _s, size_t src_len, BaseString &dst) {
  const unsigned char * s = (const unsigned char*)_s;
  size_t i = 0;
  size_t len = 0;
  while(i < src_len) {
    if(len == 76){
      len = 0;
      dst.append('\n');
    }
    
    unsigned c;
    c = s[i++];
    c <<= 8;

    if(i < src_len)
      c += s[i];
    c <<= 8;
    i++;
    
    if(i < src_len)
      c += s[i];
    i++;
    
    dst.append(base64_table[(c >> 18) & 0x3f]);
    dst.append(base64_table[(c >> 12) & 0x3f]);

    if(i > (src_len + 1))
      dst.append('=');
    else
      dst.append(base64_table[(c >> 6) & 0x3f]);

    if(i > src_len)
      dst.append('=');
    else
      dst.append(base64_table[(c >> 0) & 0x3f]);

    len += 4;
  }
  return 0;
}

static inline unsigned
pos(unsigned char c) {
  return strchr(base64_table, c) - base64_table;
}


int
base64_decode(const BaseString &src, UtilBuffer &dst) {
  return base64_decode(src.c_str(), src.length(), dst);
}

#define SKIP_SPACE(src, i, size){     \
  while(i < size && isspace(* src)){  \
    i++;                              \
    src++;                            \
  }                                   \
  if(i == size){                      \
    i = size + 1;                     \
    break;                            \
  }                                   \
}

int
base64_decode(const char * src, size_t size, UtilBuffer &dst) {
  size_t i = 0;
  while(i < size){
    unsigned c = 0;
    int mark = 0;

    SKIP_SPACE(src, i, size);
    
    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    if(* src != '=')
      c += pos(*src++);
    else {
      i = size;
      mark = 2;
      c <<= 6;
      goto end;
    }
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    if(*src != '=')
      c += pos(*src++);
    else {
      i = size;
      mark = 1;
      goto end;
    }
    i++;

  end:
    char b[3];
    b[0] = (c >> 16) & 0xff;
    b[1] = (c >>  8) & 0xff;
    b[2] = (c >>  0) & 0xff;
    
    dst.append((void *)b, 3-mark);
  }
  
  if(i != size){
    abort();
    return -1;
  }
  return 0;
}

#ifdef __TEST__B64
/**
 * USER_FLAGS="-D__TEST__B64" make Base64.o && g++ Base64.o BaseString.o
 */
inline
void
require(bool b){
  if(!b)
    abort();
}

int
main(void){
  for(int i = 0; i < 500; i++){
    const size_t len = rand() % 10000 + 1;
    UtilBuffer src;
    for(size_t j = 0; j<len; j++){
      char c = rand();
      src.append(&c, 1);
    }
    require(src.length() == len);

    BaseString str;
    require(base64_encode(src, str) == 0);

    if(str.length() == 3850){
      printf(">%s<\n", str.c_str());
    }

    UtilBuffer dst;
    require(base64_decode(str, dst) == 0);
    require(dst.length() == src.length());

    const char * c_src = (char*)src.get_data();
    const char * c_dst = (char*)dst.get_data();
    if(memcmp(src.get_data(), dst.get_data(), src.length()) != 0){
      printf("-- src --\n");
      for(int i2 = 0; i2<len; i2++){
	unsigned char c = c_src[i2];
	printf("%.2x ", (unsigned)c);
	if((i2 % 8) == 7)
	  printf("\n");
      }
      printf("\n");

      printf("-- dst --\n");
      for(int i2 = 0; i2<len; i2++){
	unsigned char c = c_dst[i2];
	printf("%.2x ", (unsigned)c);
	if((i2 % 8) == 7)
	  printf("\n");
      }
      printf("\n");
      abort();
    }
  }
  return 0;
}

#endif
