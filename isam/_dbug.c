/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Support rutiner with are using with dbug */

#include "isamdef.h"

	/* Print a key in user understandable format */

void _nisam_print_key(FILE *stream, register N_KEYSEG *keyseg, const uchar *key)
{
  int flag;
  short int s_1;
  long	int l_1;
  float f_1;
  double d_1;
  uchar *end;

  VOID(fputs("Key: \"",stream));
  flag=0;
  for (; keyseg->base.type ;keyseg++)
  {
    if (flag++)
      VOID(putc('-',stream));
    end= (uchar*) key+ keyseg->base.length;
    switch (keyseg->base.type) {
    case HA_KEYTYPE_BINARY:
      if (!(keyseg->base.flag & HA_SPACE_PACK) && keyseg->base.length == 1)
      {						/* packed binary digit */
	VOID(fprintf(stream,"%d",(uint) *key++));
	break;
      }
      /* fall through */
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_NUM:
      if (keyseg->base.flag & HA_SPACE_PACK)
      {
	VOID(fprintf(stream,"%.*s",(int) *key,key+1));
	key+= (int) *key+1;
      }
      else
      {
	VOID(fprintf(stream,"%.*s",(int) keyseg->base.length,key));
	key=end;
      }
      break;
    case HA_KEYTYPE_INT8:
      VOID(fprintf(stream,"%d",(int) *((signed char*) key)));
      key=end;
      break;
    case HA_KEYTYPE_SHORT_INT:
      shortget(s_1,key);
      VOID(fprintf(stream,"%d",(int) s_1));
      key=end;
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
	ushort u_1;
	ushortget(u_1,key);
	VOID(fprintf(stream,"%u",(uint) u_1));
	key=end;
	break;
      }
    case HA_KEYTYPE_LONG_INT:
      longget(l_1,key);
      VOID(fprintf(stream,"%ld",l_1));
      key=end;
      break;
    case HA_KEYTYPE_ULONG_INT:
      longget(l_1,key);
      VOID(fprintf(stream,"%lu",(ulong) l_1));
      key=end;
      break;
    case HA_KEYTYPE_INT24:
      VOID(fprintf(stream,"%ld",sint3korr(key)));
      key=end;
      break;
    case HA_KEYTYPE_UINT24:
      VOID(fprintf(stream,"%ld",uint3korr(key)));
      key=end;
      break;
    case HA_KEYTYPE_FLOAT:
      bmove((byte*) &f_1,(byte*) key,(int) sizeof(float));
      VOID(fprintf(stream,"%g",(double) f_1));
      key=end;
      break;
    case HA_KEYTYPE_DOUBLE:
      doubleget(d_1,key);
      VOID(fprintf(stream,"%g",d_1));
      key=end;
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      char buff[21];
      longlong tmp;
      longlongget(tmp,key);
      longlong2str(tmp,buff,-10);
      VOID(fprintf(stream,"%s",buff));
      key=end;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      char buff[21];
      longlong tmp;
      longlongget(tmp,key);
      longlong2str(tmp,buff,10);
      VOID(fprintf(stream,"%s",buff));
      key=end;
      break;
    }
#endif
    default: break;			/* This never happens */
    }
  }
  VOID(fputs("\n",stream));
  return;
} /* print_key */
