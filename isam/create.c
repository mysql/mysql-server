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

/* Skapar en isam-databas */

#include "isamdef.h"
#if defined(MSDOS) || defined(__WIN__)
#ifdef __WIN__
#include <fcntl.h>
#else
#include <process.h>			/* Prototype for getpid */
#endif
#endif

	/*
	** Old options is used when recreating database, from isamchk
	** Note that the minimun reclength that MySQL allows for static rows
	** are 5.  (Will be fixed in the next generation)
	*/

int nisam_create(const char *name,uint keys,N_KEYDEF *keyinfo,
		 N_RECINFO *recinfo,
		 ulong records,ulong reloc, uint flags,uint old_options,
		 ulong data_file_length)
{
  register uint i,j;
  File dfile,file;
  int errpos,save_errno;
  uint fields,length,max_key_length,packed,pointer,reclength,min_pack_length,
       key_length,info_length,key_segs,options,min_key_length_skipp,max_block,
       base_pos;
  char buff[max(FN_REFLEN,512)];
  ulong tot_length,pack_reclength;
  enum en_fieldtype type;
  ISAM_SHARE share;
  N_KEYDEF *keydef;
  N_KEYSEG *keyseg;
  N_RECINFO *rec;
  DBUG_ENTER("nisam_create");

  LINT_INIT(dfile);
  pthread_mutex_lock(&THR_LOCK_isam);
  errpos=0;
  options=0;
  base_pos=512;					/* Enough for N_STATE_INFO */
  bzero((byte*) &share,sizeof(share));
  if ((file = my_create(fn_format(buff,name,"",N_NAME_IEXT,4),0,
       O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
    goto err;
  errpos=1;
  VOID(fn_format(buff,name,"",N_NAME_DEXT,2+4));
  if (!(flags & HA_DONT_TOUCH_DATA))
  {
    if ((dfile = my_create(buff,0,O_RDWR | O_TRUNC,MYF(MY_WME))) < 0)
      goto err;
    errpos=2;
  }
  else if (!(old_options & HA_OPTION_TEMP_COMPRESS_RECORD))
    options=old_options & (HA_OPTION_COMPRESS_RECORD |
			   HA_OPTION_READ_ONLY_DATA | HA_OPTION_PACK_RECORD);
  if (reloc > records)
    reloc=records;				/* Check if wrong parameter */

	/* Start by checking fields and field-types used */
  reclength=0;
  for (rec=recinfo, fields=packed=min_pack_length=0, pack_reclength=0L;
       rec->base.type != (int) FIELD_LAST;
       rec++,fields++)
  {
    reclength+=rec->base.length;
    if ((type=(enum en_fieldtype) rec->base.type))
    {
      packed++;
      if (type == FIELD_BLOB)
      {
	share.base.blobs++;
	rec->base.length-= sizeof(char*);	/* Don't calc pointer */
	if (pack_reclength != NI_POS_ERROR)
	{
	  if (rec->base.length == 4)
	    pack_reclength= (ulong) NI_POS_ERROR;
	  else
	    pack_reclength+=sizeof(char*)+(1 << (rec->base.length*8));
	}
      }
      else if (type == FIELD_SKIPP_PRESPACE ||
	       type == FIELD_SKIPP_ENDSPACE)
      {
	if (pack_reclength != NI_POS_ERROR)
	  pack_reclength+= rec->base.length > 255 ? 2 : 1;
	min_pack_length++;
      }
      else if (type == FIELD_ZERO)
	packed--;
      else if (type != FIELD_SKIPP_ZERO)
      {
	min_pack_length+=rec->base.length;
	packed--;				/* Not a pack record type */
      }
    }
    else
      min_pack_length+=rec->base.length;
  }
  if ((packed & 7) == 1)
  {				/* Bad packing, try to remove a zero-field */
    while (rec != recinfo)
    {
      rec--;
      if (rec->base.type == (int) FIELD_SKIPP_ZERO && rec->base.length == 1)
      {
	rec->base.type=(int) FIELD_NORMAL;
	packed--;
	min_pack_length++;
	break;
      }
    }
  }
  if (packed && !(options & HA_OPTION_COMPRESS_RECORD))
    options|=HA_OPTION_PACK_RECORD;	/* Must use packed records */

  packed=(packed+7)/8;
  if (pack_reclength != NI_POS_ERROR)
    pack_reclength+= reclength+packed;
  min_pack_length+=packed;

  if (options & HA_OPTION_COMPRESS_RECORD)
  {
    if (data_file_length >= (1L << 24))
      pointer=4;
    else if (data_file_length >= (1L << 16))
      pointer=3;
    else
      pointer=2;
  }
  else if (((records == 0L && pack_reclength < 255) ||
	    options & HA_OPTION_PACK_RECORD) ||
	   records >= (ulong) 16000000L ||
	   pack_reclength == (ulong) NI_POS_ERROR ||
	   ((options & HA_OPTION_PACK_RECORD) &&
	    pack_reclength+4 >= (ulong) 14000000L/records))
    pointer=4;
  else if (records == 0L || records >= (ulong) 65000L ||
	   ((options & HA_OPTION_PACK_RECORD) &&
	    pack_reclength+4 >= (ulong) 60000L/records))
    pointer=3;
  else
    pointer=2;

  max_block=max_key_length=0; tot_length=key_segs=0;
  for (i=0, keydef=keyinfo ; i < keys ; i++ , keydef++)
  {
    share.state.key_root[i]= share.state.key_del[i]= NI_POS_ERROR;
    share.base.rec_per_key[i]= (keydef->base.flag & HA_NOSAME) ? 1L : 0L;
    min_key_length_skipp=length=0;
    key_length=pointer;

    if (keydef->base.flag & HA_PACK_KEY &&
	keydef->seg[0].base.length > 127)
      keydef->base.flag&= ~HA_PACK_KEY;		/* Can't pack long keys */
    if (keydef->base.flag & HA_PACK_KEY)
    {
      if ((keydef->seg[0].base.flag & HA_SPACE_PACK) &&
	  keydef->seg[0].base.type == (int) HA_KEYTYPE_NUM)
	keydef->seg[0].base.flag&= ~HA_SPACE_PACK;
      if (!(keydef->seg[0].base.flag & HA_SPACE_PACK))
	length++;
      keydef->seg[0].base.flag|=HA_PACK_KEY;	/* for easyer intern test */
      options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
      if (!(keydef->seg[0].base.flag & HA_SPACE_PACK))
	min_key_length_skipp+=keydef->seg[0].base.length;
    }
    keydef->base.keysegs=0;
    for (keyseg=keydef->seg ; keyseg->base.type ; keyseg++)
    {
      keydef->base.keysegs++;
      if (keyseg->base.length > 127)
	keyseg->base.flag&= ~(HA_SPACE_PACK | HA_PACK_KEY);
      if (keyseg->base.flag & HA_SPACE_PACK)
      {
	keydef->base.flag |= HA_SPACE_PACK_USED;
	options|=HA_OPTION_PACK_KEYS;		/* Using packed keys */
	length++;
	min_key_length_skipp+=keyseg->base.length;
      }
      key_length+= keyseg->base.length;
    }
    bzero((gptr) keyseg,sizeof(keyseg[0]));
    keyseg->base.length=(uint16) pointer;	/* Last key part is pointer */
    key_segs+=keydef->base.keysegs;
    length+=key_length;
    keydef->base.block_length=nisam_block_size;
    keydef->base.keylength= (uint16) key_length;
    keydef->base.minlength= (uint16) (length-min_key_length_skipp);
    keydef->base.maxlength= (uint16) length;

    if ((uint) keydef->base.block_length > max_block)
      max_block=(uint) keydef->base.block_length;
    if (length > max_key_length)
      max_key_length= length;
    tot_length+= (records/(ulong) (((uint) keydef->base.block_length-5)/
				   (length*2)))*
      (ulong) keydef->base.block_length;
  }
  info_length=(uint) (base_pos+sizeof(N_BASE_INFO)+keys*sizeof(N_SAVE_KEYDEF)+
		      (keys+key_segs)*sizeof(N_SAVE_KEYSEG)+
		      fields*sizeof(N_SAVE_RECINFO));

  bmove(share.state.header.file_version,(byte*) nisam_file_magic,4);
  old_options=options| (old_options & HA_OPTION_TEMP_COMPRESS_RECORD ?
			HA_OPTION_COMPRESS_RECORD |
			HA_OPTION_TEMP_COMPRESS_RECORD: 0);
  int2store(share.state.header.options,old_options);
  int2store(share.state.header.header_length,info_length);
  int2store(share.state.header.state_info_length,sizeof(N_STATE_INFO));
  int2store(share.state.header.base_info_length,sizeof(N_BASE_INFO));
  int2store(share.state.header.base_pos,base_pos);

  share.state.dellink = NI_POS_ERROR;
  share.state.process=	(ulong) getpid();
  share.state.uniq=	(ulong) file;
  share.state.loop=	0;
  share.state.version=	(ulong) time((time_t*) 0);
  share.base.options=options;
  share.base.rec_reflength=pointer;
  share.base.key_reflength=((!tot_length || tot_length > 30000000L) ? 3 :
			   tot_length > 120000L ? 2 : 1);
  share.base.keys= share.state.keys = keys;
  share.base.keystart = share.state.key_file_length=MY_ALIGN(info_length,
							  nisam_block_size);
  share.base.max_block=max_block;
  share.base.max_key_length=ALIGN_SIZE(max_key_length+4);
  share.base.records=records;
  share.base.reloc=reloc;
  share.base.reclength=reclength;
  share.base.pack_reclength=reclength+packed-share.base.blobs*sizeof(char*);
  share.base.max_pack_length=pack_reclength;
  share.base.min_pack_length=min_pack_length;
  share.base.pack_bits=packed;
  share.base.fields=fields;
  share.base.pack_fields=packed;
  share.base.sortkey= (ushort) ~0;
  share.base.max_data_file_length= (pointer == 4) ? ~0L :
    (options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
    (1L << (pointer*8)) :
    (pointer == 3 && reclength >= 256L) ? NI_POS_ERROR :
    ((ulong) reclength * (1L << (pointer*8)));
  share.base.max_key_file_length= (share.base.key_reflength == 3 ?
				  NI_POS_ERROR :
				  (1L << (share.base.key_reflength*8))*512);
  share.base.min_block_length=
    (share.base.pack_reclength+3 < N_EXTEND_BLOCK_LENGTH &&
     ! share.base.blobs) ?
    max(share.base.pack_reclength,N_MIN_BLOCK_LENGTH) :
    N_EXTEND_BLOCK_LENGTH;
  if (! (flags & HA_DONT_TOUCH_DATA))
    share.base.create_time= (long) time((time_t*) 0);

  bzero(buff,base_pos);
  if (my_write(file,(char*) &share.state,sizeof(N_STATE_INFO),MYF(MY_NABP)) ||
      my_write(file,buff,base_pos-sizeof(N_STATE_INFO),MYF(MY_NABP)) ||
      my_write(file,(char*) &share.base,sizeof(N_BASE_INFO),MYF(MY_NABP)))
    goto err;

  for (i=0 ; i < share.base.keys ; i++)
  {
    if (my_write(file,(char*) &keyinfo[i].base,sizeof(N_SAVE_KEYDEF),
		 MYF(MY_NABP)))
      goto err;
    for (j=0 ; j <= keyinfo[i].base.keysegs ; j++)
    {
      if (my_write(file,(char*) &keyinfo[i].seg[j].base,sizeof(N_SAVE_KEYSEG),
		   MYF(MY_NABP)))
	goto err;
    }
  }
  for (i=0 ; i < share.base.fields ; i++)
    if (my_write(file,(char*) &recinfo[i].base, (uint) sizeof(N_SAVE_RECINFO),
		 MYF(MY_NABP)))
      goto err;

	/* Enlarge files */
  if (my_chsize(file,(ulong) share.base.keystart,MYF(0)))
    goto err;

  if (! (flags & HA_DONT_TOUCH_DATA))
  {
#ifdef USE_RELOC
    if (my_chsize(dfile,share.base.min_pack_length*reloc,MYF(0)))
      goto err;
#endif
    errpos=1;
    if (my_close(dfile,MYF(0)))
      goto err;
  }
  errpos=0;
  pthread_mutex_unlock(&THR_LOCK_isam);
  if (my_close(file,MYF(0)))
    goto err;
  DBUG_RETURN(0);

err:
  pthread_mutex_unlock(&THR_LOCK_isam);
  save_errno=my_errno;
  switch (errpos) {
  case 2:
    VOID(my_close(dfile,MYF(0)));
    /* fall through */
  case 1:
    VOID(my_close(file,MYF(0)));
  }
  my_errno=save_errno;				/* R{tt felkod tillbaka */
  DBUG_RETURN(-1);
} /* nisam_create */
