/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "control_events.h"


/*
const size_t Uuid::TEXT_LENGTH;
const size_t Uuid::BYTE_LENGTH;
const size_t Uuid::BIT_LENGTH;
*/
namespace binary_log
{

const int Uuid::bytes_per_section[NUMBER_OF_SECTIONS]=
{ 4, 2, 2, 2, 6 };
const int Uuid::hex_to_byte[]=
{
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

int Uuid::parse(const char *s)
{
  unsigned char *u= bytes;
  unsigned char *ss= (unsigned char *)s;
  for (int i= 0; i < NUMBER_OF_SECTIONS; i++)
  {
    if (i > 0)
    {
      if (*ss != '-')
        return 1;
      ss++;
    }
    for (int j= 0; j < bytes_per_section[i]; j++)
    {
      int hi= hex_to_byte[*ss];
      if (hi == -1)
        return 1;
      ss++;
      int lo= hex_to_byte[*ss];
      if (lo == -1)
        return 1;
      ss++;
      *u= (hi << 4) + lo;
      u++;
    }
  }
  return 0;
}

bool Uuid::is_valid(const char *s)
{
  const unsigned char *ss= (const unsigned char *)s;
  for (int i= 0; i < NUMBER_OF_SECTIONS; i++)
  {
    if (i > 0)
    {
      if (*ss != '-')
        return (false);
      ss++;
    }
    for (int j= 0; j < bytes_per_section[i]; j++)
    {
      if (hex_to_byte[*ss] == -1)
        return (false);
      ss++;
      if (hex_to_byte[*ss] == -1)
        return (false);
      ss++;
    }
  }
  return (true);
}
size_t Uuid::to_string(const unsigned char* bytes_arg, char *buf)
{
  static const char byte_to_hex[]= "0123456789abcdef";
  const unsigned char *u= bytes_arg;
  for (int i= 0; i < NUMBER_OF_SECTIONS; i++)
  {
    if (i > 0)
    {
      *buf= '-';
      buf++;
    }
    for (int j= 0; j < bytes_per_section[i]; j++)
    {
      int byte= *u;
      *buf= byte_to_hex[byte >> 4];
      buf++;
      *buf= byte_to_hex[byte & 0xf];
      buf++;
      u++;
    }
  }
  *buf= '\0';
  return TEXT_LENGTH;
}


size_t Uuid::to_string(char *buf) const
{
  return to_string(bytes, buf);
}
}//namespace binary_log
