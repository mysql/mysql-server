/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA */

#include <my_global.h>
#include <base64.h>
#include <tap.h>
#include <string.h>

int
main(void)
{
  int i, cmp;
  size_t j, k, l, dst_len, needed_length;

  for (i= 0; i < 500; i++)
  {
    /* Create source data */
    const size_t src_len= rand() % 1000 + 1;

    char * src= (char *) malloc(src_len);
    char * s= src;
    char * str;
    char * dst;

    for (j= 0; j<src_len; j++)
    {
      char c= rand();
      *s++= c;
    }

    /* Encode */
    needed_length= base64_needed_encoded_length(src_len);
    str= (char *) malloc(needed_length);
    for (k= 0; k < needed_length; k++)
      str[k]= 0xff; /* Fill memory to check correct NUL termination */
    ok(base64_encode(src, src_len, str) == 0,
       "base64_encode: size %d", i);
    ok(needed_length == strlen(str) + 1,
       "base64_needed_encoded_length: size %d", i);

    /* Decode */
    dst= (char *) malloc(base64_needed_decoded_length(strlen(str)));
    dst_len= base64_decode(str, strlen(str), dst);
    ok(dst_len == src_len, "Comparing lengths");

    cmp= memcmp(src, dst, src_len);
    ok(cmp == 0, "Comparing encode-decode result");
    if (cmp != 0)
    {
      char buf[80];
      diag("       --------- src ---------   --------- dst ---------");
      for (k= 0; k<src_len; k+=8)
      {
        sprintf(buf, "%.4x   ", (uint) k);
        for (l=0; l<8 && k+l<src_len; l++)
        {
          unsigned char c= src[k+l];
          sprintf(buf, "%.2x ", (unsigned)c);
        }

        sprintf(buf, "  ");

        for (l=0; l<8 && k+l<dst_len; l++)
        {
          unsigned char c= dst[k+l];
          sprintf(buf, "%.2x ", (unsigned)c);
        }
        diag(buf);
      }
      diag("src length: %.8x, dst length: %.8x\n",
           (uint) src_len, (uint) dst_len);
    }
  }
  return exit_status();
}
