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

	/* Functions to compressed records */

#include "fulltext.h"

#define IS_CHAR ((uint) 32768)		/* Bit if char (not offset) in tree */

#if INT_MAX > 65536L
#define BITS_SAVED 32
#define MAX_QUICK_TABLE_BITS 9		/* Because we may shift in 24 bits */
#else
#define BITS_SAVED 16
#define MAX_QUICK_TABLE_BITS 6
#endif

#define get_bit(BU) ((BU)->bits ? \
		     (BU)->current_byte & ((mi_bit_type) 1 << --(BU)->bits) :\
		     (fill_buffer(BU), (BU)->bits= BITS_SAVED-1,\
		      (BU)->current_byte & ((mi_bit_type) 1 << (BITS_SAVED-1))))
#define skip_to_next_byte(BU) ((BU)->bits&=~7)
#define get_bits(BU,count) (((BU)->bits >= count) ? (((BU)->current_byte >> ((BU)->bits-=count)) & mask[count]) : fill_and_get_bits(BU,count))

#define decode_bytes_test_bit(bit) \
  if (low_byte & (1 << (7-bit))) \
    pos++; \
  if (*pos & IS_CHAR) \
  { bits-=(bit+1); break; } \
  pos+= *pos

#define OFFSET_TABLE_SIZE 512

static uint read_huff_table(MI_BIT_BUFF *bit_buff,MI_DECODE_TREE *decode_tree,
			    uint16 **decode_table,byte **intervall_buff,
			    uint16 *tmp_buff);
static void make_quick_table(uint16 *to_table,uint16 *decode_table,
			     uint *next_free,uint value,uint bits,
			     uint max_bits);
static void fill_quick_table(uint16 *table,uint bits, uint max_bits,
			     uint value);
static uint copy_decode_table(uint16 *to_pos,uint offset,
			      uint16 *decode_table);
static uint find_longest_bitstream(uint16 *table, uint16 *end);
static void (*get_unpack_function(MI_COLUMNDEF *rec))(MI_COLUMNDEF *field,
						    MI_BIT_BUFF *buff,
						    uchar *to,
						    uchar *end);
static void uf_zerofill_skip_zero(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
				   uchar *to,uchar *end);
static void uf_skip_zero(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			  uchar *to,uchar *end);
static void uf_space_normal(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			    uchar *to,uchar *end);
static void uf_space_endspace_selected(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end);
static void uf_endspace_selected(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
				 uchar *to,uchar *end);
static void uf_space_endspace(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			      uchar *to,uchar *end);
static void uf_endspace(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			uchar *to,uchar *end);
static void uf_space_prespace_selected(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end);
static void uf_prespace_selected(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
				 uchar *to,uchar *end);
static void uf_space_prespace(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			      uchar *to,uchar *end);
static void uf_prespace(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			uchar *to,uchar *end);
static void uf_zerofill_normal(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			       uchar *to,uchar *end);
static void uf_constant(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			uchar *to,uchar *end);
static void uf_intervall(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			 uchar *to,uchar *end);
static void uf_zero(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
		    uchar *to,uchar *end);
static void uf_blob(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		    uchar *to, uchar *end);
static void uf_varchar(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		       uchar *to, uchar *end);
static void decode_bytes(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			 uchar *to,uchar *end);
static uint decode_pos(MI_BIT_BUFF *bit_buff,MI_DECODE_TREE *decode_tree);
static void init_bit_buffer(MI_BIT_BUFF *bit_buff,uchar *buffer,uint length);
static uint fill_and_get_bits(MI_BIT_BUFF *bit_buff,uint count);
static void fill_buffer(MI_BIT_BUFF *bit_buff);
static uint max_bit(uint value);
#ifdef HAVE_MMAP
static uchar *_mi_mempack_get_block_info(MI_INFO *myisam,MI_BLOCK_INFO *info,
					 uchar *header);
#endif

static mi_bit_type mask[]=
{
   0x00000000,
   0x00000001, 0x00000003, 0x00000007, 0x0000000f,
   0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
   0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
   0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
#if BITS_SAVED > 16
   0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
   0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
   0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
   0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff,
#endif
 };


	/* Read all packed info, allocate memory and fix field structs */

