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


const char *charsets_dir = NULL;
static int charset_initialized=0;
CHARSET_INFO all_charsets[256];

#define MAX_LINE  1024

#define CTYPE_TABLE_SIZE      257
#define TO_LOWER_TABLE_SIZE   256
#define TO_UPPER_TABLE_SIZE   256
#define SORT_ORDER_TABLE_SIZE 256
#define TO_UNI_TABLE_SIZE     256

struct simpleconfig_buf_st {
  FILE *f;
  char  buf[MAX_LINE];
  char *p;
};


static my_bool get_word(struct simpleconfig_buf_st *fb, char *buf)
{
  char *endptr=fb->p;

  for (;;)
  {
    while (my_isspace(system_charset_info, *endptr))
      ++endptr;
    if (*endptr && *endptr != '#')		/* Not comment */
      break;					/* Found something */
    if ((fgets(fb->buf, sizeof(fb->buf), fb->f)) == NULL)
      return TRUE; /* end of file */
    endptr = fb->buf;
  }

  while (!my_isspace(system_charset_info, *endptr))
    *buf++= *endptr++;
  *buf=0;
  fb->p = endptr;

  return FALSE;
}


char *get_charsets_dir(char *buf)
{
  const char *sharedir = SHAREDIR;
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
  DBUG_PRINT("info",("charsets dir='%s'", buf));
  DBUG_RETURN(strend(buf));
}


static my_bool read_charset_index(myf myflags)
{
  struct simpleconfig_buf_st fb;
  char buf[MAX_LINE], num_buf[MAX_LINE];
    
  strmov(get_charsets_dir(buf), "Index");

  if ((fb.f = my_fopen(buf, O_RDONLY, myflags)) == NULL)
    return TRUE;
  fb.buf[0] = '\0';
  fb.p = fb.buf;

  
  while (!get_word(&fb, buf) && !get_word(&fb, num_buf))
  {
    uint csnum;
    uint length;
    CHARSET_INFO *cs;

    if (!(csnum = atoi(num_buf)))
    {
      /* corrupt Index file */
      my_fclose(fb.f,myflags);
      return TRUE;
    }

    cs=&all_charsets[csnum];
    
    if (!(cs->name=
           (char*) my_once_alloc(length= (uint) strlen(buf)+1, myflags)))
    {
      my_fclose(fb.f,myflags);
      return TRUE;
    }
    memcpy((char*)cs->name,buf,length);
    cs->number=csnum;
  }
  my_fclose(fb.f,myflags);

  return FALSE;
}

static void set_max_sort_char(CHARSET_INFO *cs)
{
  uchar max_char;
  uint  i;
  
  if (!cs->sort_order)
    return;
  
  max_char=cs->sort_order[(uchar) cs->max_sort_char];
  for (i = 0; i < 256; i++)
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
    CHARSET_INFO *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
    pthread_mutex_lock(&THR_LOCK_charset);

    bzero(&all_charsets,sizeof(all_charsets));
    
    /* Copy compiled charsets */
    
    for (cs=compiled_charsets; cs->name; cs++)
    {
      all_charsets[cs->number]=cs[0];
      set_max_sort_char(&all_charsets[cs->number]);
    }
    error = read_charset_index(myflags);
    charset_initialized=1;
    pthread_mutex_unlock(&THR_LOCK_charset);
  }
  return error;
}


void free_charsets(void)
{
  charset_initialized=0;
}


static my_bool fill_array(uchar *array, int sz, struct simpleconfig_buf_st *fb)
{
  char buf[MAX_LINE];
  while (sz--)
  {
    if (get_word(fb, buf))
    {
      DBUG_PRINT("error",("get_word failed, expecting %d more words", sz + 1));
      return 1;
    }
    *array++ = (uchar) strtol(buf, NULL, 16);
  }
  return 0;
}

static my_bool fill_uint16_array(uint16 *array, int sz, struct simpleconfig_buf_st *fb)
{
  char buf[MAX_LINE];
  while (sz--)
  {
    if (get_word(fb, buf))
    {
      DBUG_PRINT("error",("get_word failed, expecting %d more words", sz + 1));
      return 1;
    }
    *array++ = (uint16) strtol(buf, NULL, 16);
  }
  return 0;
}


static void get_charset_conf_name(const char *cs_name, char *buf)
{
  strxmov(get_charsets_dir(buf), cs_name, ".conf", NullS);
}

typedef struct {
	int		nchars;
	MY_UNI_IDX	uidx;
} uni_idx;

#define PLANE_SIZE	0x100
#define PLANE_NUM	0x100
#define PLANE_NUMBER(x)	(((x)>>8) % PLANE_NUM)

static int pcmp(const void * f, const void * s)
{
  const uni_idx *F=(const uni_idx*)f;
  const uni_idx *S=(const uni_idx*)s;
  int res;

  if(!(res=((S->nchars)-(F->nchars))))
    res=((F->uidx.from)-(S->uidx.to));
  return res;
}

