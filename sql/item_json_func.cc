/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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


/* JSON Function items used by mysql */

#include "item_json_func.h"

#include <memory>               // std::auto_ptr

#include "derror.h"             // ER_THD
#include "mysqld.h"             // key_memory_JSON, current_thd
#include "sql_class.h"          // THD
#include "sql_exception_handler.h"              // handle_std_exception
#include "item_cmpfunc.h"       // Item_func_like
#include "json_dom.h"
#include "json_path.h"
#include "prealloced_array.h"   // Prealloced_array
#include "sql_time.h"
#include "sql_alloc.h"
#include "template_utils.h"     // down_cast

#include <boost/variant.hpp>                    // boost::variant
#include <boost/variant/polymorphic_get.hpp>    // boost::polymorphic_get

// Used by the Json_path_cache
#define JPC_UNINITIALIZED -1

/** Helper routines */

// see the contract for this function in item_json_func.h
bool ensure_utf8mb4(String *val, String *buf,
                    const char **resptr, size_t *reslength, bool require_string)
{
  const CHARSET_INFO *cs= val->charset();

  if (cs == &my_charset_bin)
  {
    if (require_string)
      my_error(ER_INVALID_JSON_CHARSET, MYF(0), my_charset_bin.csname);
    return true;
  }

  const char *s= val->ptr();
  size_t ss= val->length();

  if (my_charset_same(cs, &my_charset_utf8mb4_bin) ||
      my_charset_same(cs, &my_charset_utf8_bin) ||
      !std::strcmp(cs->csname, "ascii"))
  {
    /*
      Character data is directly converted to JSON if the character
      set is utf8mb4 or a subset.
    */
  }
  else
  { // If not, we convert, possibly with loss (best effort).
    uint dummy_errors;
    if (buf->copy(val->ptr(), val->length(), val->charset(),
                  &my_charset_utf8mb4_bin, &dummy_errors))
    {
      return true;                            /* purecov: inspected */
    }
    buf->set_charset(&my_charset_utf8mb4_bin);
    s= buf->ptr();
    ss= buf->length();
  }

  *resptr= s;
  *reslength= ss;
  return false;
}

/**
  Parse a JSON dom out of an argument to a JSON function.

  @param[in]  res          Pointer to string value of arg.
  @param[in]  arg_idx      0-based index of corresponding JSON function argument
  @param[in]  func_name    Name of the user-invoked JSON_ function
  @param[in,out] dom       If non-null, we want any text parsed DOM
                           returned at the location pointed to
  @param[in]  require_str_or_json
                           If true, generate an error if other types used
                           as input
  @param[out] parse_error  set to true if the parser was run and found an error
                           else false
  @param[in]  preserve_neg_zero_int
                           Whether integer negative zero should be preserved.
                           If set to TRUE, -0 is handled as a DOUBLE. Double
                           negative zero (-0.0) is preserved regardless of what
                           this parameter is set to.

  @returns false if the arg parsed as valid JSON, true otherwise
*/
static bool parse_json(String *res,
                       uint arg_idx,
                       const char *func_name,
                       Json_dom **dom,
                       bool require_str_or_json,
                       bool *parse_error,
                       bool preserve_neg_zero_int= false)
{
  char buff[MAX_FIELD_WIDTH];
  String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);

  const char *safep;         // contents of res, possibly converted
  size_t safe_length;        // length of safep

  *parse_error= false;

  if (ensure_utf8mb4(res, &utf8_res, &safep, &safe_length,
                      require_str_or_json))
  {
    return true;
  }

  if (!dom)
  {
    DBUG_ASSERT(!require_str_or_json);
    return !is_valid_json_syntax(safep, safe_length);
  }

  const char *parse_err;
  size_t err_offset;
  *dom= Json_dom::parse(safep, safe_length, &parse_err, &err_offset,
                        preserve_neg_zero_int);

  if (*dom == NULL && parse_err != NULL)
  {
    /*
      Report syntax error. The last argument is no longer used, but kept to
      avoid changing error message format.
    */
    my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0),
             arg_idx + 1, func_name, parse_err, err_offset,
             "");
    *parse_error= true;
  }
  return *dom == NULL;
}


/**
  Get the field type of an item. This function returns the same value
  as arg->field_type() in most cases, but in some cases it may return
  another field type in order to ensure that the item gets handled the
  same way as items of a different type.
*/
static enum_field_types get_normalized_field_type(Item *arg)
{
  enum_field_types ft= arg->field_type();
  switch (ft)
  {
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
    /*
      TINYTEXT, TEXT, MEDIUMTEXT, and LONGTEXT have type
      MYSQL_TYPE_BLOB. We want to treat them like strings. We check
      the collation to see if the blob is really a string.
    */
    if (arg->collation.collation != &my_charset_bin)
      return MYSQL_TYPE_STRING;
    break;
  case MYSQL_TYPE_VARCHAR:
    /*
      If arg represents a parameter to a prepared statement, its field
      type will be MYSQL_TYPE_VARCHAR instead of the actual type of
      the parameter. The item type will have the info, so adjust
      field_type to match.

      If arg is a bit-field literal (such as b'1010'), its field type
      will be MYSQL_TYPE_VARCHAR. Adjust it to MYSQL_TYPE_BIT to match
      the type of BIT fields.
    */
    switch (arg->type())
    {
    case Item::NULL_ITEM:
      return MYSQL_TYPE_NULL;
    case Item::INT_ITEM:
      return MYSQL_TYPE_LONGLONG;
    case Item::REAL_ITEM:
      return MYSQL_TYPE_DOUBLE;
    case Item::DECIMAL_ITEM:
      return MYSQL_TYPE_NEWDECIMAL;
    case Item::VARBIN_ITEM:
      return MYSQL_TYPE_BIT;
    default:
      break;
    }
  default:
    break;
  }
  return ft;
}


bool get_json_string(Item *arg_item,
                     String *value,
                     String *utf8_res,
                     const char *func_name,
                     const char **safep,
                     size_t *safe_length)
{
  String *const res= arg_item->val_str(value);

  if (!res)
  {
    return true;
  }

  if (ensure_utf8mb4(res, utf8_res, safep, safe_length,
                      true))
  {
    return true;
  }

  return false;
}

/**
  Helper method for Item_func_json_* methods. Check if a JSON item or
  JSON text is valid and, for the latter, optionally construct a DOM
  tree (i.e. only if valid).

  @param[in]     args       Item_func::args alias
  @param[in]     arg_idx    Index (0-based) of argument into the args array
  @param[out]    value      Item_func_json_*::m_value alias
  @param[in]     func_name  Name of the user-invoked JSON_ function
  @param[in,out] dom        If non-null, we want any text parsed DOM
                            returned at the location pointed to
  @param[in]     require_str_or_json
                            If true, generate an error if other types used
                            as input
  @param[out]    valid      true if a valid JSON value was found (or NULL),
                            else false
  @param[in]     preserve_neg_zero_int
                            Whether integer negative zero should be preserved.
                            If set to TRUE, -0 is handled as a DOUBLE. Double
                            negative zero (-0.0) is preserved regardless of what
                            this parameter is set to.

  @returns true iff syntax error *and* dom != null, else false
*/
static bool json_is_valid(Item **args,
                          uint arg_idx,
                          String *value,
                          const char *func_name,
                          Json_dom **dom,
                          bool require_str_or_json,
                          bool *valid,
                          bool preserve_neg_zero_int= false)
{
  Item *const arg_item= args[arg_idx];

  switch (get_normalized_field_type(arg_item))
  {
  case MYSQL_TYPE_NULL:
    arg_item->update_null_value();
    DBUG_ASSERT(arg_item->null_value);
    *valid= true;
    return false;
  case MYSQL_TYPE_JSON:
  {
    Json_wrapper w;
    // Also sets the null_value flag
    *valid= !arg_item->val_json(&w);
    return !*valid;
  }
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_TINY_BLOB:
    {
      String *const res= arg_item->val_str(value);
      if (arg_item->type() == Item::FIELD_ITEM)
      {
        Item_field *fi= down_cast<Item_field *>(arg_item);
        Field *field= fi->field;
        if (field->flags & (ENUM_FLAG | SET_FLAG))
        {
          *valid= false;
          return false;
        }
      }

      if (arg_item->null_value)
      {
        *valid= true;
        return false;
      }

      bool parse_error= false;
      const bool failure= parse_json(res, arg_idx, func_name,
                                     dom, require_str_or_json,
                                     &parse_error, preserve_neg_zero_int);
      *valid= !failure;
      return parse_error;
    }
  default:
    if (require_str_or_json)
    {
      *valid= false;
      my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), arg_idx + 1, func_name);
      return true;
    }

    *valid= false;
    return false;
  }
}


/**
  Helper method for Item_func_json_* methods. Assumes that the caller
  has already verified that the path expression is not null. Raises an
  error if the path expression is syntactically incorrect. Raises an
  error if the path expression contains wildcard tokens but is not
  supposed to. Otherwise updates the supplied Json_path object with
  the parsed path.

  @param[in]  path_expression  A string Item to be interpreted as a path.
  @param[out] value            Holder for path string
  @param[in]  forbid_wildcards True if the path shouldn't contain * or **
  @param[out] json_path        The object that will hold the parsed path

  @returns false on success, true on error or if the path is NULL
*/
static bool parse_path(Item * path_expression, String *value,
                       bool forbid_wildcards, Json_path *json_path)
{
  String *path_value= path_expression->val_str(value);

  if (!path_value)
  {
    return true;
  }

  const char * path_chars= path_value->ptr();
  size_t path_length= path_value->length();
  char buff[STRING_BUFFER_USUAL_SIZE];
  String res(buff, sizeof(buff), &my_charset_utf8mb4_bin);

  if (ensure_utf8mb4(path_value, &res, &path_chars, &path_length, true))
  {
    return true;
  }

  // OK, we have a string encoded in utf-8. Does it parse?
  size_t bad_idx= 0;
  if (parse_path(false, path_length, path_chars, json_path, &bad_idx))
  {
    /*
      Issue an error message. The last argument is no longer used, but kept to
      avoid changing error message format.
    */
    my_error(ER_INVALID_JSON_PATH, MYF(0), bad_idx, "");
    return true;
  }

  if (forbid_wildcards && json_path->contains_wildcard_or_ellipsis())
  {
    my_error(ER_INVALID_JSON_PATH_WILDCARD, MYF(0));
    return true;
  }

  return false;
}


