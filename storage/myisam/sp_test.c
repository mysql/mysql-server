/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.
   
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

/* Testing of the basic functions of a MyISAM spatial table        */
/* Written by Alex Barkov, who has a shared copyright to this code */

#include "myisam.h"

#ifdef HAVE_SPATIAL
#include "sp_defs.h"

#define MAX_REC_LENGTH 1024
#define KEYALG HA_KEY_ALG_RTREE

static void create_linestring(uchar *record,uint rownr);
static void print_record(uchar * record,my_off_t offs,const char * tail);

static void create_key(uchar *key,uint rownr);
static void print_key(const uchar *key,const char * tail);

static int run_test(const char *filename);
static int read_with_pos(MI_INFO * file, int silent);

static int rtree_CreateLineStringWKB(double *ords, uint n_dims, uint n_points,
                                     uchar *wkb);
static  void rtree_PrintWKB(uchar *wkb, uint n_dims);

static char blob_key[MAX_REC_LENGTH];


int main(int argc  __attribute__((unused)),char *argv[])
{
  MY_INIT(argv[0]);
  exit(run_test("sp_test"));
}


int run_test(const char *filename)
{
  MI_INFO        *file;
  MI_UNIQUEDEF   uniquedef;
  MI_CREATE_INFO create_info;
  MI_COLUMNDEF   recinfo[20];
  MI_KEYDEF      keyinfo[20];
  HA_KEYSEG      keyseg[20];
  key_range	 min_range, max_range;
  int silent=0;
  int create_flag=0;
  int null_fields=0;
  int nrecords=30;
  int uniques=0;
  int i;
  int error;
  int row_count=0;
  uchar record[MAX_REC_LENGTH];
  uchar key[MAX_REC_LENGTH];
  uchar read_record[MAX_REC_LENGTH];
  int upd=10;
  ha_rows hrows;
  
  /* Define a column for NULLs and DEL markers*/
  
  recinfo[0].type=FIELD_NORMAL;
  recinfo[0].length=1; /* For NULL bits */
  
  
  /* Define spatial column  */
  
  recinfo[1].type=FIELD_BLOB;
  recinfo[1].length=4 + portable_sizeof_char_ptr;
  
  
  
  /* Define a key with 1 spatial segment */
  
  keyinfo[0].seg=keyseg;
  keyinfo[0].keysegs=1;
  keyinfo[0].flag=HA_SPATIAL;
  keyinfo[0].key_alg=KEYALG;
  
  keyinfo[0].seg[0].type= HA_KEYTYPE_BINARY;
  keyinfo[0].seg[0].flag=0;
  keyinfo[0].seg[0].start= 1;
  keyinfo[0].seg[0].length=1; /* Spatial ignores it anyway */
  keyinfo[0].seg[0].null_bit= null_fields ? 2 : 0;
  keyinfo[0].seg[0].null_pos=0;
  keyinfo[0].seg[0].language=default_charset_info->number;
  keyinfo[0].seg[0].bit_start=4; /* Long BLOB */
  
  
  if (!silent)
    printf("- Creating isam-file\n");
  
  memset(&create_info, 0, sizeof(create_info));
  create_info.max_rows=10000000;
  
  if (mi_create(filename,
                1,            /*  keys   */
                keyinfo,
                2, /* columns */
                recinfo,uniques,&uniquedef,&create_info,create_flag))
    goto err;
  
  if (!silent)
    printf("- Open isam-file\n");
  
  if (!(file=mi_open(filename,2,HA_OPEN_ABORT_IF_LOCKED)))
    goto err;
  
  if (!silent)
    printf("- Writing key:s\n");
  
  for (i=0; i<nrecords; i++ )
  {
    create_linestring(record,i);
    error=mi_write(file,record);
    print_record(record,mi_position(file),"\n");
    if (!error)
    {
      row_count++;
    }
    else
    {
      printf("mi_write: %d\n", error);
      goto err;
    }
  }

  if ((error=read_with_pos(file,silent)))
    goto err;

  if (!silent)
    printf("- Deleting rows with position\n");
  for (i=0; i < nrecords/4; i++)
  {
    my_errno=0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error=mi_rrnd(file,read_record,i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error)
    {
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n",i,error,my_errno);
      goto err;
    }
    print_record(read_record,mi_position(file),"\n");
    error=mi_delete(file,read_record);
    if (error)
    {
      printf("pos: %2d mi_delete: %3d errno: %3d\n",i,error,my_errno);
      goto err;
    }
  }

  if (!silent)
    printf("- Updating rows with position\n");
  for (i=0; i < nrecords/2 ; i++)
  {
    my_errno=0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error=mi_rrnd(file,read_record,i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error)
    {
      if (error==HA_ERR_RECORD_DELETED)
        continue;
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n",i,error,my_errno);
      goto err;
    }
    print_record(read_record,mi_position(file),"");
    create_linestring(record,i+nrecords*upd);
    printf("\t-> ");
    print_record(record,mi_position(file),"\n");
    error=mi_update(file,read_record,record);
    if (error)
    {
      printf("pos: %2d  mi_update: %3d  errno: %3d\n",i,error,my_errno);
      goto err;
    }
  }

  if ((error=read_with_pos(file,silent)))
    goto err;

  if (!silent)
    printf("- Test mi_rkey then a sequence of mi_rnext_same\n");
  
  create_key(key, nrecords*4/5);
  print_key(key,"  search for INTERSECT\n");
  
  if ((error=mi_rkey(file,read_record,0,key,0,HA_READ_MBR_INTERSECT)))
  {
    printf("mi_rkey: %3d  errno: %3d\n",error,my_errno);
    goto err;
  }
  print_record(read_record,mi_position(file),"  mi_rkey\n");
  row_count=1;

  for (;;)
  {
    if ((error=mi_rnext_same(file,read_record)))
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      printf("mi_next: %3d  errno: %3d\n",error,my_errno);
      goto err;
    }
    print_record(read_record,mi_position(file),"  mi_rnext_same\n");
      row_count++;
  }
  printf("     %d rows\n",row_count);

  if (!silent)
    printf("- Test mi_rfirst then a sequence of mi_rnext\n");

  error=mi_rfirst(file,read_record,0);
  if (error)
  {
    printf("mi_rfirst: %3d  errno: %3d\n",error,my_errno);
    goto err;
  }
  row_count=1;
  print_record(read_record,mi_position(file),"  mi_frirst\n");
  
  for(i=0;i<nrecords;i++) {
    if ((error=mi_rnext(file,read_record,0)))
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      printf("mi_next: %3d  errno: %3d\n",error,my_errno);
      goto err;
    }
    print_record(read_record,mi_position(file),"  mi_rnext\n");
    row_count++;
  }
  printf("     %d rows\n",row_count);

  if (!silent)
    printf("- Test mi_records_in_range()\n");

  create_key(key, nrecords*upd);
  print_key(key," INTERSECT\n");
  min_range.key= key;
  min_range.length= 1000;                       /* Big enough */
  min_range.flag= HA_READ_MBR_INTERSECT;
  max_range.key= record+1;
  max_range.length= 1000;                       /* Big enough */
  max_range.flag= HA_READ_KEY_EXACT;
  hrows= mi_records_in_range(file, 0, &min_range, &max_range);
  printf("     %ld rows\n", (long) hrows);

  if (mi_close(file)) goto err;
  my_end(MY_CHECK_ERROR);
  return 0;
  
