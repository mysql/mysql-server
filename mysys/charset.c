/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_ctype.h>
#include <m_string.h>
#include <my_dir.h>
#include <my_xml.h>


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


static uint
get_collation_number_internal(const char *name)
{
  CHARSET_INFO **cs;
  for (cs= all_charsets;
       cs < all_charsets + array_elements(all_charsets);
       cs++)
  {
    if ( cs[0] && cs[0]->name && 
         !my_strcasecmp(&my_charset_latin1, cs[0]->name, name))
      return cs[0]->number;
  }  
  return 0;
}


static my_bool init_state_maps(CHARSET_INFO *cs)
{
  uint i;
  uchar *state_map;
  uchar *ident_map;

  if (!(cs->state_map= (uchar*) my_once_alloc(256, MYF(MY_WME))))
    return 1;
    
  if (!(cs->ident_map= (uchar*) my_once_alloc(256, MYF(MY_WME))))
    return 1;

  state_map= cs->state_map;
  ident_map= cs->ident_map;
  
  /* Fill state_map with states to get a faster parser */
  for (i=0; i < 256 ; i++)
  {
    if (my_isalpha(cs,i))
      state_map[i]=(uchar) MY_LEX_IDENT;
    else if (my_isdigit(cs,i))
      state_map[i]=(uchar) MY_LEX_NUMBER_IDENT;
#if defined(USE_MB) && defined(USE_MB_IDENT)
    else if (my_mbcharlen(cs, i)>1)
      state_map[i]=(uchar) MY_LEX_IDENT;
#endif
    else if (my_isspace(cs,i))
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
  state_map[(uchar)';']=(uchar) MY_LEX_SEMICOLON;
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
  state_map[(uchar)'b']= state_map[(uchar)'B']= (uchar) MY_LEX_IDENT_OR_BIN;
  state_map[(uchar)'n']= state_map[(uchar)'N']= (uchar) MY_LEX_IDENT_OR_NCHAR;
  return 0;
}


static void simple_cs_init_functions(CHARSET_INFO *cs)
{
  if (cs->state & MY_CS_BINSORT)
    cs->coll= &my_collation_8bit_bin_handler;
  else
    cs->coll= &my_collation_8bit_simple_ci_handler;
  
  cs->cset= &my_charset_8bit_handler;
}



static int cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->number= from->number ? from->number : to->number;

  if (from->csname)
    if (!(to->csname= my_once_strdup(from->csname,MYF(MY_WME))))
      goto err;
  
  if (from->name)
    if (!(to->name= my_once_strdup(from->name,MYF(MY_WME))))
      goto err;
  
  if (from->comment)
    if (!(to->comment= my_once_strdup(from->comment,MYF(MY_WME))))
      goto err;
  
  if (from->ctype)
  {
    if (!(to->ctype= (uchar*) my_once_memdup((char*) from->ctype,
					     MY_CS_CTYPE_TABLE_SIZE,
					     MYF(MY_WME))))
      goto err;
    if (init_state_maps(to))
      goto err;
  }
  if (from->to_lower)
    if (!(to->to_lower= (uchar*) my_once_memdup((char*) from->to_lower,
						MY_CS_TO_LOWER_TABLE_SIZE,
						MYF(MY_WME))))
      goto err;

  if (from->to_upper)
    if (!(to->to_upper= (uchar*) my_once_memdup((char*) from->to_upper,
						MY_CS_TO_UPPER_TABLE_SIZE,
						MYF(MY_WME))))
      goto err;
  if (from->sort_order)
  {
    if (!(to->sort_order= (uchar*) my_once_memdup((char*) from->sort_order,
						  MY_CS_SORT_ORDER_TABLE_SIZE,
						  MYF(MY_WME))))
      goto err;

  }
  if (from->tab_to_uni)
  {
    uint sz= MY_CS_TO_UNI_TABLE_SIZE*sizeof(uint16);
    if (!(to->tab_to_uni= (uint16*)  my_once_memdup((char*)from->tab_to_uni,
						    sz, MYF(MY_WME))))
      goto err;
  }
  if (from->tailoring)
    if (!(to->tailoring= my_once_strdup(from->tailoring,MYF(MY_WME))))
      goto err;

  return 0;

err:
  return 1;
}



