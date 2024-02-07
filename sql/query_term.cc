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

#include <sstream>
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_tmp_table.h"
#include "sql/sql_union.h"

void Query_term::print_order(const THD *thd, String *str, ORDER *order,
                             enum_query_type query_type) {
  for (; order; order = order->next) {
    unwrap_rollup_group(*order->item)
        ->print_for_order(thd, str, query_type, order->used_alias);
    if (order->direction == ORDER_DESC) str->append(STRING_WITH_LEN(" desc"));
    if (order->next) str->append(',');
  }
}

std::pair<bool, bool> Query_term::redundant_order_by(Query_block *cand,
                                                     int level) {
  /// Not very object oriented with this switch, but nice to keep logic in one
  /// place.
  switch (term_type()) {
    case QT_UNARY:
    case QT_UNION:
    case QT_INTERSECT:
    case QT_EXCEPT: {
      /// Logic here presumes that query expressions that only add
      /// limit (not order by) will have been pushed down
      if (query_block() == cand) {
        if (level == 0 || cand->has_limit()) return {true, false};
        return {true, true};
      }
      auto op = down_cast<Query_term_set_op *>(this);
      for (Query_term *child : op->m_children) {
        auto result = child->redundant_order_by(cand, level + 1);
        if (result.first /* done */) return result;
      }
    } break;
    case QT_QUERY_BLOCK: {
      if (query_block() == cand) {
        if (level == 0 || cand->has_limit()) return {true, false};
        return {true, true};
      }
    } break;
  }
  return {false, false};
}

Query_term *Query_term::pushdown_limit_order_by(Query_term_set_op *parent) {
  m_parent = parent;
  switch (term_type()) {
    case QT_UNION:
    case QT_INTERSECT:
    case QT_EXCEPT: {
      auto setop = down_cast<Query_term_set_op *>(this);
      for (Query_term *&child : setop->m_children) {
        child = child->pushdown_limit_order_by(
            down_cast<Query_term_set_op *>(this));
      }
    } break;
    case QT_UNARY: {
      auto *unary = down_cast<Query_term_unary *>(this);
      Query_block *const this_block = unary->query_block();
      Query_block *const child_block = unary->m_children[0]->query_block();
      if (this_block->order_list.elements == 0 &&
          child_block->absorb_limit_of(this_block)) {
        // Eliminate this level by pushing down LIMIT/OFFSET, if any.
        // E.g.
        //     (SELECT a, c FROM t1 ORDER BY a LIMIT 4) LIMIT 2
        // to
        //     SELECT a, c FROM t1 ORDER BY a LIMIT 2
        //
        // Recurse: we might be able to eliminate more levels
        return unary->m_children[0]->pushdown_limit_order_by(parent);
      } else {
        // The outer block has ORDER BY, and possibly a LIMIT/OFFSET.  If the
        // inner block has no ORDER BY and no LIMIT/OFFSET, we eliminate the
        // outer level by pushing down both ORDER BY and LIMIT/OFFSET: This is
        // ok, since order of subquery is unspecified, we can impose one
        // possible ordering. However, note that this makes name binding too
        // lenient (non-std). E.g in allowing b in ordering expr here:
        //
        //    (SELECT a, b AS c FROM t1) ORDER BY b+1 LIMIT 5
        // The above would be allowed, since we re-write to:
        //    SELECT a, b AS c FROM t1 ORDER BY b+1 LIMIT 5
        //
        if (child_block->order_list.elements == 0 &&
            child_block->select_limit == nullptr) {
          child_block->order_list = this_block->order_list;
          child_block->absorb_limit_of(this_block);
          child_block->m_windows.prepend(&this_block->m_windows);
          child_block->select_n_where_fields +=
              this_block->select_n_where_fields;
          child_block->n_sum_items += this_block->n_sum_items;
          child_block->n_child_sum_items += this_block->n_child_sum_items;
          child_block->n_scalar_subqueries += this_block->n_scalar_subqueries;

          if (this_block->first_inner_query_expression() != nullptr) {
            // Change context of any items in ORDER BY to child block
            Item_ident::Change_context ctx(&child_block->context);
            for (ORDER *o = this_block->order_list.first; o; o = o->next) {
              o->item_initial->walk(&Item::change_context_processor,
                                    enum_walk::POSTFIX,
                                    reinterpret_cast<uchar *>(&ctx));
            }

            // Also move any inner query expression's to the child block.
            // This can happen if an ORDER BY expression has a subquery
            Query_expression *qe = this_block->first_inner_query_expression();
            while (qe != nullptr) {
              // Save next ptr, will be destroyed by include_down
              Query_expression *const next_qe = qe->next_query_expression();
              qe->include_down(this_block->parent_lex, child_block);
              qe->first_query_block()->context.outer_context->query_block =
                  child_block;
              qe = next_qe;
            }
          }
          // Recurse: we might be able to eliminate more levels
          return unary->m_children[0]->pushdown_limit_order_by(parent);
        } else {
          // We can't push down, simplify lower levels
          unary->m_children[0] = unary->m_children[0]->pushdown_limit_order_by(
              down_cast<Query_term_set_op *>(this));
        }
      }
    } break;
    case QT_QUERY_BLOCK: {
    } break;
  }
  return this;
}

