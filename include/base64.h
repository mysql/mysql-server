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

#ifndef __BASE64_H_INCLUDED__
#define __BASE64_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

/*
  Calculate how much memory needed for dst of base64_encode()
*/
int base64_needed_encoded_length(int length_of_data);

/*
  Calculate how much memory needed for dst of base64_decode()
*/
int base64_needed_decoded_length(int length_of_encoded_data);

/*
  Encode data as a base64 string
*/
int base64_encode(const void *src, size_t src_len, char *dst);

/*
  Decode a base64 string into data
*/
int base64_decode(const char *src, size_t src_len,
                  void *dst, const char **end_ptr);


#ifdef __cplusplus
}
#endif
#endif /* !__BASE64_H_INCLUDED__ */
