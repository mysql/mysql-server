/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_NAMES_H_
#define PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_NAMES_H_

#include <string>

const std::string k_variable_result_rows_affected = "%RESULT_ROWS_AFFECTED%";
const std::string k_variable_result_last_insert_id = "%RESULT_LAST_INSERT_ID%";
const std::string k_variable_active_client_id = "%ACTIVE_CLIENT_ID%";
const std::string k_variable_active_socket_id = "%ACTIVE_SOCKET_ID%";
const std::string k_variable_option_user = "%OPTION_CLIENT_USER%";
const std::string k_variable_option_pass = "%OPTION_CLIENT_PASSWORD%";
const std::string k_variable_option_host = "%OPTION_CLIENT_HOST%";
const std::string k_variable_option_socket = "%OPTION_CLIENT_SOCKET%";
const std::string k_variable_option_schema = "%OPTION_CLIENT_SCHEMA%";
const std::string k_variable_option_port = "%OPTION_CLIENT_PORT%";
const std::string k_variable_option_ssl_mode = "%OPTION_SSL_MODE%";
const std::string k_variable_option_ssl_cipher = "%OPTION_SSL_CIPHER%";
const std::string k_variable_option_tls_version = "%OPTION_TLS_VERSION%";
const std::string k_variable_option_compression_algorithm =
    "%OPTION_COMPRESSION_ALGORITHM%";
const std::string k_variable_option_compression_max_combine_messages =
    "%OPTION_COMPRESSION_MAX_COMBINE_MESSAGES%";
const std::string k_variable_option_compression_combine_mixed_messages =
    "%OPTION_COMPRESSION_COMBINE_MIXED_MESSAGES%";
const std::string k_variable_option_compression_level =
    "%OPTION_COMPRESSION_LEVEL%";

#endif  // PLUGIN_X_TESTS_DRIVER_PROCESSOR_VARIABLE_NAMES_H_
