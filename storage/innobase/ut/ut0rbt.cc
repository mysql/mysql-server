/****************************************************************************
 Copyright (c) 2007, 2023, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License, version 2.0, as published by the
 Free Software Foundation.

 This program is also distributed with certain software (including but not
 limited to OpenSSL) that is licensed under separate terms, as designated in a
 particular file or component or in included license documentation. The authors
 of MySQL hereby grant you an additional permission to link the program and
 your derivative works with the separately licensed software that they have
 included with MySQL.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
 for more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

 *****************************************************************************/
/** Red-Black tree implementation

 (c) 2007 Oracle/Innobase Oy

 Created 2007-03-20 Sunny Bains
 ***********************************************************************/

#include "ut0rbt.h"
#include "univ.i"
#include "ut0new.h"

/** Definition of a red-black tree
 ==============================

 A red-black tree is a binary search tree which has the following
 red-black properties:

    1. Every node is either red or black.
    2. Every leaf (NULL - in our case tree->nil) is black.
    3. If a node is red, then both its children are black.
    4. Every simple path from a node to a descendant leaf contains the
       same number of black nodes.

    from (3) above, the implication is that on any path from the root
    to a leaf, red nodes must not be adjacent.

    However, any number of black nodes may appear in a sequence.
  */

#if defined(IB_RBT_TESTING)
#warning "Testing enabled!"
#endif

inline static ib_rbt_node_t *ROOT(const ib_rbt_t *t) { return t->root->left; }
inline static size_t SIZEOF_NODE(const ib_rbt_t *t) {
  return sizeof(ib_rbt_node_t) + t->sizeof_value - 1;
}

#if defined UNIV_DEBUG || defined IB_RBT_TESTING
/** Verify that the keys are in order.
 @param[in] tree tree to verify
 @return true of OK. false if not ordered */
static bool rbt_check_ordering(const ib_rbt_t *tree) {
  const ib_rbt_node_t *node;
  const ib_rbt_node_t *prev = nullptr;

  /* Iterate over all the nodes, comparing each node with the prev */
  for (node = rbt_first(tree); node; node = rbt_next(tree, prev)) {
    if (prev) {
      int result;

      if (tree->cmp_arg) {
        result =
            tree->compare_with_arg(tree->cmp_arg, prev->value, node->value);
      } else {
        result = tree->compare(prev->value, node->value);
      }

      if (result >= 0) {
        return false;
      }
    }

    prev = node;
  }

  return true;
}

/** Check that every path from the root to the leaves has the same count.
 Count is expressed in the number of black nodes.
 @return 0 on failure else black height of the subtree */
static ulint rbt_count_black_nodes(
    const ib_rbt_t *tree,      /*!< in: tree to verify */
    const ib_rbt_node_t *node) /*!< in: start of sub-tree */
{
  ulint result;

  if (node != tree->nil) {
    ulint left_height = rbt_count_black_nodes(tree, node->left);

    ulint right_height = rbt_count_black_nodes(tree, node->right);

    if (left_height == 0 || right_height == 0 || left_height != right_height) {
      result = 0;
    } else if (node->color == IB_RBT_RED) {
      /* Case 3 */
      if (node->left->color != IB_RBT_BLACK ||
          node->right->color != IB_RBT_BLACK) {
        result = 0;
      } else {
        result = left_height;
      }
      /* Check if it's anything other than RED or BLACK. */
    } else if (node->color != IB_RBT_BLACK) {
      result = 0;
    } else {
      result = right_height + 1;
    }
  } else {
    result = 1;
  }

  return (result);
}
#endif /* UNIV_DEBUG || IB_RBT_TESTING */

/** Turn the node's right child's left sub-tree into node's right sub-tree.
 This will also make node's right child it's parent. */
