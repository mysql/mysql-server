/*****************************************************************************

Copyright (c) 2007, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file fts/fts0ast.cc
 Full Text Search parser helper file.

 Created 2007/3/16 Sunny Bains.
 ***********************************************************************/

#include <stdlib.h>

#include "fts0ast.h"
#include "fts0fts.h"
#include "fts0pars.h"
#include "ha_prototypes.h"
#include "my_inttypes.h"
#include "row0sel.h"

/* The FTS ast visit pass. */
enum fts_ast_visit_pass_t {
  FTS_PASS_FIRST, /*!< First visit pass,
                  process operators excluding
                  FTS_EXIST and FTS_IGNORE */
  FTS_PASS_EXIST, /*!< Exist visit pass,
                  process operator FTS_EXIST */
  FTS_PASS_IGNORE /*!< Ignore visit pass,
                  process operator FTS_IGNORE */
};

/** Create an empty fts_ast_node_t.
 @return Create a new node */
static fts_ast_node_t *fts_ast_node_create(void) {
  fts_ast_node_t *node;

  node = (fts_ast_node_t *)ut_zalloc_nokey(sizeof(*node));

  return (node);
}

/** Track node allocations, in case there is an error during parsing. */
static void fts_ast_state_add_node(
    fts_ast_state_t *state, /*!< in: ast instance */
    fts_ast_node_t *node)   /*!< in: node to add to ast */
{
  if (!state->list.head) {
    ut_a(!state->list.tail);

    state->list.head = state->list.tail = node;
  } else {
    state->list.tail->next_alloc = node;
    state->list.tail = node;
  }
}

/** Create a operator fts_ast_node_t.
 @return new node */
fts_ast_node_t *fts_ast_create_node_oper(
    void *arg,           /*!< in: ast state instance */
    fts_ast_oper_t oper) /*!< in: ast operator */
{
  fts_ast_node_t *node = fts_ast_node_create();

  node->type = FTS_AST_OPER;
  node->oper = oper;

  fts_ast_state_add_node((fts_ast_state_t *)arg, node);

  return (node);
}

/** This function takes ownership of the ptr and is responsible
 for free'ing it
 @return new node or a node list with tokenized words */
fts_ast_node_t *fts_ast_create_node_term(
    void *arg,                   /*!< in: ast state instance */
    const fts_ast_string_t *ptr) /*!< in: ast term string */
{
  fts_ast_state_t *state = static_cast<fts_ast_state_t *>(arg);
  ulint len = ptr->len;
  ulint cur_pos = 0;
  fts_ast_node_t *node = NULL;
  fts_ast_node_t *node_list = NULL;
  fts_ast_node_t *first_node = NULL;

  /* Scan the incoming string and filter out any "non-word" characters */
  while (cur_pos < len) {
    fts_string_t str;
    ulint cur_len;

    cur_len = innobase_mysql_fts_get_token(
        state->charset, reinterpret_cast<const byte *>(ptr->str) + cur_pos,
        reinterpret_cast<const byte *>(ptr->str) + len, &str);

    if (cur_len == 0) {
      break;
    }

    cur_pos += cur_len;

    if (str.f_n_char > 0) {
      /* If the subsequent term (after the first one)'s size
      is less than fts_min_token_size or the term is greater
      than fts_max_token_size, we shall ignore that. This is
      to make consistent with MyISAM behavior */
      if ((first_node && (str.f_n_char < fts_min_token_size)) ||
          str.f_n_char > fts_max_token_size) {
        continue;
      }

      node = fts_ast_node_create();

      node->type = FTS_AST_TERM;

      node->term.ptr = fts_ast_string_create(str.f_str, str.f_len);

      fts_ast_state_add_node(static_cast<fts_ast_state_t *>(arg), node);

      if (first_node) {
        /* There is more than one word, create
        a list to organize them */
        if (!node_list) {
          node_list = fts_ast_create_node_list(
              static_cast<fts_ast_state_t *>(arg), first_node);
        }

        fts_ast_add_node(node_list, node);
      } else {
        first_node = node;
      }
    }
  }

  return ((node_list != NULL) ? node_list : first_node);
}

/** Create an AST term node, makes a copy of ptr for plugin parser
 @return node */