bool Query_term::validate_structure(const Query_term *parent [[maybe_unused]],
                                    int level) const {
  assert(m_parent == parent);
  if (level > MAX_SELECT_NESTING) {
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, MYF(0));
    return true;
  }
  if (term_type() == QT_QUERY_BLOCK) return false;
  for (Query_term *child :
       down_cast<const Query_term_set_op *>(this)->m_children) {
    if (child->validate_structure(this, level + 1)) return true;
  }
  query_block()->renumber(query_block()->parent_lex);
  return false;
}

void Query_term::cleanup_query_result(bool full) {
  const bool has_query_result =
      m_owning_operand && m_setop_query_result != nullptr;
  if (has_query_result) m_setop_query_result->cleanup();

  if (full) {
    if (has_query_result && setop_query_result_union()->table != nullptr)
      close_tmp_table(setop_query_result_union()->table);
  }
}

bool Query_term_set_op::has_mixed_distinct_operators() {
  return (m_last_distinct > 0) &&
         (static_cast<size_t>(m_last_distinct) < (m_children.size() - 1));
}

void Query_term_set_op::print(int level, std::ostringstream &buf,
                              const char *type) const {
  buf << std::endl;
  indent(level, buf);
  buf << type;
  printPointers(buf);
  buf << std::endl;
  if (query_block() != nullptr) query_block()->qbPrint(level, buf);
  indent(level, buf);
  buf << "first distinct index: " << m_first_distinct;
  buf << "  last distinct index: " << m_last_distinct;
  buf << std::endl;
  for (Query_term *child : m_children) {
    child->debugPrint(level + 1, buf);
  }
}

bool Query_term_set_op::open_result_tables(THD *thd, int level) {
  if (level > 0) {
    Query_result_union *const qr = setop_query_result_union();

    if (qr->table != nullptr && !qr->table->is_created() &&
        !qr->skip_create_table() && instantiate_tmp_table(thd, qr->table))
      return true;
  }
  for (Query_term *child : m_children) {
    if (child->open_result_tables(thd, level + 1)) return true;
  }
  return false;
}

void Query_term_set_op::cleanup(bool full) {
  cleanup_query_result(full);
  query_block()->cleanup(full);
}

void Query_term::indent(int level, std::ostringstream &buf) {
  for (int i = 0; i < level; i++) buf << "  ";
}

void Query_term::printPointers(std::ostringstream &buf) const {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), ": %p parent: %p ", this, m_parent);
  buf << buffer;
}

void Query_term_union::debugPrint(int level, std::ostringstream &buf) const {
  Query_term_set_op::print(level, buf, "Query_term_union");
}

void Query_term_intersect::debugPrint(int level,
                                      std::ostringstream &buf) const {
  Query_term_set_op::print(level, buf, "Query_term_intersect");
}

void Query_term_except::debugPrint(int level, std::ostringstream &buf) const {
  Query_term_set_op::print(level, buf, "Query_term_except");
}

/**
  Used for debugging/trace. Dumps some info on access path, minion of
  Query_block::qbPrint.
  @param level level in tree
  @param p     the access path to print
  @param buf   buf the buffer to print into
 */