static void rbt_rotate_left(
    const ib_rbt_node_t *nil, /*!< in: nil node of the tree */
    ib_rbt_node_t *node)      /*!< in: node to rotate */
{
  ib_rbt_node_t *right = node->right;

  node->right = right->left;

  if (right->left != nil) {
    right->left->parent = node;
  }

  /* Right's new parent was node's parent. */
  right->parent = node->parent;

  /* Since root's parent is tree->nil and root->parent->left points
  back to root, we can avoid the check. */
  if (node == node->parent->left) {
    /* Node was on the left of its parent. */
    node->parent->left = right;
  } else {
    /* Node must have been on the right. */
    node->parent->right = right;
  }

  /* Finally, put node on right's left. */
  right->left = node;
  node->parent = right;
}

/** Turn the node's left child's right sub-tree into node's left sub-tree.
 This also make node's left child it's parent. */
static void rbt_rotate_right(
    const ib_rbt_node_t *nil, /*!< in: nil node of tree */
    ib_rbt_node_t *node)      /*!< in: node to rotate */
{
  ib_rbt_node_t *left = node->left;

  node->left = left->right;

  if (left->right != nil) {
    left->right->parent = node;
  }

  /* Left's new parent was node's parent. */
  left->parent = node->parent;

  /* Since root's parent is tree->nil and root->parent->left points
  back to root, we can avoid the check. */
  if (node == node->parent->right) {
    /* Node was on the left of its parent. */
    node->parent->right = left;
  } else {
    /* Node must have been on the left. */
    node->parent->left = left;
  }

  /* Finally, put node on left's right. */
  left->right = node;
  node->parent = left;
}

/** Append a node to the tree. */
static ib_rbt_node_t *rbt_tree_add_child(const ib_rbt_t *tree,
                                         ib_rbt_bound_t *parent,
                                         ib_rbt_node_t *node) {
  /* Cast away the const. */
  ib_rbt_node_t *last = (ib_rbt_node_t *)parent->last;

  if (last == tree->root || parent->result < 0) {
    last->left = node;
  } else {
    /* FIXME: We don't handle duplicates (yet)! */
    ut_a(parent->result != 0);

    last->right = node;
  }

  node->parent = last;

  return (node);
}

/** Generic binary tree insert */
static ib_rbt_node_t *rbt_tree_insert(ib_rbt_t *tree, const void *key,
                                      ib_rbt_node_t *node) {
  ib_rbt_bound_t parent;
  ib_rbt_node_t *current = ROOT(tree);

  parent.result = 0;
  parent.last = tree->root;

  /* Regular binary search. */
  while (current != tree->nil) {
    parent.last = current;

    if (tree->cmp_arg) {
      parent.result =
          tree->compare_with_arg(tree->cmp_arg, key, current->value);
    } else {
      parent.result = tree->compare(key, current->value);
    }

    if (parent.result < 0) {
      current = current->left;
    } else {
      current = current->right;
    }
  }

  ut_a(current == tree->nil);

  rbt_tree_add_child(tree, &parent, node);

  return (node);
}

/** Balance a tree after inserting a node. */
static void rbt_balance_tree(
    const ib_rbt_t *tree, /*!< in: tree to balance */
    ib_rbt_node_t *node)  /*!< in: node that was inserted */
{
  const ib_rbt_node_t *nil = tree->nil;
  ib_rbt_node_t *parent = node->parent;

  /* Restore the red-black property. */
  node->color = IB_RBT_RED;

  while (node != ROOT(tree) && parent->color == IB_RBT_RED) {
    ib_rbt_node_t *grand_parent = parent->parent;

    if (parent == grand_parent->left) {
      ib_rbt_node_t *uncle = grand_parent->right;

      if (uncle->color == IB_RBT_RED) {
        /* Case 1 - change the colors. */
        uncle->color = IB_RBT_BLACK;
        parent->color = IB_RBT_BLACK;
        grand_parent->color = IB_RBT_RED;

        /* Move node up the tree. */
        node = grand_parent;

      } else {
        if (node == parent->right) {
          /* Right is a black node and node is
          to the right, case 2 - move node
          up and rotate. */
          node = parent;
          rbt_rotate_left(nil, node);
        }

        grand_parent = node->parent->parent;

        /* Case 3. */
        node->parent->color = IB_RBT_BLACK;
        grand_parent->color = IB_RBT_RED;

        rbt_rotate_right(nil, grand_parent);
      }

    } else {
      ib_rbt_node_t *uncle = grand_parent->left;

      if (uncle->color == IB_RBT_RED) {
        /* Case 1 - change the colors. */
        uncle->color = IB_RBT_BLACK;
        parent->color = IB_RBT_BLACK;
        grand_parent->color = IB_RBT_RED;

        /* Move node up the tree. */
        node = grand_parent;

      } else {
        if (node == parent->left) {
          /* Left is a black node and node is to
          the right, case 2 - move node up and
          rotate. */
          node = parent;
          rbt_rotate_right(nil, node);
        }

        grand_parent = node->parent->parent;

        /* Case 3. */
        node->parent->color = IB_RBT_BLACK;
        grand_parent->color = IB_RBT_RED;

        rbt_rotate_left(nil, grand_parent);
      }
    }

    parent = node->parent;
  }

  /* Color the root black. */
  ROOT(tree)->color = IB_RBT_BLACK;
}