my_bool _mi_read_pack_info(MI_INFO *info, pbool fix_keys)
{
  File file;
  int diff_length;
  uint i,trees,huff_tree_bits,rec_reflength,length;
  uint16 *decode_table,*tmp_buff;
  ulong elements,intervall_length;
  char *disk_cache,*intervall_buff;
  uchar header[32];
  MYISAM_SHARE *share=info->s;
  MI_BIT_BUFF bit_buff;
  DBUG_ENTER("_mi_read_pack_info");

  if (myisam_quick_table_bits < 4)
    myisam_quick_table_bits=4;
  else if (myisam_quick_table_bits > MAX_QUICK_TABLE_BITS)
    myisam_quick_table_bits=MAX_QUICK_TABLE_BITS;

  file=info->dfile;
  my_errno=0;
  if (my_read(file,(byte*) header,sizeof(header),MYF(MY_NABP)))
  {
    if (!my_errno)
      my_errno=HA_ERR_END_OF_FILE;
    goto err0;
  }
  if (memcmp((byte*) header, (byte*) myisam_pack_file_magic, 3))
  {
    my_errno=HA_ERR_WRONG_IN_RECORD;
    goto err0;
  }
  share->pack.version= header[3];
  share->pack.header_length=	uint4korr(header+4);
  share->min_pack_length=(uint) uint4korr(header+8);
  share->max_pack_length=(uint) uint4korr(header+12);
  set_if_bigger(share->base.pack_reclength,share->max_pack_length);
  elements=uint4korr(header+16);
  intervall_length=uint4korr(header+20);
  trees=uint2korr(header+24);
  share->pack.ref_length=header[26];
  rec_reflength=header[27];
  diff_length=(int) rec_reflength - (int) share->base.rec_reflength;
  if (fix_keys)
    share->rec_reflength=rec_reflength;
  share->base.min_block_length=share->min_pack_length+1;
  if (share->min_pack_length > 254)
    share->base.min_block_length+=2;

  if (!(share->decode_trees=(MI_DECODE_TREE*)
	my_malloc((uint) (trees*sizeof(MI_DECODE_TREE)+
			  intervall_length*sizeof(byte)),
		  MYF(MY_WME))))
    goto err0;
  intervall_buff=(byte*) (share->decode_trees+trees);

  length=(uint) (elements*2+trees*(1 << myisam_quick_table_bits));
  if (!(share->decode_tables=(uint16*)
	my_malloc((length+OFFSET_TABLE_SIZE)*sizeof(uint16)+
		  (uint) (share->pack.header_length+7),
		  MYF(MY_WME | MY_ZEROFILL))))
    goto err1;
  tmp_buff=share->decode_tables+length;
  disk_cache=(byte*) (tmp_buff+OFFSET_TABLE_SIZE);

  if (my_read(file,disk_cache,
	      (uint) (share->pack.header_length-sizeof(header)),
	      MYF(MY_NABP)))
    goto err2;

  huff_tree_bits=max_bit(trees ? trees-1 : 0);
  init_bit_buffer(&bit_buff, (uchar*) disk_cache,
		  (uint) (share->pack.header_length-sizeof(header)));
	/* Read new info for each field */
  for (i=0 ; i < share->base.fields ; i++)
  {
    share->rec[i].base_type=(enum en_fieldtype) get_bits(&bit_buff,5);
    share->rec[i].pack_type=(uint) get_bits(&bit_buff,6);
    share->rec[i].space_length_bits=get_bits(&bit_buff,5);
    share->rec[i].huff_tree=share->decode_trees+(uint) get_bits(&bit_buff,
								huff_tree_bits);
    share->rec[i].unpack=get_unpack_function(share->rec+i);
  }
  skip_to_next_byte(&bit_buff);
  decode_table=share->decode_tables;
  for (i=0 ; i < trees ; i++)
    if (read_huff_table(&bit_buff,share->decode_trees+i,&decode_table,
                        &intervall_buff,tmp_buff))
      goto err3;
  decode_table=(uint16*)
    my_realloc((gptr) share->decode_tables,
	       (uint) ((byte*) decode_table - (byte*) share->decode_tables),
	       MYF(MY_HOLD_ON_ERROR));
  {
    long diff=PTR_BYTE_DIFF(decode_table,share->decode_tables);
    share->decode_tables=decode_table;
    for (i=0 ; i < trees ; i++)
      share->decode_trees[i].table=ADD_TO_PTR(share->decode_trees[i].table,
                                              diff, uint16*);
  }

	/* Fix record-ref-length for keys */
  if (fix_keys)
  {
    for (i=0 ; i < share->base.keys ; i++)
    {
      MI_KEYDEF *keyinfo= &share->keyinfo[i];
      keyinfo->keylength+= (uint16) diff_length;
      keyinfo->minlength+= (uint16) diff_length;
      keyinfo->maxlength+= (uint16) diff_length;
      keyinfo->seg[keyinfo->flag & HA_FULLTEXT ?
                   FT_SEGS : keyinfo->keysegs].length= (uint16) rec_reflength;
    }
    if (share->ft2_keyinfo.seg)
    {
      MI_KEYDEF *ft2_keyinfo= &share->ft2_keyinfo;
      ft2_keyinfo->keylength+= (uint16) diff_length;
      ft2_keyinfo->minlength+= (uint16) diff_length;
      ft2_keyinfo->maxlength+= (uint16) diff_length;
    }
  }

  if (bit_buff.error || bit_buff.pos < bit_buff.end)
    goto err3;

  DBUG_RETURN(0);

err3:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err2:
  my_free((gptr) share->decode_tables,MYF(0));
err1:
  my_free((gptr) share->decode_trees,MYF(0));
err0:
  DBUG_RETURN(1);
}


	/* Read on huff-code-table from datafile */