static void dumpAccessPath(int level, AccessPath *p, std::ostringstream &buf) {
  std::string ret;
  std::string str;
  char buffer[256];
  while (p) {
    Mem_root_array<MaterializePathParameters::Operand> *operands = nullptr;
    Mem_root_array<AppendPathParameters> *append_children = nullptr;
    snprintf(buffer, sizeof(buffer), "AP: %p ", p);
    str.append(buffer);
    switch (p->type) {
      case AccessPath::LIMIT_OFFSET:
        str.append("AccessPath::LIMIT_OFFSET ");
        snprintf(buffer, sizeof(buffer), "%llu", p->limit_offset().limit);
        str.append(buffer);
        p = p->limit_offset().child;
        break;
      case AccessPath::TABLE_SCAN:
        str.append("AccessPath::TABLE_SCAN alias: ");
        str.append(p->table_scan().table->alias ? p->table_scan().table->alias
                                                : "<no alias>");
        p = nullptr;
        break;
      case AccessPath::SORT:
        str.append("AccessPath::SORT");
        p = p->sort().child;
        break;
      case AccessPath::MATERIALIZE:
        str.append("AccessPath::MATERIALIZE ");
        operands = &p->materialize().param->m_operands;
        str.append(p->materialize().param->table->alias);
        p = p->materialize().table_path;
        break;
      case AccessPath::FAKE_SINGLE_ROW:
        str.append("AccessPath::FAKE_SINGLE_ROW ");
        p = nullptr;
        break;
      case AccessPath::TABLE_VALUE_CONSTRUCTOR:
        str.append("AccessPath::TABLE_VALUE_CONSTRUCTOR ");
        p = nullptr;
        break;
      case AccessPath::AGGREGATE:
        str.append("AccessPath::AGGREGATE ");
        if (p->aggregate().olap != UNSPECIFIED_OLAP_TYPE) {
          str.append(GroupByModifierString(p->aggregate().olap));
        }
        p = p->aggregate().child;
        break;
      case AccessPath::FILTER:
        str.append("AccessPath::FILTER ...");
        p = p->filter().child;
        break;
      case AccessPath::HASH_JOIN:
        str.append("AccessPath::HASH_JOIN outer: ... inner: ");
        p = p->hash_join().inner;
        break;
      case AccessPath::NESTED_LOOP_JOIN:
        str.append("AccessPath::NESTED loop outer: ... inner: ");
        p = p->nested_loop_join().inner;
        break;
      case AccessPath::FOLLOW_TAIL:
        str.append("AccessPath::FOLLOW_TAIL ");
        str.append(p->follow_tail().table->alias ? p->follow_tail().table->alias
                                                 : "<no alias>");
        p = nullptr;
        break;
      case AccessPath::MATERIALIZED_TABLE_FUNCTION:
        str.append("AccessPath::MATERIALIZED_TABLE_FUNCTION ");
        str.append(p->materialized_table_function().table->alias
                       ? p->materialized_table_function().table->alias
                       : "<no alias>");
        p = p->materialized_table_function().table_path;
        break;
      case AccessPath::INDEX_SCAN:
        str.append("AccessPath::INDEX_SCAN ");
        str.append(p->index_scan().table->alias ? p->index_scan().table->alias
                                                : "<no alias>");
        p = nullptr;
        break;
      case AccessPath::APPEND:
        str.append("AccessPath::APPEND ");
        append_children = p->append().children;
        p = nullptr;
        break;
      case AccessPath::TEMPTABLE_AGGREGATE:
        str.append("AccessPath::TEMPTABLE_AGGREGATE ");
        str.append(p->temptable_aggregate().table->alias);
        p = p->temptable_aggregate().subquery_path;
        break;
      case AccessPath::STREAM:
        str.append("AccessPath::STREAM ");
        p = p->stream().child;
        break;
      case AccessPath::WINDOW:
        str.append("AccessPath::WINDOW ");
        str.append(p->window().temp_table->alias ? p->window().temp_table->alias
                                                 : "<no alias>");
        p = p->window().child;
        break;
      case AccessPath::WEEDOUT:
        str.append("AccessPath::WEEDOUT ");
        p = p->weedout().child;
        break;
      case AccessPath::ZERO_ROWS:
        str.append("AccessPath::ZeroRows");
        p = p->zero_rows().child;
        break;
      default:
        assert(false);
    }
    Query_term::indent(level, buf);
    ret.append(level * 2, ' ');
    ret += "-> ";
    ret += str;
    ret += "\n";
    buf << ret;
    ret.clear();
    str.clear();
    ++level;
    if (operands != nullptr)
      for (MaterializePathParameters::Operand subp : *operands) {
        dumpAccessPath(level + 1, subp.subquery_path, buf);
      }
    if (append_children != nullptr)
      for (AppendPathParameters subp : *append_children) {
        dumpAccessPath(level + 1, subp.path, buf);
      }
  }
}

