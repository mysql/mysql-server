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

/* some definitions for full-text indices */

/* #include "myisam.h" */

#ifndef _ft_global_h
#define _ft_global_h
#ifdef  __cplusplus
extern "C" {
#endif

#define FT_QUERY_MAXLEN 1024

typedef struct ft_doc_rec {
  my_off_t  dpos;
  double    weight;
} FT_DOC;

typedef struct st_ft_doclist {
  int       ndocs;
  int       curdoc;
  void      *info;  /* actually (MI_INFO *) but don't want to include myisam.h */
  FT_DOC    doc[1];
} FT_DOCLIST;

int ft_init_stopwords(const char **);

FT_DOCLIST * ft_init_search(MI_INFO *, uint, byte *, uint, bool);
double ft_read_next(FT_DOCLIST *, char *);
#define ft_close_search(handler)        my_free(((gptr)(handler)),MYF(0))

#ifdef  __cplusplus
}
#endif
#endif