fts_ast_node_t *fts_ast_create_node_term_for_parser(
    void *arg,       /*!< in: ast state */
    const char *ptr, /*!< in: term string */
    const ulint len) /*!< in: term string length */
{
  fts_ast_node_t *node = NULL;

  /* '%' as first char is forbidden for LIKE in internal SQL parser;
  '%' as last char is reserved for wildcard search;*/
  if (len == 0 || len > FTS_MAX_WORD_LEN || ptr[0] == '%' ||
      ptr[len - 1] == '%') {
    return (NULL);
  }

  node = fts_ast_node_create();

  node->type = FTS_AST_TERM;

  node->term.ptr =
      fts_ast_string_create(reinterpret_cast<const byte *>(ptr), len);

  fts_ast_state_add_node(static_cast<fts_ast_state_t *>(arg), node);

  return (node);
}

/** This function takes ownership of the ptr and is responsible
 for free'ing it.
 @return new node */
fts_ast_node_t *fts_ast_create_node_text(
    void *arg,                   /*!< in: ast state instance */
    const fts_ast_string_t *ptr) /*!< in: ast text string */
{
  ulint len = ptr->len;
  fts_ast_node_t *node = NULL;

  /* Once we come here, the string must have at least 2 quotes ""
  around the query string, which could be empty. Also the query
  string may contain 0x00 in it, we don't treat it as null-terminated. */
  ut_ad(len >= 2);
  ut_ad(ptr->str[0] == '\"' && ptr->str[len - 1] == '\"');

  if (len == 2) {
    /* If the query string contains nothing except quotes,
    it's obviously an invalid query. */
    return (NULL);
  }

  node = fts_ast_node_create();

  /*!< We ignore the actual quotes "" */
  len -= 2;

  node->type = FTS_AST_TEXT;
  /*!< Skip copying the first quote */
  node->text.ptr =
      fts_ast_string_create(reinterpret_cast<const byte *>(ptr->str + 1), len);
  node->text.distance = ULINT_UNDEFINED;

  fts_ast_state_add_node((fts_ast_state_t *)arg, node);

  return (node);
}

/** Create an AST phrase list node for plugin parser
 @return node */
fts_ast_node_t *fts_ast_create_node_phrase_list(void *arg) /*!< in: ast state */
{
  fts_ast_node_t *node = fts_ast_node_create();

  node->type = FTS_AST_PARSER_PHRASE_LIST;

  node->text.distance = ULINT_UNDEFINED;
  node->list.head = node->list.tail = NULL;

  fts_ast_state_add_node(static_cast<fts_ast_state_t *>(arg), node);

  return (node);
}

/** This function takes ownership of the expr and is responsible
 for free'ing it.
 @return new node */
fts_ast_node_t *fts_ast_create_node_list(
    void *arg,            /*!< in: ast state instance */
    fts_ast_node_t *expr) /*!< in: ast expr instance */
{
  fts_ast_node_t *node = fts_ast_node_create();

  node->type = FTS_AST_LIST;
  node->list.head = node->list.tail = expr;

  fts_ast_state_add_node((fts_ast_state_t *)arg, node);

  return (node);
}

/** Create a sub-expression list node. This function takes ownership of
 expr and is responsible for deleting it.
 @return new node */
fts_ast_node_t *fts_ast_create_node_subexp_list(
    void *arg,            /*!< in: ast state instance */
    fts_ast_node_t *expr) /*!< in: ast expr instance */
{
  fts_ast_node_t *node = fts_ast_node_create();

  node->type = FTS_AST_SUBEXP_LIST;
  node->list.head = node->list.tail = expr;

  fts_ast_state_add_node((fts_ast_state_t *)arg, node);

  return (node);
}

/** Free an expr list node elements. */
static void fts_ast_free_list(fts_ast_node_t *node) /*!< in: ast node to free */
{
  ut_a(node->type == FTS_AST_LIST || node->type == FTS_AST_SUBEXP_LIST ||
       node->type == FTS_AST_PARSER_PHRASE_LIST);

  for (node = node->list.head; node != NULL; node = fts_ast_free_node(node)) {
    /*!< No op */
  }
}

/** Free a fts_ast_node_t instance.
 @return next node to free */