/**
  Parse a oneOrAll argument.

  @param[in]  candidate   The string to compare to "one" or "all"
  @param[in]  func_name   The name of the calling function

  @returns ooa_one, ooa_all, or ooa_error, based on the match
*/
enum_one_or_all_type parse_one_or_all(const char *candidate,
                                      const char *func_name)
{
  if (!my_strcasecmp(&my_charset_utf8mb4_general_ci, candidate, "all"))
    return ooa_all;

  if (!my_strcasecmp(&my_charset_utf8mb4_general_ci, candidate, "one"))
    return ooa_one;

  my_error(ER_JSON_BAD_ONE_OR_ALL_ARG, MYF(0), func_name);
  return ooa_error;
}

/**
  Parse and cache a (possibly constant) oneOrAll argument.

  @param[in]  arg           The oneOrAll arg passed to the JSON function.
  @param[in]  string_value  String variable to use for parsing.
  @param[in]  cached_ooa    Previous result of parsing this arg.
  @param[in]  func_name     The name of the calling JSON function.

  @returns ooa_one, ooa_all, ooa_null or ooa_error, based on the match
*/
enum_one_or_all_type parse_and_cache_ooa(Item *arg,
                                         String *string_value,
                                         enum_one_or_all_type *cached_ooa,
                                         const char *func_name)
{
  bool is_constant= arg->const_during_execution();

  if (is_constant)
  {
    if (*cached_ooa != ooa_uninitialized)
    {
      return *cached_ooa;
    }
  }

  String *const one_or_all= arg->val_str(string_value);
  if (!one_or_all || arg->null_value)
  {
    *cached_ooa= ooa_null;
  }
  else
  {
    *cached_ooa= parse_one_or_all(one_or_all->c_ptr_safe(), func_name);
  }

  return *cached_ooa;
}

/** Json_path_cache */

Json_path_cache::Json_path_cache(THD *thd, uint size)
  : m_paths(key_memory_JSON),
    m_arg_idx_to_vector_idx(thd->mem_root, size),
    m_arg_idx_to_problem_indicator(thd->mem_root, size),
    m_size(size)
{
  reset_cache();
}


Json_path_cache::~Json_path_cache()
{}


bool Json_path_cache::parse_and_cache_path(Item ** args, uint arg_idx,
                                           bool forbid_wildcards)
{
  Item *arg= args[arg_idx];

  bool is_constant= arg->const_during_execution();
  int vector_idx= m_arg_idx_to_vector_idx[arg_idx];

  if (is_constant)
  {
    // nothing to do if it has already been parsed
    if (vector_idx >= 0)
    {
      if (m_arg_idx_to_problem_indicator[vector_idx])
      {
        return true;
      }

      return false;
    }
  }

  DBUG_ASSERT((vector_idx == JPC_UNINITIALIZED) || (vector_idx >= 0));

  if (vector_idx == JPC_UNINITIALIZED)
  {
    vector_idx= (int) m_paths.size();
    if (m_paths.push_back(Json_path()))
      return true;                            /* purecov: inspected */
    m_arg_idx_to_vector_idx[arg_idx]= vector_idx;
  }
  else
  {
    // re-parsing a non-constant path for the next row
    m_paths[vector_idx].clear();
  }

  if (parse_path(arg, &m_path_value, forbid_wildcards, &m_paths[vector_idx]))
  {
    // oops, parsing failed

    if (is_constant)
    {
      // remember that we had a problem
      m_arg_idx_to_problem_indicator[vector_idx]= true;
    }

    return true;
  }

  return false;
}


Json_path *Json_path_cache::get_path(uint arg_idx)
{
  int vector_idx= m_arg_idx_to_vector_idx[arg_idx];

  if ((vector_idx < 0) || m_arg_idx_to_problem_indicator[vector_idx])
  {
    return NULL;
  }

  return &(m_paths.at(vector_idx));
}


void Json_path_cache::reset_cache()
{
  for (uint arg_idx= 0; arg_idx < m_size; arg_idx++)
  {
    m_arg_idx_to_vector_idx[arg_idx]= JPC_UNINITIALIZED;
    m_arg_idx_to_problem_indicator[arg_idx] = false;
  }

  m_paths.clear();
}


/** JSON_*() support methods */

void Item_json_func::cleanup()
{
  Item_func::cleanup();

  m_path_cache.reset_cache();
}


type_conversion_status
Item_json_func::save_in_field_inner(Field *field, bool no_conversions)
{
  return save_possibly_as_json(field, no_conversions);
}


longlong Item_func_json_valid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  try
  {
    bool ok;
    if (json_is_valid(args, 0, &m_value, func_name(), NULL, false, &ok))
    {
      return error_int();
    }

    null_value= args[0]->null_value;

    if (null_value || !ok)
      return 0;

    return 1;
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
}


/// Base class for predicates that compare elements in a JSON array.
class Array_comparator
{
  const Json_wrapper &m_wrapper;
protected:
  Array_comparator(const Json_wrapper &wrapper) : m_wrapper(wrapper) {}
  int cmp(size_t idx1, size_t idx2) const
  {
    return m_wrapper[idx1].compare(m_wrapper[idx2]);
  }
};

/// Predicate that checks if one array element is less than another.
struct Array_less : public Array_comparator
{
  Array_less(const Json_wrapper &wrapper) : Array_comparator(wrapper) {}
  bool operator() (size_t idx1, size_t idx2) const
  {
    return cmp(idx1, idx2) < 0;
  }
};

/// Predicate that checks if two array elements are equal.
struct Array_equal : public Array_comparator
{
  Array_equal(const Json_wrapper &wrapper) : Array_comparator(wrapper) {}
  bool operator() (size_t idx1, size_t idx2) const
  {
    return cmp(idx1, idx2) == 0;
  }
};

typedef Prealloced_array<size_t, 16> Sorted_index_array;

/**
  Sort the elements of a JSON array and remove duplicates.

  @param[in]  orig  the original JSON array
  @param[out] v     vector that will be filled with the indexes of the array
                    elements in increasing order
  @return false on success, true on error
*/
static bool sort_array(const Json_wrapper &orig, Sorted_index_array *v)
{
  if (v->reserve(orig.length()))
    return true;                              /* purecov: inspected */

  for (size_t i=0; i < orig.length(); i++)
    v->push_back(i);

  // Sort the array...
  std::sort(v->begin(), v->end(), Array_less(orig));
  // ... and remove duplicates.
  v->erase(std::unique(v->begin(), v->end(), Array_equal(orig)), v->end());

  return false;
}


/**
  Check if one Json_wrapper contains all the elements of another
  Json_wrapper.

  @param[in]  doc_wrapper   the containing document
  @param[in]  containee_wr  the possibly contained document
  @param[out] result        true if doc_wrapper contains containee_wr,
                            false otherwise
  @retval false on success
  @retval true on failure
*/
static bool contains_wr(const Json_wrapper &doc_wrapper,
                        const Json_wrapper &containee_wr,
                        bool *result)
{
  if (doc_wrapper.type() == Json_dom::J_OBJECT)
  {
    if (containee_wr.type() != Json_dom::J_OBJECT)
    {
      *result= false;
      return false;
    }

    Json_wrapper_object_iterator d_oi= doc_wrapper.object_iterator();
    Json_wrapper_object_iterator c_oi= containee_wr.object_iterator();
    Json_key_comparator cmp;

    while (!c_oi.empty() && !d_oi.empty())
    {
      for(; !d_oi.empty() && cmp(d_oi.elt().first, c_oi.elt().first);
          d_oi.next()) {}

      if (d_oi.empty() || cmp(c_oi.elt().first, d_oi.elt().first))
      {
        *result= false;
        return false;
      }

      // key is the same, now compare values
      if (contains_wr(d_oi.elt().second, c_oi.elt().second, result))
        return true;                          /* purecov: inspected */
      if (!*result)
      {
        // Value didn't match, give up.
        return false;
      }
      c_oi.next();
    }
    *result= c_oi.empty(); // must be exhausted
    return false;
  }

  if (doc_wrapper.type() == Json_dom::J_ARRAY)
  {
    const Json_wrapper *wr= &containee_wr;
    Json_wrapper a_wr;

    if (containee_wr.type() != Json_dom::J_ARRAY)
    {
      // auto-wrap scalar or object in an array for uniform treatment later
      Json_wrapper scalar= containee_wr;
      Json_array *array_dom= new (std::nothrow) Json_array();
      if (!array_dom || array_dom->append_clone(scalar.to_dom()))
      {
        delete array_dom;                       /* purecov: inspected */
        return true;                            /* purecov: inspected */
      }
      Json_wrapper nw(array_dom);
      a_wr.steal(&nw);
      wr= &a_wr;
    }

    // Indirection vectors containing the original indices
    Sorted_index_array d(key_memory_JSON);
    Sorted_index_array c(key_memory_JSON);

    // Sort both vectors, so we can compare efficiently
    if (sort_array(doc_wrapper, &d) || sort_array(*wr, &c))
      return true;                              /* purecov: inspected */

    size_t doc_i= 0;

    for (size_t c_i= 0; c_i < c.size(); c_i++)
    {
      Json_dom::enum_json_type candt= (*wr)[c[c_i]].type();

      if (candt == Json_dom::J_ARRAY)
      {
        while (doc_i < d.size() &&
               doc_wrapper[d[doc_i]].type() < candt)
        {
          doc_i++;
        }

        bool found= false;
        /*
          We do not increase doc_i here, use a tmp. We might need to check again
          against doc_i: this allows duplicates in the candidate.
        */
        for (size_t tmp= doc_i;
             tmp < d.size() && doc_wrapper[d[tmp]].type() == Json_dom::J_ARRAY;
             tmp++)
        {
          if (contains_wr(doc_wrapper[d[tmp]], (*wr)[c[c_i]], result))
            return true;                      /* purecov: inspected */
          if (*result)
          {
            found= true;
            break;
          }
        }

        if (!found)
        {
          *result= false;
          return false;
        }
      }
      else
      {
        bool found= false;
        size_t tmp= doc_i;

        while (tmp < d.size())
        {
          if (doc_wrapper[d[tmp]].type() == Json_dom::J_ARRAY ||
              doc_wrapper[d[tmp]].type() == Json_dom::J_OBJECT)
          {
            if (contains_wr(doc_wrapper[d[tmp]], (*wr)[c[c_i]], result))
              return true;                    /* purecov: inspected */
            if (*result)
            {
              found= true;
              break;
            }
          }
          else if (doc_wrapper[d[tmp]].compare((*wr)[c[c_i]]) == 0)
          {
            found= true;
            break;
          }
          tmp++;
        }

        if (doc_i == d.size() || !found)
        {
          *result= false;
          return false;
        }
      }
    }

    *result= true;
    return false;
  }

  *result= (doc_wrapper.compare(containee_wr) == 0);
  return false;
}


