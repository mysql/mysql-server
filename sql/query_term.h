/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */
#ifndef QUERY_NODE_INCLUDED
#define QUERY_NODE_INCLUDED

#include "sql/join_optimizer/materialize_path_parameters.h"  // MaterializePathParameters
#include "sql/sql_list.h"
#include "sql/sql_union.h"
#include "sql/table.h"

class Query_block;

/**
  This class hierarchy is used to represent SQL structures between \<query
  expression\> and \<query specification\>. The class Query_expression
  represents \<query expression\> and the class Query_block represents \<query
  specification\>, \<table value constructor\> and \<explicit table\>,
  cf. definitions in sql_lex.h. Originally, MySQL supported only one set
  operation, UNION. This was implicitly represented by having one
  Query_expression object own several Query_block objects via next pointers.
  This made things simple, but limited us to left-deep nesting of unions, and
  also from representing INTERSECTION and EXCEPT, as well as nesting several
  layers of \<query primary\>s containing \<query expression body\>s, e.g.

     (((SELECT a,b FROM t) ORDER BY a LIMIT 5) ORDER BY -b LIMIT 3) ORDER BY a;

  could not be supported. With the present class hierarchy we enable the full
  set of set operations and arbitrary nesting, as allowed by the SQL grammar,
  viz:
  \verbatim
  <query expression> ::=
      [ <with clause> ] <query expression body>
      [ <order by clause> ] [ <limit/offset> ]

  <query expression body> ::=
     <query term>
   | <query expression body> UNION [ ALL | DISTINCT ]
     [ <corresponding spec> ] <query term>
   | <query expression body> EXCEPT [ ALL | DISTINCT ]
     [ <corresponding spec> ] <query term>

  <query term> ::=
     <query primary>
   | <query expression body> INTERSECT [ ALL | DISTINCT ]
     [ <corresponding spec> ] <query primary>

  <query primary> ::=
     <simple table>
   | <left paren> <query expression body>
     [ <order by clause> ] [ <limit/offset (*)> ]
     <right paren>

  <simple table> ::=
     <query specification>
   | <table value constructor>
   | <explicit table>

  (*) MySQL syntax and semantics. The standard uses /<result offset clause/> and
      \<fetch first clause\>.

  \endverbatim
  Note that INTERSECT binds tighter than UNION and EXCEPT.
  Now, let's turn to how these structures are represented in MySQL.
  The node types are enumerated by Query_term_type. The nodes themselves
  by the class hierarchy rooted in Query_term (abstract).
*/

/// Node type of the query term tree nodes
enum Query_term_type {
  /// Represents Query specification, table value constructor and
  /// explicit table
  QT_QUERY_BLOCK,
  /// Represents a query primary with parentesized query expression body with
  /// order by clause and/or limit/offset clause. If none of order by
  /// or limit is present, we collapse this level of parentheses.
  QT_UNARY,
  /// Represents the three set operations. Nodes are N-ary, i.e. a node can hold
  /// two or more operands.
  QT_INTERSECT,
  QT_EXCEPT,
  QT_UNION
};

/// Query term iterator template argument type: how to visit nodes in tree
enum Visit_order { QTC_POST_ORDER, QTC_PRE_ORDER };
/// Query term iterator template argument type: whether to visit leaf nodes
enum Visit_leaves { VL_VISIT_LEAVES, VL_SKIP_LEAVES };