static my_bool create_fromuni(CHARSET_INFO *cs){
  uni_idx	idx[PLANE_NUM];
  int		i,n;
  
  /* Clear plane statistics */
  bzero(idx,sizeof(idx));
  
  /* Count number of characters in each plane */
  for(i=0;i<0x100;i++)
  {
    uint16 wc=cs->tab_to_uni[i];
    int pl= PLANE_NUMBER(wc);
    
    if(wc || !i)
    {
      if(!idx[pl].nchars)
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
  
  for(i=0;i<PLANE_NUM;i++)
  {
    int ch,numchars;
    
    /* Skip empty plane */
    if(!idx[i].nchars)
      break;
    
    numchars=idx[i].uidx.to-idx[i].uidx.from+1;
    idx[i].uidx.tab=(unsigned char*)my_once_alloc(numchars*sizeof(*idx[i].uidx.tab),MYF(MY_WME));
    bzero(idx[i].uidx.tab,numchars*sizeof(*idx[i].uidx.tab));
    
    for(ch=1;ch<PLANE_SIZE;ch++)
    {
      uint16 wc=cs->tab_to_uni[ch];
      if(wc>=idx[i].uidx.from && wc<=idx[i].uidx.to && wc)
      {
        int ofs=wc-idx[i].uidx.from;
        idx[i].uidx.tab[ofs]=ch;
      }
    }
  }
  
  /* Allocate and fill reverse table for each plane */
  n=i;
  cs->tab_from_uni=(MY_UNI_IDX*)my_once_alloc(sizeof(MY_UNI_IDX)*(n+1),MYF(MY_WME));
  for(i=0;i<n;i++)
    cs->tab_from_uni[i]=idx[i].uidx;
  
  /* Set end-of-list marker */
  bzero(&cs->tab_from_uni[i],sizeof(MY_UNI_IDX));
  return FALSE;
}


static my_bool read_charset_file(const char *cs_name, CHARSET_INFO *set,
				 myf myflags)
{
  struct simpleconfig_buf_st fb;
  char buf[FN_REFLEN];
  my_bool result;
  DBUG_ENTER("read_charset_file");
  DBUG_PRINT("enter",("cs_name: %s", cs_name));

  get_charset_conf_name(cs_name, buf);
  DBUG_PRINT("info",("file name: %s", buf));

  if ((fb.f = my_fopen(buf, O_RDONLY, myflags)) == NULL)
    DBUG_RETURN(TRUE);

  fb.buf[0] = '\0';				/* Init for get_word */
  fb.p = fb.buf;

  result=FALSE;
  if (fill_array(set->ctype,      CTYPE_TABLE_SIZE,      &fb) ||
      fill_array(set->to_lower,   TO_LOWER_TABLE_SIZE,   &fb) ||
      fill_array(set->to_upper,   TO_UPPER_TABLE_SIZE,   &fb) ||
      fill_array(set->sort_order, SORT_ORDER_TABLE_SIZE, &fb) ||
      fill_uint16_array(set->tab_to_uni,TO_UNI_TABLE_SIZE,&fb))
    result=TRUE;

  my_fclose(fb.f, MYF(0));
  create_fromuni(set);
  DBUG_RETURN(result);
}


static CHARSET_INFO *add_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  uchar  tmp_ctype[CTYPE_TABLE_SIZE];
  uchar  tmp_to_lower[TO_LOWER_TABLE_SIZE];
  uchar  tmp_to_upper[TO_UPPER_TABLE_SIZE];
  uchar  tmp_sort_order[SORT_ORDER_TABLE_SIZE];
  uint16 tmp_to_uni[TO_UNI_TABLE_SIZE];

  /* Note: cs->name is already initialized */
  
  cs=&all_charsets[cs_number];

  cs->ctype=tmp_ctype;
  cs->to_lower=tmp_to_lower;
  cs->to_upper=tmp_to_upper;
  cs->sort_order=tmp_sort_order;
  cs->tab_to_uni=tmp_to_uni;
  if (read_charset_file(cs->name, cs, flags))
    return NULL;

  cs->ctype    = (uchar*) my_once_alloc(CTYPE_TABLE_SIZE,      MYF(MY_WME));
  cs->to_lower = (uchar*) my_once_alloc(TO_LOWER_TABLE_SIZE,   MYF(MY_WME));
  cs->to_upper = (uchar*) my_once_alloc(TO_UPPER_TABLE_SIZE,   MYF(MY_WME));
  cs->sort_order=(uchar*) my_once_alloc(SORT_ORDER_TABLE_SIZE, MYF(MY_WME));
  cs->tab_to_uni=(uint16*)my_once_alloc(TO_UNI_TABLE_SIZE*sizeof(uint16), MYF(MY_WME));
  cs->number   = cs_number;
  memcpy((char*) cs->ctype,	 (char*) tmp_ctype,	sizeof(tmp_ctype));
  memcpy((char*) cs->to_lower, (char*) tmp_to_lower,	sizeof(tmp_to_lower));
  memcpy((char*) cs->to_upper, (char*) tmp_to_upper,	sizeof(tmp_to_upper));
  memcpy((char*) cs->sort_order, (char*) tmp_sort_order,
	 sizeof(tmp_sort_order));
  memcpy((char*) cs->tab_to_uni, (char*) tmp_to_uni, sizeof(tmp_to_uni));

  cs->caseup_str  = my_caseup_str_8bit;
  cs->casedn_str  = my_casedn_str_8bit;
  cs->caseup      = my_caseup_8bit;
  cs->casedn      = my_casedn_8bit;
  cs->strcasecmp  = my_strcasecmp_8bit;
  cs->strncasecmp = my_strncasecmp_8bit;
  cs->mb_wc       = my_mb_wc_8bit;
  cs->wc_mb       = my_wc_mb_8bit;
  
  set_max_sort_char(cs);
  
  return cs;
}


uint get_charset_number(const char *charset_name)
{
  CHARSET_INFO *cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return 0;
  
  for (cs = all_charsets; cs < all_charsets+255; ++cs)
    if ( cs->name && !strcmp(cs->name, charset_name))
      return cs->number;
  
  return 0;   /* this mimics find_type() */
}


const char *get_charset_name(uint charset_number)
{
  CHARSET_INFO *cs;
  if (init_available_charsets(MYF(0)))	/* If it isn't initialized */
    return "?";

  cs=&all_charsets[charset_number];
  if ( (cs->number==charset_number) && cs->name )
    return (char*) cs->name;
  
  return (char*) "?";   /* this mimics find_type() */
}


static CHARSET_INFO *get_internal_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  /*
    To make things thread safe we are not allowing other threads to interfere
    while we may changing the cs_info_table
  */
  pthread_mutex_lock(&THR_LOCK_charset);

  cs = &all_charsets[cs_number];
  if (!(cs->state & (MY_CS_COMPILED | MY_CS_LOADED)))
    cs=add_charset(cs_number, flags);

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
    strmov(get_charsets_dir(index_file), "Index");
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
  new_charset = get_charset(cs, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }
  default_charset_info = new_charset;
  system_charset_info = new_charset;
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
    strmov(get_charsets_dir(index_file), "Index");
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  return cs;
}

