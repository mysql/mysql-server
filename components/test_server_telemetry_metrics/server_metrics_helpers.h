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

#ifndef TEST_SERVER_METRICS_HELPERS_INCLUDED
#define TEST_SERVER_METRICS_HELPERS_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <cstdarg>  // va_list
#include <string>

class FileLogger {
 public:
  explicit FileLogger(const char *filename) : m_path(filename) {}

  void write(const char *format, ...)
#ifdef __GNUC__
      __attribute__((format(printf, 2, 3)))
#endif
  {
    if (m_suppress) return;

    // to be thread-safe each call opens the file by itself
    FILE *outfile = fopen(m_path.c_str(), "a+");
    if (outfile) {
      char msg[2048];

      va_list args;
      va_start(args, format);
      const int len = vsnprintf(msg, sizeof(msg), format, args);
      va_end(args);

      const int bytes = std::min(len, (int)(sizeof(msg) - 1));
      auto written [[maybe_unused]] = fwrite(msg, sizeof(char), bytes, outfile);
      (void)fclose(outfile);
    }
  }

  inline void suppress(bool set) { m_suppress = set; }

 private:
  std::string m_path;
  bool m_suppress{false};
};

int enumerate_meters_with_metrics(FileLogger &log);
int enumerate_metrics(const char *meter, FileLogger &log,
                      bool read_value = true);
long long get_metric_value(const char *meter, const char *metric);

#endif /* TEST_SERVER_METRICS_HELPERS_INCLUDED */
