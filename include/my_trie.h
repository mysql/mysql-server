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

#ifndef _trie_h
#define _trie_h
#ifdef	__cplusplus
extern "C" {
#endif

typedef struct st_trie_node
{
  uint16 leaf;                /* Depth from root node if match, 0 else */
  uchar c;                    /* Label on this edge */
  struct st_trie_node *next;  /* Next label */
  struct st_trie_node *links; /* Array of edges leaving this node */
  struct st_trie_node *fail;  /* AC failure function */
} TRIE_NODE;

typedef struct st_trie
{
  TRIE_NODE root;
  MEM_ROOT mem_root;
  CHARSET_INFO *charset;
  uint32 nnodes;
  uint32 nwords;
} TRIE;

typedef struct st_ac_trie_state
{
  TRIE *trie;
  TRIE_NODE *node;
} AC_TRIE_STATE;

extern TRIE *trie_init (TRIE *trie, CHARSET_INFO *charset);
extern void trie_free (TRIE *trie);
extern my_bool trie_insert (TRIE *trie, const uchar *key, uint keylen);
extern my_bool ac_trie_prepare (TRIE *trie);
extern void ac_trie_init (TRIE *trie, AC_TRIE_STATE *state);


/* `trie_goto' is internal function and shouldn't be used. */

static inline TRIE_NODE *trie_goto (TRIE_NODE *root, TRIE_NODE *node, uchar c)
{
  TRIE_NODE *next;
  DBUG_ENTER("trie_goto");
  for (next= node->links; next; next= next->next)
    if (next->c == c)
      DBUG_RETURN(next);
  if (root == node)
    DBUG_RETURN(root);
  DBUG_RETURN(NULL);
}


/*
  SYNOPSIS
    int ac_trie_next (AC_TRIE_STATE *state, uchar *c);
    state - valid pointer to `AC_TRIE_STATE'
    c - character to lookup

  DESCRIPTION
    Implementation of search using Aho-Corasick automaton.
    Performs char-by-char search.

  RETURN VALUE
    `ac_trie_next' returns length of matched word or 0.
*/

static inline int ac_trie_next (AC_TRIE_STATE *state, uchar *c)
{
  TRIE_NODE *root, *node;
  DBUG_ENTER("ac_trie_next");
  DBUG_ASSERT(state && c);
  root= &state->trie->root;
  node= state->node;
  while (! (state->node= trie_goto(root, node, *c)))
    node= node->fail;
  DBUG_RETURN(state->node->leaf);
}


/*
  SYNOPSIS
    my_bool trie_search (TRIE *trie, const uchar *key, uint keylen);
    trie - valid pointer to `TRIE'
    key - valid pointer to key to insert
    keylen - non-0 key length

  DESCRIPTION
    Performs key lookup in trie.

  RETURN VALUE
    `trie_search' returns `true' if key is in `trie'. Otherwise,
    `false' is returned.

  NOTES
    Consecutive search here is "best by test". arrays are very short, so
    binary search or hashing would add too much complexity that would
    overweight speed gain. Especially because compiler can optimize simple
    consecutive loop better (tested)
*/

static inline my_bool trie_search (TRIE *trie, const uchar *key, uint keylen)
{
  TRIE_NODE *node;
  uint k;
  DBUG_ENTER("trie_search");
  DBUG_ASSERT(trie && key && keylen);
  node= &trie->root;

  for (k= 0; k < keylen; k++)
  {
    uchar p;
    if (! (node= node->links))
      DBUG_RETURN(FALSE);
    p= key[k];
    while (p != node->c)
      if (! (node= node->next))
        DBUG_RETURN(FALSE);
  }

  DBUG_RETURN(node->leaf > 0);
}

#ifdef	__cplusplus
}
#endif
#endif