my_bool set_default_charset_by_name(const char *cs_name, myf flags)
{
  CHARSET_INFO *new_charset;
  DBUG_ENTER("set_default_charset_by_name");
  DBUG_PRINT("enter",("character set: %s", cs_name));
  new_charset = get_charset_by_name(cs_name, flags);
  if (!new_charset)
  {
    DBUG_PRINT("error",("Couldn't set default character set"));
    DBUG_RETURN(TRUE);   /* error */
  }

  default_charset_info = new_charset;
  system_charset_info = new_charset;
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
  if (!charset_in_string(name, s)) {
    dynstr_append(s, name);
    dynstr_append(s, " ");
  }
}


/* Returns a dynamically-allocated string listing the character sets
   requested.  The caller is responsible for freeing the memory. */

char * list_charsets(myf want_flags)
{
  DYNAMIC_STRING s;
  char *p;

  (void)init_available_charsets(MYF(0));
  init_dynamic_string(&s, NullS, 256, 1024);

  if (want_flags & MY_CS_COMPILED)
  {
    CHARSET_INFO *cs;
    for (cs = compiled_charsets; cs->number > 0; cs++)
    {
      dynstr_append(&s, cs->name);
      dynstr_append(&s, " ");
    }
  }

  if (want_flags & MY_CS_CONFIG)
  {
    CHARSET_INFO *cs;
    char buf[FN_REFLEN];
    MY_STAT status;

    for (cs=all_charsets; cs < all_charsets+255; cs++)
    {
      if (!cs->name || charset_in_string(cs->name, &s))
	continue;
      get_charset_conf_name(cs->name, buf);
      if (!my_stat(buf, &status, MYF(0)))
	continue;       /* conf file doesn't exist */
      dynstr_append(&s, cs->name);
      dynstr_append(&s, " ");
    }
  }

  if (want_flags & (MY_CS_INDEX|MY_CS_LOADED))
  {
    CHARSET_INFO *cs;
    for (cs = all_charsets; cs < all_charsets + 255; cs++)
      if (cs->name && (cs->state & want_flags) )
        charset_append(&s, cs->name);
  }
  
  if (s.length)
  {
    s.str[s.length - 1] = '\0';   /* chop trailing space */
    p = my_strdup(s.str, MYF(MY_WME));
  }
  else
  {
    p = my_strdup("", MYF(MY_WME));
  }
  dynstr_free(&s);
  
  return p;
}
