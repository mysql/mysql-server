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

/*
  Code for handling red-black (balanced) binary trees.
  key in tree is allocated accrding to following:
  1) If free_element function is given to init_tree or size < 0 then tree
     will not allocate keys and only a pointer to each key is saved in tree.
     key_sizes must be 0 to init and search.
     compare and search functions uses and returns key-pointer.
  2) if key_size is given to init_tree then each node will continue the
     key and calls to insert_key may increase length of key.
     if key_size > sizeof(pointer) and key_size is a multiple of 8 (double
     allign) then key will be put on a 8 alligned adress. Else
     the key will be on adress (element+1). This is transparent for user
     compare and search functions uses a pointer to given key-argument.
  3) If init_tree - keysize is 0 then key_size must be given to tree_insert
     and tree_insert will alloc space for key.
     compare and search functions uses a pointer to given key-argument.

  The actual key in TREE_ELEMENT is saved as a pointer or after the
  TREE_ELEMENT struct.
  If one uses only pointers in tree one can use tree_set_pointer() to
  change address of data.
  Copyright Monty Program KB.
  By monty.
*/

#include "mysys_priv.h"
#include <m_string.h>
#include <my_tree.h>

#define BLACK		1
#define RED		0
#define DEFAULT_ALLOC_SIZE (8192-MALLOC_OVERHEAD)

static void delete_tree_element(TREE *,TREE_ELEMENT *);
static int tree_walk_left_root_right(TREE *,TREE_ELEMENT *,
				     tree_walk_action,void *);
static int tree_walk_right_root_left(TREE *,TREE_ELEMENT *,
				     tree_walk_action,void *);
static void left_rotate(TREE_ELEMENT **parent,TREE_ELEMENT *leaf);
static void right_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf);
static void rb_insert(TREE *tree,TREE_ELEMENT ***parent,
		      TREE_ELEMENT *leaf);
static void rb_delete_fixup(TREE *tree,TREE_ELEMENT ***parent);


	/* The actuall code for handling binary trees */

void init_tree(TREE *tree, uint default_alloc_size, int size,
	       qsort_cmp2 compare, my_bool with_delete,
	       void (*free_element) (void *))
{
  DBUG_ENTER("init_tree");
  DBUG_PRINT("enter",("tree: %lx  size: %d",tree,size));

  if (!default_alloc_size)
    default_alloc_size= DEFAULT_ALLOC_SIZE;
  bzero((gptr) &tree->null_element,sizeof(tree->null_element));
  tree->root= &tree->null_element;
  tree->compare=compare;
  tree->size_of_element=size > 0 ? (uint) size : 0;
  tree->free=free_element;
  tree->elements_in_tree=0;
  tree->cmp_arg = 0;
  tree->null_element.colour=BLACK;
  tree->null_element.left=tree->null_element.right=0;
  if (!free_element && size >= 0 &&
      ((uint) size <= sizeof(void*) || ((uint) size & (sizeof(void*)-1))))
  {
    tree->offset_to_key=sizeof(TREE_ELEMENT); /* Put key after element */
    /* Fix allocation size so that we don't lose any memory */
    default_alloc_size/=(sizeof(TREE_ELEMENT)+size);
    if (!default_alloc_size)
      default_alloc_size=1;
    default_alloc_size*=(sizeof(TREE_ELEMENT)+size);
  }
  else
  {
    tree->offset_to_key=0;		/* use key through pointer */
    tree->size_of_element+=sizeof(void*);
  }
  if (!(tree->with_delete=with_delete))
  {
    init_alloc_root(&tree->mem_root, default_alloc_size,0);
    tree->mem_root.min_malloc=(sizeof(TREE_ELEMENT)+tree->size_of_element);
  }
  DBUG_VOID_RETURN;
}

static void free_tree(TREE *tree, myf free_flags)
{
  DBUG_ENTER("free_tree");
  DBUG_PRINT("enter",("tree: %lx",tree));

  if (tree->root)				/* If initialized */
  {
    if (tree->with_delete)
      delete_tree_element(tree,tree->root);
    else
    {
      if (tree->free)
	delete_tree_element(tree,tree->root);
      free_root(&tree->mem_root, free_flags);
    }
  }
  tree->root= &tree->null_element;
  tree->elements_in_tree=0;

  DBUG_VOID_RETURN;
}

void delete_tree(TREE* tree)
{
  free_tree(tree, MYF(0)); /* my_free() mem_root if applicable */
}

void reset_tree(TREE* tree)
{
  free_tree(tree, MYF(MY_MARK_BLOCKS_FREE));
  /* do not my_free() mem_root if applicable, just mark blocks as free */
}


