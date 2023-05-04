/* Copyright (c) 2013, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PARSE_TREE_NODE_BASE_INCLUDED
#define PARSE_TREE_NODE_BASE_INCLUDED

#include <assert.h>
#include <cstdarg>
#include <cstdlib>
#include <new>
#include <queue>

#include "memory_debugging.h"
#include "my_alloc.h"
#include "my_compiler.h"

#include "mem_root_deque.h"
#include "my_inttypes.h"  // TODO: replace with cstdint
#include "sql/check_stack.h"
#include "sql/parse_location.h"
#include "sql/sql_const.h"

class Query_block;
class THD;

// uncachable cause
#define UNCACHEABLE_DEPENDENT 1
#define UNCACHEABLE_RAND 2
#define UNCACHEABLE_SIDEEFFECT 4
/* For uncorrelated SELECT in an UNION with some correlated SELECTs */
#define UNCACHEABLE_UNITED 8
#define UNCACHEABLE_CHECKOPTION 16

/**
  Names for different query parse tree parts
*/

enum enum_parsing_context {
  CTX_NONE = 0,       ///< Empty value
  CTX_MESSAGE,        ///< "No tables used" messages etc.
  CTX_TABLE,          ///< for single-table UPDATE/DELETE/INSERT/REPLACE
  CTX_SELECT_LIST,    ///< SELECT (subquery), (subquery)...
  CTX_UPDATE_VALUE,   ///< UPDATE ... SET field=(subquery)...
  CTX_INSERT_VALUES,  ///< INSERT ... VALUES
  CTX_INSERT_UPDATE,  ///< INSERT ... ON DUPLICATE KEY UPDATE ...
  CTX_JOIN,
  CTX_QEP_TAB,
  CTX_MATERIALIZATION,
  CTX_DUPLICATES_WEEDOUT,
  CTX_DERIVED,                  ///< "Derived" subquery
  CTX_WHERE,                    ///< Subquery in WHERE clause item tree
  CTX_ON,                       ///< ON clause context
  CTX_WINDOW,                   ///< Named or unnamed window
  CTX_HAVING,                   ///< Subquery in HAVING clause item tree
  CTX_ORDER_BY,                 ///< ORDER BY clause execution context
  CTX_GROUP_BY,                 ///< GROUP BY clause execution context
  CTX_SIMPLE_ORDER_BY,          ///< ORDER BY clause execution context
  CTX_SIMPLE_GROUP_BY,          ///< GROUP BY clause execution context
  CTX_DISTINCT,                 ///< DISTINCT clause execution context
  CTX_SIMPLE_DISTINCT,          ///< DISTINCT clause execution context
  CTX_BUFFER_RESULT,            ///< see SQL_BUFFER_RESULT in the manual
  CTX_ORDER_BY_SQ,              ///< Subquery in ORDER BY clause item tree
  CTX_GROUP_BY_SQ,              ///< Subquery in GROUP BY clause item tree
  CTX_OPTIMIZED_AWAY_SUBQUERY,  ///< Subquery executed once during optimization
  CTX_UNION,
  CTX_UNION_RESULT,  ///< Pseudo-table context for UNION result
  CTX_INTERSECT,
  CTX_INTERSECT_RESULT,  ///< Pseudo-table context
  CTX_EXCEPT,
  CTX_EXCEPT_RESULT,  ///< Pseudo-table context
  CTX_UNARY,
  CTX_UNARY_RESULT,  ///< Pseudo-table context
  CTX_QUERY_SPEC     ///< Inner SELECTs of UNION expression
};

class Query_term;
enum Surrounding_context {
  SC_TOP,
  SC_QUERY_SPECIFICATION,
  SC_TABLE_VALUE_CONSTRUCTOR,
  SC_QUERY_EXPRESSION,
  SC_SUBQUERY,
  SC_UNION_DISTINCT,
  SC_UNION_ALL,
  SC_INTERSECT_DISTINCT,
  SC_INTERSECT_ALL,
  SC_EXCEPT_DISTINCT,
  SC_EXCEPT_ALL
};

struct QueryLevel {
  Surrounding_context m_type;
  mem_root_deque<Query_term *> m_elts;
  bool m_has_order{false};
  QueryLevel(MEM_ROOT *mem_root, Surrounding_context sc, bool has_order = false)
      : m_type(sc), m_elts(mem_root), m_has_order(has_order) {}
};
/**
  Environment data for the contextualization phase
*/
struct Parse_context {
  THD *const thd;                      ///< Current thread handler
  MEM_ROOT *mem_root;                  ///< Current MEM_ROOT
  Query_block *select;                 ///< Current Query_block object
  mem_root_deque<QueryLevel> m_stack;  ///< Aids query term tree construction
  /// Call upon parse completion.
  /// @returns true on error, else false
  bool finalize_query_expression();
  Parse_context(THD *thd, Query_block *sl);
  bool is_top_level_union_all(
      Surrounding_context op);  ///< Determine if there is anything but
                                ///< UNION ALL above in m_stack
};