err:
  printf("got error: %3d when using myisam-database\n",my_errno);
  return 1;           /* skip warning */
}


static int read_with_pos (MI_INFO * file,int silent)
{
  int error;
  int i;
  uchar read_record[MAX_REC_LENGTH];
  int rows=0;

  if (!silent)
    printf("- Reading rows with position\n");
  for (i=0;;i++)
  {
    my_errno=0;
    memset(read_record, 0, MAX_REC_LENGTH);
    error=mi_rrnd(file,read_record,i == 0 ? 0L : HA_OFFSET_ERROR);
    if (error)
    {
      if (error==HA_ERR_END_OF_FILE)
        break;
      if (error==HA_ERR_RECORD_DELETED)
        continue;
      printf("pos: %2d  mi_rrnd: %3d  errno: %3d\n",i,error,my_errno);
      return error;
    }
    rows++;
    print_record(read_record,mi_position(file),"\n");
  }
  printf("     %d rows\n",rows);
  return 0;
}


static void print_record(uchar * record, my_off_t offs,const char * tail)
{
  uchar *pos;
  char *ptr;
  uint len;
  
  printf("     rec=(%d)",(unsigned char)record[0]);
  pos=record+1;
  len=sint4korr(pos);
  pos+=4;
  printf(" len=%d ",len);
  memcpy(&ptr, pos, sizeof(char*));
  if (ptr)
    rtree_PrintWKB((uchar*) ptr,SPDIMS);
  else
    printf("<NULL> ");
  printf(" offs=%ld ",(long int)offs);
  printf("%s",tail);
}


