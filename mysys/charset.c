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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_ctype.h>
#include <m_string.h>
#include <my_dir.h>
#include <my_xml.h>

typedef struct
{
  int		nchars;
  MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
#define PLANE_NUMBER(x)	(((x)>>8) % PLANE_NUM)


/*
  The code below implements this functionality:
  
    - Initializing charset related structures
    - Loading dynamic charsets
    - Searching for a proper CHARSET_INFO 
      using charset name, collation name or collation ID
    - Setting server default character set
*/

my_bool my_charset_same(CHARSET_INFO *cs1, CHARSET_INFO *cs2)
{
  return ((cs1 == cs2) || !strcmp(cs1->csname,cs2->csname));
}


static void set_max_sort_char(CHARSET_INFO *cs)
{
  uchar max_char;
  uint  i;
  
  if (!cs->sort_order)
    return;
  
  max_char=cs->sort_order[(uchar) cs->max_sort_char];
  for (i= 0; i < 256; i++)
  {
    if ((uchar) cs->sort_order[i] > max_char)
    {
      max_char=(uchar) cs->sort_order[i];
      cs->max_sort_char= (char) i;
    }
  }
}


static void init_state_maps(CHARSET_INFO *cs)
{
  uint i;
  uchar *state_map= cs->state_map;
  uchar *ident_map= cs->ident_map;

  /* Fill state_map with states to get a faster parser */
  for (i=0; i < 256 ; i++)
  {
    if (my_isalpha(cs,i))
      state_map[i]=(uchar) MY_LEX_IDENT;
    else if (my_isdigit(cs,i))
      state_map[i]=(uchar) MY_LEX_NUMBER_IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
    else if (use_mb(cs) && (my_mbcharlen(cs, i)>1))
      state_map[i]=(uchar) MY_LEX_IDENT;
#endif
    else if (!my_isgraph(cs,i))
      state_map[i]=(uchar) MY_LEX_SKIP;
    else
      state_map[i]=(uchar) MY_LEX_CHAR;
  }
  state_map[(uchar)'_']=state_map[(uchar)'$']=(uchar) MY_LEX_IDENT;
  state_map[(uchar)'\'']=(uchar) MY_LEX_STRING;
  state_map[(uchar)'.']=(uchar) MY_LEX_REAL_OR_POINT;
  state_map[(uchar)'>']=state_map[(uchar)'=']=state_map[(uchar)'!']= (uchar) MY_LEX_CMP_OP;
  state_map[(uchar)'<']= (uchar) MY_LEX_LONG_CMP_OP;
  state_map[(uchar)'&']=state_map[(uchar)'|']=(uchar) MY_LEX_BOOL;
  state_map[(uchar)'#']=(uchar) MY_LEX_COMMENT;
  state_map[(uchar)';']=(uchar) MY_LEX_COLON;
  state_map[(uchar)':']=(uchar) MY_LEX_SET_VAR;
  state_map[0]=(uchar) MY_LEX_EOL;
  state_map[(uchar)'\\']= (uchar) MY_LEX_ESCAPE;
  state_map[(uchar)'/']= (uchar) MY_LEX_LONG_COMMENT;
  state_map[(uchar)'*']= (uchar) MY_LEX_END_LONG_COMMENT;
  state_map[(uchar)'@']= (uchar) MY_LEX_USER_END;
  state_map[(uchar) '`']= (uchar) MY_LEX_USER_VARIABLE_DELIMITER;
  state_map[(uchar)'"']= (uchar) MY_LEX_STRING_OR_DELIMITER;

  /*
    Create a second map to make it faster to find identifiers
  */
  for (i=0; i < 256 ; i++)
  {
    ident_map[i]= (uchar) (state_map[i] == MY_LEX_IDENT ||
			   state_map[i] == MY_LEX_NUMBER_IDENT);
  }

  /* Special handling of hex and binary strings */
  state_map[(uchar)'x']= state_map[(uchar)'X']= (uchar) MY_LEX_IDENT_OR_HEX;
  state_map[(uchar)'b']= state_map[(uchar)'b']= (uchar) MY_LEX_IDENT_OR_BIN;
  state_map[(uchar)'n']= state_map[(uchar)'N']= (uchar) MY_LEX_IDENT_OR_NCHAR;
}


static void simple_cs_init_functions(CHARSET_INFO *cs)
{
  if (cs->state & MY_CS_BINSORT)
    cs->coll= &my_collation_8bit_bin_handler;
  else
    cs->coll= &my_collation_8bit_simple_ci_handler;
  
  cs->cset= &my_charset_8bit_handler;
  cs->mbmaxlen    = 1;
}


static int pcmp(const void * f, const void * s)
{
  const uni_idx *F= (const uni_idx*) f;
  const uni_idx *S= (const uni_idx*) s;
  int res;

  if (!(res=((S->nchars)-(F->nchars))))
    res=((F->uidx.from)-(S->uidx.to));
  return res;
}


static my_bool create_fromuni(CHARSET_INFO *cs)
{
  uni_idx	idx[PLANE_NUM];
  int		i,n;
  
  /* Clear plane statistics */
  bzero(idx,sizeof(idx));
  
  /* Count number of characters in each plane */
  for (i=0; i< 0x100; i++)
  {
    uint16 wc=cs->tab_to_uni[i];
    int pl= PLANE_NUMBER(wc);
    
    if (wc || !i)
    {
      if (!idx[pl].nchars)
      {
        idx[pl].uidx.from=wc;
        idx[pl].uidx.to=wc;
      }else
      {
        idx[pl].uidx.from=wc<idx[pl].uidx.from?wc:idx[pl].uidx.from;
        idx[pl].uidx.to=wc>idx[pl].uidx.to?wc:idx[pl].uidx.to;
      }
      idx[pl].nchars++;
    }
  }
  
  /* Sort planes in descending order */
  qsort(&idx,PLANE_NUM,sizeof(uni_idx),&pcmp);
  
  for (i=0; i < PLANE_NUM; i++)
  {
    int ch,numchars;
    
    /* Skip empty plane */
    if (!idx[i].nchars)
      break;
    
    numchars=idx[i].uidx.to-idx[i].uidx.from+1;
    idx[i].uidx.tab=(unsigned char*)my_once_alloc(numchars *
						  sizeof(*idx[i].uidx.tab),
						  MYF(MY_WME));
    bzero(idx[i].uidx.tab,numchars*sizeof(*idx[i].uidx.tab));
    
    for (ch=1; ch < PLANE_SIZE; ch++)
    {
      uint16 wc=cs->tab_to_uni[ch];
      if (wc >= idx[i].uidx.from && wc <= idx[i].uidx.to && wc)
      {
        int ofs= wc - idx[i].uidx.from;
        idx[i].uidx.tab[ofs]= ch;
      }
    }
  }
  
  /* Allocate and fill reverse table for each plane */
  n=i;
  cs->tab_from_uni= (MY_UNI_IDX*) my_once_alloc(sizeof(MY_UNI_IDX)*(n+1),
					       MYF(MY_WME));
  for (i=0; i< n; i++)
    cs->tab_from_uni[i]= idx[i].uidx;
  
  /* Set end-of-list marker */
  bzero(&cs->tab_from_uni[i],sizeof(MY_UNI_IDX));
  return FALSE;
}


static void simple_cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->number= from->number ? from->number : to->number;

  if (from->csname)
    to->csname= my_once_strdup(from->csname,MYF(MY_WME));
  
  if (from->name)
    to->name= my_once_strdup(from->name,MYF(MY_WME));
  
  if (from->comment)
    to->comment= my_once_strdup(from->comment,MYF(MY_WME));
  
  if (from->ctype)
  {
    to->ctype= (uchar*) my_once_memdup((char*) from->ctype,
				       MY_CS_CTYPE_TABLE_SIZE, MYF(MY_WME));
    init_state_maps(to);
  }
  if (from->to_lower)
    to->to_lower= (uchar*) my_once_memdup((char*) from->to_lower,
					  MY_CS_TO_LOWER_TABLE_SIZE, MYF(MY_WME));
  if (from->to_upper)
    to->to_upper= (uchar*) my_once_memdup((char*) from->to_upper,
					  MY_CS_TO_UPPER_TABLE_SIZE, MYF(MY_WME));
  if (from->sort_order)
  {
    to->sort_order= (uchar*) my_once_memdup((char*) from->sort_order,
					    MY_CS_SORT_ORDER_TABLE_SIZE,
					    MYF(MY_WME));
    set_max_sort_char(to);
  }
  if (from->tab_to_uni)
  {
    uint sz= MY_CS_TO_UNI_TABLE_SIZE*sizeof(uint16);
    to->tab_to_uni= (uint16*)  my_once_memdup((char*)from->tab_to_uni, sz,
					     MYF(MY_WME));
    create_fromuni(to);
  }
  to->mbmaxlen= 1;
}


