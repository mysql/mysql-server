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


static void set_max_sort_char(CHARSET_INFO *cs);
static my_bool create_fromuni(CHARSET_INFO *cs);


#define MY_CHARSET_INDEX "Index.xml"

const char *charsets_dir= NULL;
static int charset_initialized=0;

#define MAX_LINE  1024

#define CTYPE_TABLE_SIZE      257
#define TO_LOWER_TABLE_SIZE   256
#define TO_UPPER_TABLE_SIZE   256
#define SORT_ORDER_TABLE_SIZE 256
#define TO_UNI_TABLE_SIZE     256


char *get_charsets_dir(char *buf)
{
  const char *sharedir= SHAREDIR;
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
  convert_dirname(buf,buf,NullS);
  DBUG_PRINT("info",("charsets dir: '%s'", buf));
  DBUG_RETURN(strend(buf));
}


#define MAX_BUF 1024*16

#ifndef DBUG_OFF
static void mstr(char *str,const char *src,uint l1,uint l2)
{
  l1= l1<l2 ? l1 : l2;
  memcpy(str,src,l1);
  str[l1]='\0';
}
#endif


struct my_cs_file_section_st
{
  int        state;
  const char *str;
};

#define _CS_MISC	1
#define _CS_ID		2
#define _CS_NAME	3
#define _CS_FAMILY	4
#define _CS_ORDER	5
#define _CS_COLNAME	6
#define _CS_FLAG	7
#define _CS_CHARSET	8
#define _CS_COLLATION	9
#define _CS_UPPERMAP	10
#define _CS_LOWERMAP	11
#define _CS_UNIMAP	12
#define _CS_COLLMAP	13
#define _CS_CTYPEMAP	14

static struct my_cs_file_section_st sec[] =
{
  {_CS_MISC,		"xml"},
  {_CS_MISC,		"xml.version"},
  {_CS_MISC,		"xml.encoding"},
  {_CS_MISC,		"charsets"},
  {_CS_MISC,		"charsets.max-id"},
  {_CS_MISC,		"charsets.description"},
  {_CS_CHARSET,		"charsets.charset"},
  {_CS_NAME,		"charsets.charset.name"},
  {_CS_FAMILY,		"charsets.charset.family"},
  {_CS_MISC,		"charsets.charset.alias"},
  {_CS_MISC,		"charsets.charset.ctype"},
  {_CS_CTYPEMAP,	"charsets.charset.ctype.map"},
  {_CS_MISC,		"charsets.charset.upper"},
  {_CS_UPPERMAP,	"charsets.charset.upper.map"},
  {_CS_MISC,		"charsets.charset.lower"},
  {_CS_LOWERMAP,	"charsets.charset.lower.map"},
  {_CS_MISC,		"charsets.charset.unicode"},
  {_CS_UNIMAP,		"charsets.charset.unicode.map"},
  {_CS_COLLATION,	"charsets.charset.collation"},
  {_CS_COLNAME,		"charsets.charset.collation.name"},
  {_CS_ID,		"charsets.charset.collation.id"},
  {_CS_ORDER,		"charsets.charset.collation.order"},
  {_CS_FLAG,		"charsets.charset.collation.flag"},
  {_CS_COLLMAP,		"charsets.charset.collation.map"},
  {0,	NULL}
};

static struct my_cs_file_section_st * cs_file_sec(const char *attr, uint len)
{
  struct my_cs_file_section_st *s;
  for (s=sec; s->str; s++)
  {
    if (!strncmp(attr,s->str,len))
      return s;
  }
  return NULL;
}

#define CS_MAX_NM_LEN	32

struct my_cs_file_info 
{
  char   csname[CS_MAX_NM_LEN];
  char   name[CS_MAX_NM_LEN];
  uchar  ctype[CTYPE_TABLE_SIZE];
  uchar  to_lower[TO_LOWER_TABLE_SIZE];
  uchar  to_upper[TO_UPPER_TABLE_SIZE];
  uchar  sort_order[SORT_ORDER_TABLE_SIZE];
  uint16 tab_to_uni[TO_UNI_TABLE_SIZE];
  CHARSET_INFO cs;
  myf myflags;
};

