/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "zgroups.h"


#ifdef HAVE_UGID


const int Uuid::TEXT_LENGTH;
const int Uuid::BYTE_LENGTH;
const int Uuid::BIT_LENGTH;
const int Uuid::bytes_per_section[Uuid::NUMBER_OF_SECTIONS]=
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


/**
  Parses the given string, which should be on the form:

    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX,
    where each X is a hex digit

  Stores the given UUID in this Uuid object.

  @return GS_SUCCESS or GS_PARSE_ERROR
*/
enum_group_status Uuid::parse(const char *s)
{
  DBUG_ENTER("Uuid::parse");
  unsigned char *u= bytes;
  unsigned char *ss= (unsigned char *)s;
  for (int i= 0; i < NUMBER_OF_SECTIONS; i++)
  {
    if (i > 0)
    {
      if (*ss != '-')
        DBUG_RETURN(GS_ERROR_PARSE);
      ss++;
    }
    for (int j= 0; j < bytes_per_section[i]; j++)
    {
      int hi= hex_to_byte[*ss];
      if (hi == -1)
        DBUG_RETURN(GS_ERROR_PARSE);
      ss++;
      int lo= hex_to_byte[*ss];
      if (lo == -1)
        DBUG_RETURN(GS_ERROR_PARSE);
      ss++;
      *u= (hi << 4) + lo;
      u++;
    }
  }
  DBUG_RETURN(GS_SUCCESS);
}


/**
  Validates that the given string is a correct UUID, i.e.:

    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX,
    where each X is a hex digit

  @retval true the string represents a correct UUID.
  @retval false the string does not represent a correct UUID.
 */
bool Uuid::is_valid(const char *s)
{
  DBUG_ENTER("Uuid::is_valid");
  const unsigned char *ss= (const unsigned char *)s;
  for (int i= 0; i < NUMBER_OF_SECTIONS; i++)
  {
    if (i > 0)
    {
      if (*ss != '-')
        DBUG_RETURN(false);
      ss++;
    }        
    for (int j= 0; j < bytes_per_section[i]; j++)
    {
      if (hex_to_byte[*ss] == -1)
        DBUG_RETURN(false);
      ss++;
      if (hex_to_byte[*ss] == -1)
        DBUG_RETURN(false);
      ss++;
    }
  }
  DBUG_RETURN(true);
}


/**
   Stores this Uuid in the given buffer, NULL-terminated.

  The buffer should have space for at least Uuid::TEXT_LENGTH + 1
  characters.

  @return Pointer to the end of the buffer, i.e., &buf[Uuid::TEXT_LENGTH]
*/
int Uuid::to_string(char *buf) const
{
  DBUG_ENTER("Uuid::to_string");
  static const char byte_to_hex[]= "0123456789ABCDEF";
  const unsigned char *u= bytes;
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
  DBUG_RETURN(TEXT_LENGTH);
}


#endif /* HAVE_UGID */
