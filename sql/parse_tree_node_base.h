/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PARSE_TREE_NODE_INCLUDED
#define PARSE_TREE_NODE_INCLUDED

#include "my_config.h"
#include <cstdlib>
#include <cstring>
#include "my_sys.h"
#include "sql_const.h"

#include "parse_location.h"

class THD;
class st_select_lex;

// uncachable cause
#define UNCACHEABLE_DEPENDENT   1
#define UNCACHEABLE_RAND        2
#define UNCACHEABLE_SIDEEFFECT	4
/* For uncorrelated SELECT in an UNION with some correlated SELECTs */
#define UNCACHEABLE_UNITED      8
#define UNCACHEABLE_CHECKOPTION 16

/**
  Names for different query parse tree parts
*/

enum enum_parsing_context
{
  CTX_NONE= 0, ///< Empty value
  CTX_MESSAGE, ///< "No tables used" messages etc.
  CTX_TABLE, ///< for single-table UPDATE/DELETE/INSERT/REPLACE
  CTX_SELECT_LIST, ///< SELECT (subquery), (subquery)...
  CTX_UPDATE_VALUE_LIST, ///< UPDATE ... SET field=(subquery)...
  CTX_JOIN,
  CTX_QEP_TAB,
  CTX_MATERIALIZATION,
  CTX_DUPLICATES_WEEDOUT,
  CTX_DERIVED, ///< "Derived" subquery
  CTX_WHERE, ///< Subquery in WHERE clause item tree
  CTX_ON,    ///< ON clause context
  CTX_HAVING, ///< Subquery in HAVING clause item tree
  CTX_ORDER_BY, ///< ORDER BY clause execution context
  CTX_GROUP_BY, ///< GROUP BY clause execution context
  CTX_SIMPLE_ORDER_BY, ///< ORDER BY clause execution context
  CTX_SIMPLE_GROUP_BY, ///< GROUP BY clause execution context
  CTX_DISTINCT, ///< DISTINCT clause execution context
  CTX_SIMPLE_DISTINCT, ///< DISTINCT clause execution context
  CTX_BUFFER_RESULT, ///< see SQL_BUFFER_RESULT in the manual
  CTX_ORDER_BY_SQ, ///< Subquery in ORDER BY clause item tree
  CTX_GROUP_BY_SQ, ///< Subquery in GROUP BY clause item tree
  CTX_OPTIMIZED_AWAY_SUBQUERY, ///< Subquery executed once during optimization
  CTX_UNION,
  CTX_UNION_RESULT, ///< Pseudo-table context for UNION result
  CTX_QUERY_SPEC ///< Inner SELECTs of UNION expression
};

/*
  Note: YYLTYPE doesn't overload a default constructor (as well an underlying
  Symbol_location).
  OTOH if we need a zero-initialized POS, YYLTYPE or Symbol_location object,
  we can simply call POS(), YYLTYPE() or Symbol_location(): C++ does
  value-initialization in that case.
*/
typedef YYLTYPE POS;

/**
  Environment data for the contextualization phase
*/
struct Parse_context {
  THD * const thd;              ///< Current thread handler
  MEM_ROOT *mem_root;           ///< Current MEM_ROOT
  st_select_lex * select;       ///< Current SELECT_LEX object

  Parse_context(THD *thd, st_select_lex *select);
};


// defined in sql_parse.cc:
bool check_stack_overrun(THD *thd, long margin, uchar *dummy);


/**
  Base class for parse tree nodes
*/
class Parse_tree_node
{
  friend class Item; // for direct access to the "contextualized" field

  Parse_tree_node(const Parse_tree_node &); // undefined
  void operator=(const Parse_tree_node &); // undefined

#ifndef DBUG_OFF
private:
  bool contextualized; // true if the node object is contextualized
  bool transitional; // TODO: remove that after parser refactoring
#endif//DBUG_OFF

public:
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}

protected:
  Parse_tree_node()
  {
#ifndef DBUG_OFF
    contextualized= false;
    transitional= false;
#endif//DBUG_OFF
  }

public:
  virtual ~Parse_tree_node() {}

#ifndef DBUG_OFF
  bool is_contextualized() const { return contextualized; }
#endif//DBUG_OFF

  /**
    Do all context-sensitive things and mark the node as contextualized

    @param      pc      current parse context
    
    @retval     false   success
    @retval     true    syntax/OOM/etc error
  */
  virtual bool contextualize(Parse_context *pc)
  {
#ifndef DBUG_OFF
    if (transitional)
    {
      DBUG_ASSERT(contextualized);
      return false;
    }
#endif//DBUG_OFF

    uchar dummy;
    if (check_stack_overrun(pc->thd, STACK_MIN_SIZE, &dummy))
      return true;

#ifndef DBUG_OFF
    DBUG_ASSERT(!contextualized);
    contextualized= true;
#endif//DBUG_OFF

    return false;
  }

  /**
   Intermediate version of the contextualize() function

   This function is intended to resolve parser grammar loops.

    During the step-by-step refactoring of the parser grammar we wrap
    each context-sensitive semantic action with 3 calls:
    1. Parse_tree_node() context-independent constructor call,
    2. contextualize_() function call to evaluate all context-sensitive things
       from the former context-sensitive semantic action code.
    3. Call of dummy contextualize() function.

    Then we lift the contextualize() function call to outer grammar rules but
    save the contextualize_() function call untouched.

    When all loops in the grammar rules are resolved (i.e. transformed
    as described above) we:
    a. remove all contextualize_() function calls and
    b. rename all contextualize_() function definitions to contextualize()
       function definitions.

    Note: it's not necessary to transform the whole grammar and remove
    this function calls in one pass: it's possible to transform the
    grammar statement by statement in a way described above.

    Note: remove this function together with Item::contextualize_().
  */
  virtual bool contextualize_(Parse_context *pc)
  {
#ifndef DBUG_OFF
    DBUG_ASSERT(!contextualized && !transitional);
    transitional= true;
    contextualized= true;
#endif//DBUG_OFF
    return false;
  }

  void error(Parse_context *pc,
             const POS &position,
             const char * msg= NULL) const;
};

#endif /* PARSE_TREE_NODE_INCLUDED */
