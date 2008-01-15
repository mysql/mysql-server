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

#include <my_global.h>
#include <m_string.h>  /* strchr() */
#include <m_ctype.h>  /* my_isspace() */
#include <base64.h>

#ifndef MAIN

static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";


int
base64_needed_encoded_length(int length_of_data)
{
  int nb_base64_chars;
  nb_base64_chars= (length_of_data + 2) / 3 * 4;

  return
    nb_base64_chars +            /* base64 char incl padding */
    (nb_base64_chars - 1)/ 76 +  /* newlines */
    1;                           /* NUL termination of string */
}


int
base64_needed_decoded_length(int length_of_encoded_data)
{
  return (int) ceil(length_of_encoded_data * 3 / 4);
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
    unsigned c;

    if (len == 76)
    {
      len= 0;
      *dst++= '\n';
    }

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
  *dst= '\0';

  return 0;
}


static inline uint
pos(unsigned char c)
{
  return (uint) (strchr(base64_table, c) - base64_table);
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
    break;                                                      \
  }                                                             \
}


/*
  Decode a base64 string

  SYNOPSIS
    base64_decode()
    src      Pointer to base64-encoded string
    len      Length of string at 'src'
    dst      Pointer to location where decoded data will be stored
    end_ptr  Pointer to variable that will refer to the character
             after the end of the encoded data that were decoded. Can
             be NULL.

  DESCRIPTION

    The base64-encoded data in the range ['src','*end_ptr') will be
    decoded and stored starting at 'dst'.  The decoding will stop
    after 'len' characters have been read from 'src', or when padding
    occurs in the base64-encoded data. In either case: if 'end_ptr' is
    non-null, '*end_ptr' will be set to point to the character after
    the last read character, even in the presence of error.

  NOTE
    We require that 'dst' is pre-allocated to correct size.

  SEE ALSO
    base64_needed_decoded_length().

  RETURN VALUE
    Number of bytes written at 'dst' or -1 in case of failure
*/
int
base64_decode(const char *src_base, size_t len,
              void *dst, const char **end_ptr)
{
  char b[3];
  size_t i= 0;
  char *dst_base= (char *)dst;
  char const *src= src_base;
  char *d= dst_base;
  size_t j;

  while (i < len)
  {
    unsigned c= 0;
    size_t mark= 0;

    SKIP_SPACE(src, i, len);

    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, len);

    c += pos(*src++);
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, len);

    if (*src != '=')
      c += pos(*src++);
    else
    {
      src += 2;                /* There should be two bytes padding */
      i= len;
      mark= 2;
      c <<= 6;
      goto end;
    }
    c <<= 6;
    i++;

    SKIP_SPACE(src, i, len);

    if (*src != '=')
      c += pos(*src++);
    else
    {
      src += 1;                 /* There should be one byte padding */
      i= len;
      mark= 1;
      goto end;
    }
    i++;

  end:
    b[0]= (c >> 16) & 0xff;
    b[1]= (c >>  8) & 0xff;
    b[2]= (c >>  0) & 0xff;

    for (j=0; j<3-mark; j++)
      *d++= b[j];
  }

  if (end_ptr != NULL)
    *end_ptr= src;

  /*
    The variable 'i' is set to 'len' when padding has been read, so it
    does not actually reflect the number of bytes read from 'src'.
   */
  return i != len ? -1 : d - dst_base;
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
  size_t needed_length;

  for (i= 0; i < 500; i++)
  {
    /* Create source data */
    const size_t src_len= rand() % 1000 + 1;

    char * src= (char *) malloc(src_len);
    char * s= src;
    char * str;
    char * dst;

    require(src);
    for (j= 0; j<src_len; j++)
    {
      char c= rand();
      *s++= c;
    }

    /* Encode */
    needed_length= base64_needed_encoded_length(src_len);
    str= (char *) malloc(needed_length);
    require(str);
    for (k= 0; k < needed_length; k++)
      str[k]= 0xff; /* Fill memory to check correct NUL termination */
    require(base64_encode(src, src_len, str) == 0);
    require(needed_length == strlen(str) + 1);

    /* Decode */
    dst= (char *) malloc(base64_needed_decoded_length(strlen(str)));
    require(dst);
    dst_len= base64_decode(str, strlen(str), dst, NULL);
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
