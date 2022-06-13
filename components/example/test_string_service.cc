/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
#include "m_string.h"  // strlen
#include "my_sys.h"    // my_write, my_malloc
#include "test_string_service_long.h"

#define MAX_BUFFER_LENGTH 80

#define WRITE_LOG(lit_log_text)                                             \
  strcpy(log_text, lit_log_text);                                           \
  if (fwrite((uchar *)log_text, sizeof(char), strlen(log_text), outfile) != \
      strlen(log_text))                                                     \
    return true;

/**
  This file contains a test (example) component, which tests the service
  "test_service", provided by the component "service_component".
*/

/**
  Initialization entry method for test component. It executes the tests of the
  service.
*/
mysql_service_status_t test_string_service_init() {
  FILE *outfile;

  my_h_string out_string = nullptr;
  const char *test_text = "Hello MySql-8.0";
  const char *empty_test_text = "";
  char low_test_text[MAX_BUFFER_LENGTH];
  char upper_test_text[MAX_BUFFER_LENGTH];
  char log_text[MAX_BUFFER_LENGTH];
  const char *filename = "test_string_service.log";

  unlink(filename);
  outfile = fopen(filename, "w+");

  WRITE_LOG("test_string_service init:\n");

  if (mysql_service_mysql_string_factory->create(&out_string)) {
    WRITE_LOG("Create string failed.\n");
  } else {
    mysql_service_mysql_string_factory->destroy(out_string);
    WRITE_LOG("Destroy string object.\n");
    // In buffer=NULL in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            nullptr,  // its a input buffer
            strlen(test_text), "utf8mb3")) {
      WRITE_LOG("Buffer=NULL in convert from buffer: passed.\n");
    };
    // Length is too high for buffer in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            strlen(test_text) + 10, "utf8mb3")) {
      WRITE_LOG("Length too high for buffer in convert from buffer: passed.\n");
    }
    // Length is zero for buffer in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            0, "utf8mb3")) {
      WRITE_LOG("Length is zero for buffer in convert from buffer: failed.\n");
    } else {
      WRITE_LOG("Length is zero for buffer in convert from buffer: passed.\n");
    }
    // Length is negative for buffer in convert from buffer
    // Crash as real_alloc (this=0x7f1ddc29a528, length=18446744073709551615)
    /*      if
       (mysql_service_mysql_string_converter->convert_from_buffer(&out_string,
                                                test_text, // its a input buffer
                                                -1,
                                                "utf8mb3"))
          {
            WRITE_LOG ("Length is negative for buffer in convert from buffer:
       failed.\n");
          }
          else
          {
            WRITE_LOG ("Length is negative for buffer in convert from buffer:
       passed.\n");
          }
    */
    // Empty string in convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            empty_test_text,  // its a input buffer
            strlen(empty_test_text), "utf8mb3")) {
      WRITE_LOG("Empty string as input to convert from buffer failed.\n");
    } else {
      WRITE_LOG("Empty string as input to convert from buffer passed.\n");
    }
    // Valid convert from buffer
    if (mysql_service_mysql_string_converter->convert_from_buffer(
            &out_string,
            test_text,  // its a input buffer
            strlen(test_text), "utf8mb3")) {
      WRITE_LOG("Convert from buffer failed.\n");
    } else {
      uint out_length = 0;
      WRITE_LOG("Convert from buffer passed.\n");
      // NULL as in_string in get number of chars
      if (mysql_service_mysql_string_character_access->get_char_length(
              nullptr, &out_length)) {
        WRITE_LOG("NULL as input spring in get_char_length passed.\n");
      }
      // valid get number of chars
      if (mysql_service_mysql_string_character_access->get_char_length(
              out_string, &out_length)) {
        WRITE_LOG("Get number of chars failed.\n");
      } else {
        if (out_length == 15) {
          WRITE_LOG("Number of chars right.\n");
        } else {
          WRITE_LOG("Number of chars wrong.\n");
        }
      }
      out_length = 0;
      // NULL as in_string in get number of bytes
      if (mysql_service_mysql_string_byte_access->get_byte_length(
              nullptr, &out_length)) {
        WRITE_LOG("NULL as input buffer in get_byte_length passed.\n");
      }
      // valid get number of bytes
      if (mysql_service_mysql_string_byte_access->get_byte_length(
              out_string, &out_length)) {
        WRITE_LOG("Get number of bytes failed.\n");
      } else {
        if (out_length == 15) {
          WRITE_LOG("Number of bytes right.\n");
        } else {
          WRITE_LOG("Number of bytes wrong.\n");
        }
      }
      // Convert to low string
      my_h_string low_string = nullptr;
      if (mysql_service_mysql_string_factory->create(&low_string)) {
        WRITE_LOG("Create lower string object failed.\n");
      } else {
        // NULL as input buffer in tolower
        if (mysql_service_mysql_string_case->tolower(&low_string, nullptr)) {
          WRITE_LOG("NULL as input buffer in tolower passed.\n");
        }
        if (mysql_service_mysql_string_case->tolower(&low_string, out_string)) {
          WRITE_LOG("Tolower failed.\n");
        } else {
          WRITE_LOG("Tolower passed:\n");
          // NULL as input buffer in Convert string to buffer
          if (mysql_service_mysql_string_converter->convert_to_buffer(
                  nullptr, low_test_text, MAX_BUFFER_LENGTH, "utf8mb3")) {
            WRITE_LOG("NULL as input buffer in Convert to buffer passed.\n");
          }
          // Convert low string to buffer
          if (mysql_service_mysql_string_converter->convert_to_buffer(
                  low_string, low_test_text, MAX_BUFFER_LENGTH, "utf8mb3")) {
            WRITE_LOG("Convert to buffer failed.\n");
          } else {
            WRITE_LOG(low_test_text);
            WRITE_LOG("\n");
          }
        }
      }
      mysql_service_mysql_string_factory->destroy(low_string);
      // Convert to upper string
      my_h_string upper_string = nullptr;
      if (mysql_service_mysql_string_factory->create(&upper_string)) {
        WRITE_LOG("Create upper string object failed.\n");
      } else {
        // NULL as input buffer in toupper
        if (mysql_service_mysql_string_case->toupper(&upper_string, nullptr)) {
          WRITE_LOG("NULL as input buffer in toupper passed.\n");
        }
        if (mysql_service_mysql_string_case->toupper(&upper_string,
                                                     out_string)) {
          WRITE_LOG("Toupper failed.\n");
        } else {
          WRITE_LOG("Toupper passed:\n");
          if (mysql_service_mysql_string_converter->convert_to_buffer(
                  upper_string, upper_test_text, MAX_BUFFER_LENGTH,
                  "utf8mb3")) {
            WRITE_LOG("Convert to buffer failed.\n");
          } else {
            WRITE_LOG(upper_test_text);
            WRITE_LOG("\n");
          }
        }
      }
      mysql_service_mysql_string_factory->destroy(upper_string);
      // Get char with index 1
      ulong out_char;
      if (mysql_service_mysql_string_character_access->get_char(out_string, 1,
                                                                &out_char)) {
        WRITE_LOG("Get char with index 1 failed.\n");
      } else {
        WRITE_LOG("Get char with index 1 passed.");
        WRITE_LOG("\n");
      }
      // Get char with index > strlen : Must fail
      if (mysql_service_mysql_string_character_access->get_char(
              out_string, strlen(test_text) + 1, &out_char)) {
        WRITE_LOG("Get char with index > strlen passed.\n");
      }
      // Get byte with index strlen
      uint out_byte;
      if (mysql_service_mysql_string_byte_access->get_byte(
              out_string, strlen(test_text) - 1, &out_byte)) {
        WRITE_LOG("Get byte with index strlen failed.\n");
      } else {
        WRITE_LOG("Get byte with index strlen passed.");
        WRITE_LOG("\n");
      }
      // Get byte with index > strlen : Must fail
      if (mysql_service_mysql_string_byte_access->get_byte(
              out_string, strlen(test_text) + 1, &out_byte)) {
        WRITE_LOG("Get byte with index > strlen passed.\n");
      }
      // Iterator functions:
      my_h_string_iterator out_iterator = nullptr;
      // NULL as input buffer in iterator_create
      if (mysql_service_mysql_string_iterator->iterator_create(nullptr,
                                                               &out_iterator)) {
        WRITE_LOG("NULL as input buffer in create iterator passed.\n");
      }
      if (mysql_service_mysql_string_iterator->iterator_create(out_string,
                                                               &out_iterator)) {
        WRITE_LOG("Create iterator failed.\n");
      } else {
        int out_iter_char;
        bool out = false;
        WRITE_LOG("Create iterator passed.\n");
        // NULL as iterator in get_next
        if (mysql_service_mysql_string_iterator->iterator_get_next(
                nullptr, &out_iter_char)) {
          WRITE_LOG("NULL as Iterator in get next passed.\n");
        }
        // NULL as iterator in is_lower
        if (mysql_service_mysql_string_ctype->is_lower(nullptr, &out)) {
          WRITE_LOG("NULL as iterator in Is lower passed.\n");
        }
        // NULL as iterator in is_upper
        if (mysql_service_mysql_string_ctype->is_upper(nullptr, &out)) {
          WRITE_LOG("NULL as iterator in is_upper passed.\n");
        }
        // NULL as iterator in is_digit
        if (mysql_service_mysql_string_ctype->is_digit(nullptr, &out)) {
          WRITE_LOG("NULL as iterator in is_digit passed.\n");
        }
        while (mysql_service_mysql_string_iterator->iterator_get_next(
                   out_iterator, &out_iter_char) == 0) {
          WRITE_LOG("Iterator get next passed.\n");
          if (mysql_service_mysql_string_ctype->is_lower(out_iterator, &out)) {
            WRITE_LOG("Is lower failed.\n");
          } else {
            if (out) {
              WRITE_LOG("Is lower.\n");
            }
          }
          if (mysql_service_mysql_string_ctype->is_upper(out_iterator, &out)) {
            WRITE_LOG("Is upper failed.\n");
          } else {
            if (out) {
              WRITE_LOG("Is upper.\n");
            }
          }
          if (mysql_service_mysql_string_ctype->is_digit(out_iterator, &out)) {
            WRITE_LOG("Is digit failed.\n");
          } else {
            if (out) {
              WRITE_LOG("Is digit.\n");
            }
          }
        }
        mysql_service_mysql_string_iterator->iterator_destroy(out_iterator);
        WRITE_LOG("Iterator destroyed.\n");
      }
    }
  }
  mysql_service_mysql_string_factory->destroy(out_string);
  WRITE_LOG("Destroy string object.\n");

  WRITE_LOG("End of init\n");
  fclose(outfile);

  return false;
}

/**
  De-initialization method for Component.
*/
mysql_service_status_t test_string_service_deinit() { return false; }

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_string_service)
END_COMPONENT_PROVIDES();

REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_character_access);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_byte_access);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_case);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_iterator);
REQUIRES_SERVICE_PLACEHOLDER(mysql_string_ctype);

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_string_service)
REQUIRES_SERVICE(mysql_string_factory),
    REQUIRES_SERVICE(mysql_string_converter),
    REQUIRES_SERVICE(mysql_string_character_access),
    REQUIRES_SERVICE(mysql_string_byte_access),
    REQUIRES_SERVICE(mysql_string_case),
    REQUIRES_SERVICE(mysql_string_iterator),
    REQUIRES_SERVICE(mysql_string_ctype), END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_string_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_string_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_string_service, "mysql:test_string_service")
test_string_service_init, test_string_service_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_string_service)
    END_DECLARE_LIBRARY_COMPONENTS
