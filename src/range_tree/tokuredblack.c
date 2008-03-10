/*
   Redblack balanced tree algorithm
   Copyright (C) Damian Ivereigh 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version. See the file COPYING for details.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Implement the red/black tree structure. It is designed to emulate
** the standard tsearch() stuff. i.e. the calling conventions are
** exactly the same
*/

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <tokuredblack.h>
#include <assert.h>

/* Dummy (sentinel) node, so that we can make X->left->up = X
** We then use this instead of NULL to mean the top or bottom
** end of the rb tree. It is a black node.
**
** Initialization of the last field in this initializer is left implicit
** because it could be of any type.  We count on the compiler to zero it.
*/
static struct toku_rbt_node toku_rbt__null;
static struct toku_rbt_node* RBNULL = &toku_rbt__null;


static struct toku_rbt_node *toku_rbt__alloc(struct toku_rbt_tree *rbinfo) {return (struct toku_rbt_node *) rbinfo->rb_malloc(sizeof(struct toku_rbt_node));}
static void toku_rbt__free(struct toku_rbt_tree *rbinfo, struct toku_rbt_node *x) {rbinfo->rb_free(x);}

/* These functions are always needed */
static void toku_rbt__left_rotate(struct toku_rbt_node **, struct toku_rbt_node *);
static void toku_rbt__right_rotate(struct toku_rbt_node **, struct toku_rbt_node *);
static struct toku_rbt_node *toku_rbt__successor(const struct toku_rbt_node *);
static struct toku_rbt_node *toku_rbt__predecessor(const struct toku_rbt_node *);
static struct toku_rbt_node *toku_rbt__traverse(int, const toku_range * , struct toku_rbt_tree *);

/* These functions may not be needed */
static struct toku_rbt_node* toku_rbt__insert(
    const  toku_range* key,
    struct toku_rbt_tree*   rbinfo,
    struct toku_rbt_node*   parent
);

static struct toku_rbt_node *toku_rbt__lookup(int, const toku_range * , struct toku_rbt_tree *, struct toku_rbt_node**);

static void toku_rbt__destroy(struct toku_rbt_tree *rbinfo, struct toku_rbt_node *);

static void toku_rbt__delete(struct toku_rbt_tree* rbinfo, struct toku_rbt_node **, struct toku_rbt_node *);
static void toku_rbt__delete_fix(struct toku_rbt_node **, struct toku_rbt_node *);

/*
** OK here we go, the balanced tree stuff. The algorithm is the
** fairly standard red/black taken from "Introduction to Algorithms"
** by Cormen, Leiserson & Rivest. Maybe one of these days I will
** fully understand all this stuff.
**
** Basically a red/black balanced tree has the following properties:-
** 1) Every node is either red or black (colour is RED or BLACK)
** 2) A leaf (RBNULL pointer) is considered black
** 3) If a node is red then its children are black
** 4) Every path from a node to a leaf contains the same no
**    of black nodes
**
** 3) & 4) above guarantee that the longest path (alternating
** red and black nodes) is only twice as long as the shortest
** path (all black nodes). Thus the tree remains fairly balanced.
*/

/*
 * Initialise a tree. Identifies the comparison routine and any config
 * data that must be sent to it when called.
 * Returns a pointer to the top of the tree.
 */
int toku_rbt_init (
    int (*cmp)(const toku_range*, const toku_range*),
    struct toku_rbt_tree** ptree,
    void* (*user_malloc) (size_t),
    void  (*user_free)   (void*),
    void* (*user_realloc)(void*, size_t)
)
{
    struct toku_rbt_tree* temptree = NULL;
    int r = ENOSYS;

   static int toku_rbt__null_is_initialized = 0;
   if (!toku_rbt__null_is_initialized) {
      toku_rbt__null_is_initialized = 1;
      toku_rbt__null.up     = &toku_rbt__null;
      toku_rbt__null.left   = &toku_rbt__null;
      toku_rbt__null.right  = &toku_rbt__null;
      toku_rbt__null.colour = BLACK;
      /* Key is initialized since the toku_rbt__null is static. */
   }

    if (!ptree)    { r = EINVAL; goto cleanup; }
    temptree=(struct toku_rbt_tree *) user_malloc(sizeof(struct toku_rbt_tree));
    if (!temptree) { r = ENOMEM; goto cleanup; }
    
    temptree->rb_cmp=cmp;
    temptree->rb_root=RBNULL;
    temptree->rb_malloc  = user_malloc;
    temptree->rb_free    = user_free;
    temptree->rb_realloc = user_realloc;
    

    *ptree = temptree;
    r = 0;    
cleanup:
    return r;
}

