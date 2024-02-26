/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <fcntl.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_string.h>
#include <stdio.h>

#include "m_string.h"  // strlen
#include "my_inttypes.h"
#include "my_sys.h"  // my_write, my_malloc
#include "test_string_service_long.h"

// Value must be a multiple of 16 (or TEST_TEXT_LIT_LENGTH)
#define MAX_BUFFER_LENGTH 4096

#define WRITE_LOG(format, lit_log_text)                                 \
  log_text_len = sprintf(log_text, format, lit_log_text);               \
  if (fwrite((uchar *)log_text, sizeof(char), log_text_len, outfile) != \
      static_cast<size_t>(log_text_len))                                \
    return true;

/**
  This file contains a test (example) component, which tests the service
  "string".
*/

/**
  Initialization entry method for test component. It executes the tests
  of the service.
*/
mysql_service_status_t test_string_service_init() {
  FILE *outfile;

  my_h_string out_string = nullptr;
#define TEST_TEXT_LIT_LENGTH 16
  const char *test_text_lit = "Hello Mysql-8.0!";  // 16 chars
  char *test_text;  // Buffer filled with test_text_lit
  char low_test_text[MAX_BUFFER_LENGTH + 1];
  char upper_test_text[MAX_BUFFER_LENGTH + 1];
  int log_text_len = 0;
  char log_text[MAX_BUFFER_LENGTH + 2];
  const char *filename = "test_string_service_long.log";
  unlink(filename);
  outfile = fopen(filename, "w+");

  WRITE_LOG("%s\n", "test_string_service_long init:");

  if (!(test_text = (char *)malloc(MAX_BUFFER_LENGTH + 1))) {
    WRITE_LOG("%s\n", "Allcoate buffer failed. Stopped test execution.");
    return (true);
  } else {
    for (int i = 0; i < MAX_BUFFER_LENGTH / TEST_TEXT_LIT_LENGTH; i++) {
      strcpy(&test_text[i * TEST_TEXT_LIT_LENGTH], test_text_lit);
    }
  }
  WRITE_LOG("%s\n", test_text);

  if (mysql_service_mysql_string_factory->create(&out_string)) {
    WRITE_LOG("%s\n", "Create string failed.");
  } else {
    mysql_service_mysql_string_factory->destroy(out_string);
    WRITE_LOG("%s\n", "Destroy string object.");
    // Length is too high for buffer in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            strlen(test_text) + 10, "utf8mb3")) {
      WRITE_LOG("%s\n",
                "Length too high for buffer in convert from buffer: passed.");
    }
    // Length is zero for buffer in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            0, "utf8mb3")) {
      WRITE_LOG("%s\n",
                "Length is zero for buffer in convert from buffer: failed.");
    } else {
      WRITE_LOG("%s\n",
                "Length is zero for buffer in convert from buffer: passed.");
    }
    // Valid convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            strlen(test_text), "utf8mb3")) {
      WRITE_LOG("%s\n", "Convert from buffer failed.");
    } else {
      uint out_length = 0;
      WRITE_LOG("%s\n", "Convert from buffer passed.");
      // valid get number of chars
      if (mysql_service_mysql_string_character_access->get_char_length(
              out_string, &out_length)) {
        WRITE_LOG("%s\n", "Get number of chars failed.");
      } else {
        WRITE_LOG("Number of chars: %d\n", out_length);
      }
      out_length = 0;
      // valid get number of bytes
      if (mysql_service_mysql_string_byte_access->get_byte_length(
              out_string, &out_length)) {
        WRITE_LOG("%s\n", "Get number of bytes failed.");
      } else {
        WRITE_LOG("Number of bytes: %d\n", out_length);
      }
      // Convert to low string
      my_h_string low_string = nullptr;
      if (mysql_service_mysql_string_factory->create(&low_string)) {
        WRITE_LOG("%s\n", "Create lower string object failed.");
      } else {
        if (mysql_service_mysql_string_case->tolower(&low_string, out_string)) {
          WRITE_LOG("%s\n", "Tolower failed.");
        } else {
          WRITE_LOG("%s\n", "Tolower passed:");
          // Convert low string to buffer
          if (mysql_service_mysql_string_converter->convert_to_buffer(
                  low_string, low_test_text, MAX_BUFFER_LENGTH, "utf8mb3")) {
            WRITE_LOG("%s\n", "Convert to buffer failed.");
          } else {
            WRITE_LOG("%s\n", low_test_text);
          }
        }
      }
      mysql_service_mysql_string_factory->destroy(low_string);
      // Convert to upper string
      my_h_string upper_string = nullptr;
      if (mysql_service_mysql_string_factory->create(&upper_string)) {
        WRITE_LOG("%s\n", "Create upper string object failed.");
      } else {
        if (mysql_service_mysql_string_case->toupper(&upper_string,
                                                     out_string)) {
          WRITE_LOG("%s\n", "Toupper failed.");
        } else {
          WRITE_LOG("%s\n", "Toupper passed:");
          if (mysql_service_mysql_string_converter->convert_to_buffer(
                  upper_string, upper_test_text, MAX_BUFFER_LENGTH,
                  "utf8mb3")) {
            WRITE_LOG("%s\n", "Convert to buffer failed.");
          } else {
            WRITE_LOG("%s\n", upper_test_text);
          }
        }
      }
      mysql_service_mysql_string_factory->destroy(upper_string);
      // Get char with index 1
      ulong out_char;
      if (mysql_service_mysql_string_character_access->get_char(out_string, 1,
                                                                &out_char)) {
        WRITE_LOG("%s\n", "Get char with index 1 failed.");
      } else {
        WRITE_LOG("%s\n", "Get char with index 1 passed.");
      }
      // Get char with index > strlen : Must fail
      if (mysql_service_mysql_string_character_access->get_char(
              out_string, strlen(test_text) + 1, &out_char)) {
        WRITE_LOG("%s\n", "Get char with index > strlen passed.");
      }
      // Get byte with index strlen
      uint out_byte;
      if (mysql_service_mysql_string_byte_access->get_byte(
              out_string, strlen(test_text) - 1, &out_byte)) {
        WRITE_LOG("%s\n", "Get byte with index strlen failed.");
      } else {
        WRITE_LOG("%s\n", "Get byte with index strlen passed.");
      }
      // Get byte with index > strlen : Must fail
      if (mysql_service_mysql_string_byte_access->get_byte(
              out_string, strlen(test_text) + 1, &out_byte)) {
        WRITE_LOG("%s\n", "Get byte with index > strlen passed.");
      }
      // Iterator functions:
      my_h_string_iterator out_iterator = nullptr;
      if (mysql_service_mysql_string_iterator->iterator_create(out_string,
                                                               &out_iterator)) {
        WRITE_LOG("%s\n", "Create iterator failed.");
      } else {
        int out_iter_char;
        uint count = 0;
        WRITE_LOG("%s\n", "Create iterator passed.");
        while (mysql_service_mysql_string_iterator->iterator_get_next(
                   out_iterator, &out_iter_char) == 0) {
          count++;
        }
        if (count < MAX_BUFFER_LENGTH) {
          WRITE_LOG("%s\n", "Iterator loop failed.");
        } else {
          WRITE_LOG("%s\n", "Iterator loop passed.");
        }
        mysql_service_mysql_string_iterator->iterator_destroy(out_iterator);
        WRITE_LOG("%s\n", "Iterator destroyed.");
      }
    }
  }
  mysql_service_mysql_string_factory->destroy(out_string);
  WRITE_LOG("%s\n", "Destroy string object.");

  WRITE_LOG("%s\n", "End of init");
  fclose(outfile);
  free(test_text);

  return false;
}

/**
  De-initialization method for Component.
*/
mysql_service_status_t test_string_service_deinit() { return false; }

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_string_service_long)
END_COMPONENT_PROVIDES();

REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_character_access);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_byte_access);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_case);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_iterator);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_ctype);

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_string_service_long)
REQUIRES_SERVICE(mysql_string_factory),
    REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_string_character_access),
    REQUIRES_SERVICE(mysql_string_byte_access),
    REQUIRES_SERVICE(mysql_string_case),
    REQUIRES_SERVICE(mysql_string_iterator),
    REQUIRES_SERVICE(mysql_string_ctype), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_string_service_long)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_string_long_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_string_service_long, "mysql:test_string_service_long")
test_string_service_init, test_string_service_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_string_service_long)
    END_DECLARE_LIBRARY_COMPONENTS
