/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA */

/*
  Variable length encoding.

  A method to store an arbitrary-size non-negative integer. We let the
  most significant bit of the number indicate that the next byte
  should be contatenated to form the real number.
*/

#include "my_vle.h"

/*
  Function to encode an unsigned long as VLE. The bytes for the VLE
  will be written to the location pointed to by 'out'.  The maximum
  number of bytes written will be 'max'.

  PARAMETERS

    out  Pointer to beginning of where to store VLE bytes.
    max  Maximum number of bytes to write.
    n    Number to encode.

  RETURN VALUE
    On success, one past the end of the array containing the VLE
    bytes.  On failure, the 'out' pointer is returned.
*/

uchar*
my_vle_encode(uchar* out, size_t max, ulong n) 
{
  uchar buf[my_vle_sizeof(n)];
  uchar *ptr= buf;
  size_t len;

  do
  {
    *ptr++= (uchar) (n & 0x7F);
    n>>= 7;
  }
  while (n > 0);

  len= ptr - buf;
  
  if (len <= max)
  {
    /*
      The bytes are stored in reverse order in 'buf'. Let's write them
      in correct order to the output buffer and set the MSB at the
      same time.
    */
    while (ptr-- > buf)
    {
      uchar v= *ptr;
      if (ptr > buf)
        v|= 0x80;
      *out++= v;
    }
  }

  return out;
}

/*
  Function to decode a VLE representation of an integral value.


  PARAMETERS

    result_ptr  Pointer to an unsigned long where the value will be written.
    vle         Pointer to the VLE bytes.

  RETURN VALUE

    One-past the end of the VLE bytes. The routine will never read
    more than sizeof(*result_ptr) + 1 bytes.
*/

uchar const*
my_vle_decode(ulong *result_ptr, uchar const *vle)
{
  ulong result= 0;
  size_t cnt= 1;

  do
  {
    result<<= 7;
    result|= (*vle & 0x7F);
  }
  while ((*vle++ & 0x80) && ++cnt <= sizeof(*result_ptr) + 1);

  if (cnt <= sizeof(*result_ptr) + 1)
    *result_ptr= result;

  return vle;
}