void Item_func_json_contains::cleanup()
{
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
}


longlong Item_func_json_contains::val_int()
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper doc_wrapper;

    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &doc_wrapper) ||
        args[0]->null_value)
    {
      null_value= true;
      return 0;
    }

    Json_wrapper containee_wr;

    // arg 1 is the possible containee
    if (get_json_wrapper(args, 1, &m_doc_value, func_name(), &containee_wr) ||
        args[1]->null_value)
    {
      null_value= true;
      return 0;
    }

    if (arg_count == 3)
    {
      // path is specified
      if (m_path_cache.parse_and_cache_path(args, 2, true))
      {
        null_value= true;
        return 0;
      }
      Json_path *path= m_path_cache.get_path(2);

      Json_wrapper_vector v(key_memory_JSON);
      if (doc_wrapper.seek(*path, &v, true, false))
        return error_int();                 /* purecov: inspected */

      if (v.size() == 0)
      {
        null_value= true;
        return 0;
      }

      bool ret;
      if (contains_wr(v[0], containee_wr, &ret))
        return error_int();                /* purecov: inspected */
      null_value= false;
      return ret;
    }
    else
    {
      bool ret;
      if (contains_wr(doc_wrapper, containee_wr, &ret))
        return error_int();                /* purecov: inspected */
      null_value= false;
      return ret;
    }
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }
}


void Item_func_json_contains_path::cleanup()
{
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
  m_cached_ooa= ooa_uninitialized;
}

longlong Item_func_json_contains_path::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong result= 0;
  null_value= false;

  Json_wrapper wrapper;
  Json_wrapper_vector hits(key_memory_JSON);

  try
  {
    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value)
    {
      null_value= true;
      return 0;
    }

    // arg 1 is the oneOrAll flag
    bool require_all;
    switch (parse_and_cache_ooa(args[1], &m_one_or_all_value,
                                &m_cached_ooa, func_name()))
    {
    case ooa_all:
      {
        require_all= true;
        break;
      }
    case ooa_one:
      {
        require_all= false;
        break;
      }
    case ooa_null:
      {
        null_value= true;
        return 0;
      }
    default:
      {
        return error_int();
      }
    }

    // the remaining args are paths
    for (uint32 i= 2; i < arg_count; ++i)
    {
      if (m_path_cache.parse_and_cache_path(args, i, false))
      {
        null_value= true;
        return 0;
      }
      Json_path *path= m_path_cache.get_path(i);

      hits.clear();
      if (wrapper.seek(*path, &hits, true, true))
        return error_int();               /* purecov: inspected */
      if (hits.size() > 0)
      {
        result= 1;
        if (!require_all)
        {
          break;
        }
      }
      else
      {
        if (require_all)
        {
          result= 0;
          break;
        }
      }
    }

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  return result;
}


bool json_value(Item **args, uint arg_idx, Json_wrapper *result)
{
  Item *arg= args[arg_idx];

  if (arg->field_type() == MYSQL_TYPE_NULL)
  {
    arg->update_null_value();
    DBUG_ASSERT(arg->null_value);
    return false;
  }

  if (arg->field_type() != MYSQL_TYPE_JSON)
  {
    // This is not a JSON value. Give up.
    return true;
  }

  return arg->val_json(result);
}


bool get_json_wrapper(Item **args,
                      uint arg_idx,
                      String *str,
                      const char *func_name,
                      Json_wrapper *wrapper,
                      bool preserve_neg_zero_int)
{
  if (!json_value(args, arg_idx, wrapper))
  {
    // Found a JSON value, return successfully.
    return false;
  }

  if (args[arg_idx]->field_type() == MYSQL_TYPE_JSON)
  {
    /*
      If the type of the argument is JSON and json_value() returned
      false, it means the argument didn't contain valid JSON data.
      Give up.
    */
    return true;
  }

  /*
    Otherwise, it's a non-JSON type, so we need to see if we can
    convert it to JSON.
  */

  /* Is this a JSON text? */
  Json_dom *dom; //@< we'll receive a DOM here from a successful text parse

  bool valid;
  if (json_is_valid(args, arg_idx, str, func_name, &dom, true, &valid,
                     preserve_neg_zero_int))
    return true;

  if (!valid)
  {
    my_error(ER_INVALID_TYPE_FOR_JSON, MYF(0), arg_idx + 1, func_name);
    return true;
  }

  if (args[arg_idx]->null_value)
  {
    return false;
  }

  DBUG_ASSERT(dom);

  *wrapper= Json_wrapper(dom);
  return false;

}

/**
   Compute an index into json_type_string_map
   to be applied to certain sub-types of J_OPAQUE.

   @param field_type[in] The refined field type of the opaque value.

   @return an index into json_type_string_map
*/
static uint opaque_index(enum_field_types field_type)
{
  uint offset= 0;

  switch (field_type)
  {
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
    {
      offset= static_cast<uint>(Json_dom::J_OPAQUE_BLOB);
      break;
    }

  case MYSQL_TYPE_BIT:
    {
      offset= static_cast<uint>(Json_dom::J_OPAQUE_BIT);
      break;
    }

  case MYSQL_TYPE_GEOMETRY:
    {
      /**
        Should not get here. This path should be orphaned by the
        work done on implicit CASTing of geometry values to geojson
        objects. However, that work was done late in the project
        cycle for WL#7909. Do something sensible in case we missed
        something.

        FIXME.
      */
      /* purecov: begin deadcode */
      DBUG_ABORT();
      offset= static_cast<uint>(Json_dom::J_OPAQUE_GEOMETRY);
      break;
      /* purecov: end */
    }

  default:
    {
      return static_cast<uint>(Json_dom::J_OPAQUE);
    }
  }

  return 1 + static_cast<uint>(Json_dom::J_ERROR) + offset;
}

String *Item_func_json_type::val_str(String*)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, &m_value, func_name(), &wr) ||
        args[0]->null_value)
    {
      null_value= true;
      return NULL;
    }

    const Json_dom::enum_json_type type= wr.type();
    uint typename_idx= static_cast<uint>(type);
    if (type == Json_dom::J_OPAQUE)
    {
      typename_idx= opaque_index(wr.field_type());
    }

    m_value.length(0);
    if (m_value.append(Json_dom::json_type_string_map[typename_idx]))
      return error_str();                     /* purecov: inspected */

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }

  null_value= false;
  return &m_value;
}


String *Item_json_func::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  Json_wrapper wr;
  if (val_json(&wr))
    return error_str();

  if (null_value)
    return NULL;

  m_string_buffer.length(0);

  if (wr.to_string(&m_string_buffer, true, func_name()))
    return error_str();

  null_value= false;
  return &m_string_buffer;
}


bool Item_json_func::get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate)
{
  Json_wrapper wr;
  if (val_json(&wr))
    return true;

  if (null_value)
    return true;

  return wr.coerce_date(ltime, fuzzydate, func_name());
}


bool Item_json_func::get_time(MYSQL_TIME *ltime)
{
  Json_wrapper wr;
  if (val_json(&wr))
    return true;

  if (null_value)
    return true;

  return wr.coerce_time(ltime, func_name());
}


longlong Item_json_func::val_int()
{
  Json_wrapper wr;
  if (val_json(&wr))
    return 0;

  if (null_value)
    return 0;

  return wr.coerce_int(func_name());
}


double Item_json_func::val_real()
{
  Json_wrapper wr;
  if (val_json(&wr))
    return 0.0;

  if (null_value)
    return 0.0;

  return wr.coerce_real(func_name());
}

my_decimal *Item_json_func::val_decimal(my_decimal *decimal_value)
{
  Json_wrapper wr;
  if (val_json(&wr))
  {
    my_decimal_set_zero(decimal_value);
    return decimal_value;
  }
  if (null_value)
  {
    my_decimal_set_zero(decimal_value);
    return decimal_value;
  }
  return wr.coerce_decimal(decimal_value, func_name());
}


/**
  Type that is capable of holding objects of any sub-type of
  Json_scalar. Used for pre-allocating space in query-duration memory
  for JSON scalars that are to be returned by get_json_atom_wrapper().

  Note: boost::blank is included in the variant to ensure that it
  includes a type that is known to be nothrow default-constructible.
  The presence of such a type avoids heap allocation when assigning a
  new value to the variant. Look for the "never-empty" guarantee in
  the Boost documentation for details.
*/
struct Json_scalar_holder : public Sql_alloc
{
  boost::variant<boost::blank, Json_string, Json_decimal, Json_int, Json_uint,
                 Json_double, Json_boolean, Json_null, Json_datetime,
                 Json_opaque> m_val;
};