static void create_linestring(uchar *record,uint rownr)
{
   uint tmp;
   char *ptr;
   uchar *pos= record;
   double x[200];
   int i,j;
   int npoints=2;

   for(j=0;j<npoints;j++)
     for(i=0;i<SPDIMS;i++)
       x[i+j*SPDIMS]=rownr*j;

   memset(record, 0, MAX_REC_LENGTH);
   *pos=0x01; /* DEL marker */
   pos++;

   memset(blob_key,0,sizeof(blob_key));
   tmp=rtree_CreateLineStringWKB(x,SPDIMS,npoints, (uchar*) blob_key);

   int4store(pos,tmp);
   pos+=4;

   ptr=blob_key;
   memcpy(pos, &ptr, sizeof(char*));
}


static void create_key(uchar *key,uint rownr)
{
   double c=rownr;
   uchar *pos;
   uint i;

   memset(key, 0, MAX_REC_LENGTH);
   for (pos=key, i=0; i<2*SPDIMS; i++)
   {
     float8store(pos,c);
     pos+=sizeof(c);
   }
}

static void print_key(const uchar *key,const char * tail)
{
  double c;
  uint i;
  
  printf("     key=");
  for (i=0; i<2*SPDIMS; i++)
  {
    float8get(c,key);
    key+=sizeof(c);
    printf("%.14g ",c);
  }
  printf("%s",tail);
}


static int rtree_CreateLineStringWKB(double *ords, uint n_dims, uint n_points,
				     uchar *wkb)
{
  uint i;
  uint n_ords = n_dims * n_points;

  *wkb = wkbXDR;
  ++wkb;
  int4store(wkb, wkbLineString);
  wkb += 4;
  int4store(wkb, n_points);
  wkb += 4;
  for (i=0; i < n_ords; ++i)
  {
    float8store(wkb, ords[i]);
    wkb += 8;
  }
  return 9 + n_points * n_dims * 8;
}


static void rtree_PrintWKB(uchar *wkb, uint n_dims)
{
  uint wkb_type;

  ++wkb;
  wkb_type = uint4korr(wkb);
  wkb += 4;

  switch ((enum wkbType)wkb_type)
  {
    case wkbPoint:
    {
      uint i;
      double ord;

      printf("POINT(");
      for (i=0; i < n_dims; ++i)
      {
        float8get(ord, wkb);
        wkb += 8;
        printf("%.14g", ord);
        if (i < n_dims - 1)
          printf(" ");
        else
          printf(")");
      }
      break;
    }
    case wkbLineString:
    {
      uint p, i;
      uint n_points;
      double ord;

      printf("LineString(");
      n_points = uint4korr(wkb);
      wkb += 4;
      for (p=0; p < n_points; ++p)
      {
        for (i=0; i < n_dims; ++i)
        {
          float8get(ord, wkb);
          wkb += 8;
          printf("%.14g", ord);
          if (i < n_dims - 1)
            printf(" ");
        }
        if (p < n_points - 1)
          printf(", ");
        else
          printf(")");
      }
      break;
    }
    case wkbPolygon:
    {
      printf("POLYGON(...)");
      break;
    }
    case wkbMultiPoint:
    {
      printf("MULTIPOINT(...)");
      break;
    }
    case wkbMultiLineString:
    {
      printf("MULTILINESTRING(...)");
      break;
    }
    case wkbMultiPolygon:
    {
      printf("MULTIPOLYGON(...)");
      break;
    }
    case wkbGeometryCollection:
    {
      printf("GEOMETRYCOLLECTION(...)");
      break;
    }
    default:
    {
      printf("UNKNOWN GEOMETRY TYPE");
      break;
    }
  }
}

#else
int main(int argc __attribute__((unused)),char *argv[] __attribute__((unused)))
{
  exit(0);
}
#endif /*HAVE_SPATIAL*/

#include "mi_extrafunc.h"
