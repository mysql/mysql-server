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


/*
  Collation language is implemented according to
  subset of ICU Collation Customization (tailorings):
  http://oss.software.ibm.com/icu/userguide/Collate_Customization.html
  
  Collation language elements:
  Delimiters:
    space   - skipped
  
  <char> :=  A-Z | a-z | \uXXXX
  
  Shift command:
    <shift>  := &       - reset at this letter. 
  
  Diff command:
    <d1> :=  <     - Identifies a primary difference.
    <d2> :=  <<    - Identifies a secondary difference.
    <d3> := <<<    - Idenfifies a tertiary difference.
  
  
  Collation rules:
    <ruleset> :=  <rule>  { <ruleset> }
    
    <rule> :=   <d1>    <string>
              | <d2>    <string>
              | <d3>    <string>
              | <shift> <char>
    
    <string> := <char> [ <string> ]

  An example, Polish collation:
  
    &A < \u0105 <<< \u0104
    &C < \u0107 <<< \u0106
    &E < \u0119 <<< \u0118
    &L < \u0142 <<< \u0141
    &N < \u0144 <<< \u0143
    &O < \u00F3 <<< \u00D3
    &S < \u015B <<< \u015A
    &Z < \u017A <<< \u017B    
*/


typedef enum my_coll_lexem_num_en
{
  MY_COLL_LEXEM_EOF	= 0,
  MY_COLL_LEXEM_DIFF	= 1, 
  MY_COLL_LEXEM_SHIFT	= 4,
  MY_COLL_LEXEM_CHAR	= 5,
  MY_COLL_LEXEM_ERROR	= 6
} my_coll_lexem_num;


typedef struct my_coll_lexem_st
{
  const char *beg;
  const char *end;
  const char *prev;
  int   diff;
  int   code;
} MY_COLL_LEXEM;


/*
  Initialize collation rule lexical anilizer
  
  SYNOPSIS
    my_coll_lexem_init
    lexem                Lex analizer to init
    str                  Const string to parse
    strend               End of the string
  USAGE
  
  RETURN VALUES
    N/A
*/

static void my_coll_lexem_init(MY_COLL_LEXEM *lexem,
                               const char *str, const char *strend)
{
  lexem->beg= str;
  lexem->prev= str;
  lexem->end= strend;
  lexem->diff= 0;
  lexem->code= 0;
}


/*
  Print collation customization expression parse error, with context.
  
  SYNOPSIS
    my_coll_lexem_print_error
    lexem                Lex analizer to take context from
    errstr               sting to write error to
    errsize              errstr size
    txt                  error message
  USAGE
  
  RETURN VALUES
    N/A
*/

static void my_coll_lexem_print_error(MY_COLL_LEXEM *lexem,
                                      char *errstr, size_t errsize,
                                      const char *txt)
{
  char tail[30];
  size_t len= lexem->end - lexem->prev;
  strmake (tail, lexem->prev, min(len, sizeof(tail)-1));
  errstr[errsize-1]= '\0';
  my_snprintf(errstr,errsize-1,"%s at '%s'", txt, tail);
}


/*
  Convert a hex digit into its numeric value
  
  SYNOPSIS
    ch2x
    ch                   hex digit to convert
  USAGE
  
  RETURN VALUES
    an integer value in the range 0..15
    -1 on error
*/

