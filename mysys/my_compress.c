/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Written by Sinisa Milivojevic <sinisa@mysql.com> */

#include <my_global.h>
#ifdef HAVE_COMPRESS
#include <my_sys.h>
#ifndef SCO
#include <m_string.h>
#endif
#include <zlib.h>

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

my_bool my_compress(uchar *packet, size_t *len, size_t *complen)
{
  DBUG_ENTER("my_compress");
  if (*len < MIN_COMPRESS_LENGTH)
  {
    *complen=0;
    DBUG_PRINT("note",("Packet too short: Not compressed"));
  }
  else
  {
    uchar *compbuf=my_compress_alloc(packet,len,complen);
    if (!compbuf)
      DBUG_RETURN(*complen ? 0 : 1);
    memcpy(packet,compbuf,*len);
    my_free(compbuf);
  }
  DBUG_RETURN(0);
}


uchar *my_compress_alloc(const uchar *packet, size_t *len, size_t *complen)
{
  uchar *compbuf;
  uLongf tmp_complen;
  int res;
  *complen=  *len * 120 / 100 + 12;

  if (!(compbuf= (uchar *) my_malloc(*complen, MYF(MY_WME))))
    return 0;					/* Not enough memory */

  tmp_complen= (uint) *complen;
  res= compress((Bytef*) compbuf, &tmp_complen, (Bytef*) packet, (uLong) *len);
  *complen=    tmp_complen;

  if (res != Z_OK)
  {
    my_free(compbuf);
    return 0;
  }

  if (*complen >= *len)
  {
    *complen= 0;
    my_free(compbuf);
    DBUG_PRINT("note",("Packet got longer on compression; Not compressed"));
    return 0;
  }
  /* Store length of compressed packet in *len */
  swap_variables(size_t, *len, *complen);
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

my_bool my_uncompress(uchar *packet, size_t len, size_t *complen)
{
  uLongf tmp_complen;
  DBUG_ENTER("my_uncompress");

  if (*complen)					/* If compressed */
  {
    uchar *compbuf= (uchar *) my_malloc(*complen,MYF(MY_WME));
    int error;
    if (!compbuf)
      DBUG_RETURN(1);				/* Not enough memory */

    tmp_complen= (uint) *complen;
    error= uncompress((Bytef*) compbuf, &tmp_complen, (Bytef*) packet,
                      (uLong) len);
    *complen= tmp_complen;
    if (error != Z_OK)
    {						/* Probably wrong packet */
      DBUG_PRINT("error",("Can't uncompress packet, error: %d",error));
      my_free(compbuf);
      DBUG_RETURN(1);
    }
    memcpy(packet, compbuf, *complen);
    my_free(compbuf);
  }
  else
    *complen= len;
  DBUG_RETURN(0);
}

/*
  Internal representation of the frm blob is:

  ver	  4 bytes
  orglen  4 bytes
  complen 4 bytes
*/

#define BLOB_HEADER 12


/*
  packfrm is a method used to compress the frm file for storage in a
  handler. This method was developed for the NDB handler and has been moved
  here to serve also other uses.

  SYNOPSIS
    packfrm()
    data                    Data reference to frm file data.
    len                     Length of frm file data
    out:pack_data           Reference to the pointer to the packed frm data
    out:pack_len            Length of packed frm file data

  NOTES
    data is replaced with compressed content

  RETURN VALUES
    0                       Success
    >0                      Failure
*/

int packfrm(uchar *data, size_t len,
            uchar **pack_data, size_t *pack_len)
{
  int error;
  size_t org_len, comp_len, blob_len;
  uchar *blob;
  DBUG_ENTER("packfrm");
  DBUG_PRINT("enter", ("data: 0x%lx  len: %lu", (long) data, (ulong) len));

  error= 1;
  org_len= len;
  if (my_compress((uchar*)data, &org_len, &comp_len))
    goto err;

  DBUG_PRINT("info", ("org_len: %lu  comp_len: %lu", (ulong) org_len,
                      (ulong) comp_len));
  DBUG_DUMP("compressed", data, org_len);

  error= 2;
  blob_len= BLOB_HEADER + org_len;
  if (!(blob= (uchar*) my_malloc(blob_len,MYF(MY_WME))))
    goto err;

  /* Store compressed blob in machine independent format */
  int4store(blob, 1);
  int4store(blob+4, (uint32) len);
  int4store(blob+8, (uint32) org_len);          /* compressed length */

  /* Copy frm data into blob, already in machine independent format */
  memcpy(blob+BLOB_HEADER, data, org_len);

  *pack_data= blob;
  *pack_len=  blob_len;
  error= 0;

  DBUG_PRINT("exit", ("pack_data: 0x%lx  pack_len: %lu",
                      (long) *pack_data, (ulong) *pack_len));
err:
  DBUG_RETURN(error);

}

/*
  unpackfrm is a method used to decompress the frm file received from a
  handler. This method was developed for the NDB handler and has been moved
  here to serve also other uses for other clustered storage engines.

  SYNOPSIS
    unpackfrm()
    pack_data               Data reference to packed frm file data
    out:unpack_data         Reference to the pointer to the unpacked frm data
    out:unpack_len          Length of unpacked frm file data

  RETURN VALUES¨
    0                       Success
    >0                      Failure
*/

int unpackfrm(uchar **unpack_data, size_t *unpack_len,
              const uchar *pack_data)
{
   uchar *data;
   size_t complen, orglen;
   ulong ver;
   DBUG_ENTER("unpackfrm");
   DBUG_PRINT("enter", ("pack_data: 0x%lx", (long) pack_data));

   ver=         uint4korr(pack_data);
   orglen=      uint4korr(pack_data+4);
   complen=     uint4korr(pack_data+8);

   DBUG_PRINT("blob",("ver: %lu  complen: %lu  orglen: %lu",
                      ver, (ulong) complen, (ulong) orglen));
   DBUG_DUMP("blob->data", pack_data + BLOB_HEADER, complen);

   if (ver != 1)
     DBUG_RETURN(1);
   if (!(data= my_malloc(MY_MAX(orglen, complen), MYF(MY_WME))))
     DBUG_RETURN(2);
   memcpy(data, pack_data + BLOB_HEADER, complen);

   if (my_uncompress(data, complen, &orglen))
   {
     my_free(data);
     DBUG_RETURN(3);
   }

   *unpack_data= data;
   *unpack_len=  orglen;

   DBUG_PRINT("exit", ("frmdata: 0x%lx  len: %lu", (long) *unpack_data,
                       (ulong) *unpack_len));
   DBUG_RETURN(0);
}
#endif /* HAVE_COMPRESS */
