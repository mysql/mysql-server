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

#include "sql/query_term.h"
#include <stdint.h>
#include <stdio.h>
#include <limits>
#include <sstream>
#include <utility>
#include "my_base.h"
#include "my_inttypes.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/field.h"
#include "sql/item.h"

#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/explain_access_path.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_tmp_table.h"
#include "sql/sql_union.h"
#include "sql/table.h"
#include "template_utils.h"

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
        const uint sibling_idx = child->sibling_idx();
        child = child->pushdown_limit_order_by(
            down_cast<Query_term_set_op *>(this));
        // Make sure the new child inherits the old child's sibling index
        child->set_sibling_idx(sibling_idx);
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

bool Query_term::create_tmp_table(THD *thd, ulonglong create_options) {
  const bool distinct = m_parent->last_distinct() > 0;

  auto *tr = new (thd->mem_root) Table_ref();
  if (tr == nullptr) return true;
  set_result_table(tr);

  char *buffer = new (thd->mem_root) char[64 + 1];
  if (buffer == nullptr) return true;
  snprintf(buffer, 64, "<%s temporary>", m_parent->operator_string());

  if (setop_query_result_union()->create_result_table(
          thd, *m_parent->types_array(), distinct, create_options, buffer,
          false,
          /*instantiate_tmp_table*/ m_parent->is_materialized(), m_parent))
    return true;
  setop_query_result_union()->table->pos_in_table_list = m_result_table;
  m_result_table->db = "";
  // We set the table_name and alias to an empty string here: this avoids
  // giving the user likely unwanted information about the name of the temporary
  // table e.g. as:
  //    Note  1276  Field or reference '<union temporary>.a' of SELECT #3 was
  //                resolved in SELECT #1
  // We prefer just "reference 'a'" in such a case.
  m_result_table->table_name = "";
  m_result_table->alias = "";
  m_result_table->table = setop_query_result_union()->table;
  m_result_table->query_block = query_block();
  m_result_table->set_tableno(0);
  m_result_table->set_privileges(SELECT_ACL);

  auto *pb = m_parent->query_block();
  // Parent's input is this tmp table
  pb->m_table_list.link_in_list(m_result_table, &m_result_table->next_local);
  mem_root_deque<Item *> *il =
      new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
  if (il == nullptr) return true;
  if (pb->get_table_list()->table->fill_item_list(il))
    return true;  // purecov: inspected
  pb->fields = *il;
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

void Query_term::indent(int level, std::ostringstream &buf) {
  for (int i = 0; i < level; i++) buf << "  ";
}

void Query_term::printPointers(std::ostringstream &buf) const {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), ": %p parent: %p ", this, m_parent);
  buf << buffer;
}

bool Query_term_unary::prepare_query_term(
    THD *thd, Query_expression *qe,
    Change_current_query_block *save_query_block,
    mem_root_deque<Item *> *insert_field_list, Query_result *common_result,
    ulonglong added_options, ulonglong removed_options,
    ulonglong create_options) {
  auto *qb = query_block();
  assert(m_children.size() == 1);

  qb->make_active_options(
      (added_options & (OPTION_FOUND_ROWS | OPTION_BUFFER_RESULT)) |
          OPTION_NO_CONST_TABLES | SELECT_NO_UNLOCK,
      0);

  if (m_parent == nullptr) {
    // e.g. Query_result_send or Query_result_create
    set_setop_query_result(qe->query_result());
  } else if (common_result != nullptr) {
    set_setop_query_result(common_result);
  } else {
    auto *qr = new (thd->mem_root) Query_result_union();
    if (qr == nullptr) return true;
    set_setop_query_result(qr);
    set_owning_operand();
  }
  qb->set_query_result(setop_query_result());

  if (m_children[0]->prepare_query_term(
          thd, qe, save_query_block, insert_field_list,
          /*common_result*/ nullptr, added_options, removed_options,
          create_options))
    return true;

  if (m_children[0]->create_tmp_table(thd, create_options)) return true;

  // Set up the result table for name resolution
  qb->context.table_list = qb->context.first_name_resolution_table =
      qb->get_table_list();
  qb->add_joined_table(qb->get_table_list());
  for (ORDER *order = qb->order_list.first; order != nullptr;
       order = order->next) {
    Item_ident::Change_context ctx(&qb->context);
    (*order->item)
        ->walk(&Item::change_context_processor, enum_walk::POSTFIX,
               pointer_cast<uchar *>(&ctx));
  }

  thd->lex->set_current_query_block(qb);

  if (qb->prepare(thd, nullptr)) return true;

  if (qb->base_ref_items.is_null()) qb->n_child_sum_items += qb->n_sum_items;

  if (check_joined_types()) return true;
  return false;
}