static void simple_cs_init_functions(CHARSET_INFO *cs)
{
  cs->like_range  = my_like_range_simple;
  cs->wildcmp     = my_wildcmp_8bit;
  cs->strnncoll   = my_strnncoll_simple;
  cs->caseup_str  = my_caseup_str_8bit;
  cs->casedn_str  = my_casedn_str_8bit;
  cs->caseup      = my_caseup_8bit;
  cs->casedn      = my_casedn_8bit;
  cs->tosort      = my_tosort_8bit;
  cs->strcasecmp  = my_strcasecmp_8bit;
  cs->strncasecmp = my_strncasecmp_8bit;
  cs->mb_wc       = my_mb_wc_8bit;
  cs->wc_mb       = my_wc_mb_8bit;
  cs->hash_caseup = my_hash_caseup_simple;
  cs->hash_sort   = my_hash_sort_simple;
  cs->snprintf	  = my_snprintf_8bit;
  cs->strntol     = my_strntol_8bit;
  cs->strntoul    = my_strntoul_8bit;
  cs->strntoll    = my_strntoll_8bit;
  cs->strntoull   = my_strntoull_8bit;
  cs->strntod     = my_strntod_8bit;
  cs->mbmaxlen    = 1;
}


static void simple_cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->number= from->number ? from->number : to->number;
  to->state|= from->state;

  if (from->csname)
    to->csname= my_once_strdup(from->csname,MYF(MY_WME));
  
  if (from->name)
    to->name= my_once_strdup(from->name,MYF(MY_WME));
  
  if (from->ctype)
    to->ctype= (uchar*) my_once_memdup((char*) from->ctype,
				       CTYPE_TABLE_SIZE, MYF(MY_WME));
  if (from->to_lower)
    to->to_lower= (uchar*) my_once_memdup((char*) from->to_lower,
					  TO_LOWER_TABLE_SIZE, MYF(MY_WME));
  if (from->to_upper)
    to->to_upper= (uchar*) my_once_memdup((char*) from->to_upper,
					  TO_UPPER_TABLE_SIZE, MYF(MY_WME));
  if (from->sort_order)
  {
    to->sort_order= (uchar*) my_once_memdup((char*) from->sort_order,
					    SORT_ORDER_TABLE_SIZE,
					    MYF(MY_WME));
    set_max_sort_char(to);
  }
  if (from->tab_to_uni)
  {
    uint sz=TO_UNI_TABLE_SIZE*sizeof(uint16);
    to->tab_to_uni= (uint16*)  my_once_memdup((char*)from->tab_to_uni, sz,
					     MYF(MY_WME));
    create_fromuni(to);
  }
}


static my_bool simple_cs_is_full(CHARSET_INFO *cs)
{
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
	   cs->to_lower) &&
	  (cs->number && cs->name && cs->sort_order));
}


static int fill_uchar(uchar *a,uint size,const char *str, uint len)
{
  uint i= 0;
  const char *s, *b, *e=str+len;
  
  for (s=str ; s < e ; i++)
  { 
    for ( ; (s < e) && strchr(" \t\r\n",s[0]); s++) ;
    b=s;
    for ( ; (s < e) && !strchr(" \t\r\n",s[0]); s++) ;
    if (s == b || i > size)
      break;
    a[i]= my_strntoul(my_charset_latin1,b,s-b,NULL,16);
  }
  return 0;
}

static int fill_uint16(uint16 *a,uint size,const char *str, uint len)
{
  uint i= 0;
  const char *s, *b, *e=str+len;
  for (s=str ; s < e ; i++)
  { 
    for ( ; (s < e) && strchr(" \t\r\n",s[0]); s++) ;
    b=s;
    for ( ; (s < e) && !strchr(" \t\r\n",s[0]); s++) ;
    if (s == b || i > size)
      break;
    a[i]= my_strntol(my_charset_latin1,b,s-b,NULL,16);
  }
  return 0;
}


static int cs_enter(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s= cs_file_sec(attr,len);
  
  if ( s && (s->state == _CS_CHARSET))
  {
    bzero(&i->cs,sizeof(i->cs));
  }
  return MY_XML_OK;
}


static int cs_leave(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s= cs_file_sec(attr,len);
  int    state= s ? s->state : 0;
  
  if (state == _CS_COLLATION)
  {
    if (i->cs.name && (i->cs.number ||
		       (i->cs.number=get_charset_number(i->cs.name))))
    {
      if (!all_charsets[i->cs.number])
      {
        if (!(all_charsets[i->cs.number]=
           (CHARSET_INFO*) my_once_alloc(sizeof(CHARSET_INFO),i->myflags)))
          return MY_XML_ERROR;
        bzero((void*)all_charsets[i->cs.number],sizeof(CHARSET_INFO));
      }
      
      if (!(all_charsets[i->cs.number]->state & MY_CS_COMPILED))
      {
        simple_cs_copy_data(all_charsets[i->cs.number],&i->cs);
        if (simple_cs_is_full(all_charsets[i->cs.number]))
        {
          simple_cs_init_functions(all_charsets[i->cs.number]);
          all_charsets[i->cs.number]->state |= MY_CS_LOADED;
        }
      }
      i->cs.number= 0;
      i->cs.name= NULL;
      i->cs.state= 0;
      i->cs.sort_order= NULL;
      i->cs.state= 0;
    }
  }
  return MY_XML_OK;
}