static my_bool simple_cs_is_full(CHARSET_INFO *cs)
{
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
	   cs->to_lower) &&
	  (cs->number && cs->name &&
	  (cs->sort_order || (cs->state & MY_CS_BINSORT) )));
}


static void
copy_uca_collation(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->cset= from->cset;
  to->coll= from->coll;
  to->strxfrm_multiply= from->strxfrm_multiply;
  to->min_sort_char= from->min_sort_char;
  to->max_sort_char= from->max_sort_char;
  to->mbminlen= from->mbminlen;
  to->mbmaxlen= from->mbmaxlen;
  to->state|= MY_CS_AVAILABLE | MY_CS_LOADED |
              MY_CS_STRNXFRM  | MY_CS_UNICODE;
}


static int add_collation(CHARSET_INFO *cs)
{
  if (cs->name && (cs->number ||
                   (cs->number=get_collation_number_internal(cs->name))) &&
      cs->number < array_elements(all_charsets))
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
      CHARSET_INFO *newcs= all_charsets[cs->number];
      if (cs_copy_data(all_charsets[cs->number],cs))
        return MY_XML_ERROR;

      newcs->caseup_multiply= newcs->casedn_multiply= 1;

      if (!strcmp(cs->csname,"ucs2") )
      {
#if defined(HAVE_CHARSET_ucs2) && defined(HAVE_UCA_COLLATIONS)
        copy_uca_collation(newcs, &my_charset_ucs2_unicode_ci);
        newcs->state|= MY_CS_AVAILABLE | MY_CS_LOADED | MY_CS_NONASCII;
#endif        
      }
      else if (!strcmp(cs->csname, "utf8") || !strcmp(cs->csname, "utf8mb3"))
      {
#if defined (HAVE_CHARSET_utf8) && defined(HAVE_UCA_COLLATIONS)
        copy_uca_collation(newcs, &my_charset_utf8_unicode_ci);
        newcs->ctype= my_charset_utf8_unicode_ci.ctype;
        if (init_state_maps(newcs))
          return MY_XML_ERROR;
#endif
      }
      else if (!strcmp(cs->csname, "utf8mb4"))
      {
#if defined (HAVE_CHARSET_utf8mb4) && defined(HAVE_UCA_COLLATIONS)
        copy_uca_collation(newcs, &my_charset_utf8mb4_unicode_ci);
        newcs->ctype= my_charset_utf8mb4_unicode_ci.ctype;
        newcs->state|= MY_CS_AVAILABLE | MY_CS_LOADED;
#endif
      }
      else if (!strcmp(cs->csname, "utf16"))
      {
#if defined (HAVE_CHARSET_utf16) && defined(HAVE_UCA_COLLATIONS)
        copy_uca_collation(newcs, &my_charset_utf16_unicode_ci);
        newcs->state|= MY_CS_AVAILABLE | MY_CS_LOADED | MY_CS_NONASCII;
#endif
      }
      else if (!strcmp(cs->csname, "utf32"))
      {
#if defined (HAVE_CHARSET_utf32) && defined(HAVE_UCA_COLLATIONS)
        copy_uca_collation(newcs, &my_charset_utf32_unicode_ci);
        newcs->state|= MY_CS_AVAILABLE | MY_CS_LOADED | MY_CS_NONASCII;
#endif
      }
      else
      {
        uchar *sort_order= all_charsets[cs->number]->sort_order;
        simple_cs_init_functions(all_charsets[cs->number]);
        newcs->mbminlen= 1;
        newcs->mbmaxlen= 1;
        if (simple_cs_is_full(all_charsets[cs->number]))
        {
          all_charsets[cs->number]->state |= MY_CS_LOADED;
        }
        all_charsets[cs->number]->state|= MY_CS_AVAILABLE;
        
        /*
          Check if case sensitive sort order: A < a < B.
          We need MY_CS_FLAG for regex library, and for
          case sensitivity flag for 5.0 client protocol,
          to support isCaseSensitive() method in JDBC driver 
        */
        if (sort_order && sort_order['A'] < sort_order['a'] &&
                          sort_order['a'] < sort_order['B'])
          all_charsets[cs->number]->state|= MY_CS_CSSORT; 

        if (my_charset_is_8bit_pure_ascii(all_charsets[cs->number]))
          all_charsets[cs->number]->state|= MY_CS_PUREASCII;
        if (!my_charset_is_ascii_compatible(cs))
          all_charsets[cs->number]->state|= MY_CS_NONASCII;
      }
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
	if (!(dst->comment= my_once_strdup(cs->comment,MYF(MY_WME))))
	  return MY_XML_ERROR;
      if (cs->csname)
        if (!(dst->csname= my_once_strdup(cs->csname,MYF(MY_WME))))
	  return MY_XML_ERROR;
      if (cs->name)
	if (!(dst->name= my_once_strdup(cs->name,MYF(MY_WME))))
	  return MY_XML_ERROR;
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


#define MY_MAX_ALLOWED_BUF 1024*1024
#define MY_CHARSET_INDEX "Index.xml"

const char *charsets_dir= NULL;


static my_bool my_read_charset_file(const char *filename, myf myflags)
{
  uchar *buf;
  int  fd;
  size_t len, tmp_len;
  MY_STAT stat_info;
  
  if (!my_stat(filename, &stat_info, MYF(myflags)) ||
       ((len= (uint)stat_info.st_size) > MY_MAX_ALLOWED_BUF) ||
       !(buf= (uchar*) my_malloc(len,myflags)))
    return TRUE;
  
  if ((fd= mysql_file_open(key_file_charset, filename, O_RDONLY, myflags)) < 0)
    goto error;
  tmp_len= mysql_file_read(fd, buf, len, myflags);
  mysql_file_close(fd, myflags);
  if (tmp_len != len)
    goto error;
  
  if (my_parse_charset_xml((char*) buf,len,add_collation))
  {
#ifdef NOT_YET
    printf("ERROR at line %d pos %d '%s'\n",
	   my_xml_error_lineno(&p)+1,
	   my_xml_error_pos(&p),
	   my_xml_error_string(&p));
#endif
  }
  
  my_free(buf);
  return FALSE;

error:
  my_free(buf);
  return TRUE;
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

CHARSET_INFO *all_charsets[MY_ALL_CHARSETS_SIZE]={NULL};
CHARSET_INFO *default_charset_info = &my_charset_latin1;

void add_compiled_collation(CHARSET_INFO *cs)
{
  all_charsets[cs->number]= cs;
  cs->state|= MY_CS_AVAILABLE;
}

static void *cs_alloc(size_t size)
{
  return my_once_alloc(size, MYF(MY_WME));
}


static my_pthread_once_t charsets_initialized= MY_PTHREAD_ONCE_INIT;
static my_pthread_once_t charsets_template= MY_PTHREAD_ONCE_INIT;

static void init_available_charsets(void)
{
  char fname[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
  CHARSET_INFO **cs;

  bzero(&all_charsets,sizeof(all_charsets));
  init_compiled_charsets(MYF(0));

  /* Copy compiled charsets */
  for (cs=all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
  {
    if (*cs)
    {
      if (cs[0]->ctype)
        if (init_state_maps(*cs))
          *cs= NULL;
    }
  }

  strmov(get_charsets_dir(fname), MY_CHARSET_INDEX);
  my_read_charset_file(fname, MYF(0));
}


void free_charsets(void)
{
  charsets_initialized= charsets_template;
}


static const char*
get_collation_name_alias(const char *name, char *buf, size_t bufsize)
{
  if (!strncasecmp(name, "utf8mb3_", 8))
  {
    my_snprintf(buf, bufsize, "utf8_%s", name + 8);
    return buf;
  }
  return NULL;
}


uint get_collation_number(const char *name)
{
  uint id;
  char alias[64];
  my_pthread_once(&charsets_initialized, init_available_charsets);
  if ((id= get_collation_number_internal(name)))
    return id;
  if ((name= get_collation_name_alias(name, alias, sizeof(alias))))
    return get_collation_number_internal(name);
  return 0;
}


static uint
get_charset_number_internal(const char *charset_name, uint cs_flags)
{
  CHARSET_INFO **cs;
  
  for (cs= all_charsets;
       cs < all_charsets + array_elements(all_charsets);
       cs++)
  {
    if ( cs[0] && cs[0]->csname && (cs[0]->state & cs_flags) &&
         !my_strcasecmp(&my_charset_latin1, cs[0]->csname, charset_name))
      return cs[0]->number;
  }  
  return 0;
}


static const char*
get_charset_name_alias(const char *name)
{
  if (!my_strcasecmp(&my_charset_latin1, name, "utf8mb3"))
    return "utf8";
  return NULL;
}


uint get_charset_number(const char *charset_name, uint cs_flags)
{
  uint id;
  my_pthread_once(&charsets_initialized, init_available_charsets);
  if ((id= get_charset_number_internal(charset_name, cs_flags)))
    return id;
  if ((charset_name= get_charset_name_alias(charset_name)))
    return get_charset_number_internal(charset_name, cs_flags);
  return 0;
}
                  

const char *get_charset_name(uint charset_number)
{
  CHARSET_INFO *cs;
  my_pthread_once(&charsets_initialized, init_available_charsets);

  cs=all_charsets[charset_number];
  if (cs && (cs->number == charset_number) && cs->name )
    return (char*) cs->name;
  
  return (char*) "?";   /* this mimics find_type() */
}


static CHARSET_INFO *get_internal_charset(uint cs_number, myf flags)
{
  char  buf[FN_REFLEN];
  CHARSET_INFO *cs;

  if ((cs= all_charsets[cs_number]))
  {
    if (cs->state & MY_CS_READY)  /* if CS is already initialized */
        return cs;

    /*
      To make things thread safe we are not allowing other threads to interfere
      while we may changing the cs_info_table
    */
    mysql_mutex_lock(&THR_LOCK_charset);

    if (!(cs->state & (MY_CS_COMPILED|MY_CS_LOADED))) /* if CS is not in memory */
    {
      strxmov(get_charsets_dir(buf), cs->csname, ".xml", NullS);
      my_read_charset_file(buf,flags);
    }

    if (cs->state & MY_CS_AVAILABLE)
    {
      if (!(cs->state & MY_CS_READY))
      {
        if ((cs->cset->init && cs->cset->init(cs, cs_alloc)) ||
            (cs->coll->init && cs->coll->init(cs, cs_alloc)))
          cs= NULL;
        else
          cs->state|= MY_CS_READY;
      }
    }
    else
      cs= NULL;

    mysql_mutex_unlock(&THR_LOCK_charset);
  }
  return cs;
}


CHARSET_INFO *get_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  if (cs_number == default_charset_info->number)
    return default_charset_info;

  my_pthread_once(&charsets_initialized, init_available_charsets);
  
  if (!cs_number || cs_number > array_elements(all_charsets))
    return NULL;
  
  cs=get_internal_charset(cs_number, flags);

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)], cs_string[23];
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
  my_pthread_once(&charsets_initialized, init_available_charsets);

  cs_number=get_collation_number(cs_name);
  cs= cs_number ? get_internal_charset(cs_number,flags) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_COLLATION, MYF(ME_BELL), cs_name, index_file);
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

  my_pthread_once(&charsets_initialized, init_available_charsets);

  cs_number= get_charset_number(cs_name, cs_flags);
  cs= cs_number ? get_internal_charset(cs_number, flags) : NULL;

  if (!cs && (flags & MY_WME))
  {
    char index_file[FN_REFLEN + sizeof(MY_CHARSET_INDEX)];
    strmov(get_charsets_dir(index_file),MY_CHARSET_INDEX);
    my_error(EE_UNKNOWN_CHARSET, MYF(ME_BELL), cs_name, index_file);
  }

  DBUG_RETURN(cs);
}