/**
  Query term tree structure. There are five node types, cf. Query_term_type.
  Leaf nodes are Query_block objects. We have three kinds of n-ary set operation
  nodes corresponding to INTERSECT, UNION and EXCEPT. Finally, we have a "unary"
  node which essentially adds a ORDER BY/LIMIT over another node.

  Query blocks serve a dual purpose: they represent the query specification and
  table constructors of the query. As such they are the leaf nodes of the query
  tree.  But they also serve as a way to realize ORDER BY and LIMIT for non-leaf
  nodes, accessed via the function Query_term::query_block().  Therefore, every
  non-leaf node in the tree has a companion Query_block to hold ORDER BY and
  LIMIT information.  For the leaf nodes, which are themselves query blocks, the
  query_block() function just returns a pointer to self, i.e. the leaf nodes
  handle ORDER BY and LIMIT themselves.

  \verbatim
  Example: ((SELECT * FROM t1 UNION SELECT * FROM t2 UNION ALL SELECT * FROM t3
             ORDER BY a LIMIT 5) INTERSECT
            (((SELECT * FROM t3 ORDER BY a LIMIT 4) ) EXCEPT SELECT * FROM t4)
            ORDER BY a LIMIT 4) ORDER BY -a LIMIT 3;

  ->
              m_query_term   +------------------+     slave(s)
              +--------------|-Query_expression |------------------+
              |              +------------------+                  |
              V        post_                                       |
  +-------------------+processing_ +----------------------+        |
  | Query_term_unary  |block()     |Query_block           |        |
  |                   |----------->|order by -(`a) limit 3|        |
  +-------------------+            +----------------------+        |
   |m_children                                                     |
   | +-----------------------+   +----------------------+          |
   | |Query_term_intersect   |   |Query_block           |          |
   +>|last distinct index: 1 |-->|order by `a` limit 4  |          |
     +-----------------------+   +----------------------+          |
      |m_children                                                  |
      |  +-----------------------+   +----------------------+      |
      |  |Query_term_union       |   |Query_block           |      |
      +->|last distinct index: 1 |-->|order by `a`  limit 5 |      |
      |  +-----------------------+   +----------------------+      |
      |    |m_children                                             |
      |    |   +------------+        SELECT * FROM t1             /
      |    +-->|Query_block |  <---------------------------------+
      |    |   +------------+  ----------------------------------+ next
      |    |                                                      \
      |    |   +------------+        SELECT * FROM t2             /
      |    +-->|Query_block |  <---------------------------------+
      |    |   +------------+  ----------------------------------+ next
      |    |                                                      \
      |    |   +------------+        SELECT * FROM t3             /
      |    +-->|Query_block |  <---------------------------------+
      |        +------------+  ----------------------------------+ next
      |                                                           \
      |  +-----------------------+  +------------+                 |
      |  |Query_term_except      |->|Query_block |                 |
      +->|last distinct index: 1 |  +------------+                 |
         +-----------------------+                                 |
           |m_children                                             |
           |   +----------------------+                            |
           |   |Query_block           |      SELECT * FROM t3      /
           +-->|order by `a`  limit 4 |  <------------------------+
           |   +----------------------+  -------------------------+ next
           |                                                       \
           |   +------------+                SELECT * FROM t4      |
           +-->|Query_block | <------------------------------------+
               +------------+
  \endverbatim
  Note that all leaf query blocks representing the query specifications are
  linked under Query_expression via their next pointers. The nesting is achieved
  by the arrows on the left side of the figure, via the nodes' m_children
  members. The four classes Query_term_unary and Query_term_{union, intersect,
  except} are modelled via the base class Query_term_set_op which contains a
  m_children member. Each of these also contain a Query_block which will handle
  its order by and/or limit clauses. These are similar to the old so-called
  "fake_query_block" (which is now gone), and are not linked in with "next"
  pointers.

  The is also a back pointer from the children nodes to the parent Query_term
  object (not shown).

  In the simple case of a single query specification (or table value constructor
  or explicit table), there is no super-structure over the Query_block linked
  from the Query_expression, i.e. Query_expression's m_query_term member is just
  a Query_block.

  The query blocks (QT_QUERY_BLOCK nodes) corresponding to the query
  specification (or table value constructors) are prepared and optimized by
  running over them from the Query_expression via the slave/next pointers as
  before. There are separate methods which handle prepare and optimization for
  non-leaves, i.e. nodes of types QT_UNARY, QT_INTERSECT, QT_EXCEPT and
  QT_UNION.

  We also define an iterator class (Query_terms) for iterating over all
  the nodes in the tree, see also Query_expression::query_terms() for its use.
  When possible, we access all nodes using iterators.

  The built structure can be traced with the debug trace keyword "ast", e.g.
  as SET SESSION debug = 'd,ast:O,/tmp/mysqld.trace';
*/

