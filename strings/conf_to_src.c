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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <my_global.h>
#include <m_ctype.h>
#include <my_xml.h>


#define ROW_LEN		16
#define ROW16_LEN	8
#define MAX_BUF		16*1024

static CHARSET_INFO all_charsets[256];


void
print_array(FILE *f, const char *set, const char *name, uchar *a, int n)
{
  int i;

  fprintf(f,"uchar %s_%s[] = {\n", name, set);
  
  for (i=0 ;i<n ; i++)
  {
    fprintf(f,"0x%02X",a[i]);
    fprintf(f, (i+1<n) ? "," :"" );
    fprintf(f, ((i+1) % ROW_LEN == n % ROW_LEN) ? "\n" : "" );
  }
  fprintf(f,"};\n\n");
}


void
print_array16(FILE *f, const char *set, const char *name, uint16 *a, int n)
{
  int i;

  fprintf(f,"uchar %s_%s[] = {\n", name, set);
  
  for (i=0 ;i<n ; i++)
  {
    fprintf(f,"0x%04X",a[i]);
    fprintf(f, (i+1<n) ? "," :"" );
    fprintf(f, ((i+1) % ROW16_LEN == n % ROW16_LEN) ? "\n" : "" );
  }
  fprintf(f,"};\n\n");
}


static int get_charset_number(const char *charset_name)
{
  CHARSET_INFO *cs;
  for (cs= all_charsets; cs < all_charsets+255; ++cs)
  {
    if ( cs->name && !strcmp(cs->name, charset_name))
      return cs->number;
  }  
  return 0;
}

char *mdup(const char *src, uint len)
{
  char *dst=(char*)malloc(len);
  memcpy(dst,src,len);
  return dst;
}

static void simple_cs_copy_data(CHARSET_INFO *to, CHARSET_INFO *from)
{
  to->number= from->number ? from->number : to->number;
  to->state|= from->state;

  if (from->csname)
    to->csname= strdup(from->csname);
  
  if (from->name)
    to->name= strdup(from->name);
  
  if (from->ctype)
    to->ctype= (uchar*) mdup((char*) from->ctype, MY_CS_CTYPE_TABLE_SIZE);
  if (from->to_lower)
    to->to_lower= (uchar*) mdup((char*) from->to_lower, MY_CS_TO_LOWER_TABLE_SIZE);
  if (from->to_upper)
    to->to_upper= (uchar*) mdup((char*) from->to_upper, MY_CS_TO_UPPER_TABLE_SIZE);
  if (from->sort_order)
  {
    to->sort_order= (uchar*) mdup((char*) from->sort_order, MY_CS_SORT_ORDER_TABLE_SIZE);
    /*
      set_max_sort_char(to);
    */
  }
  if (from->tab_to_uni)
  {
    uint sz= MY_CS_TO_UNI_TABLE_SIZE*sizeof(uint16);
    to->tab_to_uni= (uint16*)  mdup((char*)from->tab_to_uni, sz);
    /*
    create_fromuni(to);
    */
  }
}

static my_bool simple_cs_is_full(CHARSET_INFO *cs)
{
  return ((cs->csname && cs->tab_to_uni && cs->ctype && cs->to_upper &&
	   cs->to_lower) &&
	  (cs->number && cs->name && cs->sort_order));
}

static int add_collation(CHARSET_INFO *cs)
{
  if (cs->name && (cs->number || (cs->number=get_charset_number(cs->name))))
  {
    if (!(all_charsets[cs->number].state & MY_CS_COMPILED))
    {
      simple_cs_copy_data(&all_charsets[cs->number],cs);
      
    }
    
    cs->number= 0;
    cs->name= NULL;
    cs->state= 0;
    cs->sort_order= NULL;
    cs->state= 0;
  }
  return MY_XML_OK;
}


static int my_read_charset_file(const char *filename)
{
  char buf[MAX_BUF];
  int  fd;
  uint len;
  
  if ((fd=open(filename,O_RDONLY)) < 0)
  {
    fprintf(stderr,"Can't open '%s'\n",filename);
    return 1;
  }
  
  len=read(fd,buf,MAX_BUF);
  close(fd);
  
  if (my_parse_charset_xml(buf,len,add_collation))
  {
#if 0
    printf("ERROR at line %d pos %d '%s'\n",
	   my_xml_error_lineno(&p)+1,
	   my_xml_error_pos(&p),
	   my_xml_error_string(&p));
#endif
  }
  
  return FALSE;
}