/**
  Resolve character set by the character set name (utf8, latin1, ...).

  The function tries to resolve character set by the specified name. If
  there is character set with the given name, it is assigned to the "cs"
  parameter and FALSE is returned. If there is no such character set,
  "default_cs" is assigned to the "cs" and TRUE is returned.

  @param[in] cs_name    Character set name.
  @param[in] default_cs Default character set.
  @param[out] cs        Variable to store character set.

  @return FALSE if character set was resolved successfully; TRUE if there
  is no character set with given name.
*/

my_bool resolve_charset(const char *cs_name,
                        CHARSET_INFO *default_cs,
                        CHARSET_INFO **cs)
{
  *cs= get_charset_by_csname(cs_name, MY_CS_PRIMARY, MYF(0));

  if (*cs == NULL)
  {
    *cs= default_cs;
    return TRUE;
  }

  return FALSE;
}


/**
  Resolve collation by the collation name (utf8_general_ci, ...).

  The function tries to resolve collation by the specified name. If there
  is collation with the given name, it is assigned to the "cl" parameter
  and FALSE is returned. If there is no such collation, "default_cl" is
  assigned to the "cl" and TRUE is returned.

  @param[out] cl        Variable to store collation.
  @param[in] cl_name    Collation name.
  @param[in] default_cl Default collation.

  @return FALSE if collation was resolved successfully; TRUE if there is no
  collation with given name.
*/