static uint read_huff_table(MI_BIT_BUFF *bit_buff, MI_DECODE_TREE *decode_tree,
			    uint16 **decode_table, byte **intervall_buff,
			    uint16 *tmp_buff)
{
  uint min_chr,elements,char_bits,offset_bits,size,intervall_length,table_bits,
  next_free_offset;
  uint16 *ptr,*end;

  LINT_INIT(ptr);
  if (!get_bits(bit_buff,1))
  {
    min_chr=get_bits(bit_buff,8);
    elements=get_bits(bit_buff,9);
    char_bits=get_bits(bit_buff,5);
    offset_bits=get_bits(bit_buff,5);
    intervall_length=0;
    ptr=tmp_buff;
  }
  else
  {
    min_chr=0;
    elements=get_bits(bit_buff,15);
    intervall_length=get_bits(bit_buff,16);
    char_bits=get_bits(bit_buff,5);
    offset_bits=get_bits(bit_buff,5);
    decode_tree->quick_table_bits=0;
    ptr= *decode_table;
  }
  size=elements*2-2;

  for (end=ptr+size ; ptr < end ; ptr++)
  {
    if (get_bit(bit_buff))
      *ptr= (uint16) get_bits(bit_buff,offset_bits);
    else
      *ptr= (uint16) (IS_CHAR + (get_bits(bit_buff,char_bits) + min_chr));
  }
  skip_to_next_byte(bit_buff);

  decode_tree->table= *decode_table;
  decode_tree->intervalls= *intervall_buff;
  if (! intervall_length)
  {
    table_bits=find_longest_bitstream(tmp_buff, tmp_buff+OFFSET_TABLE_SIZE);
    if (table_bits == (uint) ~0)
      return 1;
    if (table_bits > myisam_quick_table_bits)
      table_bits=myisam_quick_table_bits;
    next_free_offset= (1 << table_bits);
    make_quick_table(*decode_table,tmp_buff,&next_free_offset,0,table_bits,
		     table_bits);
    (*decode_table)+= next_free_offset;
    decode_tree->quick_table_bits=table_bits;
  }
  else
  {
    (*decode_table)=end;
    bit_buff->pos-= bit_buff->bits/8;
    memcpy(*intervall_buff,bit_buff->pos,(size_t) intervall_length);
    (*intervall_buff)+=intervall_length;
    bit_buff->pos+=intervall_length;
    bit_buff->bits=0;
  }
  return 0;
}


static void make_quick_table(uint16 *to_table, uint16 *decode_table,
			     uint *next_free_offset, uint value, uint bits,
			     uint max_bits)
{
  if (!bits--)
  {
    to_table[value]= (uint16) *next_free_offset;
    *next_free_offset=copy_decode_table(to_table, *next_free_offset,
					decode_table);
    return;
  }
  if (!(*decode_table & IS_CHAR))
  {
    make_quick_table(to_table,decode_table+ *decode_table,
		     next_free_offset,value,bits,max_bits);
  }
  else
    fill_quick_table(to_table+value,bits,max_bits,(uint) *decode_table);
  decode_table++;
  value|= (1 << bits);
  if (!(*decode_table & IS_CHAR))
  {
    make_quick_table(to_table,decode_table+ *decode_table,
		     next_free_offset,value,bits,max_bits);
  }
  else
    fill_quick_table(to_table+value,bits,max_bits,(uint) *decode_table);
  return;
}


static void fill_quick_table(uint16 *table, uint bits, uint max_bits,
			     uint value)
{
  uint16 *end;
  value|=(max_bits-bits) << 8;
  for (end=table+ (1 << bits) ;
       table < end ;
       *table++ = (uint16) value | IS_CHAR) ;
}


static uint copy_decode_table(uint16 *to_pos, uint offset,
			      uint16 *decode_table)
{
  uint prev_offset;
  prev_offset= offset;

  if (!(*decode_table & IS_CHAR))
  {
    to_pos[offset]=2;
    offset=copy_decode_table(to_pos,offset+2,decode_table+ *decode_table);
  }
  else
  {
    to_pos[offset]= *decode_table;
    offset+=2;
  }
  decode_table++;

  if (!(*decode_table & IS_CHAR))
  {
    to_pos[prev_offset+1]=(uint16) (offset-prev_offset-1);
    offset=copy_decode_table(to_pos,offset,decode_table+ *decode_table);
  }
  else
    to_pos[prev_offset+1]= *decode_table;
  return offset;
}


static uint find_longest_bitstream(uint16 *table, uint16 *end)
{
  uint length=1,length2;
  if (!(*table & IS_CHAR))
  {
    uint16 *next= table + *table;
    if (next > end || next == table)
      return ~0;
    length=find_longest_bitstream(next, end)+1;
  }
  table++;
  if (!(*table & IS_CHAR))
  {
    uint16 *next= table + *table;
    if (next > end || next == table)
      return ~0;
    length2=find_longest_bitstream(table+ *table, end)+1;
    length=max(length,length2);
  }
  return length;
}


	/* Read record from datafile */
	/* Returns length of packed record, -1 if error */

int _mi_read_pack_record(MI_INFO *info, my_off_t filepos, byte *buf)
{
  MI_BLOCK_INFO block_info;
  File file;
  DBUG_ENTER("mi_read_pack_record");

  if (filepos == HA_OFFSET_ERROR)
    DBUG_RETURN(-1);			/* _search() didn't find record */

  file=info->dfile;
  if (_mi_pack_get_block_info(info, &block_info, file, filepos))
    goto err;
  if (my_read(file,(byte*) info->rec_buff + block_info.offset ,
	      block_info.rec_len - block_info.offset, MYF(MY_NABP)))
    goto panic;
  info->update|= HA_STATE_AKTIV;
  DBUG_RETURN(_mi_pack_rec_unpack(info,buf,info->rec_buff,block_info.rec_len));
panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err:
  DBUG_RETURN(-1);
}



