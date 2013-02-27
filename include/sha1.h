#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#define SHA1_HASH_SIZE 20 /* Hash size in bytes */

C_MODE_START

void compute_sha1_hash(uint8 *digest, const char *buf, int len);
void compute_sha1_hash_multi(uint8 *digest, const char *buf1, int len1,
                             const char *buf2, int len2);

C_MODE_END

#endif /* SHA1_INCLUDED */