class Query_term {
 public:
  // The next two methods used during construction of the tree.
  /**
    Called after contextualization to simplify query, c.f. sql_yacc.yy
    CONTEXTUALIZE_VIEW. It also sets up the parent pointers.
    @param parent parent of this if any
  */
  Query_term *pushdown_limit_order_by(Query_term_set_op *parent = nullptr);

  /**
    Return true if structure is too deep, i.e. more than MAX_SELECT_NESTING.
    As a side effect is also gives the post processing block a select number,
    this is done late so as to get numbers higher the leaf blocks, and in
    inside out order, so that the top set operation block will have the highest
    number.
    @param parent the real parent in the tree as visited recursively
    @param depth the current depth in the tree, starting at 0 at the top.
                 It is increased with one for each time we recurse into a child.
    @returns error if too deep structure, else false
  */
  bool validate_structure(const Query_term *parent, int depth = 0) const;

  /**
    Determine if we have a redundant ORDER BY in block. Used during prepare.
    @param block the query block
    @param level the current nesting level
    @return tuple {bool found, bool redundant} redundant==true means ORDER BY of
    the block is redundant and can be eliminated.
  */
  std::pair<bool, bool> redundant_order_by(Query_block *block, int level);

  // Tree structure

  /**
    Get the node tree type.
    @returns the tree node type
  */
  virtual Query_term_type term_type() const = 0;
  /**
    Get the node type description.
    @returns descriptive string for each node type.
  */
  virtual const char *operator_string() const = 0;
  /**
    Node destructor
  */
  virtual ~Query_term() = default;
  /**
    Get the number of children this node has.
    @return the number
  */
  virtual size_t child_count() const { return 0; }

 protected:
  /**
    Back pointer to the node whose child we are, or nullptr (root term).
  */
  Query_term_set_op *m_parent{nullptr};

 public:
  /// Getter for m_parent, q.v.
  Query_term_set_op *parent() const { return m_parent; }

  /**
    Reset resources used.
    @param full do full cleanup. Same semantics as for Query_expression's
                cleanup
  */
  virtual void cleanup(bool full [[maybe_unused]]) {
    assert(false);  // should be overridden
  }

  /// Destroy the query term tree structure
  virtual void destroy_tree() = 0;

  /**
    Open tmp tables for the tree of set operation query results, by recursing
    @param thd  session context
    @param level level in the tree, top should be called with 0.
    @return true on error
  */
  virtual bool open_result_tables(THD *thd [[maybe_unused]],
                                  int level [[maybe_unused]]) {
    assert(false);  // should be overridden
    return false;
  }

  // Printable representation

  /**
    Print the tree rooted at this node to buf. Call on top level with level==0
    @param level level we are at in tree.
    @param buf   the buffer to format output into
  */
  virtual void debugPrint(int level, std::ostringstream &buf) const = 0;
  /**
    Print blank space indentation (unit: two) to buf according to level. Minion.
    @param level level we are at in tree.
    @param buf   the buffer to format output into
  */
  static void indent(int level, std::ostringstream &buf);
  /**
    Print the pointer of this node and its parent to buf. Minion.
    @param buf the buffer to format output into
  */
  void printPointers(std::ostringstream &buf) const;
  /**
    Print into str the order indicated in ord, using standard print_for_order
    Used by traditional explain.
    @param thd session state
    @param str string to accumulate formatted output into
    @param ord the ORDER to be printed
    @param query_type controls how printing happens
  */
  static void print_order(const THD *thd, String *str, ORDER *ord,
                          enum_query_type query_type);