/**
  Helper method: create a materialized access path, estimate its cost and
  move it to the best place, cf. doc for MoveCompositeIteratorsFromTablePath
  @param thd      session state
  @param qt       query term for which we want to create a materialized access
                  path
  @param operands the constituent operands (query blocks) we want to materialize
  @param dest     the destination temporary (materialized) table
  @param limit    If not HA_POS_ERROR, the maximum number of rows allowed in
                  the materialized table
  @return non-empty access path. If empty, this is an error
*/
static AccessPath *add_materialized_access_path(
    THD *thd, Query_term *qt,
    Mem_root_array<MaterializePathParameters::Operand> &operands, TABLE *dest,
    ha_rows limit = HA_POS_ERROR) {
  AccessPath *path = qt->query_block()->join->root_access_path();
  path = NewMaterializeAccessPath(thd, std::move(operands),
                                  /*invalidators=*/nullptr, dest, path,
                                  /*cte=*/nullptr, /*unit=*/nullptr,
                                  /*ref_slice=*/-1,
                                  /*rematerialize=*/true, limit,
                                  /*reject_multiple_rows=*/false);
  EstimateMaterializeCost(thd, path);
  return MoveCompositeIteratorsFromTablePath(thd, path, *qt->query_block());
}

AccessPath *Query_term_unary::make_set_op_access_path(
    THD *thd, Query_term_set_op *parent, Mem_root_array<AppendPathParameters> *,
    bool calc_found_rows) {
  AccessPath *path = nullptr;

  path = m_children[0]->make_set_op_access_path(thd, this, nullptr,
                                                calc_found_rows);
  if (parent == nullptr) return path;
  TABLE *const dest = setop_query_result_union()->table;
  MaterializePathParameters::Operand param =
      query_block()->setup_materialize_query_block(path, dest);
  Mem_root_array<MaterializePathParameters::Operand> operands(thd->mem_root);
  operands.push_back(param);
  path = add_materialized_access_path(thd, parent, operands, dest);

  return path;
}

void Query_term_unary::debugPrint(int level, std::ostringstream &buf) const {
  buf << '\n';
  indent(level, buf);
  buf << "Query_term_unary";
  printPointers(buf);
  buf << '\n';
  if (query_block() != nullptr) query_block()->qbPrint(level, buf);
  assert(m_children.size() == 1);
  for (Query_term *elt : m_children) {
    elt->debugPrint(level + 1, buf);
  }
}

bool Query_term_set_op::has_mixed_distinct_operators() {
  return (m_last_distinct > 0) &&
         (static_cast<size_t>(m_last_distinct) < (m_children.size() - 1));
}