static int ch2x(int ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  
  if (ch >= 'a' && ch <= 'f')
    return 10 + ch - 'a';
  
  if (ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  
  return -1;
}


/*
  Collation language lexical parser:
  Scans the next lexem.
  
  SYNOPSIS
    my_coll_lexem_next
    lexem                Lex analizer, previously initialized by 
                         my_coll_lexem_init.
  USAGE
    Call this function in a loop
    
  RETURN VALUES
    Lexem number: eof, diff, shift, char or error.
*/

static my_coll_lexem_num my_coll_lexem_next(MY_COLL_LEXEM *lexem)
{
  for ( ;lexem->beg < lexem->end ; lexem->beg++)
  {
    lexem->prev= lexem->beg;
    if (lexem->beg[0] == ' '  || lexem->beg[0] == '\t' || 
        lexem->beg[0] == '\r' || lexem->beg[0] == '\n')
      continue;
    
    if (lexem->beg[0] == '&')
    {
      lexem->beg++;
      return MY_COLL_LEXEM_SHIFT;
    }
    
    if (lexem->beg[0] == '<')
    {
      for (lexem->beg++, lexem->diff=1; 
           (lexem->beg < lexem->end) && 
           (lexem->beg[0] == '<') && (lexem->diff<3);
           lexem->beg++, lexem->diff++);
        return MY_COLL_LEXEM_DIFF;
    }
    
    if ((lexem->beg[0] >= 'a' && lexem->beg[0] <= 'z') ||
        (lexem->beg[0] >= 'A' && lexem->beg[0] <= 'Z'))
    {
      lexem->code= lexem->beg[0];
      lexem->beg++;
      return MY_COLL_LEXEM_CHAR;
    }
    
    if ((lexem->beg[0] == '\\') && 
        (lexem->beg+2 < lexem->end) && 
        (lexem->beg[1] == 'u'))
    {
      int ch;
      
      lexem->code= 0;
      for (lexem->beg+=2; 
           (lexem->beg < lexem->end) && ((ch= ch2x(lexem->beg[0])) >= 0) ; 
           lexem->beg++)
      {
        lexem->code= (lexem->code << 4) + ch;
      }
      return MY_COLL_LEXEM_CHAR;
    }
    
    return MY_COLL_LEXEM_ERROR;
  }
  return MY_COLL_LEXEM_EOF;
}


/*
  Collation rule item
*/

typedef struct my_coll_rule_item_st
{
  uint base;     /* Base character                             */
  uint curr;     /* Current character                          */
  int diff[3];   /* Primary, Secondary and Tertiary difference */
} MY_COLL_RULE;


/*
  Collation language syntax parser.
  Uses lexical parser.
  
  SYNOPSIS
    my_coll_rule_parse
    rule                 Collation rule list to load to.
    str                  A string containin collation language expression.
    strend               End of the string.
  USAGE
    
  RETURN VALUES
    0 - OK
    1 - ERROR, e.g. too many items.
*/

static int my_coll_rule_parse(MY_COLL_RULE *rule, size_t mitems,
                              const char *str, const char *strend,
                              char *errstr, size_t errsize)
{
  MY_COLL_LEXEM lexem;
  my_coll_lexem_num lexnum;
  my_coll_lexem_num prevlexnum= MY_COLL_LEXEM_ERROR;
  MY_COLL_RULE item; 
  int state= 0;
  size_t nitems= 0;
  
  /* Init all variables */
  errstr[0]= '\0';
  bzero(&item, sizeof(item));
  my_coll_lexem_init(&lexem, str, strend);
  
  while ((lexnum= my_coll_lexem_next(&lexem)))
  {
    if (lexnum == MY_COLL_LEXEM_ERROR)
    {
      my_coll_lexem_print_error(&lexem,errstr,errsize-1,"Unknown character");
      return -1;
    }
    
    switch (state) {
    case 0:
      if (lexnum != MY_COLL_LEXEM_SHIFT)
      {
        my_coll_lexem_print_error(&lexem,errstr,errsize-1,"& expected");
        return -1;
      }
      prevlexnum= lexnum;
      state= 2;
      continue;
      
    case 1:
      if (lexnum != MY_COLL_LEXEM_SHIFT && lexnum != MY_COLL_LEXEM_DIFF)
      {
        my_coll_lexem_print_error(&lexem,errstr,errsize-1,"& or < expected");
        return -1;
      }
      prevlexnum= lexnum;
      state= 2;
      continue;
      
    case 2:
      if (lexnum != MY_COLL_LEXEM_CHAR)
      {
        my_coll_lexem_print_error(&lexem,errstr,errsize-1,"character expected");
        return -1;
      }
      
      if (prevlexnum == MY_COLL_LEXEM_SHIFT)
      {
        item.base= lexem.code;
        item.diff[0]= 0;
        item.diff[1]= 0;
        item.diff[2]= 0;
      }
      else if (prevlexnum == MY_COLL_LEXEM_DIFF)
      {
        item.curr= lexem.code;
        if (lexem.diff == 3)
        {
          item.diff[2]++;
        }
        else if (lexem.diff == 2)
        {
          item.diff[1]++;
          item.diff[2]= 0;
        }
        else if (lexem.diff == 1)
        {
          item.diff[0]++;
          item.diff[1]= 0;
          item.diff[2]= 0;
        }
        if (nitems >= mitems)
        {
          my_coll_lexem_print_error(&lexem,errstr,errsize-1,"Too many rules");
          return -1;
        }
        rule[nitems++]= item;
      }
      else
      {
        my_coll_lexem_print_error(&lexem,errstr,errsize-1,"Should never happen");
        return -1;
      }
      state= 1;
      continue;
    }
  }
  return (size_t) nitems;
}


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
      cs->max_sort_char= i;
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
    else if (my_mbcharlen(cs, i)>1)
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
  cs->mbminlen= 1;
  cs->mbmaxlen= 1;
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
    if (!(idx[i].uidx.tab=(uchar*) my_once_alloc(numchars *
						 sizeof(*idx[i].uidx.tab),
						 MYF(MY_WME))))
      return TRUE;

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
  if (!(cs->tab_from_uni= (MY_UNI_IDX*) my_once_alloc(sizeof(MY_UNI_IDX)*(n+1),
						      MYF(MY_WME))))
    return TRUE;

  for (i=0; i< n; i++)
    cs->tab_from_uni[i]= idx[i].uidx;
  
  /* Set end-of-list marker */
  bzero(&cs->tab_from_uni[i],sizeof(MY_UNI_IDX));
  return FALSE;
}


