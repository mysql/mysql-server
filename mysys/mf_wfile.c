/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Functions for finding files with wildcards */

/*
  The following file-name-test is supported:
  - "name [[,] name...]			; Matches any of used filenames.
					  Each name can have "*" and/or "?"
					  wild-cards.
  - [wildspec [,]] !wildspec2		; File that matches wildspec and not
					  wildspec2.
*/

#include "mysys_priv.h"
#include <m_string.h>

	/* Store wildcard-string in a easyer format */

WF_PACK *wf_comp(my_string str)
{
  uint ant;
  int not_pos;
  register my_string pos;
  my_string buffer;
  WF_PACK *ret;
  DBUG_ENTER("wf_comp");

  not_pos= -1;			/* Skipp space and '!' in front */
  while (*str == ' ')
    str++;
  if (*str == '!')
  {
    not_pos=0;
    while (*++str == ' ') {};
  }
  if (*str == 0)				/* Empty == everything */
    DBUG_RETURN((WF_PACK *) NULL);

  ant=1;					/* Count filespecs */
  for (pos=str ; *pos ; pos++)
    ant+= test(*pos == ' ' || *pos == ',');

#ifdef FN_UPPER_CASE
    caseup(str,(int) (pos-str));
#endif
#ifdef FN_LOWER_CASE
    casedn(str,(int) (pos-str));
#endif

  if ((ret= (WF_PACK*) my_malloc((uint) ant*(sizeof(my_string*)+2)+
				  sizeof(WF_PACK)+strlen(str)+1,MYF(MY_WME)))
	== 0)
    DBUG_RETURN((WF_PACK *) NULL);
  ret->wild= (my_string*) (ret+1);
  buffer= (my_string) (ret->wild+ant);

  ant=0;
  for (pos=str ; *pos ; str= pos)
  {
    ret->wild[ant++]=buffer;
    while (*pos != ' ' && *pos != ',' && *pos != '!' && *pos)
      *buffer++ = *pos++;

    *buffer++ = '\0';
    while (*pos == ' ' || *pos == ',' || *pos == '!' )
      if (*pos++ == '!' && not_pos <0)
	not_pos=(int) ant;
  }

  ret->wilds=ant;
  if (not_pos <0)
    ret->not_pos=ant;
  else
    ret->not_pos=(uint) not_pos;

  DBUG_PRINT("exit",("antal: %d  not_pos: %d",ret->wilds,ret->not_pos));
  DBUG_RETURN(ret);
} /* wf_comp */


	/* Test if a given filename is matched */

int wf_test(register WF_PACK *wf_pack, register const char *name)
{
  reg2 uint i;
  reg3 uint not_pos;
  DBUG_ENTER("wf_test");

  if (! wf_pack || wf_pack->wilds == 0)
    DBUG_RETURN(0);			/* Everything goes */

  not_pos=wf_pack->not_pos;
  for (i=0 ; i < not_pos; i++)
    if (wild_compare(name,wf_pack->wild[i]) == 0)
      goto found;
  if (i)
    DBUG_RETURN(1);			/* No-match */

found:
/* Test that it isn't in not-list */

  for (i=not_pos ; i < wf_pack->wilds; i++)
    if (wild_compare(name,wf_pack->wild[i]) == 0)
      DBUG_RETURN(1);
  DBUG_RETURN(0);
} /* wf_test */


	/* We need this because program don't know with malloc we used */

void wf_end(WF_PACK *buffer)
{
  DBUG_ENTER("wf_end");
  if (buffer)
    my_free((gptr) buffer,MYF(0));
  DBUG_VOID_RETURN;
} /* wf_end */
