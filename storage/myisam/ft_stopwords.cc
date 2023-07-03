/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

#include <fcntl.h>
#include <sys/types.h>

#include "my_compare.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "storage/myisam/ftdefs.h"
#include "storage/myisam/myisamdef.h"

static CHARSET_INFO *ft_stopword_cs = nullptr;

struct FT_STOPWORD {
  const char *pos;
  uint len;
};

static TREE *stopwords3 = nullptr;

static int FT_STOPWORD_cmp(const void *, const void *a, const void *b) {
  const FT_STOPWORD *w1 = static_cast<const FT_STOPWORD *>(a);
  const FT_STOPWORD *w2 = static_cast<const FT_STOPWORD *>(b);
  return ha_compare_text(ft_stopword_cs, pointer_cast<const uchar *>(w1->pos),
                         w1->len, pointer_cast<const uchar *>(w2->pos), w2->len,
                         false);
}

static void FT_STOPWORD_free(void *v_w, TREE_FREE action, const void *) {
  FT_STOPWORD *w = static_cast<FT_STOPWORD *>(v_w);
  if (action == free_free) my_free(const_cast<char *>(w->pos));
}

static int ft_add_stopword(const char *w) {
  FT_STOPWORD sw;
  return !w ||
         (((sw.len = (uint)strlen(sw.pos = w)) >= ft_min_word_len) &&
          (tree_insert(stopwords3, &sw, 0, stopwords3->custom_arg) == nullptr));
}

int ft_init_stopwords() {
  if (!stopwords3) {
    if (!(stopwords3 = (TREE *)my_malloc(mi_key_memory_ft_stopwords,
                                         sizeof(TREE), MYF(0))))
      return -1;
    init_tree(stopwords3, 0, sizeof(FT_STOPWORD), &FT_STOPWORD_cmp, false,
              (ft_stopword_file ? &FT_STOPWORD_free : nullptr), nullptr);
    /*
      Stopword engine currently does not support tricky
      character sets UCS2, UTF16, UTF32.
      Use latin1 to compare stopwords in case of these character sets.
      It's also fine to use latin1 with the built-in stopwords.
    */
    ft_stopword_cs = default_charset_info->mbminlen == 1 ? default_charset_info
                                                         : &my_charset_latin1;
  }

  if (ft_stopword_file) {
    File fd;
    size_t len;
    uchar *buffer, *start, *end;
    FT_WORD w;
    int error = -1;

    if (!*ft_stopword_file) return 0;

    if ((fd = my_open(ft_stopword_file, O_RDONLY, MYF(MY_WME))) == -1)
      return -1;
    len = (size_t)my_seek(fd, 0L, MY_SEEK_END, MYF(0));
    my_seek(fd, 0L, MY_SEEK_SET, MYF(0));
    if (!(start = buffer = (uchar *)my_malloc(mi_key_memory_ft_stopwords,
                                              len + 1, MYF(MY_WME))))
      goto err0;
    len = my_read(fd, buffer, len, MYF(MY_WME));
    end = start + len;
    while (ft_simple_get_word(ft_stopword_cs, &start, end, &w, true)) {
      if (ft_add_stopword(my_strndup(mi_key_memory_ft_stopwords, (char *)w.pos,
                                     w.len, MYF(0))))
        goto err1;
    }
    error = 0;
  err1:
    my_free(buffer);
  err0:
    my_close(fd, MYF(MY_WME));
    return error;
  } else {
    /* compatibility mode: to be removed */
    const char **sws = ft_precompiled_stopwords;

    for (; *sws; sws++) {
      if (ft_add_stopword(*sws)) return -1;
    }
    ft_stopword_file = "(built-in)"; /* for SHOW VARIABLES */
  }
  return 0;
}

int is_stopword(char *word, uint len) {
  FT_STOPWORD sw;
  sw.pos = word;
  sw.len = len;
  return tree_search(stopwords3, &sw, stopwords3->custom_arg) != nullptr;
}

void ft_free_stopwords() {
  if (stopwords3) {
    delete_tree(stopwords3); /* purecov: inspected */
    my_free(stopwords3);
    stopwords3 = nullptr;
  }
  ft_stopword_file = nullptr;
}