my_bool resolve_collation(const char *cl_name,
                          CHARSET_INFO *default_cl,
                          CHARSET_INFO **cl)
{
  *cl= get_charset_by_name(cl_name, MYF(0));

  if (*cl == NULL)
  {
    *cl= default_cl;
    return TRUE;
  }

  return FALSE;
}


/*
  Escape string with backslashes (\)

  SYNOPSIS
    escape_string_for_mysql()
    charset_info        Charset of the strings
    to                  Buffer for escaped string
    to_length           Length of destination buffer, or 0
    from                The string to escape
    length              The length of the string to escape

  DESCRIPTION
    This escapes the contents of a string by adding backslashes before special
    characters, and turning others into specific escape sequences, such as
    turning newlines into \n and null bytes into \0.

  NOTE
    To maintain compatibility with the old C API, to_length may be 0 to mean
    "big enough"

  RETURN VALUES
    (size_t) -1 The escaped string did not fit in the to buffer
    #           The length of the escaped string
*/

size_t escape_string_for_mysql(CHARSET_INFO *charset_info,
                               char *to, size_t to_length,
                               const char *from, size_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + (to_length ? to_length-1 : 2*length);
  my_bool overflow= FALSE;
#ifdef USE_MB
  my_bool use_mb_flag= use_mb(charset_info);
#endif
  for (end= from + length; from < end; from++)
  {
    char escape= 0;
#ifdef USE_MB
    int tmp_length;
    if (use_mb_flag && (tmp_length= my_ismbchar(charset_info, from, end)))
    {
      if (to + tmp_length > to_end)
      {
        overflow= TRUE;
        break;
      }
      while (tmp_length--)
	*to++= *from++;
      from--;
      continue;
    }
    /*
     If the next character appears to begin a multi-byte character, we
     escape that first byte of that apparent multi-byte character. (The
     character just looks like a multi-byte character -- if it were actually
     a multi-byte character, it would have been passed through in the test
     above.)

     Without this check, we can create a problem by converting an invalid
     multi-byte character into a valid one. For example, 0xbf27 is not
     a valid GBK character, but 0xbf5c is. (0x27 = ', 0x5c = \)
    */
    if (use_mb_flag && (tmp_length= my_mbcharlen(charset_info, *from)) > 1)
      escape= *from;
    else
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      escape= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      escape= 'n';
      break;
    case '\r':
      escape= 'r';
      break;
    case '\\':
      escape= '\\';
      break;
    case '\'':
      escape= '\'';
      break;
    case '"':				/* Better safe than sorry */
      escape= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      escape= 'Z';
      break;
    }
    if (escape)
    {
      if (to + 2 > to_end)
      {
        overflow= TRUE;
        break;
      }
      *to++= '\\';
      *to++= escape;
    }
    else
    {
      if (to + 1 > to_end)
      {
        overflow= TRUE;
        break;
      }
      *to++= *from;
    }
  }
  *to= 0;
  return overflow ? (size_t) -1 : (size_t) (to - to_start);
}


