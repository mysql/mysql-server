/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file pars/pars0sym.cc
 SQL parser symbol table

 Created 12/15/1997 Heikki Tuuri
 *******************************************************/
#include "current_thd.h"

#include "pars0sym.h"

#include "data0data.h"
#include "data0type.h"
#include "dict0dd.h"
#include "eval0eval.h"
#include "mem0mem.h"
#include "pars0grm.h"
#include "pars0pars.h"
#include "que0que.h"
#include "row0sel.h"

/** Creates a symbol table for a single stored procedure or query.
 @return own: symbol table */
sym_tab_t *sym_tab_create(
    mem_heap_t *heap) /*!< in: memory heap where to create */
{
  sym_tab_t *sym_tab;

  sym_tab = static_cast<sym_tab_t *>(mem_heap_alloc(heap, sizeof(sym_tab_t)));

  UT_LIST_INIT(sym_tab->sym_list);
  UT_LIST_INIT(sym_tab->func_node_list);

  sym_tab->heap = heap;

  return (sym_tab);
}

/** Frees the memory allocated dynamically AFTER parsing phase for variables
 etc. in the symbol table. Does not free the mem heap where the table was
 originally created. Frees also SQL explicit cursor definitions. */
void sym_tab_free_private(sym_tab_t *sym_tab) /*!< in, own: symbol table */
{
  THD *thd = current_thd;

  for (auto sym : sym_tab->sym_list) {
    /* Close the tables opened in pars_retrieve_table_def(). */

    if (sym->token_type == SYM_TABLE_REF_COUNTED) {
      if (sym->mdl != nullptr) {
        dd_table_close(sym->table, thd, &sym->mdl, false);
      } else {
        dd_table_close(sym->table, nullptr, nullptr, false);
      }

      sym->table = nullptr;
      sym->resolved = false;
      sym->token_type = SYM_UNSET;
      sym->mdl = nullptr;
    }

    eval_node_free_val_buf(sym);

    if (sym->prefetch_buf) {
      sel_col_prefetch_buf_free(sym->prefetch_buf);
    }

    if (sym->cursor_def) {
      que_graph_free_recursive(sym->cursor_def);
    }
  }

  for (auto func : sym_tab->func_node_list) {
    eval_node_free_val_buf(func);
  }
}

/** Adds an integer literal to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_int_lit(sym_tab_t *sym_tab, /*!< in: symbol table */
                                ulint val)          /*!< in: integer value */
{
  sym_node_t *node;
  byte *data;

  node = static_cast<sym_node_t *>(
      mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t)));

  node->common.type = QUE_NODE_SYMBOL;

  node->table = nullptr;
  node->resolved = true;
  node->token_type = SYM_LIT;

  node->indirection = nullptr;

  dtype_set(dfield_get_type(&node->common.val), DATA_INT, 0, 4);

  data = static_cast<byte *>(mem_heap_alloc(sym_tab->heap, 4));
  mach_write_to_4(data, val);

  dfield_set_data(&(node->common.val), data, 4);

  node->common.val_buf_size = 0;
  node->prefetch_buf = nullptr;
  node->cursor_def = nullptr;

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  node->like_node = nullptr;

  node->sym_table = sym_tab;

  return (node);
}

/** Adds a string literal to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_str_lit(sym_tab_t *sym_tab, /*!< in: symbol table */
                                const byte *str, /*!< in: string with no quotes
                                                 around it */
                                ulint len)       /*!< in: string length */
{
  sym_node_t *node;
  byte *data;

  node = static_cast<sym_node_t *>(
      mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t)));

  node->common.type = QUE_NODE_SYMBOL;

  node->table = nullptr;
  node->resolved = true;
  node->token_type = SYM_LIT;

  node->indirection = nullptr;

  dtype_set(dfield_get_type(&node->common.val), DATA_VARCHAR, DATA_ENGLISH, 0);

  data = (len) ? static_cast<byte *>(mem_heap_dup(sym_tab->heap, str, len))
               : nullptr;

  dfield_set_data(&(node->common.val), data, len);

  node->common.val_buf_size = 0;
  node->prefetch_buf = nullptr;
  node->cursor_def = nullptr;

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  node->like_node = nullptr;

  node->sym_table = sym_tab;

  return (node);
}