/**
  Get a JSON value from a function, field or subselect scalar.

  @param[in]     arg         the function argument
  @param[in]     calling_function the name of the calling function
  @param[in,out] value       a scratch area
  @param[in,out] tmp         temporary scratch space for converting strings to
                             the correct charset; only used if accept_string is
                             true and conversion is needed
  @param[out]    wr          the retrieved JSON value
  @param[in,out] scalar      pointer to pre-allocated memory that can be
                             borrowed by the result wrapper to hold the scalar
                             result. If the pointer is NULL, memory will be
                             allocated on the heap.
  @param[in]     accept_string
                             if true, accept SQL strings as scalars
                             (false implies we need a valid
                              JSON parsable string)
  @return false if we could get a value or NULL, otherwise true
*/
bool val_json_func_field_subselect(Item* arg,
                                   const char *calling_function,
                                   String *value,
                                   String *tmp,
                                   Json_wrapper *wr,
                                   Json_scalar_holder *scalar,
                                   bool accept_string)
{
  enum_field_types field_type= get_normalized_field_type(arg);
  Json_dom *dom= NULL;

  switch (field_type)
  {
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_LONGLONG:
    {
      longlong i= arg->val_int();

      if (arg->null_value)
        return false;

      if (arg->unsigned_flag)
      {
        if (scalar)
        {
          scalar->m_val= Json_uint(i);
        }
        else
        {
          dom= new (std::nothrow) Json_uint(i);
          if (!dom)
            return true;                       /* purecov: inspected */
        }
      }
      else if (scalar)
      {
        scalar->m_val= Json_int(i);
      }
      else
      {
        dom= new (std::nothrow) Json_int(i);
        if (!dom)
          return true;                         /* purecov: inspected */
      }

      break;
    }
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_TIME:
    {
      longlong dt= arg->val_temporal_by_field_type();

      if (arg->null_value)
        return false;

      MYSQL_TIME t;
      TIME_from_longlong_datetime_packed(&t, dt);
      t.time_type= field_type_to_timestamp_type(field_type);
      if (scalar)
      {
        scalar->m_val= Json_datetime(t, field_type);
      }
      else
      {
        dom= new (std::nothrow) Json_datetime(t, field_type);
        if (!dom)
          return true;                         /* purecov: inspected */
      }
      break;
    }
  case MYSQL_TYPE_NEWDECIMAL:
    {
      my_decimal m;
      my_decimal *r= arg->val_decimal(&m);

      if (arg->null_value)
        return false;

      if (!r)
      {
        my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
        return true;
      }

      if (scalar)
      {
        scalar->m_val= Json_decimal(*r);
      }
      else
      {
        dom= new (std::nothrow) Json_decimal(*r);
        if (!dom)
          return true;                         /* purecov: inspected */
      }
      break;
    }
  case MYSQL_TYPE_DOUBLE:
  case MYSQL_TYPE_FLOAT:
    {
      double d= arg->val_real();

      if (arg->null_value)
        return false;

      if (scalar)
      {
        scalar->m_val= Json_double(d);
      }
      else
      {
        dom= new (std::nothrow) Json_double(d);
        if (!dom)
          return true;                         /* purecov: inspected */
      }
      break;
    }
  case MYSQL_TYPE_GEOMETRY:
    {
      uint32 geometry_srid;
      bool retval= geometry_to_json(wr, arg, calling_function, INT_MAX32,
                                    false, false, false, &geometry_srid);

      /**
        Scalar processing is irrelevant. Geometry types are converted
        to JSON objects.
      */
      return retval;
    }
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_BIT:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_YEAR:
    {
      String *oo= arg->val_str(value);

      if (arg->null_value)
        return false;

      if (arg->type() == Item::FIELD_ITEM && field_type == MYSQL_TYPE_BLOB)
      {
        Item_field *it_f= down_cast<Item_field *>(arg);
        Field *f= it_f->field;
        Field_blob *fb= down_cast<Field_blob *>(f);
        switch (fb->pack_length() - portable_sizeof_char_ptr)
        {
        case 1:
          field_type= MYSQL_TYPE_TINY_BLOB;
          break;
        case 2:
          field_type= MYSQL_TYPE_BLOB;
          break;
        case 3:
          field_type= MYSQL_TYPE_MEDIUM_BLOB;
          break;
        case 4:
          field_type= MYSQL_TYPE_LONG_BLOB;
          break;
        default:
          DBUG_ABORT();
        }
      }

      if (scalar)
      {
        scalar->m_val= Json_opaque(field_type, oo->ptr(), oo->length());
      }
      else
      {
        dom= new (std::nothrow) Json_opaque(field_type,
                                            oo->ptr(), oo->length());
        if (!dom)
          return true;                         /* purecov: inspected */
      }
      break;
    }
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_STRING:
    {
      /*
        Wrong charset or Json syntax error (the latter: only if !accept_string,
        in which case a binary character set is our only hope for success).
      */
      String *res= arg->val_str(value);

      if (arg->null_value)
        return false;
      const CHARSET_INFO *cs= res->charset();

      if (cs == &my_charset_bin)
      {
        // BINARY or similar
        if (scalar)
        {
          scalar->m_val= Json_opaque(field_type, res->ptr(), res->length());
        }
        else
        {
          dom= new (std::nothrow) Json_opaque(field_type,
                                              res->ptr(), res->length());
          if (!dom)
            return true;                       /* purecov: inspected */
        }
        break;
      }
      else if (accept_string)
      {
        const char *s= res->ptr();
        size_t ss= res->length();

        if (ensure_utf8mb4(res, tmp, &s, &ss, true))
        {
          return true;
        }

        if (scalar)
        {
          scalar->m_val= Json_string(std::string(s, ss));
        }
        else
        {
          dom= new (std::nothrow) Json_string(std::string(s, ss));
          if (!dom)
            return true;                       /* purecov: inspected */
        }
      }
      else
      {
        my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
        return true;
      }
      break;
    }
  case MYSQL_TYPE_DECIMAL:                      // pre 5.0
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "old decimal type");
    return true;

  case MYSQL_TYPE_NULL:
    /*
      This shouldn't happen, since the only caller of this function
      returns earlier if it sees that the type is MYSQL_TYPE_NULL.
    */
    /* purecov: begin inspected */
    arg->update_null_value();
    DBUG_ASSERT(arg->null_value);
    return false;
    /* purecov: end */

  case MYSQL_TYPE_JSON:
    DBUG_ABORT();                               /* purecov: inspected */
    // fall-through
  default:
    my_error(ER_INVALID_CAST_TO_JSON, MYF(0));
    return true;
  }

  // Exactly one of scalar and dom should be used.
  DBUG_ASSERT((scalar == NULL) != (dom == NULL));
  DBUG_ASSERT((scalar == NULL) ||
              (get_json_scalar_from_holder(scalar) != NULL));

  Json_wrapper w(scalar ? get_json_scalar_from_holder(scalar) : dom);
  if (scalar)
  {
    /*
      The DOM object lives in memory owned by the caller. Tell the
      wrapper that it's not the owner.
    */
    w.set_alias();
  }
  wr->steal(&w);

  return false;
}


/**
  Try to determine whether an argument has a boolean (as opposed
  to an int) type, and if so, return its boolean value.

  @param[in] arg The argument to inspect.
  @param[in/out] result Fill in the result if this is a boolean arg.

  @return True if the arg can be determined to have a boolean type.
*/
bool extract_boolean(Item *arg, bool *result)
{
  if (arg->is_bool_func())
  {
    *result= arg->val_int();
    return true;
  }

  if (arg->type() == Item::SUBSELECT_ITEM)
  {
    // EXISTS, IN, ALL, ANY subqueries have boolean type
    Item_subselect *subs= down_cast<Item_subselect *>(arg);
    switch (subs->substype())
    {
    case Item_subselect::EXISTS_SUBS:
    case Item_subselect::IN_SUBS:
    case Item_subselect::ALL_SUBS:
    case Item_subselect::ANY_SUBS:
      *result= arg->val_int();
      return true;
    default:
      break;
    }
  }

  if (arg->type() == Item::INT_ITEM)
  {
    const Name_string * const name= &arg->item_name;
    const bool is_literal_false= name->is_set() && name->eq("FALSE");
    const bool is_literal_true= name->is_set() && name->eq("TRUE");
    if (is_literal_false || is_literal_true)
    {
      *result= is_literal_true;
      return true;
    }
  }

  // doesn't fit any of the checks we perform
  return false;
}

// see the contract for this function in item_json_func.h
bool get_json_atom_wrapper(Item **args,
                           uint arg_idx,
                           const char *calling_function,
                           String *value,
                           String *tmp,
                           Json_wrapper *wr,
                           Json_scalar_holder *scalar,
                           bool accept_string)
{
  bool result= false;

  Item * const arg= args[arg_idx];

  try
  {
    if (!json_value(args, arg_idx, wr))
    {
      return false;
    }

    if (arg->field_type() == MYSQL_TYPE_JSON)
    {
      /*
        If the type of the argument is JSON and json_value() returned
        false, it means the argument didn't contain valid JSON data.
        Give up.
      */
      return true;
    }

    // boolean operators should produce boolean values
    bool  boolean_value;
    if (extract_boolean(arg, &boolean_value))
    {
      Json_dom *boolean_dom;
      if (scalar)
      {
        scalar->m_val= Json_boolean(boolean_value);
        boolean_dom= get_json_scalar_from_holder(scalar);
      }
      else
      {
        boolean_dom= new (std::nothrow) Json_boolean(boolean_value);
        if (!boolean_dom)
          return true;                         /* purecov: inspected */
      }
      Json_wrapper wrapper(boolean_dom);
      if (scalar)
      {
        /*
          The DOM object lives in memory owned by the caller. Tell the
          wrapper that it's not the owner.
        */
        wrapper.set_alias();
      }
      wr->steal(&wrapper);
      return false;
    }

    /*
      Allow other types as first-class or opaque JSON values.
      But how to determine what the type is? We do a best effort...
    */
    result= val_json_func_field_subselect(arg, calling_function, value, tmp, wr,
                                          scalar, accept_string);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(calling_function);
    return true;
    /* purecov: end */
  }

  return result;
}


bool get_atom_null_as_null(Item **args, uint arg_idx,
                           const char *calling_function, String *value,
                           String *tmp, Json_wrapper *wr)
{
  if (get_json_atom_wrapper(args, arg_idx, calling_function, value,
                            tmp, wr, NULL, true))
    return true;

  if (args[arg_idx]->null_value)
  {
    Json_wrapper null_wrapper(new (std::nothrow) Json_null());
    wr->steal(&null_wrapper);
  }

  return false;
}


bool Item_json_typecast::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  Json_dom *dom= NULL;       //@< if non-null we want a DOM from parse

  if (args[0]->field_type() == MYSQL_TYPE_NULL)
  {
    null_value= true;
    return false;
  }

  if (args[0]->field_type() == MYSQL_TYPE_JSON)
  {
    if (json_value(args, 0, wr))
      return error_json();

    null_value= args[0]->null_value;
    return false;
  }

  bool valid;
  if (json_is_valid(args, 0, &m_value, func_name(),
                    &dom, false, &valid))
    return error_json();

  if (valid)
  {
    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }
    // We were able to parse a JSON value from a string.
    DBUG_ASSERT(dom);
    // Pass on the DOM wrapped
    Json_wrapper w(dom);
    wr->steal(&w);
    null_value= false;
    return false;
  }

  // Not a non-binary string, nor a JSON value, wrap the rest

  if (get_json_atom_wrapper(args, 0, func_name(), &m_value,
                            &m_conversion_buffer,
                            wr, NULL, true))
    return error_json();

  null_value= args[0]->null_value;
  return false;
}


void Item_json_typecast::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as "));
  str->append(cast_type());
  str->append(')');
}


void Item_func_json_length::cleanup()
{
  Item_int_func::cleanup();

  m_path_cache.reset_cache();
}