int _mi_pack_rec_unpack(register MI_INFO *info, register byte *to, byte *from,
			ulong reclength)
{
  byte *end_field;
  reg3 MI_COLUMNDEF *end;
  MI_COLUMNDEF *current_field;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_pack_rec_unpack");

  init_bit_buffer(&info->bit_buff, (uchar*) from,reclength);

  for (current_field=share->rec, end=current_field+share->base.fields ;
       current_field < end ;
       current_field++,to=end_field)
  {
    end_field=to+current_field->length;
    (*current_field->unpack)(current_field,&info->bit_buff,(uchar*) to,
			     (uchar*) end_field);
  }
  if (! info->bit_buff.error &&
      info->bit_buff.pos - info->bit_buff.bits/8 == info->bit_buff.end)
    DBUG_RETURN(0);
  info->update&= ~HA_STATE_AKTIV;
  DBUG_RETURN(my_errno=HA_ERR_WRONG_IN_RECORD);
} /* _mi_pack_rec_unpack */


	/* Return function to unpack field */

static void (*get_unpack_function(MI_COLUMNDEF *rec))
(MI_COLUMNDEF *, MI_BIT_BUFF *, uchar *, uchar *)
{
  switch (rec->base_type) {
  case FIELD_SKIP_ZERO:
    if (rec->pack_type & PACK_TYPE_ZERO_FILL)
      return &uf_zerofill_skip_zero;
    return &uf_skip_zero;
  case FIELD_NORMAL:
    if (rec->pack_type & PACK_TYPE_SPACE_FIELDS)
      return &uf_space_normal;
    if (rec->pack_type & PACK_TYPE_ZERO_FILL)
      return &uf_zerofill_normal;
    return &decode_bytes;
  case FIELD_SKIP_ENDSPACE:
    if (rec->pack_type & PACK_TYPE_SPACE_FIELDS)
    {
      if (rec->pack_type & PACK_TYPE_SELECTED)
	return &uf_space_endspace_selected;
      return &uf_space_endspace;
    }
    if (rec->pack_type & PACK_TYPE_SELECTED)
      return &uf_endspace_selected;
    return &uf_endspace;
  case FIELD_SKIP_PRESPACE:
    if (rec->pack_type & PACK_TYPE_SPACE_FIELDS)
    {
      if (rec->pack_type & PACK_TYPE_SELECTED)
	return &uf_space_prespace_selected;
      return &uf_space_prespace;
    }
    if (rec->pack_type & PACK_TYPE_SELECTED)
      return &uf_prespace_selected;
    return &uf_prespace;
  case FIELD_CONSTANT:
    return &uf_constant;
  case FIELD_INTERVALL:
    return &uf_intervall;
  case FIELD_ZERO:
  case FIELD_CHECK:
    return &uf_zero;
  case FIELD_BLOB:
    return &uf_blob;
  case FIELD_VARCHAR:
    return &uf_varchar;
  case FIELD_LAST:
  default:
    return 0;			/* This should never happend */
  }
}

	/* De different functions to unpack a field */

static void uf_zerofill_skip_zero(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				   uchar *to, uchar *end)
{
  if (get_bit(bit_buff))
    bzero((char*) to,(uint) (end-to));
  else
  {
    end-=rec->space_length_bits;
    decode_bytes(rec,bit_buff,to,end);
    bzero((char*) end,rec->space_length_bits);
  }
}

static void uf_skip_zero(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			  uchar *end)
{
  if (get_bit(bit_buff))
    bzero((char*) to,(uint) (end-to));
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_normal(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			    uchar *end)
{
  if (get_bit(bit_buff))
    bfill((byte*) to,(end-to),' ');
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_endspace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    bfill((byte*) to,(end-to),' ');
  else
  {
    if (get_bit(bit_buff))
    {
      if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
      {
	bit_buff->error=1;
	return;
      }
      if (to+spaces != end)
	decode_bytes(rec,bit_buff,to,end-spaces);
      bfill((byte*) end-spaces,spaces,' ');
    }
    else
      decode_bytes(rec,bit_buff,to,end);
  }
}

static void uf_endspace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				 uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    if (to+spaces != end)
      decode_bytes(rec,bit_buff,to,end-spaces);
    bfill((byte*) end-spaces,spaces,' ');
  }
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_endspace(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			      uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    bfill((byte*) to,(end-to),' ');
  else
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    if (to+spaces != end)
      decode_bytes(rec,bit_buff,to,end-spaces);
    bfill((byte*) end-spaces,spaces,' ');
  }
}

static void uf_endspace(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			uchar *end)
{
  uint spaces;
  if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
  {
    bit_buff->error=1;
    return;
  }
  if (to+spaces != end)
    decode_bytes(rec,bit_buff,to,end-spaces);
  bfill((byte*) end-spaces,spaces,' ');
}

static void uf_space_prespace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    bfill((byte*) to,(end-to),' ');
  else
  {
    if (get_bit(bit_buff))
    {
      if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
      {
	bit_buff->error=1;
	return;
      }
      bfill((byte*) to,spaces,' ');
      if (to+spaces != end)
	decode_bytes(rec,bit_buff,to+spaces,end);
    }
    else
      decode_bytes(rec,bit_buff,to,end);
  }
}