fts_ast_node_t *fts_ast_free_node(
    fts_ast_node_t *node) /*!< in: the node to free */
{
  fts_ast_node_t *next_node;

  switch (node->type) {
    case FTS_AST_TEXT:
      if (node->text.ptr) {
        fts_ast_string_free(node->text.ptr);
        node->text.ptr = NULL;
      }
      break;

    case FTS_AST_TERM:
      if (node->term.ptr) {
        fts_ast_string_free(node->term.ptr);
        node->term.ptr = NULL;
      }
      break;

    case FTS_AST_LIST:
    case FTS_AST_SUBEXP_LIST:
    case FTS_AST_PARSER_PHRASE_LIST:
      fts_ast_free_list(node);
      node->list.head = node->list.tail = NULL;
      break;

    case FTS_AST_OPER:
      break;

    default:
      ut_error;
  }

  /*!< Get next node before freeing the node itself */
  next_node = node->next;

  ut_free(node);

  return (next_node);
}

/** This AST takes ownership of the expr and is responsible
 for free'ing it.
 @return in param "list" */
fts_ast_node_t *fts_ast_add_node(
    fts_ast_node_t *node, /*!< in: list instance */
    fts_ast_node_t *elem) /*!< in: node to add to list */
{
  if (!elem) {
    return (NULL);
  }

  ut_a(!elem->next);
  ut_a(node->type == FTS_AST_LIST || node->type == FTS_AST_SUBEXP_LIST ||
       node->type == FTS_AST_PARSER_PHRASE_LIST);

  if (!node->list.head) {
    ut_a(!node->list.tail);

    node->list.head = node->list.tail = elem;
  } else {
    ut_a(node->list.tail);

    node->list.tail->next = elem;
    node->list.tail = elem;
  }

  return (node);
}

/** Set the wildcard attribute of a term. */
void fts_ast_term_set_wildcard(
    fts_ast_node_t *node) /*!< in/out: set attribute of
                          a term node */
{
  if (!node) {
    return;
  }

  /* If it's a node list, the wildcard should be set to the tail node*/
  if (node->type == FTS_AST_LIST) {
    ut_ad(node->list.tail != NULL);
    node = node->list.tail;
  }

  ut_a(node->type == FTS_AST_TERM);
  ut_a(!node->term.wildcard);

  node->term.wildcard = TRUE;
}

/** Set the proximity attribute of a text node. */
void fts_ast_text_set_distance(fts_ast_node_t *node, /*!< in/out: text node */
                               ulint distance)       /*!< in: the text proximity
                                                     distance */
{
  if (node == NULL) {
    return;
  }

  ut_a(node->type == FTS_AST_TEXT);
  ut_a(node->text.distance == ULINT_UNDEFINED);

  node->text.distance = distance;
}

/** Free node and expr allocations. */
void fts_ast_state_free(fts_ast_state_t *state) /*!< in: ast state to free */
{
  fts_ast_node_t *node = state->list.head;

  /* Free the nodes that were allocated during parsing. */
  while (node) {
    fts_ast_node_t *next = node->next_alloc;

    if (node->type == FTS_AST_TEXT && node->text.ptr) {
      fts_ast_string_free(node->text.ptr);
      node->text.ptr = NULL;
    } else if (node->type == FTS_AST_TERM && node->term.ptr) {
      fts_ast_string_free(node->term.ptr);
      node->term.ptr = NULL;
    }

    ut_free(node);
    node = next;
  }

  state->root = state->list.head = state->list.tail = NULL;
}

/** Print the ast string
@param[in]	ast_str	string to print */
static void fts_ast_string_print(const fts_ast_string_t *ast_str) {
  for (ulint i = 0; i < ast_str->len; ++i) {
    printf("%c", ast_str->str[i]);
  }

  printf("\n");
}

/** Print an ast node recursively. */
static void fts_ast_node_print_recursive(
    fts_ast_node_t *node, /*!< in: ast node to print */
    ulint level)          /*!< in: recursive level */
{
  /* Print alignment blank */
  for (ulint i = 0; i < level; i++) {
    printf("  ");
  }

  switch (node->type) {
    case FTS_AST_TEXT:
      printf("TEXT: ");
      fts_ast_string_print(node->text.ptr);
      break;

    case FTS_AST_TERM:
      printf("TERM: ");
      fts_ast_string_print(node->term.ptr);
      break;

    case FTS_AST_LIST:
      printf("LIST: \n");

      for (node = node->list.head; node; node = node->next) {
        fts_ast_node_print_recursive(node, level + 1);
      }
      break;

    case FTS_AST_SUBEXP_LIST:
      printf("SUBEXP_LIST: \n");

      for (node = node->list.head; node; node = node->next) {
        fts_ast_node_print_recursive(node, level + 1);
      }
      break;

    case FTS_AST_OPER:
      printf("OPER: %d\n", node->oper);
      break;

    case FTS_AST_PARSER_PHRASE_LIST:
      printf("PARSER_PHRASE_LIST: \n");

      for (node = node->list.head; node; node = node->next) {
        fts_ast_node_print_recursive(node, level + 1);
      }
      break;

    default:
      ut_error;
  }
}

