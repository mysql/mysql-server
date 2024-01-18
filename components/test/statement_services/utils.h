/* Copyright (c) 2023, 2024, Oracle and/or its affiliates. */
#include <mysql/components/services/mysql_statement_service.h>
#include <cstring>
#include <string>
#include <vector>
#include "mysql/components/component_implementation.h"
#include "mysql/components/services/udf_metadata.h"
#if !defined(NDEBUG)
#include "mysql/components/services/mysql_debug_keyword_service.h"
#include "mysql/components/services/mysql_debug_sync_service.h"
#endif
#include <mysql/components/services/udf_registration.h>

#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

auto handle_non_select_statement_result(my_h_statement statement,
                                        unsigned char *error) -> std::string;

auto parse_headers(uint64_t num_fields, my_h_statement statement,
                   unsigned char *error) -> std::vector<std::string>;

auto get_field_type(my_h_statement statement, size_t index,
                    unsigned char *error) -> uint64_t;

auto parse_value_at_index(uint64_t field_type, my_h_row row, size_t index)
    -> std::string;

auto parse_rows(my_h_statement statement, size_t num_fields,
                unsigned char *error) -> std::vector<std::vector<std::string>>;

auto string_from_result(std::vector<std::string> &header_row,
                        std::vector<std::vector<std::string>> &data_rows)
    -> std::string;

auto handle_error(my_h_statement statement, unsigned char *error, char *result,
                  unsigned long *length) -> char *;

auto print_output(char *result, unsigned long *length, std::string message)
    -> char *;

#endif  // !UTILS_INCLUDED