static void uf_prespace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				 uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    bfill((byte*) to,spaces,' ');
    if (to+spaces != end)
      decode_bytes(rec,bit_buff,to+spaces,end);
  }
  else
    decode_bytes(rec,bit_buff,to,end);
}


static void uf_space_prespace(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			      uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    bfill((byte*) to,(end-to),' ');
  else
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    bfill((byte*) to,spaces,' ');
    if (to+spaces != end)
      decode_bytes(rec,bit_buff,to+spaces,end);
  }
}

static void uf_prespace(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			uchar *end)
{
  uint spaces;
  if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
  {
    bit_buff->error=1;
    return;
  }
  bfill((byte*) to,spaces,' ');
  if (to+spaces != end)
    decode_bytes(rec,bit_buff,to+spaces,end);
}

static void uf_zerofill_normal(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			       uchar *end)
{
  end-=rec->space_length_bits;
  decode_bytes(rec,bit_buff,(uchar*) to,end);
  bzero((char*) end,rec->space_length_bits);
}

static void uf_constant(MI_COLUMNDEF *rec,
			MI_BIT_BUFF *bit_buff __attribute__((unused)),
			uchar *to,
			uchar *end)
{
  memcpy(to,rec->huff_tree->intervalls,(size_t) (end-to));
}

static void uf_intervall(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			 uchar *end)
{
  reg1 uint field_length=(uint) (end-to);
  memcpy(to,rec->huff_tree->intervalls+field_length*decode_pos(bit_buff,
							       rec->huff_tree),
	 (size_t) field_length);
}


/*ARGSUSED*/
static void uf_zero(MI_COLUMNDEF *rec __attribute__((unused)),
		    MI_BIT_BUFF *bit_buff __attribute__((unused)),
		    uchar *to, uchar *end)
{
  bzero((char*) to,(uint) (end-to));
}

static void uf_blob(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		    uchar *to, uchar *end)
{
  if (get_bit(bit_buff))
    bzero((byte*) to,(end-to));
  else
  {
    ulong length=get_bits(bit_buff,rec->space_length_bits);
    uint pack_length=(uint) (end-to)-mi_portable_sizeof_char_ptr;
    if (bit_buff->blob_pos+length > bit_buff->blob_end)
    {
      bit_buff->error=1;
      bzero((byte*) to,(end-to));
      return;
    }
    decode_bytes(rec,bit_buff,bit_buff->blob_pos,bit_buff->blob_pos+length);
    _my_store_blob_length((byte*) to,pack_length,length);
    memcpy_fixed((char*) to+pack_length,(char*) &bit_buff->blob_pos,
		 sizeof(char*));
    bit_buff->blob_pos+=length;
  }
}

static void uf_varchar(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		       uchar *to, uchar *end __attribute__((unused)))
{
  if (get_bit(bit_buff))
    to[0]=to[1]=0;				/* Zero lengths */
  else
  {
    ulong length=get_bits(bit_buff,rec->space_length_bits);
    int2store(to,length);
    decode_bytes(rec,bit_buff,to+2,to+2+length);
  }
}

	/* Functions to decode of buffer of bits */

#if BITS_SAVED == 64

static void decode_bytes(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,uchar *to,
			 uchar *end)
{
  reg1 uint bits,low_byte;
  reg3 uint16 *pos;
  reg4 uint table_bits,table_and;
  MI_DECODE_TREE *decode_tree;

  decode_tree=rec->decode_tree;
  bits=bit_buff->bits;			/* Save in reg for quicker access */
  table_bits=decode_tree->quick_table_bits;
  table_and= (1 << table_bits)-1;

  do
  {
    if (bits <= 32)
    {
      if (bit_buff->pos > bit_buff->end+4)
      {
	bit_buff->error=1;
	return;				/* Can't be right */
      }
      bit_buff->current_byte= (bit_buff->current_byte << 32) +
	((((uint) bit_buff->pos[3])) +
	 (((uint) bit_buff->pos[2]) << 8) +
	 (((uint) bit_buff->pos[1]) << 16) +
	 (((uint) bit_buff->pos[0]) << 24));
      bit_buff->pos+=4;
      bits+=32;
    }
	/* First use info in quick_table */
    low_byte=(uint) (bit_buff->current_byte >> (bits - table_bits)) & table_and;
    low_byte=decode_tree->table[low_byte];
    if (low_byte & IS_CHAR)
    {
      *to++ = (low_byte & 255);		/* Found char in quick table */
      bits-=  ((low_byte >> 8) & 31);	/* Remove bits used */
    }
    else
    {					/* Map through rest of decode-table */
      pos=decode_tree->table+low_byte;
      bits-=table_bits;
      for (;;)
      {
	low_byte=(uint) (bit_buff->current_byte >> (bits-8));
	decode_bytes_test_bit(0);
	decode_bytes_test_bit(1);
	decode_bytes_test_bit(2);
	decode_bytes_test_bit(3);
	decode_bytes_test_bit(4);
	decode_bytes_test_bit(5);
	decode_bytes_test_bit(6);
	decode_bytes_test_bit(7);
	bits-=8;
      }
      *to++ = *pos;
    }
  } while (to != end);

  bit_buff->bits=bits;
  return;
}

#else

