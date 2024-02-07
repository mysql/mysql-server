/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/relational_expression.h"
#include "sql/item_cmpfunc.h"
#include "sql/join_optimizer/estimate_selectivity.h"
#include "sql/sql_array.h"

namespace {

/**
   A class for formatting a list into a string where elements are separated by
   a separator.
*/
class StringJoiner final {
 public:
  explicit StringJoiner(const char *separator) : m_separator(separator) {}

  /**
     Start a new list element. Add a separator if needed.
     @return The ostream to which we add the representation of the element.
   */
  std::ostream &StartElement() {
    if (!m_first) {
      m_stream << m_separator;
    }
    m_first = false;
    return m_stream;
  }

  std::string GetResult() const { return m_stream.str(); }

 private:
  /// The separator between elements.
  const char *m_separator;

  /// The string represenation of the list.
  std::ostringstream m_stream;

  /// True if we have not added any elements yet.
  bool m_first{true};
};

}  // Anonymous namespace.

void CompanionSet::AddEquijoinCondition(THD *thd, const Item_func_eq &eq) {
  const auto contains_field = [](const EqualTerm *term, const Field *field) {
    return std::find(term->fields->cbegin(), term->fields->cend(), field) !=
           term->fields->cend();
  };

  const auto find_term = [&](const Field *field) {
    return std::find_if(
        m_equal_terms.begin(), m_equal_terms.end(),
        [&](const EqualTerm &term) { return contains_field(&term, field); });
  };

  if (eq.arguments()[0]->type() != Item::FIELD_ITEM ||
      eq.arguments()[1]->type() != Item::FIELD_ITEM) {
    return;
  }

  const Item_field *left = down_cast<const Item_field *>(eq.arguments()[0]);
  const Item_field *right = down_cast<const Item_field *>(eq.arguments()[1]);

  if (right->field->table == left->field->table) {
    // Ignore equal fields from the same table, as
    // EstimateSelectivityFromIndexStatistics() does not use these.
    return;
  }

  auto left_term = find_term(left->field);
  auto right_term = find_term(right->field);

  if (left_term == right_term) {
    if (left_term == m_equal_terms.cend()) {  // Both fields unknown.
      FieldArray *fields = new (thd->mem_root) FieldArray(thd->mem_root);
      fields->push_back(left->field);
      fields->push_back(right->field);
      const table_map tables = left->used_tables() | right->used_tables();
      m_equal_terms.push_back({fields, tables});
    }

  } else if (left_term == m_equal_terms.cend()) {  // 'left' unknown.
    right_term->fields->push_back(left->field);
    right_term->tables |= left->used_tables();

  } else if (right_term == m_equal_terms.cend()) {  // 'right' unknown.
    left_term->fields->push_back(right->field);
    left_term->tables |= right->used_tables();

  } else {  // Both known but in different terms. Merge them.
    for (const Field *field : *left_term->fields) {
      right_term->fields->push_back(field);
    }
    right_term->tables |= left_term->tables;
    m_equal_terms.erase(left_term);
  }
}

table_map CompanionSet::GetEqualityMap(const Field &field) const {
  for (const EqualTerm &term : m_equal_terms) {
    for (const Field *equal_field : *term.fields) {
      if (&field == equal_field) {
        return term.tables;
      }
    }
  }
  return 0;
}

std::string CompanionSet::ToString() const {
  StringJoiner set_joiner(", ");

  for (const EqualTerm &term : m_equal_terms) {
    StringJoiner element_joiner(", ");

    for (const Field *field : *term.fields) {
      element_joiner.StartElement()
          << *field->table_name << "." << field->field_name;
    }

    set_joiner.StartElement() << "{" << element_joiner.GetResult() << "}";
  }

  return "{" + set_joiner.GetResult() + "}";
}

void CompanionSetCollection::Compute(THD *thd, RelationalExpression *expr,
                                     CompanionSet *current_set) {
  if (current_set == nullptr) {
    current_set = new (thd->mem_root) CompanionSet(thd);
  }

  expr->companion_set = current_set;

  switch (expr->type) {
    case RelationalExpression::TABLE:
      m_table_num_to_companion_set[expr->table->tableno()] = current_set;
      break;
    case RelationalExpression::STRAIGHT_INNER_JOIN:
    case RelationalExpression::FULL_OUTER_JOIN:
      Compute(thd, expr->left, /*current_set=*/nullptr);
      Compute(thd, expr->right, /*current_set=*/nullptr);
      break;
    case RelationalExpression::INNER_JOIN:
      Compute(thd, expr->left, current_set);
      Compute(thd, expr->right, current_set);
      break;
    case RelationalExpression::LEFT_JOIN:
    case RelationalExpression::SEMIJOIN:
    case RelationalExpression::ANTIJOIN:
      Compute(thd, expr->left, current_set);
      Compute(thd, expr->right, /*current_set=*/nullptr);
      break;
    case RelationalExpression::MULTI_INNER_JOIN:
      assert(false);
  }
}

std::string CompanionSetCollection::ToString() const {
  std::array<CompanionSet *, MAX_TABLES> all_sets(m_table_num_to_companion_set);

  std::sort(all_sets.begin(), all_sets.end());
  // Remove duplicates.
  const auto end = std::unique(all_sets.begin(), all_sets.end());
  std::ostringstream stream;

  for (auto iter = all_sets.begin(); iter < end; iter++) {
    if (*iter != nullptr) {
      stream << "Companion set: " << *iter << ":" << (*iter)->ToString()
             << "\n";
    }
  }

  return stream.str();
}

CompanionSet *CompanionSetCollection::FindInternal(table_map tables) const {
  assert(tables != 0);

  CompanionSet *ret = nullptr;
  for (int table_num : BitsSetIn(tables & ~PSEUDO_TABLE_BITS)) {
    if (m_table_num_to_companion_set[table_num] == nullptr) {
      // This table is not part of an equijoin, but a lateral reference (to a
      // preceding table in the FROM-clause).
      return nullptr;
    }
    if (ret == nullptr) {
      // First table.
      ret = m_table_num_to_companion_set[table_num];
    } else if (ret != m_table_num_to_companion_set[table_num]) {
      // Incompatible sets.
      return nullptr;
    }
  }
  return ret;
}