#ifdef BACKSLASH_MBTAIL
static CHARSET_INFO *fs_cset_cache= NULL;

CHARSET_INFO *fs_character_set()
{
  if (!fs_cset_cache)
  {
    char buf[10]= "cp";
    GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE,
                  buf+2, sizeof(buf)-3);
    /*
      We cannot call get_charset_by_name here
      because fs_character_set() is executed before
      LOCK_THD_charset mutex initialization, which
      is used inside get_charset_by_name.
      As we're now interested in cp932 only,
      let's just detect it using strcmp().
    */
    fs_cset_cache= !strcmp(buf, "cp932") ?
                   &my_charset_cp932_japanese_ci : &my_charset_bin;
  }
  return fs_cset_cache;
}
#endif

/*
  Escape apostrophes by doubling them up

  SYNOPSIS
    escape_quotes_for_mysql()
    charset_info        Charset of the strings
    to                  Buffer for escaped string
    to_length           Length of destination buffer, or 0
    from                The string to escape
    length              The length of the string to escape

  DESCRIPTION
    This escapes the contents of a string by doubling up any apostrophes that
    it contains. This is used when the NO_BACKSLASH_ESCAPES SQL_MODE is in
    effect on the server.

  NOTE
    To be consistent with escape_string_for_mysql(), to_length may be 0 to
    mean "big enough"

  RETURN VALUES
    ~0          The escaped string did not fit in the to buffer
    >=0         The length of the escaped string
*/