/** Find the given node's successor.
 @return successor node or NULL if no successor */
static ib_rbt_node_t *rbt_find_successor(
    const ib_rbt_t *tree,         /*!< in: rb tree */
    const ib_rbt_node_t *current) /*!< in: this is declared const
                                  because it can be called via
                                  rbt_next() */
{
  const ib_rbt_node_t *nil = tree->nil;
  ib_rbt_node_t *next = current->right;

  /* Is there a sub-tree to the right that we can follow. */
  if (next != nil) {
    /* Follow the left most links of the current right child. */
    while (next->left != nil) {
      next = next->left;
    }

  } else { /* We will have to go up the tree to find the successor. */
    ib_rbt_node_t *parent = current->parent;

    /* Cast away the const. */
    next = (ib_rbt_node_t *)current;

    while (parent != tree->root && next == parent->right) {
      next = parent;
      parent = next->parent;
    }

    next = (parent == tree->root) ? nullptr : parent;
  }

  return (next);
}

/** Find the given node's precedecessor.
 @return predecessor node or NULL if no predecessor */
static ib_rbt_node_t *rbt_find_predecessor(
    const ib_rbt_t *tree,         /*!< in: rb tree */
    const ib_rbt_node_t *current) /*!< in: this is declared const
                                  because it can be called via
                                  rbt_prev() */
{
  const ib_rbt_node_t *nil = tree->nil;
  ib_rbt_node_t *prev = current->left;

  /* Is there a sub-tree to the left that we can follow. */
  if (prev != nil) {
    /* Follow the right most links of the current left child. */
    while (prev->right != nil) {
      prev = prev->right;
    }

  } else { /* We will have to go up the tree to find the precedecessor. */
    ib_rbt_node_t *parent = current->parent;

    /* Cast away the const. */
    prev = (ib_rbt_node_t *)current;

    while (parent != tree->root && prev == parent->left) {
      prev = parent;
      parent = prev->parent;
    }

    prev = (parent == tree->root) ? nullptr : parent;
  }

  return (prev);
}

/** Replace node with child. After applying transformations eject becomes
 an orphan. */
static void rbt_eject_node(ib_rbt_node_t *eject, /*!< in: node to eject */
                           ib_rbt_node_t *node) /*!< in: node to replace with */
{
  /* Update the to be ejected node's parent's child pointers. */
  if (eject->parent->left == eject) {
    eject->parent->left = node;
  } else if (eject->parent->right == eject) {
    eject->parent->right = node;
  } else {
    ut_error;
  }
  /* eject is now an orphan but otherwise its pointers
  and color are left intact. */

  node->parent = eject->parent;
}

/** Replace a node with another node. */
static void rbt_replace_node(
    ib_rbt_node_t *replace, /*!< in: node to replace */
    ib_rbt_node_t *node)    /*!< in: node to replace with */
{
  ib_rbt_color_t color = node->color;

  /* Update the node pointers. */
  node->left = replace->left;
  node->right = replace->right;

  /* Update the child node pointers. */
  node->left->parent = node;
  node->right->parent = node;

  /* Make the parent of replace point to node. */
  rbt_eject_node(replace, node);

  /* Swap the colors. */
  node->color = replace->color;
  replace->color = color;
}

