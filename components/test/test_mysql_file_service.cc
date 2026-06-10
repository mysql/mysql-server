/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <filesystem>

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_file.h>
#include <mysql/components/services/udf_registration.h>

REQUIRES_SERVICE_PLACEHOLDER(mysql_file);
REQUIRES_SERVICE_PLACEHOLDER(udf_registration);

/*
  Macro supporting line number.
*/
#define report_error(result, length, msg) \
  report_error_ln(result, length, msg, __LINE__)

static bool report_error_ln(char *result, unsigned long *length,
                            const char *msg, int line) {
  *length = sprintf(result, "[%d] ERROR: %s", line, msg);
  return true;
}

static bool compare_buffer(char *result, unsigned long *length,
                           const unsigned char *to_check,
                           const unsigned char *buffer, size_t buffer_length) {
  const unsigned char *end_buf = buffer + buffer_length;

  while (buffer < end_buf) {
    if (*to_check++ != *buffer++) {
      return report_error(result, length, "File content is invalid.");
    }
  }
  return false;
}

static bool test_write(char *r, unsigned long *l) {
  FILE_h f = mysql_service_mysql_file->open("my_file.txt", MY_FILE_O_RDONLY);

  if (f != nullptr) {
    return report_error(r, l,
                        "Should not open a non-existent file for reading.");
  }

  f = mysql_service_mysql_file->open("my_file.txt", MY_FILE_O_WRONLY);

  if (f != nullptr) {
    return report_error(r, l,
                        "Should not open a non-existent file for writing.");
  }

  f = mysql_service_mysql_file->open("my_file.txt", MY_FILE_O_RDWR);

  if (f != nullptr) {
    return report_error(
        r, l, "Should not open a non-existent file for reading or writing.");
  }

  f = mysql_service_mysql_file->open("my_file.txt", MY_FILE_O_CREAT);

  if (f == nullptr) {
    return report_error(r, l, "Cannot create the file.");
  }

  if (!std::filesystem::exists("my_file.txt")) {
    return report_error(r, l, "The file should have been created.");
  }

  unsigned char buffer[10] = {
      1, 2, 3, 4, 5,
  };

  size_t i = mysql_service_mysql_file->read(f, buffer, std::size(buffer));

  if (i != 0) {
    return report_error(r, l,
                        "Should not read any data from a newly opened file.");
  }

  i = mysql_service_mysql_file->write(f, buffer, 5);

  if (i != MY_FILE_ERROR_IO) {
    return report_error(r, l, "Should not be able to write 5 bytes.");
  }

  int ri = mysql_service_mysql_file->close(f);

  if (ri != 0) {
    return report_error(r, l, "Failed to close the file.");
  }

  return false;
}

static bool test_binary_write_read(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->open(
      "my_file.txt", MY_FILE_O_WRONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for writing.");
  }

  unsigned char buffer[10] = {
      1, 2, 3, 4, 5,
  };
  size_t rw = mysql_service_mysql_file->write(file, buffer, 5);

  if (rw != 5) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot write 5 bytes to the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  file = mysql_service_mysql_file->open("my_file.txt",
                                        MY_FILE_O_RDONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for reading.");
  }

  size_t rr = mysql_service_mysql_file->read(file, buffer, 10);

  if (rr != 5) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot read from the file.");
  }

  rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  return false;
}

static bool test_binary_write_overwrite_read(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->open(
      "my_file.txt", MY_FILE_O_WRONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for writing.");
  }

  unsigned char buffer[10] = {
      6,
      7,
      8,
  };
  unsigned long long rw = mysql_service_mysql_file->write(file, buffer, 3);

  if (rw != 3) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot write 3 bytes to the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  file = mysql_service_mysql_file->open("my_file.txt",
                                        MY_FILE_O_RDONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for reading.");
  }

  size_t rr = mysql_service_mysql_file->read(file, buffer, 10);

  if (rr != 5) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot read from the file.");
  }

  rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  unsigned char buf[] = {6, 7, 8, 4, 5, 0};

  if (compare_buffer(r, l, buffer, buf, std::size(buf))) {
    return true;
  }

  return false;
}

static bool test_binary_write_append_read(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->open(
      "my_file.txt", MY_FILE_O_WRONLY | MY_FILE_O_APPEND | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for writing.");
  }

  unsigned char buffer[10] = {
      9,
      10,
      11,
  };
  unsigned long long rw = mysql_service_mysql_file->write(file, buffer, 3);

  if (rw != 3) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot write 3 bytes to the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  file = mysql_service_mysql_file->open("my_file.txt",
                                        MY_FILE_O_RDONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for reading.");
  }

  size_t rr = mysql_service_mysql_file->read(file, buffer, 10);

  if (rr != 8) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot read from the file.");
  }

  rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  unsigned char buf[] = {6, 7, 8, 4, 5, 9, 10, 11};

  if (compare_buffer(r, l, buffer, buf, std::size(buf))) {
    return true;
  }

  return false;
}

