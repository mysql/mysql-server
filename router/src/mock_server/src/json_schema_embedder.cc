/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <cerrno>
#include <cstdio>
#include <cstring>  // strerror()
#include <stdexcept>
#include <string>

/**
 * This program takes a text file as input (presumably a JSON schema), and
 * writes a .cc file as output, which contains contents of the input file as an
 * array of chars. You can think of it as our own customised version of 'xxd -i'
 * Unix command.
 */

int main(int argc, const char **argv) {
  auto throw_error = [&](const std::string &msg, const std::string &filename) {
    throw std::runtime_error(msg + " '" + filename + "': " + strerror(errno));
  };

  try {
    // get commandline args
    if (argc != 3)
      throw std::runtime_error(std::string("USAGE: ") + argv[0] +
                               " <in_file> <out_file>");
    const char *in_filename = argv[1];
    const char *out_filename = argv[2];

    // open input and output files
    FILE *in_file;
    FILE *out_file;
    if (!(in_file = fopen(in_filename, "r")))
      throw_error("Failed to open input file", in_filename);
    if (!(out_file = fopen(out_filename, "w")))
      throw_error("Failed to open output file", out_filename);

    // write commend and 1st part of the array definition
    if (!fprintf(out_file,
                 "// This file was auto-generated during CMake build process, "
                 "using command:\n"
                 "//\n"
                 "//   %s %s %s\n"
                 "//\n"
                 "// (see " __FILE__ ")\n"
                 "\n"
                 "extern const char kSqlQueryJsonSchema[] = {\n",
                 argv[0], argv[1], argv[2]))
      throw_error("Failed writing output file", out_filename);

    // write array elements
    int cnt = 0;
    while (true) {
      // read char and write it as array element; break loop on EOF or throw on
      // I/O error
      char c;
      if (fread(&c, 1, 1, in_file)) {
        if (!fprintf(out_file, "0x%02x, ", c))
          throw_error("Failed writing output file", out_filename);
      } else {
        if (feof(in_file))
          break;
        else
          throw_error("Failed reading input file", in_filename);
      }

      // line break every 16th element
      if ((cnt++ & 0xf) == 0xf)
        if (!fprintf(out_file, "\n"))
          throw_error("Failed writing output file", out_filename);
    }

    // write last part of array definition
    if (!fprintf(out_file, "0x00 };\n"))  // 0x00 is the string terminator
      throw_error("Failed writing output file", out_filename);

    // close files
    if (fclose(in_file)) throw_error("Failed closing input file", in_filename);
    if (fclose(out_file))
      throw_error("Failed closing output file", out_filename);

    return 0;

  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