/** Detach node from the tree replacing it with one of it's children.
 @return the child node that now occupies the position of the detached node */
static ib_rbt_node_t *rbt_detach_node(
    const ib_rbt_t *tree, /*!< in: rb tree */
    ib_rbt_node_t *node)  /*!< in: node to detach */
{
  ib_rbt_node_t *child;
  const ib_rbt_node_t *nil = tree->nil;

  if (node->left != nil && node->right != nil) {
    /* Case where the node to be deleted has two children. */
    ib_rbt_node_t *successor = rbt_find_successor(tree, node);

    ut_a(successor != nil);
    ut_a(successor->parent != nil);
    ut_a(successor->left == nil);

    child = successor->right;

    /* Remove the successor node and replace with its child. */
    rbt_eject_node(successor, child);

    /* Replace the node to delete with its successor node. */
    rbt_replace_node(node, successor);
  } else {
    ut_a(node->left == nil || node->right == nil);

    child = (node->left != nil) ? node->left : node->right;

    /* Replace the node to delete with one of it's children. */
    rbt_eject_node(node, child);
  }

  /* Reset the node links. */
  node->parent = node->right = node->left = tree->nil;

  return (child);
}

/** Rebalance the right sub-tree after deletion.
 @return node to rebalance if more rebalancing required else NULL */
static ib_rbt_node_t *rbt_balance_right(
    const ib_rbt_node_t *nil, /*!< in: rb tree nil node */
    ib_rbt_node_t *parent,    /*!< in: parent node */
    ib_rbt_node_t *sibling)   /*!< in: sibling node */
{
  ib_rbt_node_t *node = nullptr;

  ut_a(sibling != nil);

  /* Case 3. */
  if (sibling->color == IB_RBT_RED) {
    parent->color = IB_RBT_RED;
    sibling->color = IB_RBT_BLACK;

    rbt_rotate_left(nil, parent);

    sibling = parent->right;

    ut_a(sibling != nil);
  }

  /* Since this will violate case 3 because of the change above. */
  if (sibling->left->color == IB_RBT_BLACK &&
      sibling->right->color == IB_RBT_BLACK) {
    node = parent; /* Parent needs to be rebalanced too. */
    sibling->color = IB_RBT_RED;

  } else {
    if (sibling->right->color == IB_RBT_BLACK) {
      ut_a(sibling->left->color == IB_RBT_RED);

      sibling->color = IB_RBT_RED;
      sibling->left->color = IB_RBT_BLACK;

      rbt_rotate_right(nil, sibling);

      sibling = parent->right;
      ut_a(sibling != nil);
    }

    sibling->color = parent->color;
    sibling->right->color = IB_RBT_BLACK;

    parent->color = IB_RBT_BLACK;

    rbt_rotate_left(nil, parent);
  }

  return (node);
}

/** Rebalance the left sub-tree after deletion.
 @return node to rebalance if more rebalancing required else NULL */
static ib_rbt_node_t *rbt_balance_left(
    const ib_rbt_node_t *nil, /*!< in: rb tree nil node */
    ib_rbt_node_t *parent,    /*!< in: parent node */
    ib_rbt_node_t *sibling)   /*!< in: sibling node */
{
  ib_rbt_node_t *node = nullptr;

  ut_a(sibling != nil);

  /* Case 3. */
  if (sibling->color == IB_RBT_RED) {
    parent->color = IB_RBT_RED;
    sibling->color = IB_RBT_BLACK;

    rbt_rotate_right(nil, parent);
    sibling = parent->left;

    ut_a(sibling != nil);
  }

  /* Since this will violate case 3 because of the change above. */
  if (sibling->right->color == IB_RBT_BLACK &&
      sibling->left->color == IB_RBT_BLACK) {
    node = parent; /* Parent needs to be rebalanced too. */
    sibling->color = IB_RBT_RED;

  } else {
    if (sibling->left->color == IB_RBT_BLACK) {
      ut_a(sibling->right->color == IB_RBT_RED);

      sibling->color = IB_RBT_RED;
      sibling->right->color = IB_RBT_BLACK;

      rbt_rotate_left(nil, sibling);

      sibling = parent->left;

      ut_a(sibling != nil);
    }

    sibling->color = parent->color;
    sibling->left->color = IB_RBT_BLACK;

    parent->color = IB_RBT_BLACK;

    rbt_rotate_right(nil, parent);
  }

  return (node);
}