longlong Item_func_json_length::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong result= 0;

  Json_wrapper wrapper;

  try
  {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value)
    {
      null_value= true;
      return 0;
    }
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  if (arg_count > 1)
  {
    if (m_path_cache.parse_and_cache_path(args, 1, true))
    {
      null_value= true;
      return 0;
    }
    Json_path *json_path= m_path_cache.get_path(1);

    Json_wrapper_vector hits(key_memory_JSON);
    if (wrapper.seek(*json_path, &hits, true, true))
      return error_int();                 /* purecov: inspected */

    if (hits.size() != 1)
    {
      // path does not exist. return null.
      null_value= true;
      return 0;
    }

    // there should only be one hit because wildcards were forbidden
    DBUG_ASSERT(hits.size() == 1);

    wrapper.steal(&hits[0]);
  }

  result= wrapper.length();

  null_value= false;
  return result;
}


longlong Item_func_json_depth::val_int()
{
  DBUG_ASSERT(fixed == 1);
  longlong result= 0;

  Json_wrapper wrapper;

  try
  {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper) ||
        args[0]->null_value)
    {
      null_value= true;
      return 0;
    }
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_int();
    /* purecov: end */
  }

  result= wrapper.depth();

  null_value= false;
  return result;
}


bool Item_func_json_keys::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  Json_wrapper wrapper;

  try
  {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_json();
    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    if (arg_count > 1)
    {
      if (m_path_cache.parse_and_cache_path(args, 1, true))
      {
        null_value= true;
        return false;
      }
      Json_path *path= m_path_cache.get_path(1);

      Json_wrapper_vector hits(key_memory_JSON);
      if (wrapper.seek(*path, &hits, false, true))
        return error_json();              /* purecov: inspected */

      if (hits.size() != 1)
      {
        null_value= true;
        return false;
      }

      wrapper.steal(&hits[0]);
    }

    if (wrapper.type() != Json_dom::J_OBJECT)
    {
      null_value= true;
      return false;
    }

    // We have located a JSON object value, now collect its keys
    // and return them as a JSON array.
    Json_array *res= new (std::nothrow) Json_array();
    if (!res)
      return error_json();                /* purecov: inspected */
    for (Json_wrapper_object_iterator i(wrapper.object_iterator());
         !i.empty(); i.next())
    {
      if (res->append_alias(new (std::nothrow) Json_string(i.elt().first)))
      {
        delete res;                             /* purecov: inspected */
        return error_json();              /* purecov: inspected */
      }
    }
    Json_wrapper resw(res);
    wr->steal(&resw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}


bool Item_func_json_extract::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper w;

    // multiple paths means multiple possible matches
    bool could_return_multiple_matches= (arg_count > 2);

    // collect results here
    Json_wrapper_vector v(key_memory_JSON);

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &w))
      return error_json();

    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint32 i= 1; i < arg_count; ++i)
    {
      if (m_path_cache.parse_and_cache_path(args, i, false))
      {
        null_value= true;
        return false;
      }
      Json_path *path= m_path_cache.get_path(i);

      if (path->contains_wildcard_or_ellipsis())
      {
        could_return_multiple_matches= true;
      }

      if (w.seek(*path, &v, true, false))
        return error_json();              /* purecov: inspected */
    }

    if (v.size() == 0)
    {
      null_value= true;
      return false;
    }
    else if (could_return_multiple_matches)
    {
      Json_array *a= new (std::nothrow) Json_array();
      if (!a)
        return error_json();              /* purecov: inspected */
      for (Json_wrapper_vector::iterator it= v.begin(); it != v.end(); ++it)
      {
        if (a->append_clone(it->to_dom()))
        {
          delete a;                             /* purecov: inspected */
          return error_json();            /* purecov: inspected */
        }
      }
      Json_wrapper w(a);
      wr->steal(&w);
    }
    else // one path, no ellipsis or wildcard
    {
      // there should only be one match
      DBUG_ASSERT(v.size() == 1);
      wr->steal(&v[0]);
    }
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}

/**
  If there is no parent in v, we must have a path that specifified either
  - the root ('$'), or
  - an array cell at index 0 that any non-array element at the top level could
    have been autowrapped to (since we got a hit), i.e. '$[0]' or
    $[0][0]...[0]'.

  @param[in] path the specified path which gave a match
  @param[in] v    the JSON item matched
  @return true if v is a top level item
*/
static inline bool wrapped_top_level_item(Json_path *path, Json_dom *v)
{
  if (v->parent())
    return false;

#ifndef DBUG_OFF
  for (size_t i= 0; i < path->leg_count(); i++)
  {
    DBUG_ASSERT(path->get_leg_at(i)->get_type() == jpl_array_cell);
    DBUG_ASSERT(path->get_leg_at(i)->get_array_cell_index() == 0);
  }
#endif

  return true;
}


bool Item_func_json_array_append::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();
    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint32 i= 1; i < arg_count; i += 2)
    {
      // Need a DOM to be able to manipulate arrays
      Json_dom *doc= docw.to_dom();
      if (!doc)
        return error_json();              /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true))
      {
        // empty path (error signalled already)
        null_value= true;
        return false;
      }
      Json_path *path= m_path_cache.get_path(i);

      Json_dom_vector hits(key_memory_JSON);
      if (doc->seek(*path, &hits, true, true))
        return error_json();                  /* purecov: inspected */

      if (hits.size() < 1)
      {
        continue;
      }

      /*
        Iterate backwards lest we get into trouble with replacing outer
        parts of the doc before we get to paths to inner parts when we have
        ellipses in the path. Make sure we do the most nested replacements
        first. Json_dom::seek returns outermost hits first.

        Note that, later on, we decide to forbid ellipses in the path
        arguments to json_array_append().
      */
      for (Json_dom_vector::iterator it= hits.end(); it != hits.begin();)
      {
        --it;
        Json_wrapper valuew;
        if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                  &m_conversion_buffer,
                                  &valuew))
          return error_json();

        if ((*it)->json_type() == Json_dom::J_ARRAY)
        {
          Json_array *arr= down_cast<Json_array *>(*it);
          if (arr->append_alias(valuew.to_dom()))
            return error_json();   /* purecov: inspected */
          valuew.set_alias(); // we have taken over the DOM
        }
        else
        {
          Json_array *arr= new (std::nothrow) Json_array();
          if (!arr ||
              arr->append_clone(*it) ||
              arr->append_alias(valuew.to_dom()))
          {
            delete arr;                         /* purecov: inspected */
            return error_json();          /* purecov: inspected */
          }
          valuew.set_alias(); // we have taken over the DOM
          /*
            This value will replace the old document we found using path, since
            we did an auto-wrap. If this is root, this is trivial, but if it's
            inside an array or object, we need to find the parent DOM to be
            able to replace it in situ.
          */
          if (wrapped_top_level_item(path, (*it)))
          {
            Json_wrapper newroot(arr);
            docw.steal(&newroot);
          }
          else
          {
            Json_dom *parent= (*it)->parent();
            parent->replace_dom_in_container(*it, arr);
          }
        }
      }
    }

    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}


bool Item_func_json_insert::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if(args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint32 i= 1; i < arg_count; i += 2)
    {
      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc= docw.to_dom();
      if (!doc)
        return error_json();              /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true))
      {
        // empty path (error signalled already)
        null_value= true;
        return false;
      }
      Json_path *current_path= m_path_cache.get_path(i);

      /**
        Clone the path so that we won't mess up the cached version
        when we pop the trailing leg below.
      */
      m_path.set(current_path);

      {
        Json_dom_vector hits(key_memory_JSON);
        if (doc->seek(m_path, &hits, false, true))
          return error_json();                /* purecov: inspected */

        if (hits.size() != 0 || // already exists
            m_path.leg_count() == 0) // is root
        {
          continue;
        }
      }

      /*
        Need to look one step up the path: if we are specifying an array slot
        we need to find the array. If we are specifying an object element, we
        need to find the object. In both cases so we can insert into them.

        Remove the first path leg and search again.
      */
      Json_dom_vector hits(key_memory_JSON);
      const Json_path_leg *leg= m_path.pop();
      if (doc->seek(m_path, &hits, false, true))
        return error_json();                  /* purecov: inspected */

      if (hits.size() < 1)
      {
        // no unique object found at parent position, so bail out
        continue;
      }

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer,
                                &valuew))
      {
        return error_json();
      }

      /*
        Iterate backwards lest we get into trouble with replacing outer
        parts of the doc before we get to paths to inner parts when we have
        ellipses in the path. Make sure we do the most nested replacements
        first. Json_dom::seek returns outermost hits first.

        Note that, later on, we decided to forbid ellipses in the path
        arguments to json_insert().
      */
      for (Json_dom_vector::iterator it= hits.end(); it != hits.begin();)
      {
        --it;
        // We found *something* at that parent path

        // What did we specify in the path, object or array?
        if (leg->get_type() == jpl_array_cell)
        {
          // We specified an array, what did we find at that position?
          if ((*it)->json_type() == Json_dom::J_ARRAY)
          {
            Json_array *arr= down_cast<Json_array *>(*it);
            DBUG_ASSERT(leg->get_type() == jpl_array_cell);
            if (arr->insert_clone(leg->get_array_cell_index(), valuew.to_dom()))
              return error_json();        /* purecov: inspected */
          }
          else if (leg->get_array_cell_index() > 0)
          {
            /*
              Found a scalar or object and we didn't specify position 0:
              auto-wrap it
            */
            Json_dom *a= *it;
            Json_array *newarr= new (std::nothrow) Json_array();
            if (!newarr ||
                newarr->append_clone(a) /* auto-wrap this */ ||
                newarr->insert_clone(leg->get_array_cell_index(),
                                     valuew.to_dom()))
            {
              delete newarr;                    /* purecov: inspected */
              return error_json();        /* purecov: inspected */
            }

            /*
              Now we need this value to replace the old document we found using
              path. If this is root, this is trivial, but if it's inside an
              array or object, we need to find the parent DOM to be able to
              replace it in situ.
            */
            if (m_path.leg_count() == 0) // root
            {
              Json_wrapper newroot(newarr);
              docw.steal(&newroot);
            } else
            {
              Json_dom *parent= a->parent();
              DBUG_ASSERT(parent);

              parent->replace_dom_in_container(a, newarr);
            }
          }
        }
        else if (leg->get_type() == jpl_member &&
                 (*it)->json_type() == Json_dom::J_OBJECT)
        {
          Json_object *o= down_cast<Json_object *>(*it);
          const char *ename= leg->get_member_name();
          size_t enames= leg->get_member_name_length();
          if (o->add_clone(std::string(ename, enames), valuew.to_dom()))
            return error_json();          /* purecov: inspected */
        }
      }

    } // end of loop through paths
    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}