void
toku_rbt_destroy(struct toku_rbt_tree *rbinfo)
{
    if (rbinfo==NULL)
        return;

    if (rbinfo->rb_root!=RBNULL)
        toku_rbt__destroy(rbinfo, rbinfo->rb_root);
    
    rbinfo->rb_free(rbinfo);
}

int toku_rbt_finger_delete(struct toku_rbt_node* node, struct toku_rbt_tree *rbinfo) {
    int r = ENOSYS;

    if (!rbinfo || !node || node == RBNULL) { r = EINVAL; goto cleanup; }
    toku_rbt__delete(rbinfo, &rbinfo->rb_root, node);
    r = 0;
cleanup:
    return r;
}

int toku_rbt_lookup(
    int mode,
    const  toku_range*  key,
    struct toku_rbt_tree*    rbinfo,
    struct toku_rbt_node**   pinsert_finger,
    struct toku_rbt_node**   pelement_finger,
    const  toku_range** pdata
)
{
    int r = ENOSYS;

    if (!rbinfo || !rbinfo->rb_root || !pdata ||
        !pinsert_finger || !pelement_finger ||
        (
           (mode == RB_LUFIRST || mode == RB_LULAST) != (key == NULL)
        )) {
        r = EINVAL; goto cleanup; }
    
    *pelement_finger = toku_rbt__lookup(mode, key, rbinfo, pinsert_finger);
    *pdata = *pelement_finger == RBNULL ? NULL : RB_GET((*pelement_finger), key);
    r = 0;
cleanup:
    return r;
}

/* --------------------------------------------------------------------- */

/* Search for and if not found and insert is true, will add a new
** node in. Returns a pointer to the new node, or the node found
*/
static struct toku_rbt_node *
toku_rbt__traverse(int insert, const toku_range *key, struct toku_rbt_tree *rbinfo)
{
    struct toku_rbt_node *x,*y;
    int cmp;
    int found=0;
    int cmpmods();

    y=RBNULL; /* points to the parent of x */
    x=rbinfo->rb_root;

    /* walk x down the tree */
    while(x!=RBNULL && found==0)
    {
        y=x;
        /* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
        cmp=rbinfo->rb_cmp(key, RB_GET(x, key));

        if (cmp<0)
            x=x->left;
        else if (cmp>0)
            x=x->right;
        else
            found=1;
    }

    if (found || !insert)
        return(x);

    return toku_rbt__insert(key, rbinfo, y);
}

static struct toku_rbt_node* toku_rbt__insert(
    const  toku_range* key,
    struct toku_rbt_tree*   rbinfo,
    struct toku_rbt_node*   parent
) {
    struct toku_rbt_node* x;
    struct toku_rbt_node* y = parent;
    struct toku_rbt_node* z;
    int cmp;

    if (parent == NULL) {
        /* This means we have NOT actually located the right spot.
           Locate it with traverse and then insert. */
        return toku_rbt__traverse(1, key, rbinfo);
    }

    if ((z=toku_rbt__alloc(rbinfo))==NULL)
    {
        /* Whoops, no memory */
        return(RBNULL);
    }

    RB_SET(z, key, key);
    z->up=y;
    if (y==RBNULL)
    {
        rbinfo->rb_root=z;
    }
    else
    {
        cmp=rbinfo->rb_cmp(RB_GET(z, key), RB_GET(y, key));
        if (cmp<0)
            y->left=z;
        else
            y->right=z;
    }

    z->left=RBNULL;
    z->right=RBNULL;

    /* colour this new node red */
    z->colour=RED;

    /* Having added a red node, we must now walk back up the tree balancing
    ** it, by a series of rotations and changing of colours
    */
    x=z;

    /* While we are not at the top and our parent node is red
    ** N.B. Since the root node is garanteed black, then we
    ** are also going to stop if we are the child of the root
    */

    while(x != rbinfo->rb_root && (x->up->colour == RED))
    {
        /* if our parent is on the left side of our grandparent */
        if (x->up == x->up->up->left)
        {
            /* get the right side of our grandparent (uncle?) */
            y=x->up->up->right;
            if (y->colour == RED)
            {
                /* make our parent black */
                x->up->colour = BLACK;
                /* make our uncle black */
                y->colour = BLACK;
                /* make our grandparent red */
                x->up->up->colour = RED;

                /* now consider our grandparent */
                x=x->up->up;
            }
            else
            {
                /* if we are on the right side of our parent */
                if (x == x->up->right)
                {
                    /* Move up to our parent */
                    x=x->up;
                    toku_rbt__left_rotate(&rbinfo->rb_root, x);
                }

                /* make our parent black */
                x->up->colour = BLACK;
                /* make our grandparent red */
                x->up->up->colour = RED;
                /* right rotate our grandparent */
                toku_rbt__right_rotate(&rbinfo->rb_root, x->up->up);
            }
        }
        else
        {
            /* everything here is the same as above, but
            ** exchanging left for right
            */

            y=x->up->up->left;
            if (y->colour == RED)
            {
                x->up->colour = BLACK;
                y->colour = BLACK;
                x->up->up->colour = RED;

                x=x->up->up;
            }
            else
            {
                if (x == x->up->left)
                {
                    x=x->up;
                    toku_rbt__right_rotate(&rbinfo->rb_root, x);
                }

                x->up->colour = BLACK;
                x->up->up->colour = RED;
                toku_rbt__left_rotate(&rbinfo->rb_root, x->up->up);
            }
        }
    }

    /* Set the root node black */
    (rbinfo->rb_root)->colour = BLACK;

    return(z);
}

