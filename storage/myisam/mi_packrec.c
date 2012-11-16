/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

	/* Functions to compressed records */

#include "fulltext.h"

#define IS_CHAR ((uint) 32768)		/* Bit if char (not offset) in tree */

/* Some definitions to keep in sync with myisampack.c */
#define HEAD_LENGTH	32              /* Length of fixed header */

#if INT_MAX > 32767
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

/* Size in uint16 of a Huffman tree for byte compression of 256 byte values. */
#define OFFSET_TABLE_SIZE 512

static uint read_huff_table(MI_BIT_BUFF *bit_buff,MI_DECODE_TREE *decode_tree,
			    uint16 **decode_table,uchar **intervall_buff,
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
static void uf_varchar1(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
                        uchar *to, uchar *end);
static void uf_varchar2(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
                        uchar *to, uchar *end);
static void decode_bytes(MI_COLUMNDEF *rec,MI_BIT_BUFF *bit_buff,
			 uchar *to,uchar *end);
static uint decode_pos(MI_BIT_BUFF *bit_buff,MI_DECODE_TREE *decode_tree);
static void init_bit_buffer(MI_BIT_BUFF *bit_buff,uchar *buffer,uint length);
static uint fill_and_get_bits(MI_BIT_BUFF *bit_buff,uint count);
static void fill_buffer(MI_BIT_BUFF *bit_buff);
static uint max_bit(uint value);
#ifdef HAVE_MMAP
static uchar *_mi_mempack_get_block_info(MI_INFO *myisam, MI_BIT_BUFF *bit_buff,
                                         MI_BLOCK_INFO *info, uchar **rec_buff_p,
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
  uchar *disk_cache;
  uchar *intervall_buff;
  uchar header[HEAD_LENGTH];
  MYISAM_SHARE *share=info->s;
  MI_BIT_BUFF bit_buff;
  DBUG_ENTER("_mi_read_pack_info");

  if (myisam_quick_table_bits < 4)
    myisam_quick_table_bits=4;
  else if (myisam_quick_table_bits > MAX_QUICK_TABLE_BITS)
    myisam_quick_table_bits=MAX_QUICK_TABLE_BITS;

  file=info->dfile;
  my_errno=0;
  if (mysql_file_read(file, (uchar*) header, sizeof(header), MYF(MY_NABP)))
  {
    if (!my_errno)
      my_errno=HA_ERR_END_OF_FILE;
    goto err0;
  }
  /* Only the first three bytes of magic number are independent of version. */
  if (memcmp((uchar*) header, (uchar*) myisam_pack_file_magic, 3))
  {
    my_errno=HA_ERR_WRONG_IN_RECORD;
    goto err0;
  }
  share->pack.version= header[3]; /* fourth byte of magic number */
  share->pack.header_length=	uint4korr(header+4);
  share->min_pack_length=(uint) uint4korr(header+8);
  share->max_pack_length=(uint) uint4korr(header+12);
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
  DBUG_PRINT("info", ("fixed header length:   %u", HEAD_LENGTH));
  DBUG_PRINT("info", ("total header length:   %lu", share->pack.header_length));
  DBUG_PRINT("info", ("pack file version:     %u", share->pack.version));
  DBUG_PRINT("info", ("min pack length:       %lu", share->min_pack_length));
  DBUG_PRINT("info", ("max pack length:       %lu", share->max_pack_length));
  DBUG_PRINT("info", ("elements of all trees: %lu", elements));
  DBUG_PRINT("info", ("distinct values bytes: %lu", intervall_length));
  DBUG_PRINT("info", ("number of code trees:  %u", trees));
  DBUG_PRINT("info", ("bytes for record lgt:  %u", share->pack.ref_length));
  DBUG_PRINT("info", ("record pointer length: %u", rec_reflength));

  /*
    Memory segment #1:
    - Decode tree heads
    - Distinct column values
  */
  if (!(share->decode_trees=(MI_DECODE_TREE*)
	my_malloc((uint) (trees*sizeof(MI_DECODE_TREE)+
			  intervall_length*sizeof(uchar)),
		  MYF(MY_WME))))
    goto err0;
  intervall_buff=(uchar*) (share->decode_trees+trees);

  /*
    Memory segment #2:
    - Decode tables
    - Quick decode tables
    - Temporary decode table
    - Compressed data file header cache
    This segment will be reallocated after construction of the tables.
  */
  length=(uint) (elements*2+trees*(1 << myisam_quick_table_bits));
  /*
    To keep some algorithms simpler, we accept that they access
    bytes beyond the end of the input data. This can affect up to
    one byte less than the "word size" size used in this file,
    which is BITS_SAVED / 8. To avoid accessing non-allocated
    data, we add (BITS_SAVED / 8) - 1 bytes to the buffer size.
  */
  if (!(share->decode_tables=(uint16*)
        my_malloc((length + OFFSET_TABLE_SIZE) * sizeof(uint16) +
                  (uint) (share->pack.header_length - sizeof(header) +
                  (BITS_SAVED / 8) - 1), MYF(MY_WME | MY_ZEROFILL))))
    goto err1;
  tmp_buff=share->decode_tables+length;
  disk_cache= (uchar*) (tmp_buff+OFFSET_TABLE_SIZE);

  if (mysql_file_read(file, disk_cache,
                      (uint) (share->pack.header_length-sizeof(header)),
                      MYF(MY_NABP)))
    goto err2;

  huff_tree_bits=max_bit(trees ? trees-1 : 0);
  init_bit_buffer(&bit_buff, disk_cache,
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
    DBUG_PRINT("info", ("col: %2u  type: %2u  pack: %u  slbits: %2u",
                        i, share->rec[i].base_type, share->rec[i].pack_type,
                        share->rec[i].space_length_bits));
  }
  skip_to_next_byte(&bit_buff);
  /*
    Construct the decoding tables from the file header. Keep track of
    the used memory.
  */
  decode_table=share->decode_tables;
  for (i=0 ; i < trees ; i++)
    if (read_huff_table(&bit_buff,share->decode_trees+i,&decode_table,
                        &intervall_buff,tmp_buff))
      goto err3;
  /* Reallocate the decoding tables to the used size. */
  decode_table=(uint16*)
    my_realloc((uchar*) share->decode_tables,
	       (uint) ((uchar*) decode_table - (uchar*) share->decode_tables),
	       MYF(MY_HOLD_ON_ERROR));
  /* Fix the table addresses in the tree heads. */
  {
    my_ptrdiff_t diff=PTR_BYTE_DIFF(decode_table,share->decode_tables);
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
  my_free(share->decode_tables);
err1:
  my_free(share->decode_trees);
err0:
  DBUG_RETURN(1);
}


/*
  Read a huff-code-table from datafile.

  SYNOPSIS
    read_huff_table()
      bit_buff                  Bit buffer pointing at start of the
                                decoding table in the file header cache.
      decode_tree               Pointer to the decode tree head.
      decode_table      IN/OUT  Address of a pointer to the next free space.
      intervall_buff    IN/OUT  Address of a pointer to the next unused values.
      tmp_buff                  Buffer for temporary extraction of a full
                                decoding table as read from bit_buff.

  RETURN
    0           OK.
    1           Error.
*/

static uint read_huff_table(MI_BIT_BUFF *bit_buff, MI_DECODE_TREE *decode_tree,
			    uint16 **decode_table, uchar **intervall_buff,
			    uint16 *tmp_buff)
{
  uint min_chr,elements,char_bits,offset_bits,size,intervall_length,table_bits,
  next_free_offset;
  uint16 *ptr,*end;
  DBUG_ENTER("read_huff_table");

  if (!get_bits(bit_buff,1))
  {
    /* Byte value compression. */
    min_chr=get_bits(bit_buff,8);
    elements=get_bits(bit_buff,9);
    char_bits=get_bits(bit_buff,5);
    offset_bits=get_bits(bit_buff,5);
    intervall_length=0;
    ptr=tmp_buff;
    DBUG_PRINT("info", ("byte value compression"));
    DBUG_PRINT("info", ("minimum byte value:    %u", min_chr));
    DBUG_PRINT("info", ("number of tree nodes:  %u", elements));
    DBUG_PRINT("info", ("bits for values:       %u", char_bits));
    DBUG_PRINT("info", ("bits for tree offsets: %u", offset_bits));
    if (elements > 256)
    {
      DBUG_PRINT("error", ("ERROR: illegal number of tree elements: %u",
                           elements));
      DBUG_RETURN(1);
    }
  }
  else
  {
    /* Distinct column value compression. */
    min_chr=0;
    elements=get_bits(bit_buff,15);
    intervall_length=get_bits(bit_buff,16);
    char_bits=get_bits(bit_buff,5);
    offset_bits=get_bits(bit_buff,5);
    decode_tree->quick_table_bits=0;
    ptr= *decode_table;
    DBUG_PRINT("info", ("distinct column value compression"));
    DBUG_PRINT("info", ("number of tree nodes:  %u", elements));
    DBUG_PRINT("info", ("value buffer length:   %u", intervall_length));
    DBUG_PRINT("info", ("bits for value index:  %u", char_bits));
    DBUG_PRINT("info", ("bits for tree offsets: %u", offset_bits));
  }
  size=elements*2-2;
  DBUG_PRINT("info", ("tree size in uint16:   %u", size));
  DBUG_PRINT("info", ("tree size in bytes:    %u",
                      size * (uint) sizeof(uint16)));

  for (end=ptr+size ; ptr < end ; ptr++)
  {
    if (get_bit(bit_buff))
    {
      *ptr= (uint16) get_bits(bit_buff,offset_bits);
      if ((ptr + *ptr >= end) || !*ptr)
      {
        DBUG_PRINT("error", ("ERROR: illegal pointer in decode tree"));
        DBUG_RETURN(1);
      }
    }
    else
      *ptr= (uint16) (IS_CHAR + (get_bits(bit_buff,char_bits) + min_chr));
  }
  skip_to_next_byte(bit_buff);

  decode_tree->table= *decode_table;
  decode_tree->intervalls= *intervall_buff;
  if (! intervall_length)
  {
    /* Byte value compression. ptr started from tmp_buff. */
    /* Find longest Huffman code from begin to end of tree in bits. */
    table_bits= find_longest_bitstream(tmp_buff, ptr);
    if (table_bits >= OFFSET_TABLE_SIZE)
      DBUG_RETURN(1);
    if (table_bits > myisam_quick_table_bits)
      table_bits=myisam_quick_table_bits;
    DBUG_PRINT("info", ("table bits:            %u", table_bits));

    next_free_offset= (1 << table_bits);
    make_quick_table(*decode_table,tmp_buff,&next_free_offset,0,table_bits,
		     table_bits);
    (*decode_table)+= next_free_offset;
    decode_tree->quick_table_bits=table_bits;
  }
  else
  {
    /* Distinct column value compression. ptr started from *decode_table */
    (*decode_table)=end;
    /*
      get_bits() moves some bytes to a cache buffer in advance. May need
      to step back.
    */
    bit_buff->pos-= bit_buff->bits/8;
    /* Copy the distinct column values from the buffer. */
    memcpy(*intervall_buff,bit_buff->pos,(size_t) intervall_length);
    (*intervall_buff)+=intervall_length;
    bit_buff->pos+=intervall_length;
    bit_buff->bits=0;
  }
  DBUG_RETURN(0);
}


/*
  Make a quick_table for faster decoding.

  SYNOPSIS
    make_quick_table()
      to_table                  Target quick_table and remaining decode table.
      decode_table              Source Huffman (sub-)tree within tmp_buff.
      next_free_offset   IN/OUT Next free offset from to_table.
                                Starts behind quick_table on the top-level.
      value                     Huffman bits found so far.
      bits                      Remaining bits to be collected.
      max_bits                  Total number of bits to collect (table_bits).

  DESCRIPTION

    The quick table is an array of 16-bit values. There exists one value
    for each possible code representable by max_bits (table_bits) bits.
    In most cases table_bits is 9. So there are 512 16-bit values.

    If the high-order bit (16) is set (IS_CHAR) then the array slot for
    this value is a valid Huffman code for a resulting byte value.

    The low-order 8 bits (1..8) are the resulting byte value.

    Bits 9..14 are the length of the Huffman code for this byte value.
    This means so many bits from the input stream were needed to
    represent this byte value. The remaining bits belong to later
    Huffman codes. This also means that for every Huffman code shorter
    than table_bits there are multiple entires in the array, which
    differ just in the unused bits.

    If the high-order bit (16) is clear (0) then the remaining bits are
    the position of the remaining Huffman decode tree segment behind the
    quick table.

  RETURN
    void
*/

static void make_quick_table(uint16 *to_table, uint16 *decode_table,
			     uint *next_free_offset, uint value, uint bits,
			     uint max_bits)
{
  DBUG_ENTER("make_quick_table");

  /*
    When down the table to the requested maximum, copy the rest of the
    Huffman table.
  */
  if (!bits--)
  {
    /*
      Remaining left  Huffman tree segment starts behind quick table.
      Remaining right Huffman tree segment starts behind left segment.
    */
    to_table[value]= (uint16) *next_free_offset;
    /*
      Re-construct the remaining Huffman tree segment at
      next_free_offset in to_table.
    */
    *next_free_offset= copy_decode_table(to_table, *next_free_offset,
                                         decode_table);
    DBUG_VOID_RETURN;
  }

  /* Descent on the left side. Left side bits are clear (0). */
  if (!(*decode_table & IS_CHAR))
  {
    /* Not a leaf. Follow the pointer. */
    make_quick_table(to_table, decode_table + *decode_table,
                     next_free_offset, value, bits, max_bits);
  }
  else
  {
    /*
      A leaf. A Huffman code is complete. Fill the quick_table
      array for all possible bit strings starting with this Huffman
      code.
    */
    fill_quick_table(to_table + value, bits, max_bits, (uint) *decode_table);
  }

  /* Descent on the right side. Right side bits are set (1). */
  decode_table++;
  value|= (1 << bits);
  if (!(*decode_table & IS_CHAR))
  {
    /* Not a leaf. Follow the pointer. */
    make_quick_table(to_table, decode_table + *decode_table,
                     next_free_offset, value, bits, max_bits);
  }
  else
  {
    /*
      A leaf. A Huffman code is complete. Fill the quick_table
      array for all possible bit strings starting with this Huffman
      code.
    */
    fill_quick_table(to_table + value, bits, max_bits, (uint) *decode_table);
  }

  DBUG_VOID_RETURN;
}


/*
  Fill quick_table for all possible values starting with this Huffman code.

  SYNOPSIS
    fill_quick_table()
      table                     Target quick_table position.
      bits                      Unused bits from max_bits.
      max_bits                  Total number of bits to collect (table_bits).
      value                     The byte encoded by the found Huffman code.

  DESCRIPTION

    Fill the segment (all slots) of the quick_table array with the
    resulting value for the found Huffman code. There are as many slots
    as there are combinations representable by the unused bits.

    In most cases we use 9 table bits. Assume a 3-bit Huffman code. Then
    there are 6 unused bits. Hence we fill 2**6 = 64 slots with the
    value.

  RETURN
    void
*/

static void fill_quick_table(uint16 *table, uint bits, uint max_bits,
			     uint value)
{
  uint16 *end;
  DBUG_ENTER("fill_quick_table");

  /*
    Bits 1..8 of value represent the decoded byte value.
    Bits 9..14 become the length of the Huffman code for this byte value.
    Bit 16 flags a valid code (IS_CHAR).
  */
  value|= (max_bits - bits) << 8 | IS_CHAR;

  for (end= table + ((my_ptrdiff_t) 1 << bits); table < end; table++)
  {
    *table= (uint16) value;
  }
  DBUG_VOID_RETURN;
}


/*
  Reconstruct a decode subtree at the target position.

  SYNOPSIS
    copy_decode_table()
      to_pos                    Target quick_table and remaining decode table.
      offset                    Next free offset from to_pos.
      decode_table              Source Huffman subtree within tmp_buff.

  NOTE
    Pointers in the decode tree are relative to the pointers position.

  RETURN
    next free offset from to_pos.
*/

static uint copy_decode_table(uint16 *to_pos, uint offset,
			      uint16 *decode_table)
{
  uint prev_offset= offset;
  DBUG_ENTER("copy_decode_table");

  /* Descent on the left side. */
  if (!(*decode_table & IS_CHAR))
  {
    /* Set a pointer to the next target node. */
    to_pos[offset]=2;
    /* Copy the left hand subtree there. */
    offset=copy_decode_table(to_pos,offset+2,decode_table+ *decode_table);
  }
  else
  {
    /* Copy the byte value. */
    to_pos[offset]= *decode_table;
    /* Step behind this node. */
    offset+=2;
  }

  /* Descent on the right side. */
  decode_table++;
  if (!(*decode_table & IS_CHAR))
  {
    /* Set a pointer to the next free target node. */
    to_pos[prev_offset+1]=(uint16) (offset-prev_offset-1);
    /* Copy the right hand subtree to the entry of that node. */
    offset=copy_decode_table(to_pos,offset,decode_table+ *decode_table);
  }
  else
  {
    /* Copy the byte value. */
    to_pos[prev_offset+1]= *decode_table;
  }
  DBUG_RETURN(offset);
}


/*
  Find the length of the longest Huffman code in this table in bits.

  SYNOPSIS
    find_longest_bitstream()
      table                     Code (sub-)table start.
      end                       End of code table.

  IMPLEMENTATION

    Recursively follow the branch(es) of the code pair on every level of
    the tree until two byte values (and no branch) are found. Add one to
    each level when returning back from each recursion stage.

    'end' is used for error checking only. A clean tree terminates
    before reaching 'end'. Hence the exact value of 'end' is not too
    important. However having it higher than necessary could lead to
    misbehaviour should 'next' jump into the dirty area.

  RETURN
    length                  Length of longest Huffman code in bits.
    >= OFFSET_TABLE_SIZE    Error, broken tree. It does not end before 'end'.
*/

static uint find_longest_bitstream(uint16 *table, uint16 *end)
{
  uint length= 1;
  uint length2;

  if (!(*table & IS_CHAR))
  {
    uint16 *next= table + *table;
    if (next > end || next == table)
    {
      DBUG_PRINT("error", ("ERROR: illegal pointer in decode tree"));
      return OFFSET_TABLE_SIZE;
    }
    length= find_longest_bitstream(next, end) + 1;
  }
  table++;
  if (!(*table & IS_CHAR))
  {
    uint16 *next= table + *table;
    if (next > end || next == table)
    {
      DBUG_PRINT("error", ("ERROR: illegal pointer in decode tree"));
      return OFFSET_TABLE_SIZE;
    }
    length2= find_longest_bitstream(next, end) + 1;
    length= MY_MAX(length, length2);
  }
  return length;
}


/*
  Read record from datafile.

  SYNOPSIS
    _mi_read_pack_record()
    info                        A pointer to MI_INFO.
    filepos                     File offset of the record.
    buf                 RETURN  The buffer to receive the record.

  RETURN
    0                                   on success
    HA_ERR_WRONG_IN_RECORD or -1        on error
*/

int _mi_read_pack_record(MI_INFO *info, my_off_t filepos, uchar *buf)
{
  MI_BLOCK_INFO block_info;
  File file;
  DBUG_ENTER("mi_read_pack_record");

  if (filepos == HA_OFFSET_ERROR)
    DBUG_RETURN(-1);			/* _search() didn't find record */

  file=info->dfile;
  if (_mi_pack_get_block_info(info, &info->bit_buff, &block_info,
                              &info->rec_buff, file, filepos))
    goto err;
  if (mysql_file_read(file, (uchar*) info->rec_buff + block_info.offset,
                      block_info.rec_len - block_info.offset, MYF(MY_NABP)))
    goto panic;
  info->update|= HA_STATE_AKTIV;
  DBUG_RETURN(_mi_pack_rec_unpack(info, &info->bit_buff, buf,
                                  info->rec_buff, block_info.rec_len));
panic:
  my_errno=HA_ERR_WRONG_IN_RECORD;
err:
  DBUG_RETURN(-1);
}



int _mi_pack_rec_unpack(MI_INFO *info, MI_BIT_BUFF *bit_buff,
                        uchar *to, uchar *from, ulong reclength)
{
  uchar *end_field;
  MI_COLUMNDEF *end;
  MI_COLUMNDEF *current_field;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_pack_rec_unpack");

  init_bit_buffer(bit_buff, (uchar*) from, reclength);

  for (current_field=share->rec, end=current_field+share->base.fields ;
       current_field < end ;
       current_field++,to=end_field)
  {
    end_field=to+current_field->length;
    (*current_field->unpack)(current_field, bit_buff, (uchar*) to,
			     (uchar*) end_field);
  }
  if (!bit_buff->error &&
      bit_buff->pos - bit_buff->bits / 8 == bit_buff->end)
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
    if (rec->length <= 256)                      /* 255 + 1 byte length */
      return &uf_varchar1;
    return &uf_varchar2;
  case FIELD_LAST:
  default:
    return 0;			/* This should never happend */
  }
}

	/* The different functions to unpack a field */

static void uf_zerofill_skip_zero(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				   uchar *to, uchar *end)
{
  if (get_bit(bit_buff))
    memset(to, 0, (end-to));
  else
  {
    end-=rec->space_length_bits;
    decode_bytes(rec,bit_buff,to,end);
    memset(end, 0, rec->space_length_bits);
  }
}

static void uf_skip_zero(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			  uchar *end)
{
  if (get_bit(bit_buff))
    memset(to, 0, (end-to));
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_normal(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			    uchar *end)
{
  if (get_bit(bit_buff))
    memset(to, ' ', end - to);
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_endspace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    memset(to, ' ', end-to);
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
      memset(end-spaces, ' ', spaces);
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
    memset(end - spaces, ' ', spaces);
  }
  else
    decode_bytes(rec,bit_buff,to,end);
}

static void uf_space_endspace(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			      uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    memset(to, ' ', end - to);
  else
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    if (to+spaces != end)
      decode_bytes(rec,bit_buff,to,end-spaces);
    memset(end - spaces, ' ', spaces);
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
  memset(end - spaces, ' ', spaces);
}

static void uf_space_prespace_selected(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
				       uchar *to, uchar *end)
{
  uint spaces;
  if (get_bit(bit_buff))
    memset(to, ' ', end - to);
  else
  {
    if (get_bit(bit_buff))
    {
      if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
      {
	bit_buff->error=1;
	return;
      }
      memset(to, ' ', spaces);
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
    memset(to, ' ', spaces);
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
    memset(to, ' ', end-to);
  else
  {
    if ((spaces=get_bits(bit_buff,rec->space_length_bits))+to > end)
    {
      bit_buff->error=1;
      return;
    }
    memset(to, ' ', spaces);
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
  memset(to, ' ', spaces);
  if (to+spaces != end)
    decode_bytes(rec,bit_buff,to+spaces,end);
}

static void uf_zerofill_normal(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff, uchar *to,
			       uchar *end)
{
  end-=rec->space_length_bits;
  decode_bytes(rec,bit_buff,(uchar*) to,end);
  memset(end, 0, rec->space_length_bits);
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
  uint field_length=(uint) (end-to);
  memcpy(to,rec->huff_tree->intervalls+field_length*decode_pos(bit_buff,
							       rec->huff_tree),
	 (size_t) field_length);
}


/*ARGSUSED*/
static void uf_zero(MI_COLUMNDEF *rec __attribute__((unused)),
		    MI_BIT_BUFF *bit_buff __attribute__((unused)),
		    uchar *to, uchar *end)
{
  memset(to, 0, (end-to));
}

static void uf_blob(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		    uchar *to, uchar *end)
{
  if (get_bit(bit_buff))
    memset(to, 0, (end-to));
  else
  {
    ulong length=get_bits(bit_buff,rec->space_length_bits);
    uint pack_length=(uint) (end-to)-portable_sizeof_char_ptr;
    if (bit_buff->blob_pos+length > bit_buff->blob_end)
    {
      bit_buff->error=1;
      memset(to, 0, (end-to));
      return;
    }
    decode_bytes(rec,bit_buff,bit_buff->blob_pos,bit_buff->blob_pos+length);
    _mi_store_blob_length((uchar*) to,pack_length,length);
    memcpy((char*) to+pack_length, &bit_buff->blob_pos, sizeof(char*));
    bit_buff->blob_pos+=length;
  }
}


static void uf_varchar1(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
		       uchar *to, uchar *end __attribute__((unused)))
{
  if (get_bit(bit_buff))
    to[0]= 0;				/* Zero lengths */
  else
  {
    ulong length=get_bits(bit_buff,rec->space_length_bits);
    *to= (uchar) length;
    decode_bytes(rec,bit_buff,to+1,to+1+length);
  }
}


static void uf_varchar2(MI_COLUMNDEF *rec, MI_BIT_BUFF *bit_buff,
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
  uint bits,low_byte;
  uint16 *pos;
  uint table_bits,table_and;
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
    /*
      First use info in quick_table.

      The quick table is an array of 16-bit values. There exists one
      value for each possible code representable by table_bits bits.
      In most cases table_bits is 9. So there are 512 16-bit values.

      If the high-order bit (16) is set (IS_CHAR) then the array slot
      for this value is a valid Huffman code for a resulting byte value.

      The low-order 8 bits (1..8) are the resulting byte value.

      Bits 9..14 are the length of the Huffman code for this byte value.
      This means so many bits from the input stream were needed to
      represent this byte value. The remaining bits belong to later
      Huffman codes. This also means that for every Huffman code shorter
      than table_bits there are multiple entires in the array, which
      differ just in the unused bits.

      If the high-order bit (16) is clear (0) then the remaining bits are
      the position of the remaining Huffman decode tree segment behind the
      quick table.
    */
    low_byte=(uint) (bit_buff->current_byte >> (bits - table_bits)) & table_and;
    low_byte=decode_tree->table[low_byte];
    if (low_byte & IS_CHAR)
    {
      /*
        All Huffman codes of less or equal table_bits length are in the
        quick table. This is one of them.
      */
      *to++ = (low_byte & 255);		/* Found char in quick table */
      bits-=  ((low_byte >> 8) & 31);	/* Remove bits used */
    }
    else
    {					/* Map through rest of decode-table */
      /* This means that the Huffman code must be longer than table_bits. */
      pos=decode_tree->table+low_byte;
      bits-=table_bits;
      /* NOTE: decode_bytes_test_bit() is a macro wich contains a break !!! */
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
  uint bits,low_byte;
  uint16 *pos;
  uint table_bits,table_and;
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


int _mi_read_rnd_pack_record(MI_INFO *info, uchar *buf,
			     my_off_t filepos,
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
    if (_mi_read_cache(&info->rec_cache, (uchar*) block_info.header,
                       filepos, share->pack.ref_length,
                       skip_deleted_blocks ? READING_NEXT : 0))
      goto err;
    b_type=_mi_pack_get_block_info(info, &info->bit_buff, &block_info,
                                   &info->rec_buff, -1, filepos);
  }
  else
    b_type=_mi_pack_get_block_info(info, &info->bit_buff, &block_info,
                                   &info->rec_buff, info->dfile, filepos);
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
    if (_mi_read_cache(&info->rec_cache, (uchar*) info->rec_buff,
                       block_info.filepos, block_info.rec_len,
                       skip_deleted_blocks ? READING_NEXT : 0))
      goto err;
  }
  else
  {
    if (mysql_file_read(info->dfile,
                        (uchar*) info->rec_buff + block_info.offset,
                        block_info.rec_len-block_info.offset, MYF(MY_NABP)))
      goto err;
  }
  info->packed_length=block_info.rec_len;
  info->lastpos=filepos;
  info->nextpos=block_info.filepos+block_info.rec_len;
  info->update|= HA_STATE_AKTIV | HA_STATE_KEY_CHANGED;

  DBUG_RETURN (_mi_pack_rec_unpack(info, &info->bit_buff, buf,
                                   info->rec_buff, block_info.rec_len));
 err:
  DBUG_RETURN(my_errno);
}


	/* Read and process header from a huff-record-file */

uint _mi_pack_get_block_info(MI_INFO *myisam, MI_BIT_BUFF *bit_buff,
                             MI_BLOCK_INFO *info, uchar **rec_buff_p,
                             File file, my_off_t filepos)
{
  uchar *header=info->header;
  uint head_length, UNINIT_VAR(ref_length);
  LINT_INIT(ref_length);

  if (file >= 0)
  {
    ref_length=myisam->s->pack.ref_length;
    /*
      We can't use mysql_file_pread() here because mi_read_rnd_pack_record assumes
      position is ok
    */
    mysql_file_seek(file, filepos, MY_SEEK_SET, MYF(0));
    if (mysql_file_read(file, header, ref_length, MYF(MY_NABP)))
      return BLOCK_FATAL_ERROR;
    DBUG_DUMP("header",(uchar*) header,ref_length);
  }
  head_length= read_pack_length((uint) myisam->s->pack.version, header,
                                &info->rec_len);
  if (myisam->s->base.blobs)
  {
    head_length+= read_pack_length((uint) myisam->s->pack.version,
                                   header + head_length, &info->blob_len);
    /*
      Ensure that the record buffer is big enough for the compressed
      record plus all expanded blobs. [We do not have an extra buffer
      for the resulting blobs. Sigh.]
    */
    if (!(mi_alloc_rec_buff(myisam,info->rec_len + info->blob_len,
			    rec_buff_p)))
      return BLOCK_FATAL_ERROR;			/* not enough memory */
    bit_buff->blob_pos= (uchar*) *rec_buff_p + info->rec_len;
    bit_buff->blob_end= bit_buff->blob_pos + info->blob_len;
    myisam->blob_length=info->blob_len;
  }
  info->filepos=filepos+head_length;
  if (file > 0)
  {
    info->offset= MY_MIN(info->rec_len, ref_length - head_length);
    memcpy(*rec_buff_p, header + head_length, info->offset);
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

static uint max_bit(uint value)
{
  uint power=1;

  while ((value>>=1))
    power++;
  return (power);
}


/*****************************************************************************
	Some redefined functions to handle files when we are using memmap
*****************************************************************************/
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_MMAP

static int _mi_read_mempack_record(MI_INFO *info,my_off_t filepos,uchar *buf);
static int _mi_read_rnd_mempack_record(MI_INFO*, uchar *,my_off_t, my_bool);

my_bool _mi_memmap_file(MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;
  my_bool eom;

  DBUG_ENTER("mi_memmap_file");

  if (!info->s->file_map)
  {
    my_off_t data_file_length= share->state.state.data_file_length;

    if (myisam_mmap_size != SIZE_T_MAX)
    {
      mysql_mutex_lock(&THR_LOCK_myisam_mmap);
      eom= data_file_length > myisam_mmap_size - myisam_mmap_used - MEMMAP_EXTRA_MARGIN;
      if (!eom)
        myisam_mmap_used+= data_file_length + MEMMAP_EXTRA_MARGIN;
      mysql_mutex_unlock(&THR_LOCK_myisam_mmap);
    }
    else
      eom= data_file_length > myisam_mmap_size - MEMMAP_EXTRA_MARGIN;

    if (eom)
    {
      DBUG_PRINT("warning", ("File is too large for mmap"));
      DBUG_RETURN(0);
    }
    if (mysql_file_seek(info->dfile, 0L, MY_SEEK_END, MYF(0)) <
        share->state.state.data_file_length+MEMMAP_EXTRA_MARGIN)
    {
      DBUG_PRINT("warning",("File isn't extended for memmap"));
      if (myisam_mmap_size != SIZE_T_MAX)
      {
        mysql_mutex_lock(&THR_LOCK_myisam_mmap);
        myisam_mmap_used-= data_file_length + MEMMAP_EXTRA_MARGIN;
        mysql_mutex_unlock(&THR_LOCK_myisam_mmap);
      }
      DBUG_RETURN(0);
    }
    if (mi_dynmap_file(info,
                       share->state.state.data_file_length + 
                         MEMMAP_EXTRA_MARGIN))
    {
      if (myisam_mmap_size != SIZE_T_MAX)
      {
        mysql_mutex_lock(&THR_LOCK_myisam_mmap);
        myisam_mmap_used-= data_file_length + MEMMAP_EXTRA_MARGIN;
        mysql_mutex_unlock(&THR_LOCK_myisam_mmap);
      }
      DBUG_RETURN(0);
    }
  }
  info->opt_flag|= MEMMAP_USED;
  info->read_record= share->read_record= _mi_read_mempack_record;
  share->read_rnd= _mi_read_rnd_mempack_record;
  DBUG_RETURN(1);
}


void _mi_unmap_file(MI_INFO *info)
{
  DBUG_ASSERT(info->s->options & HA_OPTION_COMPRESS_RECORD);

  (void) my_munmap((char*) info->s->file_map, (size_t) info->s->mmaped_length);

  if (myisam_mmap_size != SIZE_T_MAX)
  {
    mysql_mutex_lock(&THR_LOCK_myisam_mmap);
    myisam_mmap_used-= info->s->mmaped_length;
    mysql_mutex_unlock(&THR_LOCK_myisam_mmap);
  }
}


static uchar *_mi_mempack_get_block_info(MI_INFO *myisam, MI_BIT_BUFF *bit_buff,
                                         MI_BLOCK_INFO *info, uchar **rec_buff_p,
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
			    rec_buff_p)))
      return 0;				/* not enough memory */
    bit_buff->blob_pos= (uchar*) *rec_buff_p;
    bit_buff->blob_end= (uchar*) *rec_buff_p + info->blob_len;
  }
  return header;
}


static int _mi_read_mempack_record(MI_INFO *info, my_off_t filepos, uchar *buf)
{
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  uchar *pos;
  DBUG_ENTER("mi_read_mempack_record");

  if (filepos == HA_OFFSET_ERROR)
    DBUG_RETURN(-1);			/* _search() didn't find record */

  if (!(pos= (uchar*) _mi_mempack_get_block_info(info, &info->bit_buff,
                                                &block_info, &info->rec_buff,
						(uchar*) share->file_map+
						filepos)))
    DBUG_RETURN(-1);
  DBUG_RETURN(_mi_pack_rec_unpack(info, &info->bit_buff, buf,
                                  pos, block_info.rec_len));
}


/*ARGSUSED*/
static int _mi_read_rnd_mempack_record(MI_INFO *info, uchar *buf,
				       my_off_t filepos,
				       my_bool skip_deleted_blocks
				       __attribute__((unused)))
{
  MI_BLOCK_INFO block_info;
  MYISAM_SHARE *share=info->s;
  uchar *pos,*start;
  DBUG_ENTER("_mi_read_rnd_mempack_record");

  if (filepos >= share->state.state.data_file_length)
  {
    my_errno=HA_ERR_END_OF_FILE;
    goto err;
  }
  if (!(pos= (uchar*) _mi_mempack_get_block_info(info, &info->bit_buff,
                                                &block_info, &info->rec_buff,
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

  DBUG_RETURN (_mi_pack_rec_unpack(info, &info->bit_buff, buf,
                                   pos, block_info.rec_len));
 err:
  DBUG_RETURN(my_errno);
}

#endif /* HAVE_MMAP */

	/* Save length of row */

uint save_pack_length(uint version, uchar *block_buff, ulong length)
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
