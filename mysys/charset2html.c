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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/*
  Written by Alexander Barkov to check what 
  a charset is in your favorite web browser
*/

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <mysql_version.h>

#include <stdio.h>

typedef struct char_info_st 
{
  int cod;
  int srt;
  int uni;
  int low;
  int upp;
  int ctp;
} MY_CH;

static int chcmp(const void *vf, const void *vs)
{
  const MY_CH *f=vf;
  const MY_CH *s=vs;
  
  return f->srt-s->srt ? f->srt-s->srt : f->uni-s->uni;
}

static void print_cs(CHARSET_INFO *cs)
{
  uint  i;
  int   srt;
  int   clr=0;
  MY_CH ch[256];
    
  printf("<HTML>\n");
  printf("<HEAD>\n");
  printf("</HEAD>\n");
  printf("<BODY><PRE>\n");
  printf("Charset %s\n",cs->name);

  printf("<TABLE>\n");
  printf("<TR><TH>Code<TH>Uni<TH>Sort<TH>Ctype<TH>Ch<TH>Lo<TH>Up</TR>");
  
  for (i=0; i<256; i++)
  {
    ch[i].cod=i;
    ch[i].srt=cs->sort_order[i];
    ch[i].uni=cs->tab_to_uni[i];
    ch[i].low=cs->tab_to_uni[cs->to_lower[i]];
    ch[i].upp=cs->tab_to_uni[cs->to_upper[i]];
    ch[i].ctp=cs->ctype[i+1];
  }
  
  qsort(ch,256,sizeof(MY_CH),&chcmp);
  srt=ch[0].srt;
  
  for (i=0; i<256; i++)
  {
    clr = (srt!=ch[i].srt) ? !clr : clr;
    
    printf("<TR bgcolor=#%s>",clr ? "DDDDDD" : "EEEE99");
    printf("<TD>%02X",ch[i].cod);
    printf("<TD>%04X",ch[i].uni);
    printf("<TD>%d",ch[i].srt);
    
    printf("<TD>%s%s%s%s%s%s%s%s",
    		ch[i].ctp & _U ? "U" : "",
    		ch[i].ctp & _L ? "L" : "",
    		ch[i].ctp & _NMR ? "N" : "",
    		ch[i].ctp & _SPC ? "S" : "",
    		ch[i].ctp & _PNT ? "P" : "",
    		ch[i].ctp & _CTR ? "C" : "",
    		ch[i].ctp & _B ? "B" : "",
    		ch[i].ctp & _X ? "X" : "");
    
    printf("<TD>&#%d;",ch[i].uni);
    printf("<TD>&#%d;",ch[i].low);
    printf("<TD>&#%d;",ch[i].upp);
    printf("</TR>\n");
    srt=ch[i].srt;
  }
  printf("</TABLE>\n");
  printf("</PRE></BODY>\n");
  printf("</HTML>\n");
}


int main(int argc, char **argv) {
  const char *the_set = MYSQL_CHARSET;
  int argcnt = 1;

  my_init();

  if (argc > argcnt && argv[argcnt][0] == '-' && argv[argcnt][1] == '#')
    DBUG_PUSH(argv[argcnt++]+2);

  if (argc > argcnt)
    the_set = argv[argcnt++];

  if (argc > argcnt)
    charsets_dir = argv[argcnt++];

  if (set_default_charset_by_name(the_set, MYF(MY_WME)))
    return 1;

  print_cs(default_charset_info);

  return 0;
}
