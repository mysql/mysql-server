/* Copyright (C) 2000 MySQL AB

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

/* Written by Sinisa Milivojevic <sinisa@mysql.com> */

#include <my_global.h>
#ifdef HAVE_COMPRESS
#include <my_sys.h>
#ifndef SCO
#include <m_string.h>
#endif
#include <zlib.h>

/*
** This replaces the packet with a compressed packet
** Returns 1 on error
** *complen is 0 if the packet wasn't compressed
*/

my_bool my_compress(byte *packet, ulong *len, ulong *complen)
{
  DBUG_ENTER("my_compress");
  if (*len < MIN_COMPRESS_LENGTH)
  {
    *complen=0;
    DBUG_PRINT("note",("Packet too short: Not compressed"));
  }
  else
  {
    byte *compbuf=my_compress_alloc(packet,len,complen);
    if (!compbuf)
      DBUG_RETURN(*complen ? 0 : 1);
    memcpy(packet,compbuf,*len);
    my_free(compbuf,MYF(MY_WME));						  }
  DBUG_RETURN(0);
}


byte *my_compress_alloc(const byte *packet, ulong *len, ulong *complen)
{
  byte *compbuf;
  *complen=  *len * 120 / 100 + 12;
  if (!(compbuf= (byte *) my_malloc(*complen,MYF(MY_WME))))
    return 0;					/* Not enough memory */
  if (compress((Bytef*) compbuf,(ulong *) complen, (Bytef*) packet,
	       (uLong) *len ) != Z_OK)
  {
    my_free(compbuf,MYF(MY_WME));
    return 0;
  }
  if (*complen >= *len)
  {
    *complen= 0;
    my_free(compbuf, MYF(MY_WME));
    DBUG_PRINT("note",("Packet got longer on compression; Not compressed"));
    return 0;
  }
  swap_variables(ulong, *len, *complen);       /* *len is now packet length */
  return compbuf;
}


my_bool my_uncompress (byte *packet, ulong *len, ulong *complen)
{
  DBUG_ENTER("my_uncompress");
  if (*complen)					/* If compressed */
  {
    byte *compbuf= (byte *) my_malloc(*complen,MYF(MY_WME));
    int error;
    if (!compbuf)
      DBUG_RETURN(1);				/* Not enough memory */
    if ((error=uncompress((Bytef*) compbuf, complen, (Bytef*) packet, *len))
	!= Z_OK)
    {						/* Probably wrong packet */
      DBUG_PRINT("error",("Can't uncompress packet, error: %d",error));
      my_free(compbuf, MYF(MY_WME));
      DBUG_RETURN(1);
    }
    *len= *complen;
    memcpy(packet, compbuf, *len);
    my_free(compbuf, MYF(MY_WME));
  }
  DBUG_RETURN(0);
}

/*
  Internal representation of the frm blob
*/

struct frm_blob_header
{
  uint ver;      /* Version of header                         */
  uint orglen;   /* Original length of compressed data        */
  uint complen;  /* Compressed length of data, 0=uncompressed */
};

struct frm_blob_struct
{
  struct frm_blob_header head;
  char data[1];
};

/*
  packfrm is a method used to compress the frm file for storage in a
  handler. This method was developed for the NDB handler and has been moved
  here to serve also other uses.

  SYNOPSIS
    packfrm()
    data                    Data reference to frm file data
    len                     Length of frm file data
    out:pack_data           Reference to the pointer to the packed frm data
    out:pack_len            Length of packed frm file data

  RETURN VALUES
    0                       Success
    >0                      Failure
*/

int packfrm(const void *data, uint len,
            const void **pack_data, uint *pack_len)
{
  int error;
  ulong org_len, comp_len;
  uint blob_len;
  struct frm_blob_struct *blob;
  DBUG_ENTER("packfrm");
  DBUG_PRINT("enter", ("data: %x, len: %d", data, len));

  error= 1;
  org_len= len;
  if (my_compress((byte*)data, &org_len, &comp_len))
    goto err;

  DBUG_PRINT("info", ("org_len: %d, comp_len: %d", org_len, comp_len));
  DBUG_DUMP("compressed", (char*)data, org_len);

  error= 2;
  blob_len= sizeof(struct frm_blob_header)+org_len;
  if (!(blob= (struct frm_blob_struct*) my_malloc(blob_len,MYF(MY_WME))))
    goto err;

  /* Store compressed blob in machine independent format */
  int4store((char*)(&blob->head.ver), 1);
  int4store((char*)(&blob->head.orglen), comp_len);
  int4store((char*)(&blob->head.complen), org_len);

  /* Copy frm data into blob, already in machine independent format */
  memcpy(blob->data, data, org_len);

  *pack_data= blob;
  *pack_len= blob_len;
  error= 0;

  DBUG_PRINT("exit", ("pack_data: %x, pack_len: %d", *pack_data, *pack_len));
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

int unpackfrm(const void **unpack_data, uint *unpack_len,
              const void *pack_data)
{
   const struct frm_blob_struct *blob= (struct frm_blob_struct*)pack_data;
   byte *data;
   ulong complen, orglen, ver;
   DBUG_ENTER("unpackfrm");
   DBUG_PRINT("enter", ("pack_data: %x", pack_data));

   complen=     uint4korr((char*)&blob->head.complen);
   orglen=      uint4korr((char*)&blob->head.orglen);
   ver=         uint4korr((char*)&blob->head.ver);

   DBUG_PRINT("blob",("ver: %d complen: %d orglen: %d",
                     ver,complen,orglen));
   DBUG_DUMP("blob->data", (char*) blob->data, complen);

   if (ver != 1)
     DBUG_RETURN(1);
   if (!(data= my_malloc(max(orglen, complen), MYF(MY_WME))))
     DBUG_RETURN(2);
   memcpy(data, blob->data, complen);


   if (my_uncompress(data, &complen, &orglen))
   {
     my_free((char*)data, MYF(0));
     DBUG_RETURN(3);
   }

   *unpack_data= data;
   *unpack_len= complen;

   DBUG_PRINT("exit", ("frmdata: %x, len: %d", *unpack_data, *unpack_len));
   DBUG_RETURN(0);
}
#endif /* HAVE_COMPRESS */