static void delete_tree_element(TREE *tree, TREE_ELEMENT *element)
{
  if (element != &tree->null_element)
  {
    delete_tree_element(tree,element->left);
    delete_tree_element(tree,element->right);
    if (tree->free)
      (*tree->free)(ELEMENT_KEY(tree,element));
    if (tree->with_delete)
      my_free((void*) element,MYF(0));
  }
}

	/* Code for insert, search and delete of elements */
	/*   parent[0] = & parent[-1][0]->left ||
	     parent[0] = & parent[-1][0]->right */


TREE_ELEMENT *tree_insert(TREE *tree, void *key, uint key_size)
{
  int cmp;
  TREE_ELEMENT *element,***parent;

  parent= tree->parents;
  *parent = &tree->root; element= tree->root;
  for (;;)
  {
    if (element == &tree->null_element ||
	(cmp=(*tree->compare)(tree->cmp_arg,
			      ELEMENT_KEY(tree,element),key)) == 0)
      break;
    if (cmp < 0)
    {
      *++parent= &element->right; element= element->right;
    }
    else
    {
      *++parent = &element->left; element= element->left;
    }
  }
  if (element == &tree->null_element)
  {
    key_size+=tree->size_of_element;
    if (tree->with_delete)
      element=(TREE_ELEMENT *) my_malloc(sizeof(TREE_ELEMENT)+key_size,
					 MYF(MY_WME));
    else
      element=(TREE_ELEMENT *)
	alloc_root(&tree->mem_root,sizeof(TREE_ELEMENT)+key_size);
    if (!element)
      return(NULL);
    **parent=element;
    element->left=element->right= &tree->null_element;
    if (!tree->offset_to_key)
    {
      if (key_size == sizeof(void*))		 /* no length, save pointer */
	*((void**) (element+1))=key;
      else
      {
	*((void**) (element+1))= (void*) ((void **) (element+1)+1);
	memcpy((byte*) *((void **) (element+1)),key,
	       (size_t) (key_size-sizeof(void*)));
      }
    }
    else
      memcpy((byte*) element+tree->offset_to_key,key,(size_t) key_size);
    element->count=1;				/* May give warning in purify */
    tree->elements_in_tree++;
    rb_insert(tree,parent,element);		/* rebalance tree */
  }
  else
    element->count++;
  return element;
}


int tree_delete(TREE *tree, void *key)
{
  int cmp,remove_colour;
  TREE_ELEMENT *element,***parent, ***org_parent, *nod;
  if (!tree->with_delete)
    return 1;					/* not allowed */

  parent= tree->parents;
  *parent= &tree->root; element= tree->root;
  for (;;)
  {
    if (element == &tree->null_element)
      return 1;				/* Was not in tree */
    if ((cmp=(*tree->compare)(tree->cmp_arg,
			      ELEMENT_KEY(tree,element),key)) == 0)
      break;
    if (cmp < 0)
    {
      *++parent= &element->right; element= element->right;
    }
    else
    {
      *++parent = &element->left; element= element->left;
    }
  }
  if (element->left == &tree->null_element)
  {
    (**parent)=element->right;
    remove_colour= element->colour;
  }
  else if (element->right == &tree->null_element)
  {
    (**parent)=element->left;
    remove_colour= element->colour;
  }
  else
  {
    org_parent= parent;
    *++parent= &element->right; nod= element->right;
    while (nod->left != &tree->null_element)
    {
      *++parent= &nod->left; nod= nod->left;
    }
    (**parent)=nod->right;		/* unlink nod from tree */
    remove_colour= nod->colour;
    org_parent[0][0]=nod;		/* put y in place of element */
    org_parent[1]= &nod->right;
    nod->left=element->left;
    nod->right=element->right;
    nod->colour=element->colour;
  }
  if (remove_colour == BLACK)
    rb_delete_fixup(tree,parent);
  if (tree->free)
    (*tree->free)(ELEMENT_KEY(tree,element));
  my_free((gptr) element,MYF(0));
  tree->elements_in_tree--;
  return 0;
}


void *tree_search(TREE *tree, void *key)
{
  int cmp;
  TREE_ELEMENT *element=tree->root;

  for (;;)
  {
    if (element == &tree->null_element)
      return (void*) 0;
    if ((cmp=(*tree->compare)(tree->cmp_arg,
			      ELEMENT_KEY(tree,element),key)) == 0)
      return ELEMENT_KEY(tree,element);
    if (cmp < 0)
      element=element->right;
    else
      element=element->left;
  }
}


int tree_walk(TREE *tree, tree_walk_action action, void *argument, TREE_WALK visit)
{
  switch (visit) {
  case left_root_right:
    return tree_walk_left_root_right(tree,tree->root,action,argument);
  case right_root_left:
    return tree_walk_right_root_left(tree,tree->root,action,argument);
  }
  return 0;			/* Keep gcc happy */
}

