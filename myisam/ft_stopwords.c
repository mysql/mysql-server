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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include "ftdefs.h"

typedef struct st_ft_stopwords {
  const char * pos;
  uint   len;
} FT_STOPWORD;

static TREE *stopwords3=NULL;

static int FT_STOPWORD_cmp(FT_STOPWORD *w1, FT_STOPWORD *w2)
{
  return _mi_compare_text(default_charset_info,
			  (uchar *)w1->pos,w1->len,
			  (uchar *)w2->pos,w2->len,0);
}

int ft_init_stopwords(const char **sws)
{
  FT_STOPWORD sw;


  if(!stopwords3)
  {
    if(!(stopwords3=(TREE *)my_malloc(sizeof(TREE),MYF(0)))) return -1;
    init_tree(stopwords3,0,sizeof(FT_STOPWORD),(qsort_cmp)&FT_STOPWORD_cmp,0,
	      NULL);
  }

  if(!sws) return 0;

  for(;*sws;sws++)
  {
    if( (sw.len= (uint) strlen(sw.pos=*sws)) < MIN_WORD_LEN) continue;
    if(!tree_insert(stopwords3, &sw, 0))
    {
      delete_tree(stopwords3); /* purecov: inspected */
      return -1; /* purecov: inspected */
    }
  }
  return 0;
}

int is_stopword(char *word, uint len)
{
  FT_STOPWORD sw;
  sw.pos=word;
  sw.len=len;
  return tree_search(stopwords3,&sw) != NULL;
}


void ft_free_stopwords()
{
  if (stopwords3)
  {
    delete_tree(stopwords3); /* purecov: inspected */    
    stopwords3=0;
  }
}