  // The next set of members his contains all that is needed to implement ORDER
  // BY + LIMIT in several layers of unary/binary set operations: they have
  // taken over information earlier stored in a single instance directly in
  // Query_expression, which was then limited to single level only.

  /**
    The query_block which holds the ORDER BY and LIMIT information for this
    set operation. Note that for the case where the root is a simple
    query block, this will return self.
    @returns the query block
  */
  virtual Query_block *query_block() const = 0;

 protected:
  /**
    The query result for this term. Shared between n-ary set operands, the first
    one holds it, cf. owning_operand. Except at top level, this is always a
    Query_result_union.
  */
  Query_result *m_setop_query_result{nullptr};
  /**
    The operand of a n-ary set operation (that owns the common query result) has
    this set to true. It is always the first one.
  */
  bool m_owning_operand{false};
  /**
     Result temporary table for the set operation, if applicable
   */
  Table_ref *m_result_table{nullptr};
  /**
    Used only when streaming, i.e. not materialized result set
  */
  mem_root_deque<Item *> *m_fields{nullptr};

 public:
  /// Setter for m_setop_query_result, q.v.
  void set_setop_query_result(Query_result *rs) { m_setop_query_result = rs; }
  /// Getter for m_setop_query_result, q.v.
  Query_result *setop_query_result() { return m_setop_query_result; }
  /// Getter for m_setop_query_result, q.v. Use only if we can down cast.
  Query_result_union *setop_query_result_union() {
    return down_cast<Query_result_union *>(m_setop_query_result);
  }
  /// Cleanup m_setop_query_result, q.v.
  void cleanup_query_result(bool full);

  /// Setter for m_owning_operand, q.v.
  void set_owning_operand() { m_owning_operand = true; }
  /// Getter for m_owning_operand, q.v.
  bool owning_operand() { return m_owning_operand; }

  /// Setter for m_result_table, q.v.
  void set_result_table(Table_ref *tl) { m_result_table = tl; }
  /// Getter for m_result_table, q.v.
  Table_ref &result_table() { return *m_result_table; }

  // Setter for m_fields, q.v.
  void set_fields(mem_root_deque<Item *> *fields) { m_fields = fields; }
  // Getter for m_fields, q.v.
  mem_root_deque<Item *> *fields() { return m_fields; }

 private:
  /// class Query_terms iterator traversal state variable, hence friend
  /// declaration immediately below
  uint m_curr_id;
  template <Visit_order order, Visit_leaves visit_leaves>
  friend class Query_terms;
};

/// Common base class for n-ary set operations, including unary.
class Query_term_set_op : public Query_term {
  /// Replaces the old "fake" query block for post processing result set with
  /// ORDER BY, LIMIT.
  /// The query block(s) of the query specifications can be found via
  /// m_children.
  Query_block *m_block{nullptr};

 protected:  // this node type is abstract
  Query_term_set_op(MEM_ROOT *mem_root) : m_children(mem_root) {}

 public:
  /// Tree structure. Cardinality is one for unary, two or more for UNION,
  /// EXCEPT, INTERSECT
  mem_root_deque<Query_term *> m_children;
  /**
     Index of last query expression which has <set-op> DISTINCT on its left. In
     a list of <set-op>ed blocks, UNION is left-associative; so UNION DISTINCT
     eliminates duplicates in all blocks up to the first one on its right
     included. Which is why we only need to remember that query block. Is -1
     for Unary.
  */
  int64_t m_last_distinct;
  /**
    Presently only needed by EXCEPT set operator: the index of the first
    DISTINCT set operand: minimum legal value is 1. If not DISTINCT, it should
    have the value std::numeric_limits<int64_t>::max(). The value is set
    in PT_set_operation::merge_descendants.
  */
  int64_t m_first_distinct;
  /**
    true if the result of this set operation is materialized. A priori true
    unless we have a pure UNION ALL.
  */
  bool m_is_materialized{true};