/** Print an ast node */
void fts_ast_node_print(fts_ast_node_t *node) /*!< in: ast node to print */
{
  fts_ast_node_print_recursive(node, 0);
}

/** Check only union operation involved in the node
@param[in]	node	ast node to check
@return true if the node contains only union else false. */
bool fts_ast_node_check_union(fts_ast_node_t *node) {
  if (node->type == FTS_AST_LIST || node->type == FTS_AST_SUBEXP_LIST ||
      node->type == FTS_AST_PARSER_PHRASE_LIST) {
    for (node = node->list.head; node; node = node->next) {
      if (!fts_ast_node_check_union(node)) {
        return (false);
      }
    }

  } else if (node->type == FTS_AST_OPER &&
             (node->oper == FTS_IGNORE || node->oper == FTS_EXIST)) {
    return (false);
  } else if (node->type == FTS_AST_TEXT) {
    /* Distance or phrase search query. */
    return (false);
  }

  return (true);
}

/** Traverse the AST - in-order traversal, except for the FTX_EXIST and
 FTS_IGNORE nodes, which will be ignored in the first pass of each level, and
 visited in a second and third pass after all other nodes in the same level are
 visited.
 @return DB_SUCCESS if all went well */
dberr_t fts_ast_visit(fts_ast_oper_t oper,      /*!< in: current operator */
                      fts_ast_node_t *node,     /*!< in: current root node */
                      fts_ast_callback visitor, /*!< in: callback function */
                      void *arg,                /*!< in: arg for callback */
                      bool *has_ignore)         /*!< out: true, if the operator
                                                was ignored during processing,
                                                currently we ignore FTS_EXIST
                                                and FTS_IGNORE operators */
{
  dberr_t error = DB_SUCCESS;
  fts_ast_node_t *oper_node = NULL;
  fts_ast_node_t *start_node;
  bool revisit = false;
  bool will_be_ignored = false;
  fts_ast_visit_pass_t visit_pass = FTS_PASS_FIRST;
  trx_t *trx = node->trx;
  start_node = node->list.head;

  ut_a(node->type == FTS_AST_LIST || node->type == FTS_AST_SUBEXP_LIST);

  if (oper == FTS_EXIST_SKIP) {
    visit_pass = FTS_PASS_EXIST;
  } else if (oper == FTS_IGNORE_SKIP) {
    visit_pass = FTS_PASS_IGNORE;
  }

  /* In the first pass of the tree, at the leaf level of the
  tree, FTS_EXIST and FTS_IGNORE operation will be ignored.
  It will be repeated at the level above the leaf level.

  The basic idea here is that when we encounter FTS_EXIST or
  FTS_IGNORE, we will change the operator node into FTS_EXIST_SKIP
  or FTS_IGNORE_SKIP, and term node & text node with the operators
  is ignored in the first pass. We have two passes during the revisit:
  We process nodes with FTS_EXIST_SKIP in the exist pass, and then
  process nodes with FTS_IGNORE_SKIP in the ignore pass.

  The order should be restrictly followed, or we will get wrong results.
  For example, we have a query 'a +b -c d +e -f'.
  first pass: process 'a' and 'd' by union;
  exist pass: process '+b' and '+e' by intersection;
  ignore pass: process '-c' and '-f' by difference. */

  for (node = node->list.head; node && (error == DB_SUCCESS);
       node = node->next) {
    switch (node->type) {
      case FTS_AST_LIST:
        if (visit_pass != FTS_PASS_FIRST) {
          break;
        }

        error = fts_ast_visit(oper, node, visitor, arg, &will_be_ignored);

        /* If will_be_ignored is set to true, then
        we encountered and ignored a FTS_EXIST or FTS_IGNORE
        operator. */
        if (will_be_ignored) {
          revisit = true;
          /* Remember oper for list in case '-abc&def',
          ignored oper is from previous node of list.*/
          node->oper = oper;
        }

        break;

      case FTS_AST_OPER:
        oper = node->oper;
        oper_node = node;

        /* Change the operator for revisit */
        if (oper == FTS_EXIST) {
          oper_node->oper = FTS_EXIST_SKIP;
        } else if (oper == FTS_IGNORE) {
          oper_node->oper = FTS_IGNORE_SKIP;
        }

        break;

      default:
        if (node->visited) {
          continue;
        }

        ut_a(oper == FTS_NONE || !oper_node || oper_node->oper == oper ||
             oper_node->oper == FTS_EXIST_SKIP ||
             oper_node->oper == FTS_IGNORE_SKIP);

        if (oper == FTS_EXIST || oper == FTS_IGNORE) {
          *has_ignore = true;
          continue;
        }

        /* Process leaf node accroding to its pass.*/
        if (oper == FTS_EXIST_SKIP && visit_pass == FTS_PASS_EXIST) {
          error = visitor(FTS_EXIST, node, arg);
          node->visited = true;
        } else if (oper == FTS_IGNORE_SKIP && visit_pass == FTS_PASS_IGNORE) {
          error = visitor(FTS_IGNORE, node, arg);
          node->visited = true;
        } else if (visit_pass == FTS_PASS_FIRST) {
          error = visitor(oper, node, arg);
          node->visited = true;
        }
    }
  }
  if (trx_is_interrupted(trx)) {
    return (DB_INTERRUPTED);
  }

  if (revisit) {
    /* Exist pass processes the skipped FTS_EXIST operation. */
    for (node = start_node; node && error == DB_SUCCESS; node = node->next) {
      if (node->type == FTS_AST_LIST && node->oper != FTS_IGNORE) {
        error =
            fts_ast_visit(FTS_EXIST_SKIP, node, visitor, arg, &will_be_ignored);
      }
    }

    /* Ignore pass processes the skipped FTS_IGNORE operation. */
    for (node = start_node; node && error == DB_SUCCESS; node = node->next) {
      if (node->type == FTS_AST_LIST) {
        error = fts_ast_visit(FTS_IGNORE_SKIP, node, visitor, arg,
                              &will_be_ignored);
      }
    }
  }

  return (error);
}