static int simple_cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
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
    init_state_maps(to);
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
    set_max_sort_char(to);
  }
  if (from->tab_to_uni)
  {
    uint sz= MY_CS_TO_UNI_TABLE_SIZE*sizeof(uint16);
    if (!(to->tab_to_uni= (uint16*)  my_once_memdup((char*)from->tab_to_uni,
						    sz, MYF(MY_WME))))
      goto err;
    if (create_fromuni(to))
      goto err;
  }
  to->mbminlen= 1;
  to->mbmaxlen= 1;

  return 0;

err:
  return 1;
}


#ifdef HAVE_CHARSET_ucs2

#define MY_MAX_COLL_RULE 64

/*
  This function copies an UCS2 collation from
  the default Unicode Collation Algorithm (UCA)
  weights applying tailorings, i.e. a set of
  alternative weights for some characters. 
  
  The default UCA weights are stored in my_charset_ucs2_general_uca.
  They consist of 256 pages, 256 character each.
  
  If a page is not overwritten by tailoring rules,
  it is copies as is from UCA as is.
  
  If a page contains some overwritten characters, it is
  allocated. Untouched characters are copied from the
  default weights.
*/

static int ucs2_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  MY_COLL_RULE rule[MY_MAX_COLL_RULE];
  char errstr[128];
  uchar   *newlengths;
  uint16 **newweights;
  const uchar *deflengths= my_charset_ucs2_general_uca.sort_order;
  uint16     **defweights= my_charset_ucs2_general_uca.sort_order_big;
  int rc, i;
  
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
  
  to->strxfrm_multiply= my_charset_ucs2_general_uca.strxfrm_multiply;
  to->min_sort_char= my_charset_ucs2_general_uca.min_sort_char;
  to->max_sort_char= my_charset_ucs2_general_uca.max_sort_char;
  to->mbminlen= 2;
  to->mbmaxlen= 2;
  
  
  /* Parse ICU Collation Customization expression */
  if ((rc= my_coll_rule_parse(rule, MY_MAX_COLL_RULE,
                              from->sort_order,
                              from->sort_order + strlen(from->sort_order),
                              errstr, sizeof(errstr))) <= 0)
  {
    /* 
      TODO: add error message reporting.
      printf("Error: %d '%s'\n", rc, errstr);
    */
    return 1;
  }
  
  
  if (!(newweights= (uint16**) my_once_alloc(256*sizeof(uint16*),MYF(MY_WME))))
    goto err;
  bzero(newweights, 256*sizeof(uint16*));
  
  if (!(newlengths= (uchar*) my_once_memdup(deflengths,256,MYF(MY_WME))))
    goto err;
  
  /*
    Calculate maximum lenghts for the pages
    which will be overwritten.
  */
  for (i=0; i < rc; i++)
  {
    uint pageb= (rule[i].base >> 8) & 0xFF;
    uint pagec= (rule[i].curr >> 8) & 0xFF;
    
    if (newlengths[pagec] < deflengths[pageb])
      newlengths[pagec]= deflengths[pageb];
  }
  
  for (i=0; i < rc;  i++)
  {
    uint pageb= (rule[i].base >> 8) & 0xFF;
    uint pagec= (rule[i].curr >> 8) & 0xFF;
    uint chb, chc;
    
    if (!newweights[pagec])
    {
      /* Alloc new page and copy the default UCA weights */
      uint size= 256*newlengths[pagec]*sizeof(uint16);
      
      if (!(newweights[pagec]= (uint16*) my_once_alloc(size,MYF(MY_WME))))
        goto err;
      bzero((void*) newweights[pagec], size);
      
      for (chc=0 ; chc < 256; chc++)
      {
        memcpy(newweights[pagec] + chc*newlengths[pagec],
               defweights[pagec] + chc*deflengths[pagec],
               deflengths[pagec]*sizeof(uint16));
      }
    }
    
    /* 
      Aply the alternative rule:
      shift to the base character and primary difference.
    */
    chc= rule[i].curr & 0xFF;
    chb= rule[i].base & 0xFF;
    memcpy(newweights[pagec] + chc*newlengths[pagec],
           defweights[pageb] + chb*deflengths[pageb],
           deflengths[pageb]*sizeof(uint16));
    /* Apply primary difference */
    newweights[pagec][chc*newlengths[pagec]]+= rule[i].diff[0];
  }
  
  /* Copy non-overwritten pages from the default UCA weights */
  for (i= 0; i < 256 ; i++)
    if (!newweights[i])
      newweights[i]= defweights[i];
  
  to->sort_order= newlengths;
  to->sort_order_big= newweights;
  
  return 0;
  