  /// Getter for m_block, q.v.
  Query_block *query_block() const override { return m_block; }

  /// Setter for m_block, q.v.
  bool set_block(Query_block *b) {
    assert(!m_block);
    if (b == nullptr) return true;

    m_block = b;
    return false;
  }

  size_t child_count() const override { return m_children.size(); }
  bool open_result_tables(THD *thd, int level) override;
  void cleanup(bool full) override;
  void destroy_tree() override {
    m_parent = nullptr;
    for (Query_term *child : m_children) {
      child->destroy_tree();
    }
    m_children.clear();
  }
  /**
    Check if this set operation has a mix of DISTINCT and ALL.
    @return true if so.  Always false for unary
   */
  bool has_mixed_distinct_operators();
  /**
    Check if this term is a unary set operation
    @return true if so
   */
  bool is_unary() const { return term_type() == QT_UNARY; }

  // Recursively set up materialization for a query term tree. For details,
  // see implementation.
  Mem_root_array<MaterializePathParameters::Operand> setup_materialize_set_op(
      THD *thd, TABLE *dst_table, bool union_distinct_only,
      bool calc_found_rows);

 protected:
  /**
    Common printing minion for set operations.
    @param level level in tree
    @param buf   the buffer to format output into
    @param type  descriptive string of set operation to use for printing
   */
  void print(int level, std::ostringstream &buf, const char *type) const;
};

/// Node type for n-ary UNION
class Query_term_union : public Query_term_set_op {
 public:
  /**
    Constructor.
    @param mem_root      the mem_root to use for allocation
   */
  Query_term_union(MEM_ROOT *mem_root) : Query_term_set_op(mem_root) {}
  Query_term_type term_type() const override { return QT_UNION; }
  const char *operator_string() const override { return "union"; }
  void debugPrint(int level, std::ostringstream &buf) const override;
};

/// Node type for n-ary INTERSECT
class Query_term_intersect : public Query_term_set_op {
 public:
  /**
    Constructor.
    @param mem_root      the mem_root to use for allocation
  */
  Query_term_intersect(MEM_ROOT *mem_root) : Query_term_set_op(mem_root) {}
  Query_term_type term_type() const override { return QT_INTERSECT; }
  const char *operator_string() const override { return "intersect"; }
  void debugPrint(int level, std::ostringstream &buf) const override;
};

/// Node type for n-ary EXCEPT
class Query_term_except : public Query_term_set_op {
 public:
  /**
    Constructor.
    @param mem_root      the mem_root to use for allocation
  */
  Query_term_except(MEM_ROOT *mem_root) : Query_term_set_op(mem_root) {}
  Query_term_type term_type() const override { return QT_EXCEPT; }
  const char *operator_string() const override { return "except"; }
  void debugPrint(int level, std::ostringstream &buf) const override;
};

/// A \<query primary\> which is a parenthesized query expression (aka qe) body
/// with order by clause and/or limit/offset clause and the qe body
/// is not a binary set operation (union, except, intersect), but is viewed here
/// as a degenerate set operation; i.e. a "unary".
/// \verbatim
/// Example: (SELECT * FROM .. ORDER BY .. LIMIT n) ORDER BY .. LIMIT m
/// Tree:
///      Query_expression
///               | m_query_term
///      Query_term_unary
///      +--------------------+
///      |      m_block ------+---->      Query_block which holds outer
///      |        :           |           ORDER BY and LIMIT
///      |      m_children[0] |
///      +--------|-----------+
///               V
///        Query_block   holds inner SELECT and its ORDER BY/LIMIT
/// \endverbatim
/// One extra Query_term_unary is added for each level of nesting
/// with the top one representing the outermost ORDER BY/LIMIT/OFFSET
///
class Query_term_unary : public Query_term_set_op {
 public:
  /**
    Constructor.
    @param mem_root      the mem_root to use for allocation
    @param t             the child term
   */
  Query_term_unary(MEM_ROOT *mem_root, Query_term *t)
      : Query_term_set_op(mem_root) {
    m_last_distinct = 0;
    m_children.push_back(t);
  }
  Query_term_type term_type() const override { return QT_UNARY; }
  const char *operator_string() const override { return "result"; }
  void debugPrint(int level, std::ostringstream &buf) const override;
};

