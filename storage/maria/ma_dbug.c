/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Support rutiner with are using with dbug */

#include "maria_def.h"

void _ma_print_key(FILE *stream, MARIA_KEY *key)
{
  _ma_print_keydata(stream, key->keyinfo->seg, key->data, key->data_length);
}


/* Print a key in a user understandable format */

void _ma_print_keydata(FILE *stream, register HA_KEYSEG *keyseg,
                       const uchar *key, uint length)
{
  int flag;
  short int s_1;
  long	int l_1;
  float f_1;
  double d_1;
  const uchar *end;
  const uchar *key_end= key + length;

  fputs("Key: \"",stream);
  flag=0;
  for (; keyseg->type && key < key_end ;keyseg++)
  {
    if (flag++)
      putc('-',stream);
    end= key+ keyseg->length;
    if (keyseg->flag & HA_NULL_PART)
    {
      /* A NULL value is encoded by a 1-byte flag. Zero means NULL. */
      if (! *(key++))
      {
	fprintf(stream,"NULL");
	continue;
      }
      end++;
    }

    switch (keyseg->type) {
    case HA_KEYTYPE_BINARY:
      if (!(keyseg->flag & HA_SPACE_PACK) && keyseg->length == 1)
      {						/* packed binary digit */
	fprintf(stream,"%d",(uint) *key++);
	break;
      }
      /* fall through */
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_NUM:
      if (keyseg->flag & HA_SPACE_PACK)
      {
	fprintf(stream,"%.*s",(int) *key,key+1);
	key+= (int) *key+1;
      }
      else
      {
	fprintf(stream,"%.*s",(int) keyseg->length,key);
	key=end;
      }
      break;
    case HA_KEYTYPE_INT8:
      fprintf(stream,"%d",(int) *((const signed char*) key));
      key=end;
      break;
    case HA_KEYTYPE_SHORT_INT:
      s_1= mi_sint2korr(key);
      fprintf(stream,"%d",(int) s_1);
      key=end;
      break;
    case HA_KEYTYPE_USHORT_INT:
      {
	ushort u_1;
	u_1= mi_uint2korr(key);
	fprintf(stream,"%u",(uint) u_1);
	key=end;
	break;
      }
    case HA_KEYTYPE_LONG_INT:
      l_1=mi_sint4korr(key);
      fprintf(stream,"%ld",l_1);
      key=end;
      break;
    case HA_KEYTYPE_ULONG_INT:
      l_1=mi_uint4korr(key);
      fprintf(stream,"%lu",(ulong) l_1);
      key=end;
      break;
    case HA_KEYTYPE_INT24:
      fprintf(stream,"%ld",(long) mi_sint3korr(key));
      key=end;
      break;
    case HA_KEYTYPE_UINT24:
      fprintf(stream,"%lu",(ulong) mi_uint3korr(key));
      key=end;
      break;
    case HA_KEYTYPE_FLOAT:
      mi_float4get(f_1,key);
      fprintf(stream,"%g",(double) f_1);
      key=end;
      break;
    case HA_KEYTYPE_DOUBLE:
      mi_float8get(d_1,key);
      fprintf(stream,"%g",d_1);
      key=end;
      break;
#ifdef HAVE_LONG_LONG
    case HA_KEYTYPE_LONGLONG:
    {
      char buff[21];
      longlong10_to_str(mi_sint8korr(key),buff,-10);
      fprintf(stream,"%s",buff);
      key=end;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      char buff[21];
      longlong10_to_str(mi_sint8korr(key),buff,10);
      fprintf(stream,"%s",buff);
      key=end;
      break;
    }
#endif
    case HA_KEYTYPE_BIT:
    {
      uint i;
      fputs("0x",stream);
      for (i=0 ; i < keyseg->length ; i++)
        fprintf(stream, "%02x", (uint) *key++);
      key= end;
      break;
    }
    case HA_KEYTYPE_VARTEXT1:                   /* VARCHAR and TEXT */
    case HA_KEYTYPE_VARTEXT2:                   /* VARCHAR and TEXT */
    case HA_KEYTYPE_VARBINARY1:                 /* VARBINARY and BLOB */
    case HA_KEYTYPE_VARBINARY2:                 /* VARBINARY and BLOB */
    {
      uint tmp_length;
      get_key_length(tmp_length,key);
      /*
	The following command sometimes gives a warning from valgrind.
	Not yet sure if the bug is in valgrind, glibc or mysqld
      */
      fprintf(stream,"%.*s",(int) tmp_length,key);
      key+=tmp_length;
      break;
    }
    default: break;			/* This never happens */
    }
  }
  fputs("\"\n",stream);
  return;
} /* print_key */


#ifdef EXTRA_DEBUG

my_bool _ma_check_table_is_closed(const char *name, const char *where)
{
  char filename[FN_REFLEN];
  LIST *pos;
  DBUG_ENTER("_ma_check_table_is_closed");

  (void) fn_format(filename,name,"",MARIA_NAME_IEXT,4+16+32);
  mysql_mutex_lock(&THR_LOCK_maria);
  for (pos=maria_open_list ; pos ; pos=pos->next)
  {
    MARIA_HA *info=(MARIA_HA*) pos->data;
    MARIA_SHARE *share= info->s;
    if (!strcmp(share->unique_file_name.str, filename))
    {
      if (share->last_version)
      {
	fprintf(stderr,"Warning:  Table: %s is open on %s\n", name,where);
	DBUG_PRINT("warning",("Table: %s is open on %s", name,where));
        mysql_mutex_unlock(&THR_LOCK_maria);
	DBUG_RETURN(1);
      }
    }
  }
  mysql_mutex_unlock(&THR_LOCK_maria);
  DBUG_RETURN(0);
}
#endif /* EXTRA_DEBUG */
