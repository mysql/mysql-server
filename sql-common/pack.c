/* Copyright (c) 2000-2003, 2007 MySQL AB
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <my_global.h>
#include <mysql_com.h>
#include <mysql.h>

/* Get the length of next field. Change parameter to point at fieldstart */
ulong STDCALL net_field_length(uchar **packet)
{
  reg1 uchar *pos= (uchar *)*packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (ulong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (ulong) uint3korr(pos+1);
  }
  (*packet)+=9;					/* Must be 254 when here */
  return (ulong) uint4korr(pos+1);
}

/* The same as above but returns longlong */
my_ulonglong net_field_length_ll(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (my_ulonglong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return (my_ulonglong) NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (my_ulonglong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (my_ulonglong) uint3korr(pos+1);
  }
  (*packet)+=9;					/* Must be 254 when here */
#ifdef NO_CLIENT_LONGLONG
  return (my_ulonglong) uint4korr(pos+1);
#else
  return (my_ulonglong) uint8korr(pos+1);
#endif
}

/*
  Store an integer with simple packing into a output package

  SYNOPSIS
    net_store_length()
    pkg			Store the packed integer here
    length		integers to store

  NOTES
    This is mostly used to store lengths of strings.
    We have to cast the result for the LL() becasue of a bug in Forte CC
    compiler.

  RETURN
   Position in 'pkg' after the packed length
*/

uchar *net_store_length(uchar *packet, ulonglong length)
{
  if (length < (ulonglong) LL(251))
  {
    *packet=(uchar) length;
    return packet+1;
  }
  /* 251 is reserved for NULL */
  if (length < (ulonglong) LL(65536))
  {
    *packet++=252;
    int2store(packet,(uint) length);
    return packet+2;
  }
  if (length < (ulonglong) LL(16777216))
  {
    *packet++=253;
    int3store(packet,(ulong) length);
    return packet+3;
  }
  *packet++=254;
  int8store(packet,length);
  return packet+8;
}