static my_bool simple_cs_is_full(CHARSET_INFO *cs)
{
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
	   cs->to_lower) &&
	  (cs->number && cs->name &&
	  (cs->sort_order || (cs->state & MY_CS_BINSORT) )));
}


static int add_collation(CHARSET_INFO *cs)
{
  if (cs->name && (cs->number || (cs->number=get_collation_number(cs->name))))
  {
    if (!all_charsets[cs->number])
    {
      if (!(all_charsets[cs->number]=
         (CHARSET_INFO*) my_once_alloc(sizeof(CHARSET_INFO),MYF(0))))
        return MY_XML_ERROR;
      bzero((void*)all_charsets[cs->number],sizeof(CHARSET_INFO));
    }
    
    if (cs->primary_number == cs->number)
      cs->state |= MY_CS_PRIMARY;
      
    if (cs->binary_number == cs->number)
      cs->state |= MY_CS_BINSORT;
    
    all_charsets[cs->number]->state|= cs->state;
    
    if (!(all_charsets[cs->number]->state & MY_CS_COMPILED))
    {
      simple_cs_init_functions(all_charsets[cs->number]);
      simple_cs_copy_data(all_charsets[cs->number],cs);
      if (simple_cs_is_full(all_charsets[cs->number]))
      {
        all_charsets[cs->number]->state |= MY_CS_LOADED;
      }
      all_charsets[cs->number]->state|= MY_CS_AVAILABLE;
    }
    else
    {
      /*
        We need the below to make get_charset_name()
        and get_charset_number() working even if a
        character set has not been really incompiled.
        The above functions are used for example
        in error message compiler extra/comp_err.c.
        If a character set was compiled, this information
        will get lost and overwritten in add_compiled_collation().
      */
      CHARSET_INFO *dst= all_charsets[cs->number];
      dst->number= cs->number;
      if (cs->comment)
	dst->comment= my_once_strdup(cs->comment,MYF(MY_WME));
      if (cs->csname)
        dst->csname= my_once_strdup(cs->csname,MYF(MY_WME));
      if (cs->name)
        dst->name= my_once_strdup(cs->name,MYF(MY_WME));
    }
    cs->number= 0;
    cs->primary_number= 0;
    cs->binary_number= 0;
    cs->name= NULL;
    cs->state= 0;
    cs->sort_order= NULL;
    cs->state= 0;
  }
  return MY_XML_OK;
}