/* Search for a key according to mode (see redblack.h)
*/
static struct toku_rbt_node *
toku_rbt__lookup(int mode, const toku_range *key, struct toku_rbt_tree *rbinfo, struct toku_rbt_node** pinsert_finger)
{
    struct toku_rbt_node *x,*y;
    int cmp = 0;
    int found=0;

    y=RBNULL; /* points to the parent of x */
    x=rbinfo->rb_root;

    if (mode==RB_LUFIRST)
    {
        /* Keep going left until we hit a NULL */
        while(x!=RBNULL)
        {
            y=x;
            x=x->left;
        }

        return(y);
    }
    else if (mode==RB_LULAST)
    {
        /* Keep going right until we hit a NULL */
        while(x!=RBNULL)
        {
            y=x;
            x=x->right;
        }

        return(y);
    }

    /* walk x down the tree */
    while(x!=RBNULL && found==0)
    {
        y=x;
        /* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
        cmp=rbinfo->rb_cmp(key, RB_GET(x, key));


        if (cmp<0)
            x=x->left;
        else if (cmp>0)
            x=x->right;
        else
            found=1;
    }
    if (pinsert_finger) *pinsert_finger = y;

    if (found && (mode==RB_LUEQUAL || mode==RB_LUGTEQ || mode==RB_LULTEQ))
        return(x);
    
    if (!found && (mode==RB_LUEQUAL || mode==RB_LUNEXT || mode==RB_LUPREV))
        return(RBNULL);
    
    if (mode==RB_LUGTEQ || (!found && mode==RB_LUGREAT))
    {
        if (cmp>0)
            return(toku_rbt__successor(y));
        else
            return(y);
    }

    if (mode==RB_LULTEQ || (!found && mode==RB_LULESS))
    {
        if (cmp<0)
            return(toku_rbt__predecessor(y));
        else
            return(y);
    }

    if (mode==RB_LUNEXT || (found && mode==RB_LUGREAT))
        return(toku_rbt__successor(x));

    if (mode==RB_LUPREV || (found && mode==RB_LULESS))
        return(toku_rbt__predecessor(x));
    
    /* Shouldn't get here */
    return(RBNULL);
}

/*
 * Destroy all the elements blow us in the tree
 * only useful as part of a complete tree destroy.
 */
static void
toku_rbt__destroy(struct toku_rbt_tree *rbinfo, struct toku_rbt_node *x)
{
    if (x!=RBNULL)
    {
        if (x->left!=RBNULL)
            toku_rbt__destroy(rbinfo, x->left);
        if (x->right!=RBNULL)
            toku_rbt__destroy(rbinfo, x->right);
        toku_rbt__free(rbinfo,x);
    }
}

/*
** Rotate our tree thus:-
**
**             X        rb_left_rotate(X)--->            Y
**           /   \                                     /   \
**          A     Y     <---rb_right_rotate(Y)        X     C
**              /   \                               /   \
**             B     C                             A     B
**
** N.B. This does not change the ordering.
**
** We assume that neither X or Y is NULL
*/

static void
toku_rbt__left_rotate(struct toku_rbt_node **rootp, struct toku_rbt_node *x)
{
    struct toku_rbt_node *y;

    assert(x!=RBNULL);
    assert(x->right!=RBNULL);

    y=x->right; /* set Y */

    /* Turn Y's left subtree into X's right subtree (move B)*/
    x->right = y->left;

    /* If B is not null, set it's parent to be X */
    if (y->left != RBNULL)
        y->left->up = x;

    /* Set Y's parent to be what X's parent was */
    y->up = x->up;

    /* if X was the root */
    if (x->up == RBNULL)
    {
        *rootp=y;
    }
    else
    {
        /* Set X's parent's left or right pointer to be Y */
        if (x == x->up->left)
        {
            x->up->left=y;
        }
        else
        {
            x->up->right=y;
        }
    }

    /* Put X on Y's left */
    y->left=x;

    /* Set X's parent to be Y */
    x->up = y;
}

static void
toku_rbt__right_rotate(struct toku_rbt_node **rootp, struct toku_rbt_node *y)
{
    struct toku_rbt_node *x;

    assert(y!=RBNULL);
    assert(y->left!=RBNULL);

    x=y->left; /* set X */

    /* Turn X's right subtree into Y's left subtree (move B) */
    y->left = x->right;

    /* If B is not null, set it's parent to be Y */
    if (x->right != RBNULL)
        x->right->up = y;

    /* Set X's parent to be what Y's parent was */
    x->up = y->up;

    /* if Y was the root */
    if (y->up == RBNULL)
    {
        *rootp=x;
    }
    else
    {
        /* Set Y's parent's left or right pointer to be X */
        if (y == y->up->left)
        {
            y->up->left=x;
        }
        else
        {
            y->up->right=x;
        }
    }

    /* Put Y on X's right */
    x->right=y;

    /* Set Y's parent to be X */
    y->up = x;
}

/* Return a pointer to the smallest key greater than x
*/
static struct toku_rbt_node *
toku_rbt__successor(const struct toku_rbt_node *x)
{
    struct toku_rbt_node *y;

    if (x->right!=RBNULL)
    {
        /* If right is not NULL then go right one and
        ** then keep going left until we find a node with
        ** no left pointer.
        */
        for (y=x->right; y->left!=RBNULL; y=y->left);
    }
    else
    {
        /* Go up the tree until we get to a node that is on the
        ** left of its parent (or the root) and then return the
        ** parent.
        */
        y=x->up;
        while(y!=RBNULL && x==y->right)
        {
            x=y;
            y=y->up;
        }
    }
    return(y);
}

/* Return a pointer to the largest key smaller than x
*/
static struct toku_rbt_node *
toku_rbt__predecessor(const struct toku_rbt_node *x)
{
    struct toku_rbt_node *y;

    if (x->left!=RBNULL)
    {
        /* If left is not NULL then go left one and
        ** then keep going right until we find a node with
        ** no right pointer.
        */
        for (y=x->left; y->right!=RBNULL; y=y->right);
    }
    else
    {
        /* Go up the tree until we get to a node that is on the
        ** right of its parent (or the root) and then return the
        ** parent.
        */
        y=x->up;
        while(y!=RBNULL && x==y->left)
        {
            x=y;
            y=y->up;
        }
    }
    return(y);
}

int toku_rbt_finger_predecessor(const struct toku_rbt_node** pfinger,
                                           const toku_range** ppred_data) {
    int r = ENOSYS;

    if (!pfinger || !*pfinger ||
        *pfinger == RBNULL || !ppred_data) { r = EINVAL; goto cleanup; }
    *pfinger = toku_rbt__predecessor(*pfinger);
    *ppred_data = ((*pfinger==RBNULL) ? NULL : RB_GET((*pfinger), key));
    r = 0;
cleanup:
    return r;
}

int toku_rbt_finger_succecessor(const struct toku_rbt_node** pfinger,
                                           const toku_range** psucc_data) {
    int r = ENOSYS;

    if (!pfinger || !*pfinger ||
        *pfinger == RBNULL || !psucc_data) { r = EINVAL; goto cleanup; }
    *pfinger = toku_rbt__successor(*pfinger);
    *psucc_data = ((*pfinger==RBNULL) ? NULL : RB_GET((*pfinger), key));
    r = 0;
cleanup:
    return r;
}

const toku_range* toku_rbt_finger_insert(
    const  toku_range* key,
    struct toku_rbt_tree*   rbinfo,
    struct toku_rbt_node*   parent
) {
    struct toku_rbt_node* x;
    if (!parent) return NULL;
    x = toku_rbt__insert(key, rbinfo, parent);
    return ((x==RBNULL) ? NULL : RB_GET(x, key));
}

/* Delete the node z, and free up the space
*/
static void
toku_rbt__delete(struct toku_rbt_tree* rbinfo, struct toku_rbt_node **rootp, struct toku_rbt_node *z)
{
    struct toku_rbt_node *x, *y;

    if (z->left == RBNULL || z->right == RBNULL)
        y=z;
    else
        y=toku_rbt__successor(z);

    if (y->left != RBNULL)
        x=y->left;
    else
        x=y->right;

    x->up = y->up;

    if (y->up == RBNULL)
    {
        *rootp=x;
    }
    else
    {
        if (y==y->up->left)
            y->up->left = x;
        else
            y->up->right = x;
    }

    if (y!=z)
    {
        RB_SET(z, key, RB_GET(y, key));
    }

    if (y->colour == BLACK)
        toku_rbt__delete_fix(rootp, x);

    toku_rbt__free(rbinfo,y);
}

/* Restore the reb-black properties after a delete */
static void
toku_rbt__delete_fix(struct toku_rbt_node **rootp, struct toku_rbt_node *x)
{
    struct toku_rbt_node *w;

    while (x!=*rootp && x->colour==BLACK)
    {
        if (x==x->up->left)
        {
            w=x->up->right;
            if (w->colour==RED)
            {
                w->colour=BLACK;
                x->up->colour=RED;
                toku_rbt__left_rotate(rootp, x->up);
                w=x->up->right;
            }

            if (w->left->colour==BLACK && w->right->colour==BLACK)
            {
                w->colour=RED;
                x=x->up;
            }
            else
            {
                if (w->right->colour == BLACK)
                {
                    w->left->colour=BLACK;
                    w->colour=RED;
                    toku_rbt__right_rotate(rootp, w);
                    w=x->up->right;
                }


                w->colour=x->up->colour;
                x->up->colour = BLACK;
                w->right->colour = BLACK;
                toku_rbt__left_rotate(rootp, x->up);
                x=*rootp;
            }
        }
        else
        {
            w=x->up->left;
            if (w->colour==RED)
            {
                w->colour=BLACK;
                x->up->colour=RED;
                toku_rbt__right_rotate(rootp, x->up);
                w=x->up->left;
            }

            if (w->right->colour==BLACK && w->left->colour==BLACK)
            {
                w->colour=RED;
                x=x->up;
            }
            else
            {
                if (w->left->colour == BLACK)
                {
                    w->right->colour=BLACK;
                    w->colour=RED;
                    toku_rbt__left_rotate(rootp, w);
                    w=x->up->left;
                }

                w->colour=x->up->colour;
                x->up->colour = BLACK;
                w->left->colour = BLACK;
                toku_rbt__right_rotate(rootp, x->up);
                x=*rootp;
            }
        }
    }

    x->colour=BLACK;
}
