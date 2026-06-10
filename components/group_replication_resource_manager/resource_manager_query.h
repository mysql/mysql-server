/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifndef RESOURCE_MANAGER_QUERY_INCLUDED
#define RESOURCE_MANAGER_QUERY_INCLUDED

#include <string>
#include <vector>

namespace gr_resource_manager {
typedef std::vector<std::string> Row;
typedef std::vector<Row> Result_Set;

struct Query {
  Query() {}

  Query(std::string query_title, std::string query_text)
      : title{std::move(query_title)}, query{std::move(query_text)} {}

  const std::string title;
  const std::string query;
};

/**
  Query processor
*/
class Query_Manager {
 public:
  Query_Manager() = default;

  ~Query_Manager() = default;
  Query_Manager(const Query_Manager &) = delete;
  Query_Manager &operator=(const Query_Manager &) = delete;
  Query_Manager(Query_Manager &&) = delete;
  Query_Manager &operator=(Query_Manager &&) = delete;

  static int run_query(const Query &query_info, Result_Set &result_set);
};

}  // namespace gr_resource_manager

#endif  // RESOURCE_MANAGER_QUERY_INCLUDED