/** Add a bound literal to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_bound_lit(
    sym_tab_t *sym_tab, /*!< in: symbol table */
    const char *name,   /*!< in: name of bound literal */
    ulint *lit_type)    /*!< out: type of literal (PARS_*_LIT) */
{
  sym_node_t *node;
  pars_bound_lit_t *blit;
  ulint len = 0;

  blit = pars_info_get_bound_lit(sym_tab->info, name);
  ut_a(blit);

  node = static_cast<sym_node_t *>(
      mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t)));

  node->common.type = QUE_NODE_SYMBOL;
  node->common.brother = node->common.parent = nullptr;

  node->table = nullptr;
  node->resolved = true;
  node->token_type = SYM_LIT;

  node->indirection = nullptr;

  switch (blit->type) {
    case DATA_FIXBINARY:
      len = blit->length;
      *lit_type = PARS_FIXBINARY_LIT;
      break;

    case DATA_BLOB:
      *lit_type = PARS_BLOB_LIT;
      break;

    case DATA_VARCHAR:
      *lit_type = PARS_STR_LIT;
      break;

    case DATA_CHAR:
      ut_a(blit->length > 0);

      len = blit->length;
      *lit_type = PARS_STR_LIT;
      break;

    case DATA_INT:
      ut_a(blit->length > 0);
      ut_a(blit->length <= 8);

      len = blit->length;
      *lit_type = PARS_INT_LIT;
      break;

    default:
      ut_error;
  }

  dtype_set(dfield_get_type(&node->common.val), blit->type, blit->prtype, len);

  dfield_set_data(&(node->common.val), blit->address, blit->length);

  node->common.val_buf_size = 0;
  node->prefetch_buf = nullptr;
  node->cursor_def = nullptr;

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  blit->node = node;
  node->like_node = nullptr;
  node->sym_table = sym_tab;

  return (node);
}

/**********************************************************************
Rebind literal to a node in the symbol table. */
sym_node_t *sym_tab_rebind_lit(
    /* out: symbol table node */
    sym_node_t *node,    /* in: node that is bound to literal*/
    const void *address, /* in: pointer to data */
    ulint length)        /* in: length of data */
{
  dfield_t *dfield = que_node_get_val(node);
  dtype_t *dtype = dfield_get_type(dfield);

  ut_a(node->token_type == SYM_LIT);

  dfield_set_data(&node->common.val, address, length);

  if (node->like_node) {
    ut_a(dtype_get_mtype(dtype) == DATA_CHAR ||
         dtype_get_mtype(dtype) == DATA_VARCHAR);

    /* Don't force [false] creation of sub-nodes (for LIKE) */
    pars_like_rebind(node, static_cast<const byte *>(address), length);
  }

  /* FIXME: What's this ? */
  node->common.val_buf_size = 0;

  if (node->prefetch_buf) {
    sel_col_prefetch_buf_free(node->prefetch_buf);
    node->prefetch_buf = nullptr;
  }

  if (node->cursor_def) {
    que_graph_free_recursive(node->cursor_def);
    node->cursor_def = nullptr;
  }

  return (node);
}

/** Adds an SQL null literal to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_null_lit(sym_tab_t *sym_tab) /*!< in: symbol table */
{
  sym_node_t *node;

  node = static_cast<sym_node_t *>(
      mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t)));

  node->common.type = QUE_NODE_SYMBOL;

  node->table = nullptr;
  node->resolved = true;
  node->token_type = SYM_LIT;

  node->indirection = nullptr;

  dfield_get_type(&node->common.val)->mtype = DATA_ERROR;

  dfield_set_null(&node->common.val);

  node->common.val_buf_size = 0;
  node->prefetch_buf = nullptr;
  node->cursor_def = nullptr;

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  node->like_node = nullptr;

  node->sym_table = sym_tab;

  return (node);
}

/** Adds an identifier to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_id(sym_tab_t *sym_tab, /*!< in: symbol table */
                           byte *name,         /*!< in: identifier name */
                           ulint len)          /*!< in: identifier length */
{
  sym_node_t *node;

  node =
      static_cast<sym_node_t *>(mem_heap_zalloc(sym_tab->heap, sizeof(*node)));

  node->common.type = QUE_NODE_SYMBOL;

  node->name = mem_heap_strdupl(sym_tab->heap, (char *)name, len);
  node->name_len = len;

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  dfield_set_null(&node->common.val);

  node->sym_table = sym_tab;

  return (node);
}

/** Add a bound identifier to a symbol table.
 @return symbol table node */
sym_node_t *sym_tab_add_bound_id(sym_tab_t *sym_tab, /*!< in: symbol table */
                                 const char *name) /*!< in: name of bound id */
{
  sym_node_t *node;
  pars_bound_id_t *bid;

  bid = pars_info_get_bound_id(sym_tab->info, name);
  ut_a(bid);

  node = static_cast<sym_node_t *>(
      mem_heap_alloc(sym_tab->heap, sizeof(sym_node_t)));

  node->common.type = QUE_NODE_SYMBOL;

  node->table = nullptr;
  node->resolved = false;
  node->token_type = SYM_UNSET;
  node->indirection = nullptr;

  node->name = mem_heap_strdup(sym_tab->heap, bid->id);
  node->name_len = strlen(node->name);

  UT_LIST_ADD_LAST(sym_tab->sym_list, node);

  dfield_set_null(&node->common.val);

  node->common.val_buf_size = 0;
  node->prefetch_buf = nullptr;
  node->cursor_def = nullptr;

  node->like_node = nullptr;

  node->sym_table = sym_tab;

  return (node);
}