/** Delete the node and rebalance the tree if necessary */
static void rbt_remove_node_and_rebalance(
    ib_rbt_t *tree,      /*!< in: rb tree */
    ib_rbt_node_t *node) /*!< in: node to remove */
{
  /* Detach node and get the node that will be used
  as rebalance start. */
  ib_rbt_node_t *child = rbt_detach_node(tree, node);

  if (node->color == IB_RBT_BLACK) {
    ib_rbt_node_t *last = child;

    ROOT(tree)->color = IB_RBT_RED;

    while (child && child->color == IB_RBT_BLACK) {
      ib_rbt_node_t *parent = child->parent;

      /* Did the deletion cause an imbalance in the
      parents left sub-tree. */
      if (parent->left == child) {
        child = rbt_balance_right(tree->nil, parent, parent->right);

      } else if (parent->right == child) {
        child = rbt_balance_left(tree->nil, parent, parent->left);

      } else {
        ut_error;
      }

      if (child) {
        last = child;
      }
    }

    ut_a(last);

    last->color = IB_RBT_BLACK;
    ROOT(tree)->color = IB_RBT_BLACK;
  }

  /* Note that we have removed a node from the tree. */
  --tree->n_nodes;
}

/** Recursively free the nodes. */
static void rbt_free_node(ib_rbt_node_t *node, /*!< in: node to free */
                          ib_rbt_node_t *nil)  /*!< in: rb tree nil node */
{
  if (node != nil) {
    rbt_free_node(node->left, nil);
    rbt_free_node(node->right, nil);

    ut::free(node);
  }
}

/** Free all the nodes and free the tree. */
void rbt_free(ib_rbt_t *tree) /*!< in: rb tree to free */
{
  rbt_free_node(tree->root, tree->nil);
  ut::free(tree->nil);
  ut::free(tree);
}

/** Create an instance of a red black tree, whose comparison function takes
 an argument
 @return an empty rb tree */
ib_rbt_t *rbt_create_arg_cmp(
    size_t sizeof_value,        /*!< in: sizeof data item */
    ib_rbt_arg_compare compare, /*!< in: fn to compare items */
    void *cmp_arg)              /*!< in: compare fn arg */
{
  ib_rbt_t *tree;

  ut_a(cmp_arg);

  tree = rbt_create(sizeof_value, nullptr);
  tree->cmp_arg = cmp_arg;
  tree->compare_with_arg = compare;

  return (tree);
}

/** Create an instance of a red black tree.
 @return an empty rb tree */
ib_rbt_t *rbt_create(size_t sizeof_value,    /*!< in: sizeof data item */
                     ib_rbt_compare compare) /*!< in: fn to compare items */
{
  ib_rbt_t *tree;
  ib_rbt_node_t *node;

  tree =
      (ib_rbt_t *)ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*tree));

  tree->sizeof_value = sizeof_value;

  /* Create the sentinel (NIL) node. */
  node = tree->nil = (ib_rbt_node_t *)ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(*node));

  node->color = IB_RBT_BLACK;
  node->parent = node->left = node->right = node;

  /* Create the "fake" root, the real root node will be the
  left child of this node. */
  node = tree->root = (ib_rbt_node_t *)ut::zalloc_withkey(
      UT_NEW_THIS_FILE_PSI_KEY, sizeof(*node));

  node->color = IB_RBT_BLACK;
  node->parent = node->left = node->right = tree->nil;

  tree->compare = compare;

  return (tree);
}

/** Generic insert of a value in the rb tree.
 @return inserted node */