void Query_block::qbPrint(int level, std::ostringstream &buf) const {
  indent(level, buf);
  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "qb: %p join: %p ", this, join);
    buf << buffer;
    buf << std::endl;
  }

  String buffer;
  if (order_list.elements) {
    buffer.append(STRING_WITH_LEN("order by "));
    print_order(current_thd, &buffer, query_block()->order_list.first,
                QT_ORDINARY);
    buffer.append(STRING_WITH_LEN(" "));
  }

  if (select_limit != nullptr) {
    buffer.append(STRING_WITH_LEN(" limit "));
    select_limit->print(current_thd, &buffer, QT_ORDINARY);
    if (offset_limit != nullptr) {
      buffer.append(',');
      buffer.append(STRING_WITH_LEN(" offset "));
      offset_limit->print(current_thd, &buffer, QT_ORDINARY);
    }
  }
  if (buffer.length() > 0) {
    indent(level, buf);
    buf << buffer.c_ptr_safe();
    buf << std::endl;
  }
  if (join && join->root_access_path()) {
    // buf << PrintQueryPlan(0, join->root_access_path(), join, true).c_str();
    dumpAccessPath(level, join->root_access_path(), buf);
  }
}

void Query_term_unary::debugPrint(int level, std::ostringstream &buf) const {
  buf << std::endl;
  indent(level, buf);
  buf << "Query_term_unary";
  printPointers(buf);
  buf << std::endl;
  if (query_block() != nullptr) query_block()->qbPrint(level, buf);
  assert(m_children.size() == 1);
  for (Query_term *elt : m_children) {
    elt->debugPrint(level + 1, buf);
  }
}

void Query_block::debugPrint(int level, std::ostringstream &buf) const {
  buf << std::endl;
  indent(level, buf);
  buf << "Query_block";
  printPointers(buf);
  if (slave != nullptr) buf << " with subqueries";
  qbPrint(level, buf);
  Query_expression *subquery = slave;
  while (subquery != nullptr) {
    subquery->m_query_term->debugPrint(level + 1, buf);
    subquery = subquery->next;
  }
}

bool Query_block::open_result_tables(THD *thd, int) {
  Query_result_union *const qr = setop_query_result_union();
  if (qr->table != nullptr && !qr->table->is_created() &&
      !qr->skip_create_table() && instantiate_tmp_table(thd, qr->table))
    return true;
  return false;
}

bool Query_block::absorb_limit_of(Query_block *parent) {
  bool did_do = true;
  if (select_limit == nullptr) {
    select_limit = parent->select_limit;
    offset_limit = parent->offset_limit;
  } else if (parent->select_limit == nullptr) {
    ;                                       // parent is an empty level, drop it
  } else if (select_limit->const_item() &&  // ensure we can evaluate
             parent->select_limit->const_item()) {
    if (parent->select_limit->val_int() < select_limit->val_int())
      select_limit = parent->select_limit;  // the smaller wins

    if (offset_limit == nullptr) {
      offset_limit = parent->offset_limit;
    } else if (parent->offset_limit != nullptr) {
      // If both levels have offsets, we can just add them
      offset_limit = new Item_uint(offset_limit->val_int() +
                                   parent->offset_limit->val_int());
    }
  } else {
    did_do = false;
  }
  return did_do;
}

/* Local Variables:  */
/* mode: c++         */
/* fill-column: 80   */
/* End:              */
