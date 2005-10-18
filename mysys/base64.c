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

#include <base64.h>
#include <m_string.h>  // strchr()

#ifndef MAIN

static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";


int
base64_needed_encoded_length(int length_of_data)
{
  return ceil(length_of_data * 4 / 3) /* base64 chars */ +
    ceil(length_of_data / (76 * 3 / 4)) /* Newlines */ +
    3 /* Padding */;
}


int
base64_needed_decoded_length(int length_of_encoded_data)
{
  return ceil(length_of_encoded_data * 3 / 4);
}


/*
  Encode a data as base64.

  Note: We require that dst is pre-allocated to correct size.
        See base64_needed_encoded_length().
*/

int
base64_encode(const void *src, size_t src_len, char *dst)
{
  const unsigned char *s= (const unsigned char*)src;
  size_t i= 0;
  size_t len= 0;

  for (; i < src_len; len += 4)
  {
    if (len == 76)
    {
      len= 0;
      *dst++= '\n';
    }

    unsigned c;
    c= s[i++];
    c <<= 8;

    if (i < src_len)
      c += s[i];
    c <<= 8;
    i++;

    if (i < src_len)
      c += s[i];
    i++;

    *dst++= base64_table[(c >> 18) & 0x3f];
    *dst++= base64_table[(c >> 12) & 0x3f];

    if (i > (src_len + 1))
      *dst++= '=';
    else
      *dst++= base64_table[(c >> 6) & 0x3f];

    if (i > src_len)
      *dst++= '=';
    else
      *dst++= base64_table[(c >> 0) & 0x3f];
  }

  return 0;
}


static inline unsigned
pos(unsigned char c)
{
  return strchr(base64_table, c) - base64_table;
}


#define SKIP_SPACE(src, i, size)                                \
{                                                               \
  while (i < size && my_isspace(&my_charset_latin1, * src))     \
  {                                                             \
    i++;                                                        \
    src++;                                                      \
  }                                                             \
  if (i == size)                                                \
  {                                                             \
    i= size + 1;                                                \
    break;                                                      \
  }                                                             \
}


/*
  Decode a base64 string

  Note: We require that dst is pre-allocated to correct size.
        See base64_needed_decoded_length().

  RETURN  Number of bytes produced in dst or -1 in case of failure
*/
int
base64_decode(const char *src, size_t size, void *dst)
{
  char b[3];
  size_t i= 0;
  void *d= dst;
  size_t j;

  while (i < size)
  {
    unsigned c= 0;
    size_t mark= 0;

    SKIP_SPACE(src, i, size);

    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    if (* src != '=')
      c += pos(*src++);
    else
    {
      i= size;
      mark= 2;
      c <<= 6;
      goto end;
    }
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, size);

    if (*src != '=')
      c += pos(*src++);
    else
    {
      i= size;
      mark= 1;
      goto end;
    }
    i++;

  end:
    b[0]= (c >> 16) & 0xff;
    b[1]= (c >>  8) & 0xff;
    b[2]= (c >>  0) & 0xff;

    for (j=0; j<3-mark; j++)
      *(char *)d++= b[j];
  }

  if (i != size)
  {
    return -1;
  }
  return d - dst;
}


#else /* MAIN */

#define require(b) { \
  if (!(b)) { \
    printf("Require failed at %s:%d\n", __FILE__, __LINE__); \
    abort(); \
  } \
}


int
main(void)
{
  int i;
  size_t j;
  size_t k, l;
  size_t dst_len;

  for (i= 0; i < 500; i++)
  {
    /* Create source data */
    const size_t src_len= rand() % 1000 + 1;

    char * src= (char *) malloc(src_len);
    char * s= src;

    for (j= 0; j<src_len; j++)
    {
      char c= rand();
      *s++= c;
    }

    /* Encode */
    char * str= (char *) malloc(base64_needed_encoded_length(src_len));
    require(base64_encode(src, src_len, str) == 0);

    /* Decode */
    char * dst= (char *) malloc(base64_needed_decoded_length(strlen(str)));
    dst_len= base64_decode(str, strlen(str), dst);
    require(dst_len == src_len);

    if (memcmp(src, dst, src_len) != 0)
    {
      printf("       --------- src ---------   --------- dst ---------\n");
      for (k= 0; k<src_len; k+=8)
      {
        printf("%.4x   ", (uint) k);
        for (l=0; l<8 && k+l<src_len; l++)
        {
          unsigned char c= src[k+l];
          printf("%.2x ", (unsigned)c);
        }

        printf("  ");

        for (l=0; l<8 && k+l<dst_len; l++)
        {
          unsigned char c= dst[k+l];
          printf("%.2x ", (unsigned)c);
        }
        printf("\n");
      }
      printf("src length: %.8x, dst length: %.8x\n",
             (uint) src_len, (uint) dst_len);
      require(0);
    }
  }
  printf("Test succeeded.\n");
  return 0;
}

#endif
