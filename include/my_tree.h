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

#ifndef _tree_h
#define _tree_h
#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_TREE_HIGHT	40	/* = max 1048576 leafs in tree */
#define ELEMENT_KEY(tree,element)\
(tree->offset_to_key ? (void*)((byte*) element+tree->offset_to_key) :\
			*((void**) (element+1)))

#define tree_set_pointer(element,ptr) *((byte **) (element+1))=((byte*) (ptr))

typedef enum { left_root_right, right_root_left } TREE_WALK;
typedef uint32 element_count;
typedef int (*tree_walk_action)(void *,element_count,void *);

#ifdef MSDOS
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  unsigned long count;
  uchar    colour;			/* black is marked as 1 */
} TREE_ELEMENT;
#else
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  uint32 count:31,
	 colour:1;			/* black is marked as 1 */
} TREE_ELEMENT;
#endif /* MSDOS */

typedef struct st_tree {
  TREE_ELEMENT *root,null_element;
  TREE_ELEMENT **parents[MAX_TREE_HIGHT];
  uint offset_to_key,elements_in_tree,size_of_element;
  qsort_cmp2 compare;
  void* cmp_arg;
  MEM_ROOT mem_root;
  my_bool with_delete;
  void (*free)(void *);
} TREE;

	/* Functions on whole tree */
void init_tree(TREE *tree,uint default_alloc_size, int element_size,
	       qsort_cmp2 compare, my_bool with_delete,
	       void (*free_element)(void*));
void delete_tree(TREE*);
void reset_tree(TREE*);
  /* similar to delete tree, except we do not my_free() blocks in mem_root
   */
#define is_tree_inited(tree) ((tree)->root != 0)

	/* Functions on leafs */
TREE_ELEMENT *tree_insert(TREE *tree,void *key,uint key_size);
void *tree_search(TREE *tree,void *key);
int tree_walk(TREE *tree,tree_walk_action action,
	      void *argument, TREE_WALK visit);
int tree_delete(TREE *tree,void *key);

#ifdef	__cplusplus
}
#endif
#endif