/**
Create an ast string object, with NUL-terminator, so the string
has one more byte than len
@param[in] str		pointer to string
@param[in] len		length of the string
@return ast string with NUL-terminator */
fts_ast_string_t *fts_ast_string_create(const byte *str, ulint len) {
  fts_ast_string_t *ast_str;

  ut_ad(len > 0);

  ast_str = static_cast<fts_ast_string_t *>(
      ut_malloc_nokey(sizeof(fts_ast_string_t)));

  ast_str->str = static_cast<byte *>(ut_malloc_nokey(len + 1));

  ast_str->len = len;
  memcpy(ast_str->str, str, len);
  ast_str->str[len] = '\0';

  return (ast_str);
}

/**
Free an ast string instance
@param[in,out] ast_str		string to free */
void fts_ast_string_free(fts_ast_string_t *ast_str) {
  if (ast_str != NULL) {
    ut_free(ast_str->str);
    ut_free(ast_str);
  }
}

/**
Translate ast string of type FTS_AST_NUMB to unsigned long by strtoul
@param[in]	ast_str	string to translate
@param[in]	base	the base
@return translated number */
ulint fts_ast_string_to_ul(const fts_ast_string_t *ast_str, int base) {
  return (strtoul(reinterpret_cast<const char *>(ast_str->str), NULL, base));
}

#ifdef UNIV_DEBUG
const char *fts_ast_node_type_get(fts_ast_type_t type) {
  switch (type) {
    case FTS_AST_OPER:
      return ("FTS_AST_OPER");
    case FTS_AST_NUMB:
      return ("FTS_AST_NUMB");
    case FTS_AST_TERM:
      return ("FTS_AST_TERM");
    case FTS_AST_TEXT:
      return ("FTS_AST_TEXT");
    case FTS_AST_LIST:
      return ("FTS_AST_LIST");
    case FTS_AST_SUBEXP_LIST:
      return ("FTS_AST_SUBEXP_LIST");
    case FTS_AST_PARSER_PHRASE_LIST:
      return ("FTS_AST_PARSER_PHRASE_LIST");
  }
  ut_ad(0);
  return ("FTS_UNKNOWN");
}
#endif /* UNIV_DEBUG */
