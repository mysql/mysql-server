/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/system_variable_source.h>
#include "../../components/mysql_server/system_variable_source_imp.h"

#include <fcntl.h>
#include <stdio.h>
#include <typelib.h>

#define MAX_BUFFER_LENGTH 100
int log_text_len = 0;
char log_text[MAX_BUFFER_LENGTH];
FILE *outfile;
const char *filename = "test_system_variable_source.log";

#define WRITE_LOG(lit_log_text)                         \
  log_text_len = sprintf(log_text, "%s", lit_log_text); \
  fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile)

REQUIRES_SERVICE_PLACEHOLDER(system_variable_source);

/**
  This file contains a test (example) component, which tests the services of
  "system_variable_source" service provided by "mysql_server" component.
*/

/**
  Initialization entry method for test component. It executes the tests of
  the service.
*/
static mysql_service_status_t test_system_variable_source_init() {
  outfile = fopen(filename, "w+");

  WRITE_LOG("test_system_variable_source_init start:\n");

  enum enum_variable_source source;
  if (mysql_service_system_variable_source->get("innodb_buffer_pool_size", 23,
                                                &source)) {
    WRITE_LOG("get failed for innodb_buffer_pool_size.\n");
  } else {
    WRITE_LOG("Source of innodb_buffer_pool_size : ");
    switch (source) {
      case COMPILED:
        WRITE_LOG(" COMPILED.\n");
        break;
      case GLOBAL:
        WRITE_LOG(" GLOBAL.\n");
        break;
      case SERVER:
        WRITE_LOG(" SERVER.\n");
        break;
      case EXPLICIT:
        WRITE_LOG(" EXPLICIT.\n");
        break;
      case EXTRA:
        WRITE_LOG(" EXTRA.\n");
        break;
      case MYSQL_USER:
        WRITE_LOG(" MYSQL_USER.\n");
        break;
      case LOGIN:
        WRITE_LOG(" LOGIN.\n");
        break;
      case COMMAND_LINE:
        WRITE_LOG(" COMMAND_LINE.\n");
        break;
      case PERSISTED:
        WRITE_LOG(" PERSISTED.\n");
        break;
      case DYNAMIC:
        WRITE_LOG(" DYNAMIC.\n");
        break;
      default: /* We should never reach here */
        WRITE_LOG(" INVALID.\n");
        break;
    }
  }

  WRITE_LOG("test_system_variable_source_init end:\n\n");
  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_system_variable_source_deinit() {
  /* Nothing to do */
  return false;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_system_variable_source)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_system_variable_source)
REQUIRES_SERVICE(system_variable_source), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_system_variable_source)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_system_variable_source", "1"), END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_system_variable_source,
                  "mysql:test_system_variable_source")
test_system_variable_source_init,
    test_system_variable_source_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_system_variable_source)
    END_DECLARE_LIBRARY_COMPONENTS