err:
  return 1;
}
#endif


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
      if (!strcmp(cs->csname,"ucs2") )
      {
#ifdef HAVE_CHARSET_ucs2
        CHARSET_INFO *new= all_charsets[cs->number];
        new->cset= my_charset_ucs2_general_uca.cset;
        new->coll= my_charset_ucs2_general_uca.coll;
        if (ucs2_copy_data(new, cs))
          return MY_XML_ERROR;
        new->state |= MY_CS_AVAILABLE | MY_CS_LOADED;
#endif        
      }
      else
      {
        simple_cs_init_functions(all_charsets[cs->number]);
        if (simple_cs_copy_data(all_charsets[cs->number],cs))
	  return MY_XML_ERROR;
        if (simple_cs_is_full(all_charsets[cs->number]))
        {
          all_charsets[cs->number]->state |= MY_CS_LOADED;
        }
        all_charsets[cs->number]->state|= MY_CS_AVAILABLE;
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
static int charset_initialized=0;


static my_bool my_read_charset_file(const char *filename, myf myflags)
{
  char *buf;
  int  fd;
  uint len;
  MY_STAT stat_info;
  
  if (!my_stat(filename, &stat_info, MYF(myflags)) ||
       ((len= (uint)stat_info.st_size) > MY_MAX_ALLOWED_BUF) ||
       !(buf= (char *)my_malloc(len,myflags)))
    return TRUE;
  
  if ((fd=my_open(filename,O_RDONLY,myflags)) < 0)
  {
    my_free(buf,myflags);
    return TRUE;
  }
  len=read(fd,buf,len);
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
    for (cs=all_charsets;
	 cs < all_charsets+array_elements(all_charsets)-1 ;
	 cs++)
    {
      if (*cs)
      {
        set_max_sort_char(*cs);
        if (cs[0]->ctype)
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
  init_available_charsets(MYF(0));
  
  for (cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
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
  init_available_charsets(MYF(0));
  
  for (cs= all_charsets;
       cs < all_charsets+array_elements(all_charsets)-1 ;
       cs++)
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
  init_available_charsets(MYF(0));

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
  if ((cs= all_charsets[cs_number]))
  {
    if (!(cs->state & MY_CS_COMPILED) && !(cs->state & MY_CS_LOADED))
    {
      strxmov(get_charsets_dir(buf), cs->csname, ".xml", NullS);
      my_read_charset_file(buf,flags);
    }
    cs= (cs->state & MY_CS_AVAILABLE) ? cs : NULL;
  }
  pthread_mutex_unlock(&THR_LOCK_charset);
  return cs;
}


CHARSET_INFO *get_charset(uint cs_number, myf flags)
{
  CHARSET_INFO *cs;
  if (cs_number == default_charset_info->number)
    return default_charset_info;

  (void) init_available_charsets(MYF(0));	/* If it isn't initialized */
  
  if (!cs_number || cs_number >= array_elements(all_charsets)-1)
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


ulong escape_string_for_mysql(CHARSET_INFO *charset_info, char *to,
                              const char *from, ulong length)
{
  const char *to_start= to;
  const char *end;
#ifdef USE_MB
  my_bool use_mb_flag= use_mb(charset_info);
#endif
  for (end= from + length; from != end; from++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l= my_ismbchar(charset_info, from, end)))
    {
      while (l--)
	*to++= *from++;
      from--;
      continue;
    }
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      *to++= '\\';
      *to++= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      *to++= '\\';
      *to++= 'n';
      break;
    case '\r':
      *to++= '\\';
      *to++= 'r';
      break;
    case '\\':
      *to++= '\\';
      *to++= '\\';
      break;
    case '\'':
      *to++= '\\';
      *to++= '\'';
      break;
    case '"':				/* Better safe than sorry */
      *to++= '\\';
      *to++= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      *to++= '\\';
      *to++= 'Z';
      break;
    default:
      *to++= *from;
    }
  }
  *to= 0;
  return (ulong) (to - to_start);
}