static int cs_value(MY_XML_PARSER *st,const char *attr, uint len)
{
  struct my_cs_file_info *i= (struct my_cs_file_info *)st->user_data;
  struct my_cs_file_section_st *s;
  int    state= (s=cs_file_sec(st->attr,strlen(st->attr))) ? s->state : 0;
  
#ifndef DBUG_OFF
  if(0){
    char   str[1024];
    mstr(str,attr,len,sizeof(str)-1);
    printf("VALUE %d %s='%s'\n",state,st->attr,str);
  }
#endif
  
  switch (state) {
  case _CS_ID:
    i->cs.number= my_strntoul(my_charset_latin1,attr,len,(char**)NULL,0);
    break;
  case _CS_COLNAME:
    memcpy(i->name,attr,len=min(len,CS_MAX_NM_LEN-1));
    i->name[len]='\0';
    i->cs.name=i->name;
    break;
  case _CS_NAME:
    memcpy(i->csname,attr,len=min(len,CS_MAX_NM_LEN-1));
    i->csname[len]='\0';
    i->cs.csname=i->csname;
    break;
  case _CS_FLAG:
    if (!strncmp("primary",attr,len))
      i->cs.state|= MY_CS_PRIMARY;
    break;
  case _CS_UPPERMAP:
    fill_uchar(i->to_upper,TO_UPPER_TABLE_SIZE,attr,len);
    i->cs.to_upper=i->to_upper;
    break;
  case _CS_LOWERMAP:
    fill_uchar(i->to_lower,TO_LOWER_TABLE_SIZE,attr,len);
    i->cs.to_lower=i->to_lower;
    break;
  case _CS_UNIMAP:
    fill_uint16(i->tab_to_uni,TO_UNI_TABLE_SIZE,attr,len);
    i->cs.tab_to_uni=i->tab_to_uni;
    break;
  case _CS_COLLMAP:
    fill_uchar(i->sort_order,SORT_ORDER_TABLE_SIZE,attr,len);
    i->cs.sort_order=i->sort_order;
    break;
  case _CS_CTYPEMAP:
    fill_uchar(i->ctype,CTYPE_TABLE_SIZE,attr,len);
    i->cs.ctype=i->ctype;
    break;
  }
  return MY_XML_OK;
}


static my_bool read_charset_index(const char *filename, myf myflags)
{
  char *buf;
  int  fd;
  uint len;
  MY_XML_PARSER p;
  struct my_cs_file_info i;
  
  if (!(buf= (char *)my_malloc(MAX_BUF,myflags)))
    return FALSE;
  
  strmov(get_charsets_dir(buf), filename);
  
  if ((fd=my_open(buf,O_RDONLY,myflags)) < 0)
  {
    my_free(buf,myflags);
    return TRUE;
  }
  
  len=read(fd,buf,MAX_BUF);
  my_xml_parser_create(&p);
  my_close(fd,myflags);
  
  my_xml_set_enter_handler(&p,cs_enter);
  my_xml_set_value_handler(&p,cs_value);
  my_xml_set_leave_handler(&p,cs_leave);
  my_xml_set_user_data(&p,(void*)&i);
  
  if (my_xml_parse(&p,buf,len) != MY_XML_OK)
  {
#ifdef NOT_YET
    printf("ERROR at line %d pos %d '%s'\n",
	   my_xml_error_lineno(&p)+1,
	   my_xml_error_pos(&p),
	   my_xml_error_string(&p));
#endif
  }

  my_xml_parser_free(&p);
  my_free(buf, myflags);  
  return FALSE;
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

static my_bool init_available_charsets(myf myflags)
{
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
        set_max_sort_char(*cs);
    }
    error= read_charset_index(MY_CHARSET_INDEX,myflags);
    charset_initialized=1;
    pthread_mutex_unlock(&THR_LOCK_charset);
  }
  return error;
}


void free_charsets(void)
{
  charset_initialized=0;
}


static void get_charset_conf_name(const char *cs_name, char *buf)
{
  strxmov(get_charsets_dir(buf), cs_name, ".conf", NullS);
}