static void decode_bytes(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			 uchar *end)
{
  reg1 uint bits,low_byte;
  reg3 uint16 *pos;
  reg4 uint table_bits,table_and;
  MI_DECODE_TREE *decode_tree;

  decode_tree=rec->huff_tree;
  bits=bit_buff->bits;			/* Save in reg for quicker access */
  table_bits=decode_tree->quick_table_bits;
  table_and= (1 << table_bits)-1;

  do
  {
    if (bits < table_bits)
    {
      if (bit_buff->pos > bit_buff->end+1)
      {
	bit_buff->error=1;
	return;				/* Can't be right */
      }
#if BITS_SAVED == 32
      bit_buff->current_byte= (bit_buff->current_byte << 24) +
	(((uint) ((uchar) bit_buff->pos[2]))) +
	  (((uint) ((uchar) bit_buff->pos[1])) << 8) +
	    (((uint) ((uchar) bit_buff->pos[0])) << 16);
      bit_buff->pos+=3;
      bits+=24;
#else
      if (bits)				/* We must have at leasts 9 bits */
      {
	bit_buff->current_byte=  (bit_buff->current_byte << 8) +
	  (uint) ((uchar) bit_buff->pos[0]);
	bit_buff->pos++;
	bits+=8;
      }
      else
      {
	bit_buff->current_byte= ((uint) ((uchar) bit_buff->pos[0]) << 8) +
	  ((uint) ((uchar) bit_buff->pos[1]));
	bit_buff->pos+=2;
	bits+=16;
      }
#endif
    }
	/* First use info in quick_table */
    low_byte=(bit_buff->current_byte >> (bits - table_bits)) & table_and;
    low_byte=decode_tree->table[low_byte];
    if (low_byte & IS_CHAR)
    {
      *to++ = (low_byte & 255);		/* Found char in quick table */
      bits-=  ((low_byte >> 8) & 31);	/* Remove bits used */
    }
    else
    {					/* Map through rest of decode-table */
      pos=decode_tree->table+low_byte;
      bits-=table_bits;
      for (;;)
      {
	if (bits < 8)
	{				/* We don't need to check end */
#if BITS_SAVED == 32
	  bit_buff->current_byte= (bit_buff->current_byte << 24) +
	    (((uint) ((uchar) bit_buff->pos[2]))) +
	      (((uint) ((uchar) bit_buff->pos[1])) << 8) +
		(((uint) ((uchar) bit_buff->pos[0])) << 16);
	  bit_buff->pos+=3;
	  bits+=24;
#else
	  bit_buff->current_byte=  (bit_buff->current_byte << 8) +
	    (uint) ((uchar) bit_buff->pos[0]);
	  bit_buff->pos+=1;
	  bits+=8;
#endif
	}
	low_byte=(uint) (bit_buff->current_byte >> (bits-8));
	decode_bytes_test_bit(0);
	decode_bytes_test_bit(1);
	decode_bytes_test_bit(2);
	decode_bytes_test_bit(3);
	decode_bytes_test_bit(4);
	decode_bytes_test_bit(5);
	decode_bytes_test_bit(6);
	decode_bytes_test_bit(7);
	bits-=8;
      }
      *to++ = (uchar) *pos;
    }
  } while (to != end);

  bit_buff->bits=bits;
  return;
}
#endif /* BIT_SAVED == 64 */


static uint decode_pos(MI_BIT_BUFF *bit_buff, MI_DECODE_TREE *decode_tree)
{
  uint16 *pos=decode_tree->table;
  for (;;)
  {
    if (get_bit(bit_buff))
      pos++;
    if (*pos & IS_CHAR)
      return (uint) (*pos & ~IS_CHAR);
    pos+= *pos;
  }
}


int _mi_read_rnd_pack_record(MI_INFO *info, byte *buf,
			     register my_off_t filepos,
			     my_bool skip_deleted_blocks)
{
  uint b_type;
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_read_rnd_pack_record");

  if (filepos >= info->state->data_file_length)
  {
    my_errno= HA_ERR_END_OF_FILE;
    goto err;
  }

  if (info->opt_flag & READ_CACHE_USED)
  {
    if (_mi_read_cache(&info->rec_cache,(byte*) block_info.header,filepos,
		      share->pack.ref_length, skip_deleted_blocks))
      goto err;
    b_type=_mi_pack_get_block_info(info,&block_info,-1, filepos);
  }
  else
    b_type=_mi_pack_get_block_info(info,&block_info,info->dfile,filepos);
  if (b_type)
    goto err;					/* Error code is already set */
#ifndef DBUG_OFF
  if (block_info.rec_len > share->max_pack_length)
  {
    my_errno=HA_ERR_WRONG_IN_RECORD;
    goto err;
  }
#endif

  if (info->opt_flag & READ_CACHE_USED)
  {
    if (_mi_read_cache(&info->rec_cache,(byte*) info->rec_buff,
		      block_info.filepos, block_info.rec_len,
		      skip_deleted_blocks))
      goto err;
  }
  else
  {
    if (my_read(info->dfile,(byte*) info->rec_buff + block_info.offset,
		block_info.rec_len-block_info.offset,
		MYF(MY_NABP)))
      goto err;
  }
  info->packed_length=block_info.rec_len;
  info->lastpos=filepos;
  info->nextpos=block_info.filepos+block_info.rec_len;
  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;

  DBUG_RETURN (_mi_pack_rec_unpack(info,buf,info->rec_buff,
				   block_info.rec_len));
 err:
  DBUG_RETURN(my_errno);
}


	/* Read and process header from a huff-record-file */