/**
  Containing class for iterator over the query term tree. The structure is
  in part dictated by C++ conventions for iterators.
  @tparam visit_order indicates whether pre or post order visiting is requested
  @tparam visit_leaves indicated whether to visit the leaf nodes (query blocks)
*/
template <Visit_order visit_order, Visit_leaves visit_leaves>
class Query_terms {
 private:
  /**
    The iterator class itself is private. Only used directly by begin and end
   */
  class Query_term_iterator {
   public:
    /**
      Construct an iterator over the query term tree rooted in root, optionally
      skipping the leaves. Skipping is useful for those cases where the leaves
      are visited separately[1] and we only want to visit the set operation
      nodes in the tree.
      [1] By walking the Query_expression::first_query_block and
      Query_block::next_query_block chain
      @param root the node to start iteration from
     */
    Query_term_iterator(Query_term *root) {
      m_current = root;
      if (m_current != nullptr) m_current->m_curr_id = 0;
      if constexpr (visit_order == QTC_POST_ORDER) {
        // start at left-most leaf node
        while (m_current != nullptr &&
               m_current->term_type() != QT_QUERY_BLOCK) {
          m_current = down_cast<Query_term_set_op *>(m_current)
                          ->m_children[m_current->m_curr_id];
          m_current->m_curr_id = 0;
        }
        if constexpr (visit_leaves == VL_SKIP_LEAVES) operator++();
      }
    }

    Query_term_iterator() = default;

    Query_term_iterator &operator++() {
      if (m_current == nullptr) {
        return *this;
      }
      while (m_current != nullptr) {
        uint children = m_current->child_count();
        if constexpr (visit_order == QTC_PRE_ORDER) {
          while (m_current != nullptr && (m_current->m_curr_id >= children)) {
            // no more at this level, go back up
            m_current = m_current->m_parent;
            if (m_current != nullptr) {
              children = m_current->child_count();
            }
          }

          if (m_current == nullptr) return *this;
          m_current = down_cast<Query_term_set_op *>(m_current)
                          ->m_children[m_current->m_curr_id++];
          m_current->m_curr_id = 0;
        } else {
          m_current = m_current->m_parent;
          if (m_current == nullptr) return *this;
          if (++m_current->m_curr_id < m_current->child_count()) {
            while (m_current != nullptr &&
                   m_current->term_type() != QT_QUERY_BLOCK) {
              m_current = down_cast<Query_term_set_op *>(m_current)
                              ->m_children[m_current->m_curr_id];
              m_current->m_curr_id = 0;
            }
          } else {
            // return non-leaf
          }
        }
        if constexpr (visit_leaves == VL_VISIT_LEAVES)
          break;
        else if (m_current->term_type() != QT_QUERY_BLOCK)
          break;
      }
      return *this;
    }

    Query_term *operator*() { return m_current; }

    bool operator==(const Query_term_iterator &other) const {
      return m_current == other.m_current;
    }

    bool operator!=(const Query_term_iterator &other) const {
      return !((*this) == other);
    }
    /// Iterator state consists of this and Query_term::m_curr_id
    Query_term *m_current{nullptr};
  };

 public:
  /// Construct an iterator starting at root.
  Query_terms(Query_term *root) : m_root(root) {}

  Query_term_iterator begin() { return Query_term_iterator(m_root); }
  Query_term_iterator end() { return Query_term_iterator(); }

 private:
  Query_term *m_root;
};

/* Local Variables:  */
/* mode: c++         */
/* fill-column: 80   */
/* End:              */

#endif /* QUERY_NODE_INCLUDED */
