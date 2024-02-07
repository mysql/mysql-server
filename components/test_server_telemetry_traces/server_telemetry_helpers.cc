/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "server_telemetry_helpers.h"

static const char *query_attribute_return_charset = "utf8mb4";

/**
 *  Return JSON encoding of a selected set of query attributes attached to
 *  a given THD.
 *
 *  @param thd thread session handle
 *  @param filter set of allowed query attribute names (others ignored), allow
 *all if filter empty
 *  @param[out] outJson JSON output string
 *  @param log file logger
 *  @retval false  success
 *  @retval true   failure
 **/
bool query_attrs_to_json(MYSQL_THD thd, const std::set<std::string> &filter,
                         std::string &outJson, FileLogger &log) {
  mysqlh_query_attributes_iterator iter = nullptr;
  if (qa_iterator_srv->create(thd, nullptr, &iter)) {
    log.write(
        " query_attrs_to_json: failed to create query attribute iterator\n");
    return true;
  }
  bool error = true;
  my_h_string h_str_name = nullptr;
  my_h_string h_str_val = nullptr;

  while (true) {
    assert(h_str_name == nullptr);
    assert(h_str_val == nullptr);

    bool is_null_val = true;
    if (qa_isnull_srv->get(iter, &is_null_val)) {
      log.write(
          " query_attrs_to_json: failed to check is_null for a query "
          "attribute\n");
      goto end;
    }
    if (is_null_val) {
      if (qa_iterator_srv->next(iter)) {
        // no more data
        break;
      }
      continue;
    }
    if (qa_iterator_srv->get_name(iter, &h_str_name)) {
      log.write(
          " query_attrs_to_json: failed to get query attribute string name\n");
      goto end;
    }
    char qa_name[1024];
    if (string_converter_srv->convert_to_buffer(
            h_str_name, qa_name, sizeof(qa_name),
            query_attribute_return_charset)) {
      log.write(" query_attrs_to_json: failed to convert name string\n");
      goto end;
    }
    if (h_str_name) {
      string_factory_srv->destroy(h_str_name);
      h_str_name = nullptr;
    }

    // only fetch names within "filter" set input
    if (!filter.empty() && filter.find(qa_name) == filter.end()) {
      if (qa_iterator_srv->next(iter)) {
        // no more data
        break;
      }
      continue;
    }

    if (qa_string_srv->get(iter, &h_str_val)) {
      log.write(
          " query_attrs_to_json: failed to get query attribute string value\n");
      goto end;
    }
    char qa_value[1024];
    if (string_converter_srv->convert_to_buffer(
            h_str_val, qa_value, sizeof(qa_value),
            query_attribute_return_charset)) {
      log.write(" query_attrs_to_json: failed to convert value string\n");
      goto end;
    }
    if (h_str_val) {
      string_factory_srv->destroy(h_str_val);
      h_str_val = nullptr;
    }

    if (!outJson.empty()) outJson += ", ";
    outJson += "\"";
    outJson += qa_name;
    outJson += "\": \"";
    outJson += qa_value;
    outJson += "\"";

    if (qa_iterator_srv->next(iter)) {
      // no more data
      break;
    }
  }

  // finalize the JSON string
  if (!outJson.empty()) {
    outJson += "}";
    outJson.insert(0, "{");
  }

  error = false;
end:
  if (iter) qa_iterator_srv->release(iter);
  if (h_str_name) string_factory_srv->destroy(h_str_name);
  if (h_str_val) string_factory_srv->destroy(h_str_val);

  return error;
}

/**
 *  Read a value as string of a single query attribute attached to given THD.
 *
 *  @param thd thread session handle
 *  @param name query attribute name
 *  @param[out] value query attribute value
 *  @param log file logger
 *  @retval false  success
 *  @retval true   failure
 **/
