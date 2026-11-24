/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file sql/duckdb/mysql_udf.cc
 */
#include "sql/duckdb/duckdb_mysql_udf.h"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/function/function_set.hpp"
#include "sql/item_json_func.h"
#include "sql/item_regexp_func.h"
#include "sql/item_strfunc.h"
#include "sql/parse_tree_nodes.h"
namespace myduck {
/**
   Wrapper class for easy to duckdb::string_t as an MySQL Item_string;
 */
class Item_duckdb_string : public Item_string {
 public:
  explicit Item_duckdb_string(const duckdb::string_t &str)
      : Item_string(POS()) {
    String _str(str.GetData(), str.GetSize(), &my_charset_utf8mb4_bin);
    set_str_value(&_str);
    fixed = true;
  }
};

bool mysql_json_overlaps(duckdb::string_t json1, duckdb::string_t json2) {
  Item_duckdb_string item_json1(json1);
  Item_duckdb_string item_json2(json2);
  Item_func_json_overlaps json_overlaps(POS(), &item_json1, &item_json2);
  json_overlaps.m_caller_is_duckdb = true;
  json_overlaps.fixed = true;
  return json_overlaps.val_int();
}

int64_t mysql_json_depth(duckdb::string_t json) {
  Item_duckdb_string item_json(json);
  Item_func_json_depth json_depth(POS(), &item_json);
  json_depth.fixed = true;
  json_depth.m_caller_is_duckdb = true;
  return json_depth.val_int();
}

void mysql_json_unquote(duckdb::DataChunk &input,
                        duckdb::ExpressionState &state [[maybe_unused]],
                        duckdb::Vector &result) {
  auto &input_arg = input.data[0];

  if (input_arg.GetVectorType() == duckdb::VectorType::CONSTANT_VECTOR) {
    result.SetVectorType(duckdb::VectorType::CONSTANT_VECTOR);

    auto ldata = duckdb::ConstantVector::GetData<duckdb::string_t>(input_arg);
    auto result_data =
        duckdb::ConstantVector::GetData<duckdb::string_t>(result);
    if (duckdb::ConstantVector::IsNull(input_arg)) {
      duckdb::ConstantVector::SetNull(result, true);
    } else {
      duckdb::ConstantVector::SetNull(result, false);
      Item_duckdb_string item_json(*ldata);
      Item_func_json_unquote json_unquote(POS(), &item_json);
      json_unquote.fixed = true;
      json_unquote.m_caller_is_duckdb = true;
      String tmp;
      String *func_result = json_unquote.val_str(&tmp);
      if (json_unquote.null_value) {
        duckdb::ConstantVector::SetNull(result, true);
      }
      duckdb::string_t target =
          duckdb::StringVector::EmptyString(result, func_result->length());
      auto target_data = target.GetDataWriteable();
      memcpy(target_data, func_result->ptr(), func_result->length());
      target.Finalize();
      *result_data = target;
    }
  } else {
    result.SetVectorType(duckdb::VectorType::FLAT_VECTOR);
    duckdb::UnifiedVectorFormat vdata;
    input_arg.ToUnifiedFormat(input.size(), vdata);

    auto data = duckdb::UnifiedVectorFormat::GetData<duckdb::string_t>(vdata);

    auto result_data = duckdb::FlatVector::GetData<duckdb::string_t>(result);
    auto &result_validity = duckdb::FlatVector::Validity(result);

    for (idx_t i = 0; i < input.size(); i++) {
      auto idx = vdata.sel->get_index(i);
      if (!vdata.validity.RowIsValid(idx)) {
        result_validity.SetInvalid(i);
        continue;
      }
      Item_duckdb_string item_json(data[idx]);
      Item_func_json_unquote json_unquote(POS(), &item_json);
      json_unquote.fixed = true;
      json_unquote.m_caller_is_duckdb = true;
      String tmp;
      String *func_result = json_unquote.val_str(&tmp);
      if (json_unquote.null_value) {
        result_validity.SetInvalid(i);
      }
      duckdb::string_t target =
          duckdb::StringVector::EmptyString(result, func_result->length());
      auto target_data = target.GetDataWriteable();
      memcpy(target_data, func_result->ptr(), func_result->length());
      target.Finalize();
      result_data[i] = target;
    }
  }
}

/**
 1. The current implementation has performance issues. If pattern is a const
    value, we should set_pattern only once when binding, but now we set_pattern
    for each row.
 2. Whether the regexp function is case-sensitive is not affected by collation.
 */
bool mysql_regexp_like_binary(duckdb::string_t expr, duckdb::string_t pattern) {
  Item_duckdb_string item_expr(expr);
  Item_duckdb_string item_pattern(pattern);
  /** In the future, we shuold eliminate the dependency of MEM_ROOT. */
  MEM_ROOT mem_root;
  PT_item_list item_list(&mem_root);
  item_list.push_back(&item_expr);
  item_list.push_back(&item_pattern);
  Item_func_regexp_like regexpr_like(POS(), &item_list, &mem_root);
  regexpr_like.collation.set(&my_charset_utf8mb4_bin);
  return regexpr_like.val_int();
}

bool mysql_regexp_like_ternary(duckdb::string_t expr, duckdb::string_t pattern,
                               duckdb::string_t match_type) {
  Item_duckdb_string item_expr(expr);
  Item_duckdb_string item_pattern(pattern);
  Item_duckdb_string item_match_type(match_type);
  /** In the future, we shuold eliminate the dependency of MEM_ROOT. */
  MEM_ROOT mem_root;
  PT_item_list item_list(&mem_root);
  item_list.push_back(&item_expr);
  item_list.push_back(&item_pattern);
  item_list.push_back(&item_match_type);
  Item_func_regexp_like regexpr_like(POS(), &item_list, &mem_root);
  regexpr_like.collation.set(&my_charset_utf8mb4_bin);
  return regexpr_like.val_int();
}

void mysql_regexp_instr(duckdb::DataChunk &input,
                        duckdb::ExpressionState &state [[maybe_unused]],
                        duckdb::Vector &result) {
  auto count = input.size();
  MEM_ROOT mem_root;
  std::vector<PT_item_list *> item_lists;
  for (idx_t i = 0; i < count; i++) {
    PT_item_list *item_list = new (&mem_root) PT_item_list(&mem_root);
    item_lists.push_back(item_list);
  }
  result.SetVectorType(duckdb::VectorType::CONSTANT_VECTOR);
  for (idx_t i = 0; i < input.ColumnCount(); i++) {
    if (input.data[i].GetVectorType() != duckdb::VectorType::CONSTANT_VECTOR) {
      result.SetVectorType(duckdb::VectorType::FLAT_VECTOR);
      break;
    }
  }
  std::vector<bool> result_is_null(count, false);
  for (idx_t col = 0; col < input.ColumnCount(); col++) {
    auto &input_arg = input.data[col];
    if (col == 2 || col == 3 || col == 4) {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data = duckdb::UnifiedVectorFormat::GetData<int32_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        if (col == 4 && (data[idx] != 0 && data[idx] != 1)) {
          throw duckdb::InvalidInputException(
              "Incorrect arguments to regexp_instr: return_option must be 1 or "
              "0");
        }
        Item_int *item = new (&mem_root) Item_int(POS(), data[idx]);
        item_lists[i]->push_back(item);
      }
    } else {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data =
          duckdb::UnifiedVectorFormat::GetData<duckdb::string_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        Item_duckdb_string *item =
            new (&mem_root) Item_duckdb_string(data[idx]);
        item_lists[i]->push_back(item);
      }
    }
  }

  auto &result_validity =
      result.GetVectorType() == duckdb::VectorType::CONSTANT_VECTOR
          ? duckdb::ConstantVector::Validity(result)
          : duckdb::FlatVector::Validity(result);
  auto result_data = duckdb::FlatVector::GetData<int32_t>(result);
  for (idx_t i = 0; i < count; i++) {
    if (result_is_null[i]) {
      result_validity.SetInvalid(i);
    } else {
      Item_func_regexp_instr regexp_instr(POS(), item_lists[i], &mem_root);
      regexp_instr.collation.set(&my_charset_utf8mb4_bin);
      result_data[i] = regexp_instr.val_int();
      if (regexp_instr.null_value) {
        result_validity.SetInvalid(i);
      }
    }
  }
}