/**
  Base class for parse tree nodes (excluding the Parse_tree_root hierarchy)
*/
template <typename Context>
class Parse_tree_node_tmpl {
  friend class Item;  // for direct access to the "contextualized" field

  Parse_tree_node_tmpl(const Parse_tree_node_tmpl &);  // undefined
  void operator=(const Parse_tree_node_tmpl &);        // undefined

#ifndef NDEBUG
 private:
  bool contextualized = false;  // true if the node object is contextualized
#endif                          // NDEBUG

 public:
  typedef Context context_t;
  POS m_pos;

  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t &arg
                            [[maybe_unused]] = std::nothrow) noexcept {
    return mem_root->Alloc(size);
  }
  static void operator delete(void *ptr [[maybe_unused]],
                              size_t size [[maybe_unused]]) {
    TRASH(ptr, size);
  }
  static void operator delete(void *, MEM_ROOT *,
                              const std::nothrow_t &) noexcept {}

 protected:
  Parse_tree_node_tmpl() = delete;

  explicit Parse_tree_node_tmpl(const POS &pos) : m_pos(pos) {}

  explicit Parse_tree_node_tmpl(const POS &start_pos, const POS &end_pos) {
    // Range of this item list should be from first character position of
    // start_pos to the last character position of end_pos.
    m_pos.cpp.start = start_pos.cpp.start;
    m_pos.cpp.end = end_pos.cpp.end;
    m_pos.raw.start = start_pos.raw.start;
    m_pos.raw.end = end_pos.raw.end;
  }

  /**
    Do all context-sensitive things and mark the node as contextualized

    @param      pc      current parse context

    @retval     false   success
    @retval     true    syntax/OOM/etc error
  */
  virtual bool do_contextualize(Context *pc) {
    uchar dummy;
    if (check_stack_overrun(pc->thd, STACK_MIN_SIZE, &dummy)) return true;

#ifndef NDEBUG
    assert(!contextualized);
    contextualized = true;
#endif  // NDEBUG

    return false;
  }

 public:
  virtual ~Parse_tree_node_tmpl() = default;

#ifndef NDEBUG
  bool is_contextualized() const { return contextualized; }
#endif  // NDEBUG

  // Derived classes should not override this. If needed, they should override
  // do_contextualize().
  // Visual Studio with MSVC_CPPCHECK=ON gives warning C26435:
  // Function <fun> should specify exactly one of
  //    'virtual', 'override', or 'final'
  MY_COMPILER_DIAGNOSTIC_PUSH()
  MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(26435)
  virtual bool contextualize(Context *pc) final {
    // Json parse tree related things to be done pre-do_contextualize().
    if (do_contextualize(pc)) return true;
    // Json parse tree related things to be done post-do_contextualize().
    return false;
  }
  MY_COMPILER_DIAGNOSTIC_POP()

  /**
    syntax_error() function replacement for deferred reporting of syntax
    errors

    @param      pc      Current parse context.
    @param      pos     Location of the error in lexical scanner buffers.
  */
  void error(Context *pc, const POS &pos) const {
    pc->thd->syntax_error_at(pos);
  }

  /**
    syntax_error() function replacement for deferred reporting of syntax
    errors

    @param      pc      Current parse context.
    @param      pos     Location of the error in lexical scanner buffers.
    @param      msg     Error message.
  */
  void error(Context *pc, const POS &pos, const char *msg) const {
    pc->thd->syntax_error_at(pos, "%s", msg);
  }

  /**
    syntax_error() function replacement for deferred reporting of syntax
    errors

    @param      pc      Current parse context.
    @param      pos     Location of the error in lexical scanner buffers.
    @param      format  Error message format string with optional argument list.
  */
  void errorf(Context *pc, const POS &pos, const char *format, ...) const
      MY_ATTRIBUTE((format(printf, 4, 5)));
};

template <typename Context>
inline void Parse_tree_node_tmpl<Context>::errorf(Context *pc, const POS &pos,
                                                  const char *format,
                                                  ...) const {
  va_list args;
  va_start(args, format);
  pc->thd->vsyntax_error_at(pos, format, args);
  va_end(args);
}

typedef Parse_tree_node_tmpl<Parse_context> Parse_tree_node;

#endif /* PARSE_TREE_NODE_BASE_INCLUDED */