const ib_rbt_node_t *rbt_insert(
    ib_rbt_t *tree,    /*!< in: rb tree */
    const void *key,   /*!< in: key for ordering */
    const void *value) /*!< in: value of key, this value
                       is copied to the node */
{
  ib_rbt_node_t *node;

  /* Create the node that will hold the value data. */
  node = (ib_rbt_node_t *)ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                             SIZEOF_NODE(tree));

  memcpy(node->value, value, tree->sizeof_value);
  node->parent = node->left = node->right = tree->nil;

  /* Insert in the tree in the usual way. */
  rbt_tree_insert(tree, key, node);
  rbt_balance_tree(tree, node);

  ++tree->n_nodes;

  return (node);
}

/** Add a new node to the tree, useful for data that is pre-sorted.
 @return appended node */
const ib_rbt_node_t *rbt_add_node(ib_rbt_t *tree,         /*!< in: rb tree */
                                  ib_rbt_bound_t *parent, /*!< in: bounds */
                                  const void *value)      /*!< in: this value is
                                                          copied      to the node */
{
  ib_rbt_node_t *node;

  /* Create the node that will hold the value data */
  node = (ib_rbt_node_t *)ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                                             SIZEOF_NODE(tree));

  memcpy(node->value, value, tree->sizeof_value);
  node->parent = node->left = node->right = tree->nil;

  /* If tree is empty */
  if (parent->last == nullptr) {
    parent->last = tree->root;
  }

  /* Append the node, the hope here is that the caller knows
  what s/he is doing. */
  rbt_tree_add_child(tree, parent, node);
  rbt_balance_tree(tree, node);

  ++tree->n_nodes;

#if (defined IB_RBT_TESTING)
  ut_a(rbt_validate(tree));
#endif
  return (node);
}

/** Find a matching node in the rb tree.
 @return NULL if not found else the node where key was found */
static const ib_rbt_node_t *rbt_lookup(
    const ib_rbt_t *tree, /*!< in: rb tree */
    const void *key)      /*!< in: key to use for search */
{
  const ib_rbt_node_t *current = ROOT(tree);

  /* Regular binary search. */
  while (current != tree->nil) {
    int result;

    if (tree->cmp_arg) {
      result = tree->compare_with_arg(tree->cmp_arg, key, current->value);
    } else {
      result = tree->compare(key, current->value);
    }

    if (result < 0) {
      current = current->left;
    } else if (result > 0) {
      current = current->right;
    } else {
      break;
    }
  }

  return (current != tree->nil ? current : nullptr);
}

/** Delete a node identified by key.
 @return true if success false if not found */
bool rbt_delete(ib_rbt_t *tree,  /*!< in: rb tree */
                const void *key) /*!< in: key to delete */
{
  bool deleted = false;
  ib_rbt_node_t *node = (ib_rbt_node_t *)rbt_lookup(tree, key);

  if (node) {
    rbt_remove_node_and_rebalance(tree, node);

    ut::free(node);
    deleted = true;
  }

  return deleted;
}

/** Remove a node from the rb tree, the node is not free'd, that is the
 callers responsibility.
 @return deleted node but without the const */
ib_rbt_node_t *rbt_remove_node(
    ib_rbt_t *tree,                  /*!< in: rb tree */
    const ib_rbt_node_t *const_node) /*!< in: node to delete, this
                                     is a fudge and declared const
                                     because the caller can access
                                     only const nodes */
{
  /* Cast away the const. */
  rbt_remove_node_and_rebalance(tree, (ib_rbt_node_t *)const_node);

  /* This is to make it easier to do something like this:
          ut::free(rbt_remove_node(node));
  */

  return ((ib_rbt_node_t *)const_node);
}

/** Find the node that has the greatest key that is <= key.
 @return value of result */
int rbt_search(const ib_rbt_t *tree,   /*!< in: rb tree */
               ib_rbt_bound_t *parent, /*!< in: search bounds */
               const void *key)        /*!< in: key to search */
{
  ib_rbt_node_t *current = ROOT(tree);

  /* Every thing is greater than the NULL root. */
  parent->result = 1;
  parent->last = nullptr;

  while (current != tree->nil) {
    parent->last = current;

    if (tree->cmp_arg) {
      parent->result =
          tree->compare_with_arg(tree->cmp_arg, key, current->value);
    } else {
      parent->result = tree->compare(key, current->value);
    }

    if (parent->result > 0) {
      current = current->right;
    } else if (parent->result < 0) {
      current = current->left;
    } else {
      break;
    }
  }

  return (parent->result);
}