uint _mi_pack_get_block_info(MI_INFO *myisam, MI_BLOCK_INFO *info, File file,
			     my_off_t filepos)
{
  uchar *header=info->header;
  uint head_length,ref_length;
  LINT_INIT(ref_length);

  if (file >= 0)
  {
    ref_length=myisam->s->pack.ref_length;
    /*
      We can't use my_pread() here because mi_read_rnd_pack_record assumes
      position is ok
    */
    VOID(my_seek(file,filepos,MY_SEEK_SET,MYF(0)));
    if (my_read(file,(char*) header,ref_length,MYF(MY_NABP)))
      return BLOCK_FATAL_ERROR;
    DBUG_DUMP("header",(byte*) header,ref_length);
  }
  head_length= read_pack_length((uint) myisam->s->pack.version, header,
                                &info->rec_len);
  if (myisam->s->base.blobs)
  {
    head_length+= read_pack_length((uint) myisam->s->pack.version,
                                   header + head_length, &info->blob_len);
    if (!(mi_alloc_rec_buff(myisam,info->rec_len + info->blob_len,
			    &myisam->rec_buff)))
      return BLOCK_FATAL_ERROR;			/* not enough memory */
    myisam->bit_buff.blob_pos=(uchar*) myisam->rec_buff+info->rec_len;
    myisam->bit_buff.blob_end= myisam->bit_buff.blob_pos+info->blob_len;
    myisam->blob_length=info->blob_len;
  }
  info->filepos=filepos+head_length;
  if (file > 0)
  {
    info->offset=min(info->rec_len, ref_length - head_length);
    memcpy(myisam->rec_buff, header+head_length, info->offset);
  }
  return 0;
}


	/* rutines for bit buffer */
	/* Note buffer must be 6 byte bigger than longest row */

static void init_bit_buffer(MI_BIT_BUFF *bit_buff, uchar *buffer, uint length)
{
  bit_buff->pos=buffer;
  bit_buff->end=buffer+length;
  bit_buff->bits=bit_buff->error=0;
  bit_buff->current_byte=0;			/* Avoid purify errors */
}

static uint fill_and_get_bits(MI_BIT_BUFF *bit_buff, uint count)
{
  uint tmp;
  count-=bit_buff->bits;
  tmp=(bit_buff->current_byte & mask[bit_buff->bits]) << count;
  fill_buffer(bit_buff);
  bit_buff->bits=BITS_SAVED - count;
  return tmp+(bit_buff->current_byte >> (BITS_SAVED - count));
}

	/* Fill in empty bit_buff->current_byte from buffer */
	/* Sets bit_buff->error if buffer is exhausted */

static void fill_buffer(MI_BIT_BUFF *bit_buff)
{
  if (bit_buff->pos >= bit_buff->end)
  {
    bit_buff->error= 1;
    bit_buff->current_byte=0;
    return;
  }
#if BITS_SAVED == 64
  bit_buff->current_byte=  ((((uint) ((uchar) bit_buff->pos[7]))) +
			     (((uint) ((uchar) bit_buff->pos[6])) << 8) +
			     (((uint) ((uchar) bit_buff->pos[5])) << 16) +
			     (((uint) ((uchar) bit_buff->pos[4])) << 24) +
			     ((ulonglong)
			      ((((uint) ((uchar) bit_buff->pos[3]))) +
			       (((uint) ((uchar) bit_buff->pos[2])) << 8) +
			       (((uint) ((uchar) bit_buff->pos[1])) << 16) +
			       (((uint) ((uchar) bit_buff->pos[0])) << 24)) << 32));
  bit_buff->pos+=8;
#else
#if BITS_SAVED == 32
  bit_buff->current_byte=  (((uint) ((uchar) bit_buff->pos[3])) +
			     (((uint) ((uchar) bit_buff->pos[2])) << 8) +
			     (((uint) ((uchar) bit_buff->pos[1])) << 16) +
			     (((uint) ((uchar) bit_buff->pos[0])) << 24));
  bit_buff->pos+=4;
#else
  bit_buff->current_byte=  (uint) (((uint) ((uchar) bit_buff->pos[1]))+
				    (((uint) ((uchar) bit_buff->pos[0])) << 8));
  bit_buff->pos+=2;
#endif
#endif
}

	/* Get number of bits neaded to represent value */

static uint max_bit(register uint value)
{
  reg2 uint power=1;

  while ((value>>=1))
    power++;
  return (power);
}


/*****************************************************************************
	Some redefined functions to handle files when we are using memmap
*****************************************************************************/

#ifdef HAVE_MMAP

#include <sys/mman.h>

static int _mi_read_mempack_record(MI_INFO *info,my_off_t filepos,byte *buf);
static int _mi_read_rnd_mempack_record(MI_INFO*, byte *,my_off_t, my_bool);

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0		/* For irix */
#endif
#ifndef MAP_FAILED
#define MAP_FAILED -1
#endif

