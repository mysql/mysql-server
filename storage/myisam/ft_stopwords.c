/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include "ftdefs.h"
#include "my_compare.h"


static CHARSET_INFO *ft_stopword_cs= NULL;


typedef struct st_ft_stopwords
{
  const char * pos;
  uint   len;
} FT_STOPWORD;

static TREE *stopwords3=NULL;

static int FT_STOPWORD_cmp(void* cmp_arg __attribute__((unused)),
			   FT_STOPWORD *w1, FT_STOPWORD *w2)
{
  return ha_compare_text(ft_stopword_cs,
			 (uchar *)w1->pos,w1->len,
			 (uchar *)w2->pos,w2->len,0,0);
}

static void FT_STOPWORD_free(FT_STOPWORD *w, TREE_FREE action,
                             void *arg __attribute__((unused)))
{
  if (action == free_free)
    my_free((void*)w->pos);
}

static int ft_add_stopword(const char *w)
{
  FT_STOPWORD sw;
  return !w ||
         (((sw.len= (uint) strlen(sw.pos=w)) >= ft_min_word_len) &&
          (tree_insert(stopwords3, &sw, 0, stopwords3->custom_arg)==NULL));
}

int ft_init_stopwords()
{
  if (!stopwords3)
  {
    if (!(stopwords3=(TREE *)my_malloc(sizeof(TREE),MYF(0))))
      return -1;
    init_tree(stopwords3,0,0,sizeof(FT_STOPWORD),(qsort_cmp2)&FT_STOPWORD_cmp,
              0,
              (ft_stopword_file ? (tree_element_free)&FT_STOPWORD_free : 0),
              NULL);
    /*
      Stopword engine currently does not support tricky
      character sets UCS2, UTF16, UTF32.
      Use latin1 to compare stopwords in case of these character sets.
      It's also fine to use latin1 with the built-in stopwords.
    */
    ft_stopword_cs= default_charset_info->mbminlen == 1 ?
                    default_charset_info : &my_charset_latin1;
  }

  if (ft_stopword_file)
  {
    File fd;
    uint len;
    uchar *buffer, *start, *end;
    FT_WORD w;
    int error=-1;

    if (!*ft_stopword_file)
      return 0;

    if ((fd=my_open(ft_stopword_file, O_RDONLY, MYF(MY_WME))) == -1)
      return -1;
    len=(uint)my_seek(fd, 0L, MY_SEEK_END, MYF(0));
    my_seek(fd, 0L, MY_SEEK_SET, MYF(0));
    if (!(start=buffer=my_malloc(len+1, MYF(MY_WME))))
      goto err0;
    len=my_read(fd, buffer, len, MYF(MY_WME));
    end=start+len;
    while (ft_simple_get_word(ft_stopword_cs, &start, end, &w, TRUE))
    {
      if (ft_add_stopword(my_strndup((char*) w.pos, w.len, MYF(0))))
        goto err1;
    }
    error=0;
err1:
    my_free(buffer);
err0:
    my_close(fd, MYF(MY_WME));
    return error;
  }
  else
  {
    /* compatibility mode: to be removed */
    char **sws=(char **)ft_precompiled_stopwords;

    for (;*sws;sws++)
    {
      if (ft_add_stopword(*sws))
        return -1;
    }
    ft_stopword_file="(built-in)"; /* for SHOW VARIABLES */
  }
  return 0;
}

int is_stopword(char *word, uint len)
{
  FT_STOPWORD sw;
  sw.pos=word;
  sw.len=len;
  return tree_search(stopwords3,&sw, stopwords3->custom_arg) != NULL;
}


void ft_free_stopwords()
{
  if (stopwords3)
  {
    delete_tree(stopwords3); /* purecov: inspected */
    my_free(stopwords3);
    stopwords3=0;
  }
  ft_stopword_file= 0;
}
