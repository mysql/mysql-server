/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
    ch[i].srt=cs->sort_order ? cs->sort_order[i] : i;
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
    printf("<TD>%02X",ch[i].srt);
    
    printf("<TD>%s%s%s%s%s%s%s%s",
    		ch[i].ctp & _MY_U ? "U" : "",
    		ch[i].ctp & _MY_L ? "L" : "",
    		ch[i].ctp & _MY_NMR ? "N" : "",
    		ch[i].ctp & _MY_SPC ? "S" : "",
    		ch[i].ctp & _MY_PNT ? "P" : "",
    		ch[i].ctp & _MY_CTR ? "C" : "",
    		ch[i].ctp & _MY_B ? "B" : "",
    		ch[i].ctp & _MY_X ? "X" : "");
    
    if ((ch[i].uni >= 0x80) && (ch[i].uni <= 0x9F))
    {
      /* 
       Control characters 0x0080..0x009F are dysplayed by some
       browers as if they were letters. Don't print them to
       avoid confusion.
      */
      printf("<TD>ctrl<TD>ctrl<TD>ctrl");
    }
    else
    {
      printf("<TD>&#%d;",ch[i].uni);
      printf("<TD>&#%d;",ch[i].low);
      printf("<TD>&#%d;",ch[i].upp);
    }
    printf("</TR>\n");
    srt=ch[i].srt;
  }
  printf("</TABLE>\n");
  printf("</PRE></BODY>\n");
  printf("</HTML>\n");
}

static void print_index()
{
  CHARSET_INFO **cs;
  int clr=0; 
  
  get_charset_by_name("",MYF(0));	/* To execute init_available_charsets */
  
  printf("All charsets\n");
  printf("<table border=1>\n");
  printf("<tr bgcolor=EEEE99><th>ID<th>Charset<th>Collation<th>Def<th>Bin<th>Com<th>Comment\n");
  for (cs=all_charsets ; cs < all_charsets+256; cs++)
  {
    if (!cs[0])
      continue;
    printf("<tr bgcolor=#%s><td><a href=\"?%s\">%d</a><td>%s<td>%s<td>%s<td>%s<td>%s<td>%s\n",
    	   (clr= !clr) ? "DDDDDD" : "EEEE99",
    	   cs[0]->name,cs[0]->number,cs[0]->csname,
    	   cs[0]->name,
    	   (cs[0]->state & MY_CS_PRIMARY)  ? "def " : "&nbsp;",
    	   (cs[0]->state & MY_CS_BINSORT)  ? "bin " : "&nbsp;",
    	   (cs[0]->state & MY_CS_COMPILED) ? "com " : "&nbsp;",
    	   cs[0]->comment);
  }
  printf("</table>\n");
}

int main(int argc, char **argv) {
  const char *the_set = NULL;
  int argcnt = 1;
  CHARSET_INFO *cs;

  if (getenv("SCRIPT_NAME"))
  {
    printf("Content-Type: text/html\r\n\r\n");
  }
  my_init();

  if (argc > argcnt && argv[argcnt][0] == '-' && argv[argcnt][1] == '#')
  {
    DBUG_PUSH(argv[argcnt++]+2);
  }

  if (argc > argcnt)
    the_set = argv[argcnt++];

  if (argc > argcnt)
    charsets_dir = argv[argcnt++];

  if (!the_set)
  {
    print_index();
    return 0;
  }
  
  if (!(cs= get_charset_by_name(the_set, MYF(MY_WME))))
    return 1;

  print_cs(cs);

  return 0;
}
