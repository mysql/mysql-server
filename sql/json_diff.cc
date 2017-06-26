/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "json_diff.h"

#include "field.h"                              // Field_json
#include "json_dom.h"                           // Json_dom, Json_wrapper
#include "json_path.h"                          // Json_path
#include "my_dbug.h"                            // DBUG_ASSERT
#include "sql_class.h"                          // THD
#include "sql_string.h"                         // StringBuffer


Json_wrapper Json_diff::value() const
{
  Json_wrapper result(m_value.get());
  result.set_alias();
  return result;
}


/**
  Find the value at the specified path in a JSON DOM. The path should
  not contain any wildcard or ellipsis, only simple array cells or
  member names. Auto-wrapping is not performed.

  @param dom        the root of the DOM
  @param first_leg  the first path leg
  @param last_leg   the last path leg (exclusive)
  @return the JSON DOM at the given path, or `nullptr` if the path is not found
*/
static Json_dom *seek_exact_path(Json_dom *dom,
                                 const Json_path_iterator &first_leg,
                                 const Json_path_iterator &last_leg)
{
  for (auto it= first_leg; it != last_leg; ++it)
  {
    const Json_path_leg *leg= *it;
    const auto leg_type= leg->get_type();
    DBUG_ASSERT(leg_type == jpl_member || leg_type == jpl_array_cell);
    switch (dom->json_type())
    {
    case enum_json_type::J_ARRAY:
      {
        const auto array= down_cast<Json_array*>(dom);
        if (leg_type != jpl_array_cell)
          return nullptr;
        Json_array_index idx= leg->first_array_index(array->size());
        if (!idx.within_bounds())
          return nullptr;
        dom= (*array)[idx.position()];
        continue;
      }
    case enum_json_type::J_OBJECT:
      {
        const auto object= down_cast<Json_object*>(dom);
        if (leg_type != jpl_member)
          return nullptr;
        dom= object->get(leg->get_member_name());
        if (dom == nullptr)
          return nullptr;
        continue;
      }
    default:
      return nullptr;
    }
  }

  return dom;
}


enum_json_diff_status apply_json_diffs(Field_json *field,
                                       const Json_diff_vector *diffs)
{
  // Cannot apply a diff to NULL.
  if (field->is_null())
    return enum_json_diff_status::REJECTED;

  Json_wrapper doc;
  if (field->val_json(&doc))
    return enum_json_diff_status::ERROR;      /* purecov: inspected */

  // Should we collect logical diffs while applying them?
  const bool collect_logical_diffs=
    field->table->is_logical_diff_enabled(field);

  // Should we try to perform the update in place using binary diffs?
  bool binary_inplace_update= field->table->is_binary_diff_enabled(field);

  StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;

  const THD *thd= field->table->in_use;

  for (const Json_diff &diff : *diffs)
  {
    Json_wrapper val= diff.value();

    auto &path= diff.path();

    if (path.leg_count() == 0)
    {
      /*
        Cannot replace the root (then a full update will be used
        instead of creating a diff), or insert the root, or remove the
        root, so reject this diff.
      */
      return enum_json_diff_status::REJECTED;
    }

    if (collect_logical_diffs)
      field->table->add_logical_diff(field, path, diff.operation(), &val);

    if (binary_inplace_update)
    {
      if (diff.operation() == enum_json_diff_operation::REPLACE)
      {
        bool partially_updated= false;
        bool replaced_path= false;
        if (doc.attempt_binary_update(field, path, &val, false, &buffer,
                                      &partially_updated, &replaced_path))
          return enum_json_diff_status::ERROR;  /* purecov: inspected */

        if (partially_updated)
        {
          if (!replaced_path)
            return enum_json_diff_status::REJECTED;
          continue;
        }
      }
      else if (diff.operation() == enum_json_diff_operation::REMOVE)
      {
        Json_wrapper_vector hits(key_memory_JSON);
        bool found_path= false;
        if (doc.binary_remove(field, path, &buffer, &found_path))
          return enum_json_diff_status::ERROR;  /* purecov: inspected */
        if (!found_path)
          return enum_json_diff_status::REJECTED;
        continue;
      }

      // Couldn't update in place, so try full update.
      binary_inplace_update= false;
      field->table->disable_binary_diffs_for_current_row(field);
    }

    Json_dom *dom= doc.to_dom(thd);
    if (doc.to_dom(thd) == nullptr)
      return enum_json_diff_status::ERROR;      /* purecov: inspected */

    switch (diff.operation())
    {
    case enum_json_diff_operation::REPLACE:
      {
        DBUG_ASSERT(path.leg_count() > 0);
        Json_dom *old= seek_exact_path(dom, path.begin(), path.end());
        if (old == nullptr)
          return enum_json_diff_status::REJECTED;
        DBUG_ASSERT(old->parent() != nullptr);
        old->parent()->replace_dom_in_container(old, val.clone_dom(thd));
        continue;
      }
    case enum_json_diff_operation::INSERT:
      {
        DBUG_ASSERT(path.leg_count() > 0);
        Json_dom *parent= seek_exact_path(dom, path.begin(), path.end() - 1);
        if (parent == nullptr)
          return enum_json_diff_status::REJECTED;
        const Json_path_leg *last_leg= path.last_leg();
        if (parent->json_type() == enum_json_type::J_OBJECT &&
            last_leg->get_type() == jpl_member)
        {
          auto obj= down_cast<Json_object*>(parent);
          if (obj->get(last_leg->get_member_name()) != nullptr)
            return enum_json_diff_status::REJECTED;
          if (obj->add_alias(last_leg->get_member_name(), val.clone_dom(thd)))
            return enum_json_diff_status::ERROR; /* purecov: inspected */
          continue;
        }
        if (parent->json_type() == enum_json_type::J_ARRAY &&
            last_leg->get_type() == jpl_array_cell)
        {
          auto array= down_cast<Json_array*>(parent);
          Json_array_index idx= last_leg->first_array_index(array->size());
          if (array->insert_alias(idx.position(), val.clone_dom(thd)))
            return enum_json_diff_status::ERROR; /* purecov: inspected */
          continue;
        }
        return enum_json_diff_status::REJECTED;
      }
    case enum_json_diff_operation::REMOVE:
      {
        DBUG_ASSERT(path.leg_count() > 0);
        Json_dom *parent= seek_exact_path(dom, path.begin(), path.end() - 1);
        if (parent == nullptr)
          return enum_json_diff_status::REJECTED;
        const Json_path_leg *last_leg= path.last_leg();
        if (parent->json_type() == enum_json_type::J_OBJECT)
        {
          auto object= down_cast<Json_object*>(parent);
          if (last_leg->get_type() != jpl_member ||
              !object->remove(last_leg->get_member_name()))
            return enum_json_diff_status::REJECTED;
        }
        else if (parent->json_type() == enum_json_type::J_ARRAY)
        {
          if (last_leg->get_type() != jpl_array_cell)
            return enum_json_diff_status::REJECTED;
          auto array= down_cast<Json_array*>(parent);
          Json_array_index idx= last_leg->first_array_index(array->size());
          if (!idx.within_bounds() || !array->remove(idx.position()))
            return enum_json_diff_status::REJECTED;
        }
        else
        {
          return enum_json_diff_status::REJECTED;
        }
        continue;
      }
    }

    DBUG_ASSERT(false);                         /* purecov: deadcode */
  }

  if (field->store_json(&doc) != TYPE_OK)
    return enum_json_diff_status::ERROR;        /* purecov: inspected */

  return enum_json_diff_status::SUCCESS;
}