my_bool _mi_memmap_file(MI_INFO *info)
{
  byte *file_map;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_memmap_file");

  if (!share->file_map)
  {
    my_off_t data_file_length= share->state.state.data_file_length;
    if (data_file_length > (my_off_t) (~((size_t) 0)) - MEMMAP_EXTRA_MARGIN)
    {
      DBUG_PRINT("warning", ("File is too large for mmap"));
      DBUG_RETURN(0);
    }
    if (my_seek(info->dfile,0L,MY_SEEK_END,MYF(0)) <
        data_file_length + MEMMAP_EXTRA_MARGIN)
    {
      DBUG_PRINT("warning",("File isn't extended for memmap"));
      DBUG_RETURN(0);
    }
    file_map=(byte*)
      mmap(0, data_file_length + MEMMAP_EXTRA_MARGIN, PROT_READ,
	   MAP_SHARED | MAP_NORESERVE,info->dfile,0L);
    if (file_map == (byte*) MAP_FAILED)
    {
      DBUG_PRINT("warning",("mmap failed: errno: %d",errno));
      my_errno=errno;
      DBUG_RETURN(0);
    }
    share->file_map= file_map;
  }
  info->opt_flag|= MEMMAP_USED;
  info->read_record=share->read_record=_mi_read_mempack_record;
  share->read_rnd=_mi_read_rnd_mempack_record;
  DBUG_RETURN(1);
}


void _mi_unmap_file(MI_INFO *info)
{
  VOID(munmap((caddr_t) info->s->file_map,
	      (size_t) info->s->state.state.data_file_length+
	      MEMMAP_EXTRA_MARGIN));
}


static uchar *_mi_mempack_get_block_info(MI_INFO *myisam,MI_BLOCK_INFO *info,
					 uchar *header)
{
  header+= read_pack_length((uint) myisam->s->pack.version, header,
                            &info->rec_len);
  if (myisam->s->base.blobs)
  {
    header+= read_pack_length((uint) myisam->s->pack.version, header,
                              &info->blob_len);
    /* mi_alloc_rec_buff sets my_errno on error */
    if (!(mi_alloc_rec_buff(myisam, info->blob_len,
			    &myisam->rec_buff)))
      return 0;				/* not enough memory */
    myisam->bit_buff.blob_pos=(uchar*) myisam->rec_buff;
    myisam->bit_buff.blob_end= (uchar*) myisam->rec_buff + info->blob_len;
  }
  return header;
}


static int _mi_read_mempack_record(MI_INFO *info, my_off_t filepos, byte *buf)
{
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  byte *pos;
  DBUG_ENTER("mi_read_mempack_record");

  if (filepos == HA_OFFSET_ERROR)
    DBUG_RETURN(-1);			/* _search() didn't find record */

  if (!(pos= (byte*) _mi_mempack_get_block_info(info,&block_info,
						(uchar*) share->file_map+
						filepos)))
    DBUG_RETURN(-1);
  DBUG_RETURN(_mi_pack_rec_unpack(info, buf, pos, block_info.rec_len));
}


/*ARGSUSED*/
static int _mi_read_rnd_mempack_record(MI_INFO *info, byte *buf,
				       register my_off_t filepos,
				       my_bool skip_deleted_blocks
				       __attribute__((unused)))
{
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  byte *pos,*start;
  DBUG_ENTER("_mi_read_rnd_mempack_record");

  if (filepos >= share->state.state.data_file_length)
  {
    my_errno=HA_ERR_END_OF_FILE;
    goto err;
  }
  if (!(pos= (byte*) _mi_mempack_get_block_info(info,&block_info,
						(uchar*)
						(start=share->file_map+
						 filepos))))
    goto err;
#ifndef DBUG_OFF
  if (block_info.rec_len > info->s->max_pack_length)
  {
    my_errno=HA_ERR_WRONG_IN_RECORD;
    goto err;
  }
#endif
  info->packed_length=block_info.rec_len;
  info->lastpos=filepos;
  info->nextpos=filepos+(uint) (pos-start)+block_info.rec_len;
  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;

  DBUG_RETURN (_mi_pack_rec_unpack(info,buf,pos, block_info.rec_len));
 err:
  DBUG_RETURN(my_errno);
}

#endif /* HAVE_MMAP */

	/* Save length of row */

uint save_pack_length(uint version, byte *block_buff, ulong length)
{
  if (length < 254)
  {
    *(uchar*) block_buff= (uchar) length;
    return 1;
  }
  if (length <= 65535)
  {
    *(uchar*) block_buff=254;
    int2store(block_buff+1,(uint) length);
    return 3;
  }
  *(uchar*) block_buff=255;
  if (version == 1) /* old format */
  {
    DBUG_ASSERT(length <= 0xFFFFFF);
    int3store(block_buff + 1, (ulong) length);
    return 4;
  }
  else
  {
    int4store(block_buff + 1, (ulong) length);
    return 5;
  }
}


uint read_pack_length(uint version, const uchar *buf, ulong *length)
{
  if (buf[0] < 254)
  {
    *length= buf[0];
    return 1;
  }
  else if (buf[0] == 254)
  {
    *length= uint2korr(buf + 1);
    return 3;
  }
  if (version == 1) /* old format */
  {
    *length= uint3korr(buf + 1);
    return 4;
  }
  else
  {
    *length= uint4korr(buf + 1);
    return 5;
  }
}


uint calc_pack_length(uint version, ulong length)
{
  return (length < 254) ? 1 : (length < 65536) ? 3 : (version == 1) ? 4 : 5;
}