size_t escape_quotes_for_mysql(CHARSET_INFO *charset_info,
                               char *to, size_t to_length,
                               const char *from, size_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + (to_length ? to_length-1 : 2*length);
  my_bool overflow= FALSE;
#ifdef USE_MB
  my_bool use_mb_flag= use_mb(charset_info);
#endif
  for (end= from + length; from < end; from++)
  {
#ifdef USE_MB
    int tmp_length;
    if (use_mb_flag && (tmp_length= my_ismbchar(charset_info, from, end)))
    {
      if (to + tmp_length > to_end)
      {
        overflow= TRUE;
        break;
      }
      while (tmp_length--)
	*to++= *from++;
      from--;
      continue;
    }
    /*
      We don't have the same issue here with a non-multi-byte character being
      turned into a multi-byte character by the addition of an escaping
      character, because we are only escaping the ' character with itself.
     */
#endif
    if (*from == '\'')
    {
      if (to + 2 > to_end)
      {
        overflow= TRUE;
        break;
      }
      *to++= '\'';
      *to++= '\'';
    }
    else
    {
      if (to + 1 > to_end)
      {
        overflow= TRUE;
        break;
      }
      *to++= *from;
    }
  }
  *to= 0;
  return overflow ? (ulong)~0 : (ulong) (to - to_start);
}