void dispcset(FILE *f,CHARSET_INFO *cs)
{
  fprintf(f,"{\n");
  fprintf(f,"  %d,\n",cs->number);
  fprintf(f,"  MY_CS_COMPILED,\n");
  
  if (cs->name)
  {
    fprintf(f,"  \"%s\",\n",cs->name);
    fprintf(f,"  \"%s\",\n",cs->csname);
    fprintf(f,"  \"\",\n");
    fprintf(f,"  ctype_%s,\n",cs->name);
    fprintf(f,"  to_lower_%s,\n",cs->name);
    fprintf(f,"  to_upper_%s,\n",cs->name);
    fprintf(f,"  sort_order_%s,\n",cs->name);
    fprintf(f,"  to_uni_%s,\n",cs->name);
    fprintf(f,"  from_uni_%s,\n",cs->name);
  }
  else
  {
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
    fprintf(f,"  NULL,\n");
  }
  
  fprintf(f,"  %d,\n",cs->strxfrm_multiply);
  fprintf(f,"  my_strnncoll_simple,\n");
  fprintf(f,"  my_strnxfrm_simple,\n");
  fprintf(f,"  my_like_range_simple,\n");
  fprintf(f,"  my_wild_cmp_8bit,\n");
  fprintf(f,"  %d,\n",cs->mbmaxlen);
  fprintf(f,"  NULL,\n");
  fprintf(f,"  NULL,\n");
  fprintf(f,"  NULL,\n");
  fprintf(f,"  my_mb_wc_8bit,\n");
  fprintf(f,"  my_wc_mb_8bit,\n");
  fprintf(f,"  my_caseup_str_8bit,\n");
  fprintf(f,"  my_casedn_str_8bit,\n");
  fprintf(f,"  my_caseup_8bit,\n");
  fprintf(f,"  my_casedn_8bit,\n");
  fprintf(f,"  my_tosort_8bit,\n");
  fprintf(f,"  my_strcasecmp_8bit,\n");
  fprintf(f,"  my_strncasecmp_8bit,\n");
  fprintf(f,"  my_hash_caseup_simple,\n");
  fprintf(f,"  my_hash_sort_simple,\n");
  fprintf(f,"  0,\n");
  fprintf(f,"  my_snprintf_8bit,\n");
  fprintf(f,"  my_long10_to_str_8bit,\n");
  fprintf(f,"  my_longlong10_to_str_8bit,\n");
  fprintf(f,"  my_fill_8bit,\n");
  fprintf(f,"  my_strntol_8bit,\n");
  fprintf(f,"  my_strntoul_8bit,\n");
  fprintf(f,"  my_strntoll_8bit,\n");
  fprintf(f,"  my_strntoull_8bit,\n");
  fprintf(f,"  my_strntod_8bit,\n");
  fprintf(f,"  my_scan_8bit\n");
  fprintf(f,"}\n");
}


int
main(int argc, char **argv  __attribute__((unused)))
{
  CHARSET_INFO  ncs;
  CHARSET_INFO  *cs;
  char filename[256];
  FILE *f= stdout;
  
  if (argc < 2)
  {
    fprintf(stderr, "usage: %s source-dir\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  
  bzero((void*)&ncs,sizeof(ncs));
  bzero((void*)&all_charsets,sizeof(all_charsets));
  
  sprintf(filename,"%s/%s",argv[1],"Index.xml");
  my_read_charset_file(filename);
  
  for (cs=all_charsets; cs < all_charsets+256; cs++)
  {
    if (cs->number)
    {
      if ( (!simple_cs_is_full(cs)) && (cs->csname) )
      {
        sprintf(filename,"%s/%s.xml",argv[1],cs->csname);
        my_read_charset_file(filename);
      }
    }
  }
  
  for (cs=all_charsets; cs < all_charsets+256; cs++)
  {
    if (simple_cs_is_full(cs))
    {
      fprintf(f,"#ifdef HAVE_CHARSET_%s\n",cs->csname);
      print_array(f, cs->name, "ctype",      cs->ctype,      MY_CS_CTYPE_TABLE_SIZE);
      print_array(f, cs->name, "to_lower",   cs->to_lower,   MY_CS_TO_LOWER_TABLE_SIZE);
      print_array(f, cs->name, "to_upper",   cs->to_upper,   MY_CS_TO_UPPER_TABLE_SIZE);
      print_array(f, cs->name, "sort_order", cs->sort_order, MY_CS_SORT_ORDER_TABLE_SIZE);
      print_array16(f, cs->name, "to_uni",     cs->tab_to_uni, MY_CS_TO_UNI_TABLE_SIZE);
      fprintf(f,"#endif\n");
      fprintf(f,"\n");
    }
  }
  
  fprintf(f,"CHARSET_INFO compiled_charsets[] = {\n");
  for (cs=all_charsets; cs < all_charsets+256; cs++)
  {
    if (simple_cs_is_full(cs))
    {
      fprintf(f,"#ifdef HAVE_CHARSET_%s\n",cs->csname);
      dispcset(f,cs);
      fprintf(f,",\n");
      fprintf(f,"#endif\n");
    }
  }
  
  dispcset(f,&ncs);
  fprintf(f,"};\n");
  
  return 0;
}