bool Item_func_json_array_insert::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint32 i= 1; i < arg_count; i+= 2)
    {
      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc= docw.to_dom();
      if (!doc)
        return error_json();              /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true))
      {
        // empty path (error signalled already)
        null_value= true;
        return false;
      }
      Json_path *current_path= m_path_cache.get_path(i);

      /**
        Clone the path so that we won't mess up the cached version
        when we pop the trailing leg below.
      */
      m_path.set(current_path);

      // the path must end in a cell identifier
      size_t leg_count= m_path.leg_count();
      if ((leg_count == 0) ||
          (m_path.get_leg_at(leg_count - 1)->get_type() != jpl_array_cell))
      {
        my_error(ER_INVALID_JSON_PATH_ARRAY_CELL, MYF(0));
        return error_json();
      }

      /*
        Need to look one step up the path: we need to find the array.

        Remove the last path leg and search again.
      */
      Json_dom_vector hits(key_memory_JSON);
      const Json_path_leg *leg= m_path.pop();
      if (doc->seek(m_path, &hits, false, true))
        return error_json();                  /* purecov: inspected */

      if (hits.empty())
      {
        // no unique object found at parent position, so bail out
        continue;
      }

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(),
                                &m_value, &m_conversion_buffer,
                                &valuew))
      {
        return error_json();
      }

      /*
        Iterate backwards lest we get into trouble with replacing outer
        parts of the doc before we get to paths to inner parts when we have
        ellipses in the path. Make sure we do the most nested replacements
        first. Json_dom::seek returns outermost hits first.

        Note that, later on, we decided to forbid ellipses in the path
        arguments to json_insert().
      */
      for (Json_dom_vector::iterator it= hits.end(); it != hits.begin();)
      {
        --it;
        // We found *something* at that parent path

        // NOP if parent is not an array

        if ((*it)->json_type() == Json_dom::J_ARRAY)
        {
          // Excellent. Insert the value at that location.
          Json_array *arr= down_cast<Json_array *>(*it);
          DBUG_ASSERT(leg->get_type() == jpl_array_cell);
          if (arr->insert_clone(leg->get_array_cell_index(), valuew.to_dom()))
            return error_json();        /* purecov: inspected */
        }
      }

    } // end of loop through paths
    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}


/**
  Clone a source path to a target path, stripping out [0] legs
  which are made redundant by the auto-wrapping rule
  in the WL#7909 spec:

  "If a pathExpression identifies a non-array value,
  then pathExpression[ 0 ] evaluates to the same value
  as pathExpression."

  @param[in]      source_path The original path.
  @param[in,out]  target_path The clone to be filled in.
  @param[in]      doc The document to seek through.

  @returns True if an error occurred. False otherwise.
*/
static bool clone_without_autowrapping(Json_path *source_path,
                                       Json_path_clone *target_path,
                                       Json_dom *doc)
{
  Json_dom_vector hits(key_memory_JSON);

  target_path->clear();
  size_t leg_count= source_path->leg_count();
  for (size_t leg_idx= 0; leg_idx < leg_count; leg_idx++)
  {
    const Json_path_leg *path_leg= source_path->get_leg_at(leg_idx);
    if ((path_leg->get_type() == jpl_array_cell) &&
        (path_leg->get_array_cell_index() == 0))
    {
      /**
         We have a partial path of the form

         pathExpression[0]

         So see if pathExpression identifies a non-array value.
      */
      hits.clear();
      if (doc->seek(*target_path, &hits, false, true))
        return true;  /* purecov: inspected */

      if (!hits.empty())
      {
        Json_dom *candidate= hits.at(0);
        if (candidate->json_type() != Json_dom::J_ARRAY)
        {
          /**
            pathExpression identifies a non-array value.
            We satisfy the conditions of the rule above.
            So we can throw away the [0] leg.
          */
          continue;
        }
      }
    }
    // The rule above is NOT satisified. So add the leg.
    target_path->append(path_leg);
  }
  hits.clear();

  return false;
}


/**
  Common implementation for JSON_SET and JSON_REPLACE
*/
bool Item_func_json_set_replace::val_json(Json_wrapper *wr)
{
  try
  {
    Json_wrapper docw;

    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if(args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint32 i= 1; i < arg_count; i += 2)
    {
      // Need a DOM to be able to manipulate arrays and objects
      Json_dom *doc= docw.to_dom();
      if (!doc)
        return error_json();                    /* purecov: inspected */

      if (m_path_cache.parse_and_cache_path(args, i, true))
      {
        // empty path (error signalled already)
        null_value= true;
        return false;
      }
      Json_path *current_path= m_path_cache.get_path(i);

      /**
        Clone the path, stripping off redundant auto-wrapping.
      */
      if (clone_without_autowrapping(current_path, &m_path, doc))
      {
        return error_json();
      }

      Json_dom_vector hits(key_memory_JSON);
      if (doc->seek(m_path, &hits, false, true))
        return error_json();                  /* purecov: inspected */

      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i + 1, func_name(), &m_value,
                                &m_conversion_buffer,
                                &valuew))
        return error_json();

      if (hits.size() == 0)
      {
        /*
          Need to look one step up the path: if we are specifying an array slot
          we need to find the array. If we are specifying an object element, we
          need to find the object. In both cases so we can insert into them.

          Remove the first path leg and search again.
        */
        const Json_path_leg *leg= m_path.pop();
        if (doc->seek(m_path, &hits, false, true))
          return error_json();                /* purecov: inspected */

        if (hits.size() < 1)
        {
          // no unique object found at parent position, so bail out
          continue;
        }

        /*
          Iterate backwards lest we get into trouble with replacing outer
          parts of the doc before we get to paths to inner parts when we have
          ellipses in the path. Make sure we do the most nested replacements
          first. Json_dom::seek returns outermost hits first.
        */
        for (Json_dom_vector::iterator it= hits.end(); it != hits.begin();)
        {
          --it;
          // We now have either an array or an object in the parent's path
          if (leg->get_type() == jpl_array_cell)
          {
            if ((*it)->json_type() == Json_dom::J_ARRAY)
            {
              if (!m_json_set) // replace semantics, so skip if path not present
                continue;

              Json_array *arr= down_cast<Json_array *>(*it);
              DBUG_ASSERT(leg->get_type() == jpl_array_cell);
              if (arr->insert_clone(leg->get_array_cell_index(),
                                    valuew.to_dom()))
                return error_json();            /* purecov: inspected */
            }
            else
            {
              /*
                Found a scalar or object, auto-wrap it and make it the first
                element in a new array, unless the new value specifies position
                0, in which case the old gets replaced.
              */
              Json_dom *a= *it;
              Json_dom *res;

              if (leg->get_array_cell_index() == 0)
              {
                res= valuew.clone_dom();
                if (!res)
                  return error_json();          /* purecov: inspected */
              }
              else
              {
                // replace semantics, so we don't get larger array
                if (!m_json_set)
                  continue;

                Json_array *newarr= new (std::nothrow) Json_array();
                if (!newarr ||
                    newarr->append_clone(a) ||
                    newarr->insert_clone(leg->get_array_cell_index(),
                                         valuew.to_dom()))
                {
                  delete newarr;                /* purecov: inspected */
                  return error_json();          /* purecov: inspected */
                }
                res= newarr;
              }

              /*
                Now we need this value to replace the old document we found
                using path. If this is root, this is trivial, but if it's
                inside an array or object, we need to find the parent DOM to be
                able to replace it in situ.
              */
              if (m_path.leg_count() == 0) // root
              {
                Json_wrapper newroot(res);
                docw.steal(&newroot);
              } else
              {
                Json_dom *parent= a->parent();
                DBUG_ASSERT(parent);

                parent->replace_dom_in_container(a, res);
              }
            }
          }
          else if (leg->get_type() == jpl_member &&
                   (*it)->json_type() == Json_dom::J_OBJECT)
          {
            if (!m_json_set) // replace semantics, so skip if path not present
              continue;

            Json_object *o= down_cast<Json_object *>(*it);
            const char *ename= leg->get_member_name();
            size_t enames= leg->get_member_name_length();
            if (o->add_clone(std::string(ename, enames), valuew.to_dom()))
              return error_json();              /* purecov: inspected */
          }
        } // end of loop through hits

      }
      else
      {
        // we found one or more value, so replace semantics.
        for (Json_dom_vector::iterator it= hits.begin(); it != hits.end(); ++it)
        {
          Json_dom *child= *it;

          Json_dom *parent= child->parent();
          if (!parent)
          {
            Json_dom *dom= valuew.clone_dom();
            if (!dom)
              return error_json();              /* purecov: inspected */
            Json_wrapper w(dom);
            docw.steal(&w);
          }
          else
          {
            Json_dom *dom= valuew.clone_dom();
            if (!dom)
              return error_json();              /* purecov: inspected */
            parent->replace_dom_in_container(child, dom);
          }
        }
      } // if: found 1 else more values
    } // do: functions argument list run-though

    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}


bool Item_func_json_array::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_array *arr= new (std::nothrow) Json_array();
    if (!arr)
      return error_json();                /* purecov: inspected */
    Json_wrapper docw(arr);

    for (uint32 i= 0; i < arg_count; ++i)
    {
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, i, func_name(), &m_value,
                                &m_conversion_buffer,
                                &valuew))
      {
        return error_json();
      }

      if (arr->append_alias(valuew.to_dom()))
        return error_json();              /* purecov: inspected */
      valuew.set_alias(); // release the DOM
    }

    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}

bool Item_func_json_row_object::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  try
  {
    Json_object *object= new (std::nothrow) Json_object();
    if (!object)
      return error_json();                /* purecov: inspected */
    Json_wrapper docw(object);

    for (uint32 i= 0; i < arg_count; ++i)
    {
      /*
        arguments come in pairs. we have already verified that there
        are an even number of args.
      */
      uint32  key_idx= i++;
      uint32  value_idx= i;

      // key
      Item *key_item= args[key_idx];
      char buff[MAX_FIELD_WIDTH];
      String utf8_res(buff, sizeof(buff), &my_charset_utf8mb4_bin);
      const char *safep;         // contents of key_item, possibly converted
      size_t safe_length;        // length of safep

      if (get_json_string(key_item, &tmp_key_value, &utf8_res, func_name(),
                           &safep, &safe_length))
      {
        my_error(ER_JSON_DOCUMENT_NULL_KEY, MYF(0));
        return error_json();
      }

      std::string key(safep, safe_length);

      // value
      Json_wrapper valuew;
      if (get_atom_null_as_null(args, value_idx, func_name(), &m_value,
                                &m_conversion_buffer, &valuew))
      {
        return error_json();
      }

      if (object->add_alias(key, valuew.to_dom()))
        return error_json();              /* purecov: inspected */
      valuew.set_alias(); // release the DOM
    }

    // docw still owns the augmented doc, so hand it over to result
    wr->steal(&docw);

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  null_value= false;
  return false;
}