static bool test_binary_read_pos(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->open(
      "my_file.txt", MY_FILE_O_RDONLY | MY_FILE_O_BINARY);

  if (file == nullptr) {
    return report_error(r, l, "Cannot open the file for reading.");
  }

  unsigned long long rs =
      mysql_service_mysql_file->seek(file, 3, MY_FILE_SEEK_SET);

  if (rs != 3) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot set the position in the file.");
  }

  rs = mysql_service_mysql_file->tell(file);

  if (rs != 3) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Failed to get file position (tell).");
  }
  unsigned char buffer[10] = {
      0,
  };
  unsigned long long rw = mysql_service_mysql_file->read(file, buffer, 10);

  if (rw != 5) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot read 5 bytes from the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc != 0) {
    return report_error(r, l, "Cannot close the file.");
  }

  unsigned char buf[] = {4, 5, 9, 10, 11, 0};

  if (compare_buffer(r, l, buffer, buf, std::size(buf))) {
    return report_error(r, l, "File content is invalid.");
  }

  return false;
}

static bool test_create_read(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->create(
      "my_file_create_read.txt",
      MY_FILE_O_CREAT | MY_FILE_O_WRONLY | MY_FILE_O_BINARY,
      MY_FILE_PERMISSION_USER_READ | MY_FILE_PERMISSION_GROUP_READ |
          MY_FILE_PERMISSION_OTHERS_READ);

  if (file == nullptr) {
    return report_error(r, l, "Cannot create the file.");
  }

  unsigned char buffer[10] = {
      12,
      13,
      14,
      15,
  };
  unsigned long long rw = mysql_service_mysql_file->write(file, buffer, 4);

  if (rw != 4) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot write 4 bytes to the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc) {
    return report_error(r, l, "Cannot close the file.");
  }

  return false;
}

static bool test_create_write(char *r, unsigned long *l) {
  FILE_h file = mysql_service_mysql_file->create(
      "my_file_create_write.txt",
      MY_FILE_O_CREAT | MY_FILE_O_WRONLY | MY_FILE_O_BINARY,
      MY_FILE_PERMISSION_USER_WRITE | MY_FILE_PERMISSION_GROUP_WRITE |
          MY_FILE_PERMISSION_OTHERS_WRITE);

  if (file == nullptr) {
    return report_error(r, l, "Cannot create the file.");
  }

  unsigned char buffer[10] = {
      12,
      13,
      14,
      15,
  };
  unsigned long long rw = mysql_service_mysql_file->write(file, buffer, 4);

  if (rw != 4) {
    mysql_service_mysql_file->close(file);
    return report_error(r, l, "Cannot write 4 bytes to the file.");
  }

  int rc = mysql_service_mysql_file->close(file);

  if (rc) {
    return report_error(r, l, "Cannot close the file.");
  }

  return false;
}

static char *run_test_udf(UDF_INIT *initid [[maybe_unused]],
                          UDF_ARGS *args [[maybe_unused]], char *result,
                          unsigned long *length,
                          unsigned char *null_value [[maybe_unused]],
                          unsigned char *error [[maybe_unused]]) {
  *length = sprintf(result, "%s", "FAILED");

  if (test_write(result, length)) return result;
  if (test_binary_write_read(result, length)) return result;
  if (test_binary_write_overwrite_read(result, length)) return result;
  if (test_binary_write_append_read(result, length)) return result;
  if (test_binary_read_pos(result, length)) return result;
  if (test_create_read(result, length)) return result;
  if (test_create_write(result, length)) return result;

  std::remove("my_file_create_read.txt");
  std::remove("my_file_create_write.txt");
  std::remove("my_file.txt");

  *length = sprintf(result, "%s", "OK");

  return result;
}

/**
  Initialization entry method for test component.
*/
static mysql_service_status_t test_component_file_service_init() {
  if (mysql_service_udf_registration->udf_register(
          "test_mysql_file_run_test", STRING_RESULT, (Udf_func_any)run_test_udf,
          nullptr, nullptr)) {
    return 1;
  }

  return 0;
}

/**
  De-initialization method for Component.
*/
static mysql_service_status_t test_component_file_service_deinit() {
  int was_present = 0;
  mysql_service_udf_registration->udf_unregister("test_mysql_file_run_test",
                                                 &was_present);
  std::filesystem::remove("my_file.txt");
  return 0;
}

/* An empty list as no service is provided. */
BEGIN_COMPONENT_PROVIDES(test_component_mysql_file_service)
END_COMPONENT_PROVIDES();

/* A list of required services. */
BEGIN_COMPONENT_REQUIRES(test_component_mysql_file_service)
REQUIRES_SERVICE(mysql_file), REQUIRES_SERVICE(udf_registration),
    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_component_mysql_file_service)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"),
    METADATA("test_component_mysql_file_service", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_component_mysql_file_service,
                  "mysql:test_component_mysql_file_service")
test_component_file_service_init,
    test_component_file_service_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_component_mysql_file_service)
    END_DECLARE_LIBRARY_COMPONENTS