bool Query_term_set_op::check_joined_types() {
  if (m_parent != nullptr) return false;
  for (Item *type : types_iterator()) {
    if (type->result_type() == STRING_RESULT &&
        type->collation.derivation == DERIVATION_NONE) {
      my_error(ER_CANT_AGGREGATE_NCOLLATIONS, MYF(0), "UNION");
      return true;
    }
  }

  return false;
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

bool Query_term_set_op::prepare_query_term(
    THD *thd, Query_expression *qe,
    Change_current_query_block *save_query_block,
    mem_root_deque<Item *> *insert_field_list, Query_result *common_result,
    ulonglong added_options, ulonglong removed_options,
    ulonglong create_options) {
  m_types = new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
  if (m_types == nullptr) return true;

  auto *qb = query_block();
  assert(m_children.size() >= 2);

  if (term_type() == QT_EXCEPT &&
      m_first_distinct == std::numeric_limits<int64_t>::max())
    qe->m_contains_except_all = true;

  qb->make_active_options(
      (added_options & (OPTION_FOUND_ROWS | OPTION_BUFFER_RESULT)) |
          OPTION_NO_CONST_TABLES | SELECT_NO_UNLOCK,
      0);

  if (m_parent == nullptr) {
    // e.g. Query_result_send or Query_result_create
    set_setop_query_result(qe->query_result());
  } else if (common_result != nullptr) {
    /// We are part of upper level set op
    set_setop_query_result(common_result);
  } else {
    auto *rs = new (thd->mem_root) Query_result_union();
    if (rs == nullptr) return true;
    set_setop_query_result(rs);
    set_owning_operand();
  }
  qb->set_query_result(setop_query_result());

  // To support SQL T101 "Enhanced nullability determination", the rules for
  // computing nullability of the result columns of a set operation require that
  // we perform different computation for UNION, INTERSECT and EXCEPT, cf. SQL
  // 2014, Vol 2, section 7.17 <query expression>, SR 18 and 20.
  // When preparing the leaf query blocks, type unification for set operations
  // is done by calling Item_aggregate_type::unify_types() including setting
  // nullability.  This works correctly for UNION, but not if we have INTERSECT
  // and/or EXCEPT in the tree of set operations.
  // The "nullable" information is in general incorrect after the call to
  // unify_types().  But when iterating over the children, we calculate the
  // proper nullability, and when all children have been processed, we assign
  // proper nullability to the types.
  //
  Mem_root_array<bool> columns_nullable(thd->mem_root);

  for (size_t i = 0; i < m_children.size(); i++) {
    Query_result *const cmn_result =
        (i == 0) ? nullptr : m_children[0]->setop_query_result();
    // operands 1..size-1 inherit operand 0's query_result: they all
    // contribute to the same result.
    if (m_children[i]->prepare_query_term(
            thd, qe, save_query_block, insert_field_list, cmn_result,
            added_options, removed_options, create_options))
      return true;

    Query_block *const child_block =
        m_children[i]->term_type() == QT_QUERY_BLOCK
            ? m_children[i]->query_block()
            : nullptr;

    if (i == 0) {
      // operand one determines the result set column names, and sets their
      // initial type
      for (Item *item_tmp : m_children[i]->types_iterator()) {
        /*
          If the outer query has a GROUP BY clause, an outer reference to this
          query block may have been wrapped in a Item_outer_ref, which has not
          been fixed yet. An Item_type_holder must be created based on a fixed
          Item, so use the inner Item instead.
        */
        assert(item_tmp->fixed ||
               (item_tmp->type() == Item::REF_ITEM &&
                down_cast<Item_ref *>(item_tmp)->ref_type() ==
                    Item_ref::OUTER_REF));
        if (!item_tmp->fixed) item_tmp = item_tmp->real_item();
        Item_type_holder *holder;
        if (item_tmp->type() == Item::TYPE_HOLDER_ITEM) {
          holder = down_cast<Item_type_holder *>(item_tmp);
        } else {
          holder = new Item_type_holder(thd, item_tmp);
          if (holder == nullptr) return true; /* purecov: inspected */
          const bool top_level = m_parent == nullptr;
          if (top_level && qe->is_recursive()) {
            holder->set_nullable(true);  // Always nullable, per SQL standard.
            /*
              The UNION code relies on unify_types() to change some
              transitional types like MYSQL_TYPE_DATETIME2 into other types; in
              case this is the only nonrecursive query block unify_types() won't
              be called so we need an explicit call:
            */
            holder->unify_types(thd, item_tmp);
          }
        }
        if (m_types->push_back(holder)) return true;
      }
    } else {
      // join types of operand 1 with operands 2..n
      if (m_types->size() != m_children[i]->visible_column_count()) {
        my_error(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT, MYF(0));
        return true;
      }

      if (child_block != nullptr && child_block->is_recursive()) {
        /*
          Recursive query blocks don't determine output types of the result.
          The only thing to check could be if the recursive query block has a
          type which can't be cast to the output type of the result.
          But in MySQL, all types can be cast to each other (at least during
          resolution; an error may reported when trying to actually insert, for
          example an INT into a POINT). So no further compatibility check is
          needed here.
        */
      } else {
        auto it = m_children[i]->types_iterator().begin();
        auto tp = m_types->begin();
        for (; it != m_children[i]->types_iterator().end() &&
               tp != m_types->end();
             ++it, ++tp) {
          if (down_cast<Item_type_holder *>(*tp)->unify_types(thd, *it))
            return true;
        }
      }
    }
    if (child_block != nullptr && child_block->recursive_reference != nullptr &&
        (child_block->is_grouped() || child_block->m_windows.elements > 0)) {
      // Per SQL2011.
      my_error(ER_CTE_RECURSIVE_FORBIDS_AGGREGATION, MYF(0),
               qe->derived_table->alias);
      return true;
    }

    size_t j = 0;
    for (auto *type : m_children[i]->types_iterator()) {
      bool column_nullable = type->is_nullable();
      if (i == 0) {  // left side
        const bool top_level = m_parent == nullptr;
        // Always nullable, per SQL standard. Note that setting this is
        // redundant, as right hand side of UNION (top level recursive is always
        // a union), will be always nullable anyway, so we will end up
        // with the right value for result column in case QT_UNION below anyway.
        const bool recursive_nullable = top_level && qe->is_recursive();
        column_nullable = column_nullable || recursive_nullable;
        // We can only size this now after left side operand has been resolved
        columns_nullable.resize(m_children[i]->visible_column_count(), false);
        columns_nullable[j] = column_nullable;
      } else {
        switch (term_type()) {
          case QT_UNION:
            columns_nullable[j] = columns_nullable[j] || column_nullable;
            break;
          case QT_INTERSECT:
            columns_nullable[j] = columns_nullable[j] && column_nullable;
            break;
          case QT_EXCEPT:
            // Nothing to do, use left side unchanged
            break;
          default:
            assert(false);
        }
      }
      j++;
    }
  }

  for (size_t j = 0; j < m_types->size(); j++) {
    (*m_types)[j]->set_nullable(columns_nullable[j]);
  }

  // Do this only now when we have computed m_types completely
  if (m_children[0]->create_tmp_table(thd, create_options)) return true;

  // Adjust tmp table fields' nullability. It is safe to do this because
  // fields were created with nullability if at least one query block had
  // nullable field during type joining (UNION semantics), so we will
  // only ever set nullable here if result field originally was computed
  // as nullable in unify_types(). And removing nullability for a Field isn't
  // a problem.
  size_t idx = 0;
  for (auto *f : qb->visible_fields()) {
    f->set_nullable(columns_nullable[idx]);
    assert(f->type() == Item::FIELD_ITEM);
    if (columns_nullable[idx]) {
      down_cast<Item_field *>(f)->field->clear_flag(NOT_NULL_FLAG);
    } else {
      if (term_type() == QT_UNION)
        down_cast<Item_field *>(f)->field->set_flag(NOT_NULL_FLAG);
      // don't set NOT_NULL_FLAG for INTERSECT, EXCEPT since we may need
      // to store a NULL value for this field during hashing even though the
      // logical result of the set operation can not be NULL.
    }
  }

  if (m_is_materialized) {
    // Set up the result table for name resolution
    qb->context.table_list = qb->context.first_name_resolution_table =
        qb->get_table_list();
    qb->add_joined_table(qb->get_table_list());
    for (ORDER *order = qb->order_list.first; order != nullptr;
         order = order->next) {
      Item_ident::Change_context ctx(&qb->context);
      (*order->item)
          ->walk(&Item::change_context_processor, enum_walk::POSTFIX,
                 pointer_cast<uchar *>(&ctx));
    }

    thd->lex->set_current_query_block(qb);

    if (qb->prepare(thd, nullptr)) return true;

    if (qb->base_ref_items.is_null()) qb->n_child_sum_items += qb->n_sum_items;
  } else {
    if (qb->resolve_limits(thd)) return true;
    if (qb->query_result() != nullptr &&
        qb->query_result()->prepare(thd, qb->fields, qe))
      return true;

    auto *fields = new (thd->mem_root) mem_root_deque<Item *>(thd->mem_root);
    if (fields == nullptr) return true;
    set_fields(fields);
    if (query_block()->get_table_list()->table->fill_item_list(fields))
      return true;
  }

  if (check_joined_types()) return true;
  return false;
}

bool Query_term_set_op::optimize_query_term(THD *thd, Query_expression *qe) {
  thd->lex->set_current_query_block(query_block());

  // LIMIT is required for optimization
  if (qe->set_limit(thd, query_block())) return true; /* purecov: inspected */

  if ((is_unary() || m_is_materialized) &&
      query_block()->optimize(thd,
                              /*finalize_access_paths=*/true))
    return true;
  for (Query_term *child : m_children) {
    if (child->optimize_query_term(thd, qe)) return true;
  }

  return false;
}

AccessPath *Query_term_set_op::make_set_op_access_path(
    THD *thd, Query_term_set_op *parent,
    Mem_root_array<AppendPathParameters> *union_all_subpaths,
    bool calc_found_rows) {
  AccessPath *path = nullptr;

  if (!m_is_materialized) {
    // skip materialization at top level, we can stream all blocks
    ;
  } else {
    TABLE *const dest = m_children[0]->setop_query_result_union()->table;
    Mem_root_array<MaterializePathParameters::Operand> query_blocks =
        setup_materialize_set_op(thd, dest, union_all_subpaths != nullptr,
                                 calc_found_rows);
    const bool push_limit_down = term_type() == QT_UNION &&
                                 query_block()->order_list.size() == 0 &&
                                 !calc_found_rows;
    const ha_rows max_rows =
        push_limit_down
            ? query_block()->get_limit(thd) + query_block()->get_offset(thd)
            : HA_POS_ERROR;
    path =
        add_materialized_access_path(thd, this, query_blocks, dest, max_rows);
    if (union_all_subpaths != nullptr) {
      AppendPathParameters param{path, nullptr};
      union_all_subpaths->push_back(param);
    }
  }

  if (union_all_subpaths != nullptr) {
    assert(parent == nullptr);
    TABLE *dest = m_children[0]->setop_query_result_union()->table;
    size_t start_idx = m_last_distinct == 0 ? 0 : m_last_distinct + 1;
    for (size_t i = start_idx; i < m_children.size(); ++i) {
      // append UNION ALL blocks that follow last UNION [DISTINCT]
      Query_term *const term = m_children[i];
      Query_block *const block = term->query_block();
      JOIN *const join = block->join;
      AccessPath *child_path = join->root_access_path();
      if (term->term_type() != QT_QUERY_BLOCK) {
        child_path = term->make_set_op_access_path(thd, nullptr, nullptr,
                                                   calc_found_rows);
      }
      assert(join && join->is_optimized());
      ConvertItemsToCopy(*join->fields, dest->visible_field_ptr(),
                         &join->tmp_table_param);
      AppendPathParameters param{
          NewStreamingAccessPath(thd, child_path, join, &join->tmp_table_param,
                                 dest,
                                 /*ref_slice=*/-1),
          join};
      CopyBasicProperties(*child_path, param.path);
      union_all_subpaths->push_back(param);
    }
  } else if (parent != nullptr) {
    assert(union_all_subpaths == nullptr);
    TABLE *const dest = setop_query_result_union()->table;
    MaterializePathParameters::Operand param =
        query_block()->setup_materialize_query_block(path, dest);
    Mem_root_array<MaterializePathParameters::Operand> operands(thd->mem_root);
    operands.push_back(param);
    path = add_materialized_access_path(thd, parent, operands, dest);
  }

  return path;
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

bool Query_block::prepare_query_term(
    THD *thd, Query_expression *qe,
    Change_current_query_block *save_query_block,
    mem_root_deque<Item *> *insert_field_list, Query_result *common_result,
    ulonglong added_options, ulonglong removed_options, ulonglong) {
  make_active_options(added_options | SELECT_NO_UNLOCK, removed_options);
  thd->lex->set_current_query_block(this);

  if (this == qe->first_recursive) {
    // create_result_table() depends on current_query_block()
    save_query_block->restore();

    /*
      All following query blocks will read the temporary table, which we must
      thus create now:
    */
    if (qe->derived_table->setup_materialized_derived_tmp_table(thd))
      return true; /* purecov: inspected */
    thd->lex->set_current_query_block(this);
  }

  if (recursive_reference != nullptr) {  // Make tmp table known to query block:
    qe->derived_table->common_table_expr()->substitute_recursive_reference(
        thd, this);
  }

  if (prepare(thd, insert_field_list)) return true;

  if (recursive_reference != nullptr &&
      (is_grouped() || m_windows.elements > 0)) {
    // Per SQL2011.
    my_error(ER_CTE_RECURSIVE_FORBIDS_AGGREGATION, MYF(0),
             qe->derived_table->alias);
    return true;
  }

  if (qe->is_simple()) {
    assert(m_parent == nullptr);
    return false;
  }

  // Set up the consolidation tmp table as input to the parent
  Query_result *inner_qr = common_result;

  if (inner_qr == nullptr) {
    inner_qr = new (thd->mem_root) Query_result_union();
    if (inner_qr == nullptr) return true;
    set_owning_operand();
  }
  set_setop_query_result(inner_qr);
  set_query_result(inner_qr);

  return false;
}

AccessPath *Query_block::make_set_op_access_path(
    THD *thd, Query_term_set_op *parent, Mem_root_array<AppendPathParameters> *,
    bool calc_found_rows) {
  AccessPath *path = nullptr;
  TABLE *const dest = setop_query_result_union()->table;
  Mem_root_array<MaterializePathParameters::Operand> operands =
      parent->setup_materialize_set_op(thd, dest, false, calc_found_rows);
  path = add_materialized_access_path(thd, parent, operands, dest);

  return path;
}

mem_root_deque<Item *> *Query_block::types_array() { return &fields; }

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

void Query_block::debugPrint(int level, std::ostringstream &buf) const {
  buf << '\n';
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