bool Item_func_json_search::fix_fields(THD *thd, Item **items)
{
  if (Item_json_func::fix_fields(thd, items))
    return true;

  // Fabricate a LIKE node

  m_source_string_item= new Item_string(&my_charset_utf8mb4_bin);
  Item_string *default_escape= new Item_string(&my_charset_utf8mb4_bin);
  if (m_source_string_item == NULL || default_escape == NULL)
    return true;                              /* purecov: inspected */

  Item *like_string_item= args[2];
  bool escape_initialized= false;

  // Get the escape character, if any
  if (arg_count > 3)
  {
    Item *orig_escape= args[3];

    /*
      Evaluate the escape clause. For a standalone LIKE expression,
      the escape clause only has to be constant during execution.
      However, we require a stronger condition: it must be constant.
      That means that we can evaluate the escape clause at resolution time
      and copy the results from the JSON_SEARCH() args into the arguments
      for the LIKE node which we're fabricating.
    */
    if (!orig_escape->const_item())
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "ESCAPE");
      return true;
    }

    String *escape_str= orig_escape->val_str(&m_escape);
    if (thd->is_error())
      return true;
    if (escape_str)
    {
      uint escape_length= static_cast<uint>(escape_str->length());
      default_escape->set_str_with_copy(escape_str->ptr(), escape_length);
      escape_initialized= true;
    }
  }

  if (!escape_initialized)
  {
    default_escape->set_str_with_copy("\\", 1);
  }

  m_like_node= new Item_func_like(m_source_string_item, like_string_item,
                                  default_escape, true);
  if (m_like_node == NULL)
    return true;                              /* purecov: inspected */

  Item *like_args[3];
  like_args[0]= m_source_string_item;
  like_args[1]= like_string_item;
  like_args[2]= default_escape;

  if (m_like_node->fix_fields(thd, like_args))
    return true;

  // resolving the LIKE node may overwrite its arguments
  Item **resolved_like_args= m_like_node->arguments();
  m_source_string_item= down_cast<Item_string *>(resolved_like_args[0]);

  return false;
}


void Item_func_json_search::cleanup()
{
  Item_json_func::cleanup();

  m_cached_ooa= ooa_uninitialized;
}

typedef Prealloced_array<std::string, 16, false> String_set;

/**
   Recursive function to find the string values, nested inside
   a json document, which satisfy the LIKE condition. As matches
   are found, their path locations are added to an evolving
   vector of matches.

   @param[in] subdoc A subdocument of the original document.
   @param[in] path The path location of the subdocument
   @param[in,out] matches The evolving vector of matches.
   @param[in,out] duplicates Set of paths found already.
   @param[in] one_match If true, then terminate search after first match.
   @param[in] like_node The LIKE node that's evaluated on the string values.
   @param[in] source_string The input string item of the LIKE node.
   @retval false on success
   @retval true on failure
*/
static bool find_matches(const Json_wrapper &wrapper, Json_path *path,
                         Json_dom_vector *matches, String_set *duplicates,
                         bool one_match, Item *like_node,
                         Item_string *source_string)
{
  switch (wrapper.type())
  {
  case Json_dom::J_STRING:
    {
      if (one_match && !matches->empty())
      {
        return false;
      }

      // Evaluate the LIKE node on the JSON string.
      const char *data= wrapper.get_data();
      uint len= static_cast<uint>(wrapper.get_data_length());
      source_string->set_str_with_copy(data, len);
      if (like_node->val_int())
      {
        // Got a match with the LIKE node. Save the path of the JSON string.
        char buff[STRING_BUFFER_USUAL_SIZE];
        String str(buff, sizeof(buff), &my_charset_utf8mb4_bin);
        str.length(0);
        if (path->to_string(&str))
          return true;                        /* purecov: inspected */

        std::string string_contents(str.ptr(), str.length());
        std::pair<String_set::iterator, bool> res=
          duplicates->insert_unique(string_contents);

        if (res.second)
        {
          Json_string *jstr= new (std::nothrow) Json_string(string_contents);
          if (!jstr || matches->push_back(jstr))
            return true;                      /* purecov: inspected */
        }
      }
      break;
    }

  case Json_dom::J_OBJECT:
    {
      for (Json_wrapper_object_iterator jwot(wrapper.object_iterator());
           !jwot.empty(); jwot.next())
      {
        std::pair<const std::string, Json_wrapper> pair= jwot.elt();
        const std::string key= pair.first;
        Json_wrapper value= pair.second;
        Json_path_leg next_leg(key);

        // recurse
        if (path->append(next_leg) ||
            find_matches(value, path, matches, duplicates, one_match,
                         like_node, source_string))
          return true;                        /* purecov: inspected */
        path->pop();

        if (one_match && !matches->empty())
        {
          return false;
        }
      }
      break;
    }

  case Json_dom::J_ARRAY:
    {
      for (size_t idx= 0; idx < wrapper.length(); idx++)
      {
        Json_wrapper value= wrapper[idx];
        Json_path_leg next_leg(idx);

        // recurse
        if (path->append(next_leg) ||
            find_matches(value, path, matches, duplicates, one_match,
                         like_node, source_string))
          return true;                        /* purecov: inspected */
        path->pop();

        if (one_match && !matches->empty())
        {
          return false;
        }
      }
      break;
    }

  default:
    {
      break;
    }
  } // end switch on wrapper type

  return false;
}

bool Item_func_json_search::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  Json_dom_vector matches(key_memory_JSON);

  try
  {
    String_set duplicates(key_memory_JSON);
    Json_wrapper docw;

    // arg 0 is the document
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &docw))
      return error_json();

    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    // arg 1 is the oneOrAll arg
    bool one_match;
    switch (parse_and_cache_ooa(args[1], &m_one_or_all_value,
                                &m_cached_ooa, func_name()))
    {
    case ooa_all:
      {
        one_match= false;
        break;
      }
    case ooa_one:
      {
        one_match= true;
        break;
      }
    case ooa_null:
      {
        null_value= true;
        return false;
      }
    default:
      {
        return error_json();
      }
    }

    // arg 2 is the search string

    // arg 3 is the optional escape character

    // the remaining arguments are path expressions
    if (arg_count < 5) // no user-supplied path expressions
    {
      Json_path path;
      if (find_matches(docw, &path, &matches, &duplicates, one_match,
                       m_like_node, m_source_string_item))
        return error_json();            /* purecov: inspected */
    }
    else  // user-supplied path expressions
    {
      Json_wrapper_vector hits(key_memory_JSON);

      // validate the user-supplied path expressions
      for (uint32 i= 4; i < arg_count; ++i)
      {
        if (m_path_cache.parse_and_cache_path(args, i, false))
        {
          null_value= true;
          return false;
        }
      }

      // find the matches for each of the user-supplied path expressions
      for (uint32 i= 4; i < arg_count; ++i)
      {
        if (one_match && (matches.size() > 0))
        {
          break;
        }

        Json_path *path= m_path_cache.get_path(i);

        /*
          If there are wildcards in the path, then we need to
          compute the full path to the subdocument. We can only
          do this on doms.
        */
        if (path->contains_wildcard_or_ellipsis())
        {
          Json_dom *dom= docw.to_dom();
          if (!dom)
            return error_json();          /* purecov: inspected */
          Json_dom_vector dom_hits(key_memory_JSON);

          if (dom->seek(*path, &dom_hits, false, false))
            return error_json();              /* purecov: inspected */

          for (Json_dom_vector::iterator jdvi= dom_hits.begin();
               jdvi != dom_hits.end(); ++jdvi)
          {
            if (one_match && (matches.size() > 0))
            {
              break;
            }

            Json_dom *subdocument= *jdvi;
            Json_path subdocument_path= subdocument->get_location();
            Json_wrapper subdocument_wrapper(subdocument);
            subdocument_wrapper.set_alias();

            if (find_matches(subdocument_wrapper, &subdocument_path,
                             &matches, &duplicates, one_match,
                             m_like_node, m_source_string_item))
              return error_json();   /* purecov: inspected */
          } // end of loop through hits
        }
        else // no wildcards in the path
        {
          hits.clear();
          if (docw.seek(*path, &hits, false, false))
            return error_json();          /* purecov: inspected */

          for (Json_wrapper_vector::iterator jwvi= hits.begin();
               jwvi != hits.end(); ++jwvi)
          {
            if (one_match && (matches.size() > 0))
            {
              break;
            }

            Json_wrapper  subdocument_wrapper= *jwvi;

            if (find_matches(subdocument_wrapper, path, &matches, &duplicates,
                             one_match, m_like_node, m_source_string_item))
              return error_json();   /* purecov: inspected */
          } // end of loop through hits
        }  // end if the user-supplied path expression has wildcards
      }   // end of loop through user-supplied path expressions
    }     // end if there are user-supplied path expressions

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  if (matches.size() == 0)
  {
    null_value= true;
    return false;
  }
  else if (matches.size() == 1)
  {
    Json_wrapper scalar_wrapper(matches[0]);
    wr->steal(&scalar_wrapper);
  }
  else
  {
    Json_array *array= new (std::nothrow) Json_array();
    if (!array)
      return error_json();                /* purecov: inspected */
    for (Json_dom_vector::iterator vsi= matches.begin();
         vsi != matches.end(); ++vsi)
    {
      if (array->append_alias(*vsi))
      {
        delete array;                           /* purecov: inspected */
        return error_json();              /* purecov: inspected */
      }
    }

    Json_wrapper  array_wrapper(array);
    wr->steal(&array_wrapper);
  }

  null_value= false;
  return false;
}


Item_func_json_remove::Item_func_json_remove(THD *thd, const POS &pos,
                                             PT_item_list *a)
  : Item_json_func(thd, pos, a)
{}