static int tree_walk_left_root_right(TREE *tree, TREE_ELEMENT *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->left)				/* Not null_element */
  {
    if ((error=tree_walk_left_root_right(tree,element->left,action,
					  argument)) == 0 &&
	(error=(*action)(ELEMENT_KEY(tree,element),
			  (element_count) element->count,
			  argument)) == 0)
      error=tree_walk_left_root_right(tree,element->right,action,argument);
    return error;
  }
  return 0;
}

static int tree_walk_right_root_left(TREE *tree, TREE_ELEMENT *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->right)				/* Not null_element */
  {
    if ((error=tree_walk_right_root_left(tree,element->right,action,
					  argument)) == 0 &&
	(error=(*action)(ELEMENT_KEY(tree,element),
			  (element_count) element->count,
			  argument)) == 0)
     error=tree_walk_right_root_left(tree,element->left,action,argument);
    return error;
  }
  return 0;
}


	/* Functions to fix up the tree after insert and delete */

static void left_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *y;

  y=leaf->right;
  leaf->right=y->left;
  parent[0]=y;
  y->left=leaf;
}

static void right_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *x;

  x=leaf->left;
  leaf->left=x->right;
  parent[0]=x;
  x->right=leaf;
}

static void rb_insert(TREE *tree, TREE_ELEMENT ***parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *y,*par,*par2;

  leaf->colour=RED;
  while (leaf != tree->root && (par=parent[-1][0])->colour == RED)
  {
    if (par == (par2=parent[-2][0])->left)
    {
      y= par2->right;
      if (y->colour == RED)
      {
	par->colour=BLACK;
	y->colour=BLACK;
	leaf=par2;
	parent-=2;
	leaf->colour=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->right)
	{
	  left_rotate(parent[-1],par);
	  par=leaf;			/* leaf is now parent to old leaf */
	}
	par->colour=BLACK;
	par2->colour=RED;
	right_rotate(parent[-2],par2);
	break;
      }
    }
    else
    {
      y= par2->left;
      if (y->colour == RED)
      {
	par->colour=BLACK;
	y->colour=BLACK;
	leaf=par2;
	parent-=2;
	leaf->colour=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->left)
	{
	  right_rotate(parent[-1],par);
	  par=leaf;
	}
	par->colour=BLACK;
	par2->colour=RED;
	left_rotate(parent[-2],par2);
	break;
      }
    }
  }
  tree->root->colour=BLACK;
}

static void rb_delete_fixup(TREE *tree, TREE_ELEMENT ***parent)
{
  TREE_ELEMENT *x,*w,*par;

  x= **parent;
  while (x != tree->root && x->colour == BLACK)
  {
    if (x == (par=parent[-1][0])->left)
    {
      w=par->right;
      if (w->colour == RED)
      {
	w->colour=BLACK;
	par->colour=RED;
	left_rotate(parent[-1],par);
	parent[0]= &w->left;
	*++parent= &par->left;
	w=par->right;
      }
      if (w->left->colour == BLACK && w->right->colour == BLACK)
      {
	w->colour=RED;
	x=par;
	parent--;
      }
      else
      {
	if (w->right->colour == BLACK)
	{
	  w->left->colour=BLACK;
	  w->colour=RED;
	  right_rotate(&par->right,w);
	  w=par->right;
	}
	w->colour=par->colour;
	par->colour=BLACK;
	w->right->colour=BLACK;
	left_rotate(parent[-1],par);
	x=tree->root;
	break;
      }
    }
    else
    {
      w=par->left;
      if (w->colour == RED)
      {
	w->colour=BLACK;
	par->colour=RED;
	right_rotate(parent[-1],par);
	parent[0]= &w->right;
	*++parent= &par->right;
	w=par->left;
      }
      if (w->right->colour == BLACK && w->left->colour == BLACK)
      {
	w->colour=RED;
	x=par;
	parent--;
      }
      else
      {
	if (w->left->colour == BLACK)
	{
	  w->right->colour=BLACK;
	  w->colour=RED;
	  left_rotate(&par->left,w);
	  w=par->left;
	}
	w->colour=par->colour;
	par->colour=BLACK;
	w->left->colour=BLACK;
	right_rotate(parent[-1],par);
	x=tree->root;
	break;
      }
    }
  }
  x->colour=BLACK;
}


#ifdef TESTING_TREES

	/* Test that the proporties for a red-black tree holds */

static int test_rb_tree(TREE_ELEMENT *element)
{
  int count_l,count_r;

  if (!element->left)
    return 0;				/* Found end of tree */
  if (element->colour == RED &&
      (element->left->colour == RED || element->right->colour == RED))
  {
    printf("Wrong tree: Found two red in a row\n");
    return -1;
  }
  count_l=test_rb_tree(element->left);
  count_r=test_rb_tree(element->right);
  if (count_l >= 0 && count_r >= 0)
  {
    if (count_l == count_r)
      return count_l+(element->colour == BLACK);
    printf("Wrong tree: Incorrect black-count: %d - %d\n",count_l,count_r);
  }
  return -1;
}

#endif