#define MAX_BUF 1024*16
#define MY_CHARSET_INDEX "Index.xml"

const char *charsets_dir= NULL;
static int charset_initialized=0;


static my_bool my_read_charset_file(const char *filename, myf myflags)
{
  char *buf;
  int  fd;
  uint len;
  
  if (!(buf= (char *)my_malloc(MAX_BUF,myflags)))
    return FALSE;
  
  if ((fd=my_open(filename,O_RDONLY,myflags)) < 0)
  {
    my_free(buf,myflags);
    return TRUE;
  }
  len=read(fd,buf,MAX_BUF);
  my_close(fd,myflags);
  
  if (my_parse_charset_xml(buf,len,add_collation))
  {
#ifdef NOT_YET
    printf("ERROR at line %d pos %d '%s'\n",
	   my_xml_error_lineno(&p)+1,
	   my_xml_error_pos(&p),
	   my_xml_error_string(&p));
#endif
  }
  
  my_free(buf, myflags);  
  return FALSE;
}


char *get_charsets_dir(char *buf)
{
  const char *sharedir= SHAREDIR;
  char *res;
  DBUG_ENTER("get_charsets_dir");

  if (charsets_dir != NULL)
    strmake(buf, charsets_dir, FN_REFLEN-1);
  else
  {
    if (test_if_hard_path(sharedir) ||
	is_prefix(sharedir, DEFAULT_CHARSET_HOME))
      strxmov(buf, sharedir, "/", CHARSET_DIR, NullS);
    else
      strxmov(buf, DEFAULT_CHARSET_HOME, "/", sharedir, "/", CHARSET_DIR,
	      NullS);
  }
  res= convert_dirname(buf,buf,NullS);
  DBUG_PRINT("info",("charsets dir: '%s'", buf));
  DBUG_RETURN(res);
}