bool Item_func_json_remove::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  Json_wrapper wrapper;
  uint32  path_count= arg_count - 1;
  null_value= false;

  try
  {
    if (get_json_wrapper(args, 0, &m_doc_value, func_name(), &wrapper))
      return error_json();
    if (args[0]->null_value)
    {
      null_value= true;
      return false;
    }

    for (uint path_idx= 0; path_idx < path_count; ++path_idx)
    {
      if (m_path_cache.parse_and_cache_path(args, path_idx + 1, true))
      {
        null_value= true;
        return false;
      }
    }

  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_json();
    /* purecov: end */
  }

  for (uint path_idx= 0; path_idx < path_count; ++path_idx)
  {
    Json_path *path= m_path_cache.get_path(path_idx + 1);
    if (path->leg_count() == 0)
    {
      my_error(ER_JSON_VACUOUS_PATH, MYF(0));
      return error_json();
    }
  }

  // good document, good paths. do some work

  // no binary support for removal. must convert to a dom.
  Json_dom *dom= wrapper.to_dom();

  // remove elements identified by the paths, one after the other
  Json_dom_vector hits(key_memory_JSON);
  for (uint path_idx= 0; path_idx < path_count; ++path_idx)
  {
    Json_path *path= m_path_cache.get_path(path_idx + 1);
    hits.clear();

    // now find the matches
    if (dom->seek(*path, &hits, true, false))
      return error_json();                    /* purecov: inspected */

    // now remove matches
    for (Json_dom_vector::iterator it= hits.begin(); it != hits.end(); ++it)
    {
      Json_dom *child= *it;
      Json_dom *parent= child->parent();

      // no parent means the root. the path is nonsense.
      if (parent == NULL)
      {
        continue;
      }

      Json_dom::enum_json_type type= parent->json_type();
      DBUG_ASSERT((type == Json_dom::J_OBJECT) || (type == Json_dom::J_ARRAY));

      if (type == Json_dom::J_OBJECT)
      {
        Json_object *object= down_cast<Json_object *>(parent);
        object->remove(child);
      }
      else if (type == Json_dom::J_ARRAY)
      {
        Json_array *array= down_cast<Json_array *>(parent);
        array->remove(child);
      }
    } // end of loop through matches on current path
  }   // end of loop through all paths

  // wrapper still owns the pruned doc, so hand it over to result
  wr->steal(&wrapper);

  return false;
}


bool Item_func_json_merge_preserve::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed == 1);

  Json_dom *result_dom= NULL;
  bool had_error= false;
  null_value= false;

  try
  {
    for (uint idx= 0; idx < arg_count; idx++)
    {
      Json_wrapper next_wrapper;
      if (get_json_wrapper(args, idx, &m_value, func_name(), &next_wrapper))
      {
        had_error= true;
        break;
      }

      if (args[idx]->null_value)
      {
        null_value= true;
        break;
      }

      /*
        Grab the next DOM, release it from its wrapper, and merge it
        into the previous DOM.
      */
      Json_dom *next_dom= next_wrapper.to_dom();
      if (!next_dom)
      {
        delete result_dom;
        return error_json();              /* purecov: inspected */
      }
      next_wrapper.set_alias();
      result_dom= (idx == 0) ? next_dom : merge_doms(result_dom, next_dom);
    }
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    had_error= true;
    /* purecov: end */
  }

  if (had_error || null_value)
  {
    delete result_dom;
    return had_error ? error_json() : false;
  }

  // if we couldn't allocate memory, fail now
  if (!result_dom)
  {
    return error_json();              /* purecov: inspected */
  }

  // fake a wrapper so that we can hand its dom to the return arg
  Json_wrapper tmp(result_dom);
  wr->steal(&tmp);
  return false;
}


String *Item_func_json_quote::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);

  String *res= args[0]->val_str(str);
  if (!res)
  {
    null_value= true;
    return NULL;
  }

  try
  {
    const char *safep;
    size_t safep_size;

    switch (args[0]->field_type())
    {
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
      break;
    default:
      my_error(ER_INCORRECT_TYPE, MYF(0), "1", func_name());
      return error_str();
    }

    if (ensure_utf8mb4(res, &m_value, &safep, &safep_size, true))
    {
      null_value= true;
      return NULL;
    }

    /*
      One of the string buffers (str or m_value) is no longer in use
      and can be reused as the result buffer. Which of them it is,
      depends on whether or not ensure_utf8mb4() needed to do charset
      conversion. Make res point to the available buffer.
    */
    res= (str->ptr() == safep) ? &m_value : str;

    res->length(0);
    res->set_charset(&my_charset_utf8mb4_bin);
    if (double_quote(safep, safep_size, res))
      return error_str();                 /* purecov: inspected */
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }

  null_value= false;
  return res;
}


String *Item_func_json_unquote::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);

  Json_dom *dom= NULL;

  try
  {
    if (args[0]->field_type() == MYSQL_TYPE_JSON)
    {
      Json_wrapper wr;
      if (get_json_wrapper(args, 0, str, func_name(), &wr))
      {
        return error_str();
      }

      if (args[0]->null_value)
      {
        null_value= true;
        return NULL;
      }

      m_value.length(0);

      if (wr.to_string(&m_value, false, func_name()))
      {
        return error_str();
      }

      null_value= false;
      return &m_value;
    }


    String *res= args[0]->val_str(str);

    if (!res)
    {
      null_value= true;
      return NULL;
    }

    /*
      We only allow a string argument, so get rid of any other
      type arguments.
    */
    switch (args[0]->field_type())
    {
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_TINY_BLOB:
      break;
    default:
      my_error(ER_INCORRECT_TYPE, MYF(0), "1", func_name());
      return error_str();
    }

    if (res->length() < 2 || *res->ptr() != '"' ||
        res->ptr()[res->length() - 1] != '"')
    {
      null_value= false;
      return res; // return string unchanged
    }

    bool parse_error= false;
    if (parse_json(res, 0, func_name(), &dom, true, &parse_error))
    {
      return error_str();
    }

    /*
      Extract the internal string representation as a MySQL string
    */
    DBUG_ASSERT(dom->json_type() == Json_dom::J_STRING);
    Json_wrapper wr(dom);
    if (str->copy(wr.get_data(), wr.get_data_length(), collation.collation))
      return error_str();                     /* purecov: inspected */
  }
  catch (...)
  {
    /* purecov: begin inspected */
    handle_std_exception(func_name());
    return error_str();
    /* purecov: end */
  }


  null_value= false;
  return str;
}


Json_scalar_holder *create_json_scalar_holder()
{
  return new Json_scalar_holder();
}


void delete_json_scalar_holder(Json_scalar_holder *holder)
{
  delete holder;
}


Json_scalar *get_json_scalar_from_holder(Json_scalar_holder *holder)
{
  return boost::polymorphic_get<Json_scalar>(&holder->m_val);
}


String *Item_func_json_pretty::val_str(String *str)
{
  DBUG_ASSERT(fixed);
  try
  {
    Json_wrapper wr;
    if (get_json_wrapper(args, 0, str, func_name(), &wr))
      return error_str();

    null_value= args[0]->null_value;
    if (null_value)
      return NULL;

    str->length(0);
    if (wr.to_pretty_string(str, func_name()))
      return error_str();                       /* purecov: inspected */

    return str;
  }
  /* purecov: begin inspected */
  catch (...)
  {
    handle_std_exception(func_name());
    return error_str();
  }
  /* purecov: end */
}


longlong Item_func_json_storage_size::val_int()
{
  DBUG_ASSERT(fixed);

  /*
    If the input is a reference to a JSON column, return the actual storage
    size of the value in the table.
  */
  if (args[0]->type() == FIELD_ITEM && args[0]->field_type() == MYSQL_TYPE_JSON)
  {
    null_value= args[0]->is_null();
    if (null_value)
      return 0;
    return down_cast<Item_field*>(args[0])->field->data_length();
  }

  /*
    Otherwise, return the size required to store the argument if it were
    serialized to the binary JSON format.
  */
  Json_wrapper wrapper;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buffer;
  try
  {
    if (get_json_wrapper(args, 0, &buffer, func_name(), &wrapper))
      return error_int();
  }
  /* purecov: begin inspected */
  catch (...)
  {
    handle_std_exception(func_name());
    return error_int();
  }
  /* purecov: end */

  null_value= args[0]->null_value;
  if (null_value)
    return 0;

  if (wrapper.to_binary(&buffer))
    return error_int();                         /* purecov: inspected */
  return buffer.length();
}


bool Item_func_json_merge_patch::val_json(Json_wrapper *wr)
{
  DBUG_ASSERT(fixed);

  try
  {
    if (get_json_wrapper(args, 0, &m_value, func_name(), wr))
      return error_json();

    null_value= args[0]->null_value;

    Json_wrapper patch_wr;
    for (uint i= 1; i < arg_count; ++i)
    {
      if (get_json_wrapper(args, i, &m_value, func_name(), &patch_wr))
        return error_json();

      if (args[i]->null_value)
      {
        /*
          The patch is unknown, so the result so far is unknown. We
          cannot return NULL immediately, since a later patch can give
          a known result. This is because the result of a merge
          operation is the patch itself if the patch is not an object,
          regardless of what the target document is.
        */
        null_value= true;
        continue;
      }

      /*
        If a patch is not an object, the result of the merge operation
        is the patch itself. So just set the result to this patch and
        go on to the next patch.
      */
      if (patch_wr.type() != Json_dom::J_OBJECT)
      {
        wr->steal(&patch_wr);
        null_value= false;
        continue;
      }

      /*
        The target document is unknown, and we cannot tell the result
        from the patch alone when the patch is an object, so go on to
        the next patch.
      */
      if (null_value)
        continue;

      /*
        Get the DOM representation of the target document. It should
        be an object, and we will use an empty object if it is not.
      */
      std::auto_ptr<Json_object> target_dom;
      if (wr->type() == Json_dom::J_OBJECT)
      {
        target_dom.reset(down_cast<Json_object*>(wr->to_dom()));
        wr->set_alias();
      }
      else
      {
        target_dom.reset(new (std::nothrow) Json_object());
      }

      if (target_dom.get() == NULL)
        return error_json();                  /* purecov: inspected */

      // Get the DOM representation of the patch object.
      Json_object *patch_dom= down_cast<Json_object*>(patch_wr.to_dom());
      patch_wr.set_alias();    // target_dom will take over the ownership

      // Apply the patch on the target document.
      if (patch_dom == NULL || target_dom->merge_patch(patch_dom))
        return error_json();                  /* purecov: inspected */

      // Move the result of the merge operation into the result wrapper.
      Json_wrapper res(target_dom.release());
      wr->steal(&res);
      null_value= false;
    }

    return false;
  }
  /* purecov: begin inspected */
  catch (...)
  {
    handle_std_exception(func_name());
    return error_json();
  }
  /* purecov: end */
}
