/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Written by Sinisa Milivojevic <sinisa@mysql.com> */

/**
  @file mysys/my_compress.cc
*/

#include <string.h>
#include <sys/types.h>
#include <zlib.h>
#include <algorithm>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys/mysys_priv.h"

/*
   This replaces the packet with a compressed packet

   SYNOPSIS
     my_compress()
     packet	Data to compress. This is is replaced with the compressed data.
     len	Length of data to compress at 'packet'
     complen	out: 0 if packet was not compressed

   RETURN
     1   error. 'len' is not changed'
     0   ok.  In this case 'len' contains the size of the compressed packet
*/

bool my_compress(uchar *packet, size_t *len, size_t *complen) {
  DBUG_ENTER("my_compress");
  if (*len < MIN_COMPRESS_LENGTH) {
    *complen = 0;
    DBUG_PRINT("note", ("Packet too short: Not compressed"));
  } else {
    uchar *compbuf = my_compress_alloc(packet, len, complen);
    if (!compbuf) DBUG_RETURN(*complen ? 0 : 1);
    memcpy(packet, compbuf, *len);
    my_free(compbuf);
  }
  DBUG_RETURN(0);
}

uchar *my_compress_alloc(const uchar *packet, size_t *len, size_t *complen) {
  uchar *compbuf;
  uLongf tmp_complen;
  int res;
  *complen = *len * 120 / 100 + 12;

  if (!(compbuf = (uchar *)my_malloc(key_memory_my_compress_alloc, *complen,
                                     MYF(MY_WME))))
    return 0; /* Not enough memory */

  tmp_complen = (uint)*complen;
  res = compress((Bytef *)compbuf, &tmp_complen, (Bytef *)packet, (uLong)*len);
  *complen = tmp_complen;

  if (res != Z_OK) {
    my_free(compbuf);
    return 0;
  }

  if (*complen >= *len) {
    *complen = 0;
    my_free(compbuf);
    DBUG_PRINT("note", ("Packet got longer on compression; Not compressed"));
    return 0;
  }
  /* Store length of compressed packet in *len */
  std::swap(*len, *complen);
  return compbuf;
}

/*
  Uncompress packet

   SYNOPSIS
     my_uncompress()
     packet	Compressed data. This is is replaced with the orignal data.
     len	Length of compressed data
     complen	Length of the packet buffer (must be enough for the original
                data)

   RETURN
     1   error
     0   ok.  In this case 'complen' contains the updated size of the
              real data.
*/

bool my_uncompress(uchar *packet, size_t len, size_t *complen) {
  uLongf tmp_complen;
  DBUG_ENTER("my_uncompress");

  if (*complen) /* If compressed */
  {
    uchar *compbuf =
        (uchar *)my_malloc(key_memory_my_compress_alloc, *complen, MYF(MY_WME));
    int error;
    if (!compbuf) DBUG_RETURN(1); /* Not enough memory */

    tmp_complen = (uint)*complen;
    error =
        uncompress((Bytef *)compbuf, &tmp_complen, (Bytef *)packet, (uLong)len);
    *complen = tmp_complen;
    if (error != Z_OK) { /* Probably wrong packet */
      DBUG_PRINT("error", ("Can't uncompress packet, error: %d", error));
      my_free(compbuf);
      DBUG_RETURN(1);
    }
    memcpy(packet, compbuf, *complen);
    my_free(compbuf);
  } else
    *complen = len;
  DBUG_RETURN(0);
}