CHARSET_INFO *all_charsets[256];
CHARSET_INFO *default_charset_info = &my_charset_latin1;

void add_compiled_collation(CHARSET_INFO *cs)
{
  all_charsets[cs->number]= cs;
  cs->state|= MY_CS_AVAILABLE;
}



#ifdef __NETWARE__
my_bool STDCALL init_available_charsets(myf myflags)
#else
static my_bool init_available_charsets(myf myflags)
#endif
{
  char fname[FN_REFLEN];
  my_bool error=FALSE;
  /*
    We have to use charset_initialized to not lock on THR_LOCK_charset
    inside get_internal_charset...
  */
  if (!charset_initialized)
  {
    CHARSET_INFO **cs;
    /*
      To make things thread safe we are not allowing other threads to interfere
      while we may changing the cs_info_table
    */
    pthread_mutex_lock(&THR_LOCK_charset);

    bzero(&all_charsets,sizeof(all_charsets));
    init_compiled_charsets(myflags);
    
    /* Copy compiled charsets */
    for (cs=all_charsets; cs < all_charsets+255 ; cs++)
    {
      if (*cs)
      {
        set_max_sort_char(*cs);
        init_state_maps(*cs);
      }
    }
    
    strmov(get_charsets_dir(fname), MY_CHARSET_INDEX);
    error= my_read_charset_file(fname,myflags);
    charset_initialized=1;
    pthread_mutex_unlock(&THR_LOCK_charset);
  }
  return error;
}


void free_charsets(void)
{
  charset_initialized=0;
}


uint get_collation_number(const char *name)
{
  CHARSET_INFO **cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return 0;
  
  for (cs= all_charsets; cs < all_charsets+255; ++cs)
  {
    if ( cs[0] && cs[0]->name && 
         !my_strcasecmp(&my_charset_latin1, cs[0]->name, name))
      return cs[0]->number;
  }  
  return 0;   /* this mimics find_type() */
}

uint get_charset_number(const char *charset_name, uint cs_flags)
{
  CHARSET_INFO **cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return 0;
  
  for (cs= all_charsets; cs < all_charsets+255; ++cs)
  {
    if ( cs[0] && cs[0]->csname && (cs[0]->state & cs_flags) &&
         !my_strcasecmp(&my_charset_latin1, cs[0]->csname, charset_name))
      return cs[0]->number;
  }  
  return 0;
}


const char *get_charset_name(uint charset_number)
{
  CHARSET_INFO *cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return "?";

  cs=all_charsets[charset_number];
  if (cs && (cs->number == charset_number) && cs->name )
    return (char*) cs->name;
  
  return (char*) "?";   /* this mimics find_type() */
}


static CHARSET_INFO *get_internal_charset(uint cs_number, myf flags)
{
  char  buf[FN_REFLEN];
  CHARSET_INFO *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
  pthread_mutex_lock(&THR_LOCK_charset);

  cs= all_charsets[cs_number];

  if (cs && !(cs->state & MY_CS_COMPILED) && !(cs->state & MY_CS_LOADED))
  {
     strxmov(get_charsets_dir(buf), cs->csname, ".xml", NullS);
     my_read_charset_file(buf,flags);
     cs= (cs->state & MY_CS_LOADED) ? cs : NULL;
  }
  pthread_mutex_unlock(&THR_LOCK_charset);
  return cs;
}


CHARSET_INFO *get_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  
  if (!cs_number)
    return NULL;
  
  cs=get_internal_charset(cs_number, flags);

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN], cs_string[23];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    cs_string[0]='#';
    int10_to_str(cs_number, cs_string+1, 10);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_string, index_file);
  }
  return cs;
}

CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags)
{
  uint cs_number;
  CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */

  cs_number=get_collation_number(cs_name);
  cs= cs_number ? get_internal_charset(cs_number,flags) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}


CHARSET_INFO *get_charset_by_csname(const char *cs_name,
				    uint cs_flags,
				    myf flags)
{
  uint cs_number;
  CHARSET_INFO *cs;
  DBUG_ENTER("get_charset_by_csname");
  DBUG_PRINT("enter",("name: '%s'", cs_name));

  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  
  cs_number= get_charset_number(cs_name, cs_flags);
  cs= cs_number ? get_internal_charset(cs_number, flags) : NULL;
  
  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  DBUG_RETURN(cs);
}
