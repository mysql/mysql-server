/* Copyright (C) 2005 MySQL AB

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

/*
  Implementation of trie and Aho-Corasick automaton.
  Supports only charsets that can be compared byte-wise.
  
  TODO:
    Add character frequencies. Can increase lookup speed
    up to 30%.
    Implement character-wise comparision.
*/


#include "mysys_priv.h"
#include <m_string.h>
#include <my_trie.h>
#include <my_base.h>


/*
  SYNOPSIS
    TRIE *trie_init (TRIE *trie, CHARSET_INFO *charset);

  DESCRIPTION
    Allocates or initializes a `TRIE' object. If `trie' is a `NULL'
    pointer, the function allocates, initializes, and returns a new
    object. Otherwise, the object is initialized and the address of
    the object is returned.  If `trie_init()' allocates a new object,
    it will be freed when `trie_free()' is called.

  RETURN VALUE
    An initialized `TRIE*' object.  `NULL' if there was insufficient
    memory to allocate a new object.
*/

TRIE *trie_init (TRIE *trie, CHARSET_INFO *charset)
{
  MEM_ROOT mem_root;
  DBUG_ENTER("trie_init");
  DBUG_ASSERT(charset);
  init_alloc_root(&mem_root,
                  (sizeof(TRIE_NODE) * 128) + ALLOC_ROOT_MIN_BLOCK_SIZE,
                  sizeof(TRIE_NODE) * 128);
  if (! trie)
  {
    if (! (trie= (TRIE *)alloc_root(&mem_root, sizeof(TRIE))))
    {
      free_root(&mem_root, MYF(0));
      DBUG_RETURN(NULL);
    }
  }

  memcpy(&trie->mem_root, &mem_root, sizeof(MEM_ROOT));
  trie->root.leaf= 0;
  trie->root.c= 0;
  trie->root.next= NULL;
  trie->root.links= NULL;
  trie->root.fail= NULL;
  trie->charset= charset;
  trie->nnodes= 0;
  trie->nwords= 0;
  DBUG_RETURN(trie);
}


/*
  SYNOPSIS
    void trie_free (TRIE *trie);
    trie - valid pointer to `TRIE'

  DESCRIPTION
    Frees the memory allocated for a `trie'.

  RETURN VALUE
    None.
*/

void trie_free (TRIE *trie)
{
  MEM_ROOT mem_root;
  DBUG_ENTER("trie_free");
  DBUG_ASSERT(trie);
  memcpy(&mem_root, &trie->mem_root, sizeof(MEM_ROOT));
  free_root(&mem_root, MYF(0));
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
    my_bool trie_insert (TRIE *trie, const uchar *key, uint keylen);
    trie - valid pointer to `TRIE'
    key - valid pointer to key to insert
    keylen - non-0 key length

  DESCRIPTION
    Inserts new key into trie.

  RETURN VALUE
    Upon successful completion, `trie_insert' returns `FALSE'. Otherwise
    `TRUE' is returned.

  NOTES
    If this function fails you must assume `trie' is broken.
    However it can be freed with trie_free().
*/

my_bool trie_insert (TRIE *trie, const uchar *key, uint keylen)
{
  TRIE_NODE *node;
  TRIE_NODE *next;
  uchar p;
  uint k;
  DBUG_ENTER("trie_insert");
  DBUG_ASSERT(trie && key && keylen);
  node= &trie->root;
  trie->root.fail= NULL;
  for (k= 0; k < keylen; k++)
  {
    p= key[k];
    for (next= node->links; next; next= next->next)
      if (next->c == p)
        break;

    if (! next)
    {
      TRIE_NODE *tmp= (TRIE_NODE *)alloc_root(&trie->mem_root,
                                              sizeof(TRIE_NODE));
      if (! tmp)
        DBUG_RETURN(TRUE);
      tmp->leaf= 0;
      tmp->c= p;
      tmp->links= tmp->fail= tmp->next= NULL;
      trie->nnodes++;
      if (! node->links)
      {
        node->links= tmp;
      }
      else
      {
        for (next= node->links; next->next; next= next->next) /* no-op */;
        next->next= tmp;
      }
      node= tmp;
    }
    else
    {
      node= next;
    }
  }
  node->leaf= keylen;
  trie->nwords++;
  DBUG_RETURN(FALSE);
}


/*
  SYNOPSIS
    my_bool trie_prepare (TRIE *trie);
    trie - valid pointer to `TRIE'

  DESCRIPTION
    Constructs Aho-Corasick automaton.

  RETURN VALUE
    Upon successful completion, `trie_prepare' returns `FALSE'. Otherwise
    `TRUE' is returned.
*/

my_bool ac_trie_prepare (TRIE *trie)
{
  TRIE_NODE **tmp_nodes;
  TRIE_NODE *node;
  uint32 fnode= 0;
  uint32 lnode= 0;
  DBUG_ENTER("trie_prepare");
  DBUG_ASSERT(trie);

  tmp_nodes= (TRIE_NODE **)my_malloc(trie->nnodes * sizeof(TRIE_NODE *), MYF(0));
  if (! tmp_nodes)
    DBUG_RETURN(TRUE);

  trie->root.fail= &trie->root;
  for (node= trie->root.links; node; node= node->next)
  {
    node->fail= &trie->root;
    tmp_nodes[lnode++]= node;
  }

  while (fnode < lnode)
  {
    TRIE_NODE *current= (TRIE_NODE *)tmp_nodes[fnode++];
    for (node= current->links; node; node= node->next)
    {
      TRIE_NODE *fail= current->fail;
      tmp_nodes[lnode++]= node;
      while (! (node->fail= trie_goto(&trie->root, fail, node->c)))
        fail= fail->fail;
    }
  }
  my_free((uchar*)tmp_nodes, MYF(0));
  DBUG_RETURN(FALSE);
}


/*
  SYNOPSIS
    void ac_trie_init (TRIE *trie, AC_TRIE_STATE *state);
    trie - valid pointer to `TRIE'
    state - value pointer to `AC_TRIE_STATE'

  DESCRIPTION
    Initializes `AC_TRIE_STATE' object.
*/

void ac_trie_init (TRIE *trie, AC_TRIE_STATE *state)
{
  DBUG_ENTER("ac_trie_init");
  DBUG_ASSERT(trie && state);
  state->trie= trie;
  state->node= &trie->root;
  DBUG_VOID_RETURN;
}