void mysql_regexp_substr(duckdb::DataChunk &input,
                         duckdb::ExpressionState &state [[maybe_unused]],
                         duckdb::Vector &result) {
  auto count = input.size();
  MEM_ROOT mem_root;
  std::vector<PT_item_list *> item_lists;
  for (idx_t i = 0; i < count; i++) {
    PT_item_list *item_list = new (&mem_root) PT_item_list(&mem_root);
    item_lists.push_back(item_list);
  }
  result.SetVectorType(duckdb::VectorType::CONSTANT_VECTOR);
  for (idx_t i = 0; i < input.ColumnCount(); i++) {
    if (input.data[i].GetVectorType() != duckdb::VectorType::CONSTANT_VECTOR) {
      result.SetVectorType(duckdb::VectorType::FLAT_VECTOR);
      break;
    }
  }
  std::vector<bool> result_is_null(count, false);
  for (idx_t col = 0; col < input.ColumnCount(); col++) {
    auto &input_arg = input.data[col];
    if (col == 2 || col == 3) {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data = duckdb::UnifiedVectorFormat::GetData<int32_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        Item_int *item = new (&mem_root) Item_int(POS(), data[idx]);
        item_lists[i]->push_back(item);
      }
    } else {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data =
          duckdb::UnifiedVectorFormat::GetData<duckdb::string_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        Item_duckdb_string *item =
            new (&mem_root) Item_duckdb_string(data[idx]);
        item_lists[i]->push_back(item);
      }
    }
  }

  auto &result_validity =
      result.GetVectorType() == duckdb::VectorType::CONSTANT_VECTOR
          ? duckdb::ConstantVector::Validity(result)
          : duckdb::FlatVector::Validity(result);
  auto result_data = duckdb::FlatVector::GetData<duckdb::string_t>(result);

  for (idx_t i = 0; i < count; i++) {
    if (result_is_null[i]) {
      result_validity.SetInvalid(i);
    } else {
      Item_func_regexp_substr regexpr_substr(POS(), item_lists[i], &mem_root);
      regexpr_substr.collation.set(&my_charset_utf8mb4_bin);
      String tmp;
      String *func_result = regexpr_substr.val_str(&tmp);
      if (regexpr_substr.null_value) {
        result_validity.SetInvalid(i);
        continue;
      }
      duckdb::string_t target =
          duckdb::StringVector::EmptyString(result, func_result->length());
      auto target_data = target.GetDataWriteable();
      memcpy(target_data, func_result->ptr(), func_result->length());
      target.Finalize();
      result_data[i] = target;
    }
  }
}