typedef struct
{
  int		nchars;
  MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
#define PLANE_NUMBER(x)	(((x)>>8) % PLANE_NUM)

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


uint get_charset_number(const char *charset_name)
{
  CHARSET_INFO **cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return 0;
  
  for (cs= all_charsets; cs < all_charsets+255; ++cs)
  {
    if ( cs[0] && cs[0]->name && !strcmp(cs[0]->name, charset_name))
      return cs[0]->number;
  }  
  return 0;   /* this mimics find_type() */
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

  if (cs && !(cs->state & (MY_CS_COMPILED | MY_CS_LOADED)))
  {
     strxmov(buf, cs->csname, ".xml", NullS);
     read_charset_index(buf,flags);
     cs= (cs->state & MY_CS_LOADED) ? cs : NULL;
  }
  pthread_mutex_unlock(&THR_LOCK_charset);
  return cs;
}


static CHARSET_INFO *get_internal_charset_by_name(const char *name, myf flags)
{
  uint cs_number=get_charset_number(name);
  return cs_number ? get_internal_charset(cs_number,flags) : NULL;
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

my_bool set_default_charset(uint cs, myf flags)
{
  CHARSET_INFO *new_charset;
  DBUG_ENTER("set_default_charset");
  DBUG_PRINT("enter",("character set: %d",(int) cs));

  new_charset= get_charset(cs, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }
  default_charset_info= new_charset;
  system_charset_info= new_charset;

  DBUG_RETURN(FALSE);
}

CHARSET_INFO *get_charset_by_name(const char *cs_name, myf flags)
{
  CHARSET_INFO *cs;
  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  cs=get_internal_charset_by_name(cs_name, flags);

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}


my_bool set_default_charset_by_name(const char *cs_name, myf flags)
{
  CHARSET_INFO *new_charset;
  DBUG_ENTER("set_default_charset_by_name");
  DBUG_PRINT("enter",("character set: %s", cs_name));

  new_charset= get_charset_by_name(cs_name, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }

  default_charset_info= new_charset;
  system_charset_info= new_charset;
  DBUG_RETURN(FALSE);
}


/* Only append name if it doesn't exist from before */

static my_bool charset_in_string(const char *name, DYNAMIC_STRING *s)
{
  uint length= (uint) strlen(name);
  const char *pos;
  for (pos=s->str ; (pos=strstr(pos,name)) ; pos++)
  {
    if (! pos[length] || pos[length] == ' ')
      return TRUE;				/* Already existed */
  }
  return FALSE;
}


static void charset_append(DYNAMIC_STRING *s, const char *name)
{
  if (!charset_in_string(name, s))
  {
    dynstr_append(s, name);
    dynstr_append(s, " ");
  }
}


/* Returns a dynamically-allocated string listing the character sets
   requested.  The caller is responsible for freeing the memory. */

char *list_charsets(myf want_flags)
{
  DYNAMIC_STRING s;
  char *p;

  (void)init_available_charsets(MYF(0));
  init_dynamic_string(&s, NullS, 256, 1024);

  if (want_flags & MY_CS_COMPILED)
  {
    CHARSET_INFO **cs;
    for (cs= all_charsets; cs < all_charsets+255; cs++)
    {
      if (cs[0])
      {
        dynstr_append(&s, cs[0]->name);
        dynstr_append(&s, " ");
      }
    }
  }

  if (want_flags & MY_CS_CONFIG)
  {
    CHARSET_INFO **cs;
    char buf[FN_REFLEN];
    MY_STAT status;

    for (cs=all_charsets; cs < all_charsets+255; cs++)
    {
      if (!cs[0] || !cs[0]->name || charset_in_string(cs[0]->name, &s))
	continue;
      get_charset_conf_name(cs[0]->name, buf);
      if (!my_stat(buf, &status, MYF(0)))
	continue;       /* conf file doesn't exist */
      dynstr_append(&s, cs[0]->name);
      dynstr_append(&s, " ");
    }
  }

  if (want_flags & (MY_CS_INDEX|MY_CS_LOADED))
  {
    CHARSET_INFO **cs;
    for (cs= all_charsets; cs < all_charsets + 255; cs++)
      if (cs[0] && cs[0]->name && (cs[0]->state & want_flags) )
        charset_append(&s, cs[0]->name);
  }
  
  if (s.length)
  {
    s.str[s.length - 1]= '\0';   /* chop trailing space */
    p= my_strdup(s.str, MYF(MY_WME));
  }
  else
  {
    p= my_strdup("", MYF(MY_WME));
  }
  dynstr_free(&s);
  
  return p;
}