bool query_attr_read(MYSQL_THD thd, const char *name, std::string &value,
                     FileLogger &log) {
  mysqlh_query_attributes_iterator iter = nullptr;
  if (qa_iterator_srv->create(thd, name, &iter)) {
    log.write(" query_attr_read: failed to find query attribute '%s'\n", name);
    return true;
  }
  bool error = true;
  my_h_string h_str_val = nullptr;

  bool is_null_val = true;
  if (qa_isnull_srv->get(iter, &is_null_val)) {
    log.write(
        " query_attr_read: failed to check is_null for a query "
        "attribute\n");
    goto end;
  }
  if (is_null_val) goto end;

  if (qa_string_srv->get(iter, &h_str_val)) {
    log.write(" query_attr_read: failed to get query attribute string value\n");
    goto end;
  }
  char qa_value[1024];
  if (string_converter_srv->convert_to_buffer(h_str_val, qa_value,
                                              sizeof(qa_value),
                                              query_attribute_return_charset)) {
    log.write(" query_attr_read: failed to convert value string\n");
    goto end;
  }

  value = qa_value;
  error = false;
end:
  if (iter) qa_iterator_srv->release(iter);
  if (h_str_val) string_factory_srv->destroy(h_str_val);

  return error;
}

/**
 * Split the semi-colon delimited string into a set of strings (tags).
 *
 *  @param value input string to be parsed
 *  @param[out] output resulting set of strings
 **/
void parse_tags(const char *value, std::set<std::string> &output) {
  output.clear();
  std::istringstream f(value);
  std::string s;
  while (getline(f, s, ';')) {
    output.insert(s);
  }
}

/**
 * Get current query text for a given session THD
 * (buffer must be big enough to store the result).
 *
 *  @param thd THD
 *  @param[out] query buffer to receive the query text
 *  @param len length of the output buffer
 *
 *  @retval false  success
 *  @retval true   failure
 **/
bool get_query(MYSQL_THD thd, char *query, size_t len) {
  my_h_string str;
  if (thd_attributes_srv->get(thd, "sql_text",
                              reinterpret_cast<void *>(&str))) {
    return true;
  }
  static CHARSET_INFO_h ci = charset_srv->get_utf8mb4();
  charset_converter_srv->convert_to_buffer(str, query, len, ci);
  string_factory_srv->destroy(str);
  return false;
}

/**
 * Get host or IP of a client associated with a given session THD
 * (buffer must be big enough to store the result).
 *
 *  @param thd THD
 *  @param[out] host buffer to receive the host name
 *  @param len length of the output buffer
 *
 *  @retval false  success
 *  @retval true   failure
 **/
bool get_host_or_ip(MYSQL_THD thd, char *host, size_t len) {
  my_h_string str;
  if (thd_attributes_srv->get(thd, "host_or_ip",
                              reinterpret_cast<void *>(&str))) {
    return true;
  }
  static CHARSET_INFO_h ci = charset_srv->get_utf8mb4();
  charset_converter_srv->convert_to_buffer(str, host, len, ci);
  string_factory_srv->destroy(str);
  return false;
}

/**
 * Get database name in use by a client associated with a given session THD
 * (buffer must be big enough to store the result).
 *
 *  @param thd THD
 *  @param[out] schema buffer to receive the database name
 *  @param len length of the output buffer
 *
 *  @retval false  success
 *  @retval true   failure
 **/
bool get_schema(MYSQL_THD thd, char *schema, size_t len) {
  my_h_string str;
  if (thd_attributes_srv->get(thd, "schema", reinterpret_cast<void *>(&str))) {
    return true;
  }
  static CHARSET_INFO_h ci = charset_srv->get_utf8mb4();
  charset_converter_srv->convert_to_buffer(str, schema, len, ci);
  string_factory_srv->destroy(str);
  return false;
}

/**
 * Get user name of a client associated with a given session THD
 * (buffer must be big enough to store the result).
 *
 *  @param thd THD
 *  @param[out] user struct to receive the user name
 *
 *  @retval false  success
 *  @retval true   failure
 **/
bool get_user(MYSQL_THD thd, MYSQL_LEX_CSTRING &user) {
  Security_context_handle ctx = nullptr;
  if (!thd_scx_srv->get(thd, &ctx) && ctx) {
    if (!scx_options_srv->get(ctx, "user", &user)) {
      return false;
    }
  }
  return true;
}