void mysql_regexp_replace(duckdb::DataChunk &input,
                          duckdb::ExpressionState &state [[maybe_unused]],
                          duckdb::Vector &result) {
  auto count = input.size();
  MEM_ROOT mem_root;
  std::vector<PT_item_list *> item_lists;
  for (idx_t i = 0; i < count; i++) {
    PT_item_list *item_list = new (&mem_root) PT_item_list(&mem_root);
    item_lists.push_back(item_list);
  }
  result.SetVectorType(duckdb::VectorType::CONSTANT_VECTOR);
  for (idx_t i = 0; i < input.ColumnCount(); i++) {
    if (input.data[i].GetVectorType() != duckdb::VectorType::CONSTANT_VECTOR) {
      result.SetVectorType(duckdb::VectorType::FLAT_VECTOR);
      break;
    }
  }
  std::vector<bool> result_is_null(count, false);
  for (idx_t col = 0; col < input.ColumnCount(); col++) {
    auto &input_arg = input.data[col];
    if (col == 3 || col == 4) {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data = duckdb::UnifiedVectorFormat::GetData<int32_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        Item_int *item = new (&mem_root) Item_int(POS(), data[idx]);
        item_lists[i]->push_back(item);
      }
    } else {
      duckdb::UnifiedVectorFormat input_data;
      input_arg.ToUnifiedFormat(count, input_data);
      auto data =
          duckdb::UnifiedVectorFormat::GetData<duckdb::string_t>(input_data);
      for (idx_t i = 0; i < count; i++) {
        auto idx = input_data.sel->get_index(i);
        if (!input_data.validity.RowIsValid(idx)) {
          result_is_null[i] = true;
          continue;
        }
        Item_duckdb_string *item =
            new (&mem_root) Item_duckdb_string(data[idx]);
        item_lists[i]->push_back(item);
      }
    }
  }

  auto &result_validity =
      result.GetVectorType() == duckdb::VectorType::CONSTANT_VECTOR
          ? duckdb::ConstantVector::Validity(result)
          : duckdb::FlatVector::Validity(result);
  auto result_data = duckdb::FlatVector::GetData<duckdb::string_t>(result);

  for (idx_t i = 0; i < count; i++) {
    if (result_is_null[i]) {
      result_validity.SetInvalid(i);
    } else {
      Item_func_regexp_replace regexpr_replace(POS(), item_lists[i], &mem_root);
      regexpr_replace.collation.set(&my_charset_utf8mb4_bin);
      String tmp;
      String *func_result = regexpr_replace.val_str(&tmp);
      if (regexpr_replace.null_value) {
        result_validity.SetInvalid(i);
        continue;
      }
      duckdb::string_t target =
          duckdb::StringVector::EmptyString(result, func_result->length());
      auto target_data = target.GetDataWriteable();
      memcpy(target_data, func_result->ptr(), func_result->length());
      target.Finalize();
      result_data[i] = target;
    }
  }
}