/** Find the node that has the greatest key that is <= key. But use the
 supplied comparison function.
 @return value of result */
int rbt_search_cmp(const ib_rbt_t *tree,   /*!< in: rb tree */
                   ib_rbt_bound_t *parent, /*!< in: search bounds */
                   const void *key,        /*!< in: key to search */
                   ib_rbt_compare compare, /*!< in: fn to compare items */
                   ib_rbt_arg_compare arg_compare) /*!< in: fn to compare items
                                                   with argument */
{
  ib_rbt_node_t *current = ROOT(tree);

  /* Every thing is greater than the NULL root. */
  parent->result = 1;
  parent->last = nullptr;

  while (current != tree->nil) {
    parent->last = current;

    if (arg_compare) {
      ut_ad(tree->cmp_arg);
      parent->result = arg_compare(tree->cmp_arg, key, current->value);
    } else {
      parent->result = compare(key, current->value);
    }

    if (parent->result > 0) {
      current = current->right;
    } else if (parent->result < 0) {
      current = current->left;
    } else {
      break;
    }
  }

  return (parent->result);
}

/** Return the left most node in the tree. */
const ib_rbt_node_t *rbt_first(
    /* out leftmost node or NULL */
    const ib_rbt_t *tree) /* in: rb tree */
{
  ib_rbt_node_t *first = nullptr;
  ib_rbt_node_t *current = ROOT(tree);

  while (current != tree->nil) {
    first = current;
    current = current->left;
  }

  return (first);
}

/** Return the right most node in the tree.
 @return the rightmost node or NULL */
const ib_rbt_node_t *rbt_last(const ib_rbt_t *tree) /*!< in: rb tree */
{
  ib_rbt_node_t *last = nullptr;
  ib_rbt_node_t *current = ROOT(tree);

  while (current != tree->nil) {
    last = current;
    current = current->right;
  }

  return (last);
}

/** Return the next node.
 @return node next from current */
const ib_rbt_node_t *rbt_next(
    const ib_rbt_t *tree,         /*!< in: rb tree */
    const ib_rbt_node_t *current) /*!< in: current node */
{
  return (current ? rbt_find_successor(tree, current) : nullptr);
}

/** Return the previous node.
 @return node prev from current */
const ib_rbt_node_t *rbt_prev(
    const ib_rbt_t *tree,         /*!< in: rb tree */
    const ib_rbt_node_t *current) /*!< in: current node */
{
  return (current ? rbt_find_predecessor(tree, current) : nullptr);
}

/** Merge the node from dst into src. Return the number of nodes merged.
 @return no. of recs merged */
ulint rbt_merge_uniq(ib_rbt_t *dst,       /*!< in: dst rb tree */
                     const ib_rbt_t *src) /*!< in: src rb tree */
{
  ib_rbt_bound_t parent;
  ulint n_merged = 0;
  const ib_rbt_node_t *src_node = rbt_first(src);

  if (rbt_empty(src) || dst == src) {
    return (0);
  }

  for (/* No op */; src_node; src_node = rbt_next(src, src_node)) {
    if (rbt_search(dst, &parent, src_node->value) != 0) {
      rbt_add_node(dst, &parent, src_node->value);
      ++n_merged;
    }
  }

  return (n_merged);
}

#if defined UNIV_DEBUG || defined IB_RBT_TESTING
/** Check that every path from the root to the leaves has the same count and
 the tree nodes are in order.
 @return true if OK false otherwise */
bool rbt_validate(const ib_rbt_t *tree) /*!< in: RB tree to validate */
{
  if (rbt_count_black_nodes(tree, ROOT(tree)) > 0) {
    return (rbt_check_ordering(tree));
  }

  return false;
}
#endif /* UNIV_DEBUG || IB_RBT_TESTING */