void register_mysql_udf(duckdb::Connection *con) {
  con->CreateScalarFunction<bool, duckdb::string_t, duckdb::string_t>(
      "json_overlaps", &mysql_json_overlaps);
  con->CreateScalarFunction<int64_t, duckdb::string_t>("json_depth",
                                                       &mysql_json_depth);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t>(
      "json_unquote", &mysql_json_unquote);

  /** regexp_like */
  con->CreateScalarFunction<bool, duckdb::string_t, duckdb::string_t>(
      "regexp_like", &mysql_regexp_like_binary);
  con->CreateScalarFunction<bool, duckdb::string_t, duckdb::string_t,
                            duckdb::string_t>("regexp_like",
                                              &mysql_regexp_like_ternary);

  /** regexp_instr */
  con->CreateVectorizedFunction<int32_t, duckdb::string_t, duckdb::string_t>(
      "regexp_instr", &mysql_regexp_instr);
  con->CreateVectorizedFunction<int32_t, duckdb::string_t, duckdb::string_t,
                                int32_t>("regexp_instr", &mysql_regexp_instr);
  con->CreateVectorizedFunction<int32_t, duckdb::string_t, duckdb::string_t,
                                int32_t, int32_t>("regexp_instr",
                                                  &mysql_regexp_instr);
  con->CreateVectorizedFunction<int32_t, duckdb::string_t, duckdb::string_t,
                                int32_t, int32_t, duckdb::string_t>(
      "regexp_instr", &mysql_regexp_instr);
  con->CreateVectorizedFunction<int32_t, duckdb::string_t, duckdb::string_t,
                                int32_t, int32_t, duckdb::string_t,
                                duckdb::string_t>("regexp_instr",
                                                  &mysql_regexp_instr);

  /** regexp_substr */
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t>("regexp_substr",
                                                  &mysql_regexp_substr);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, int32_t>(
      "regexp_substr", &mysql_regexp_substr);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, int32_t, int32_t>(
      "regexp_substr", &mysql_regexp_substr);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, int32_t, int32_t,
                                duckdb::string_t>("regexp_substr",
                                                  &mysql_regexp_substr);

  /** regexp_replace */
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t>("regexp_replace",
                                                  &mysql_regexp_replace);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, duckdb::string_t>(
      "regexp_replace", &mysql_regexp_replace);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, duckdb::string_t, int32_t>(
      "regexp_replace", &mysql_regexp_replace);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, duckdb::string_t, int32_t,
                                int32_t>("regexp_replace",
                                         &mysql_regexp_replace);
  con->CreateVectorizedFunction<duckdb::string_t, duckdb::string_t,
                                duckdb::string_t, duckdb::string_t, int32_t,
                                int32_t, duckdb::string_t>(
      "regexp_replace", &mysql_regexp_replace);
}
}  // namespace myduck
