/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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
    if (argc != 5)
      throw std::runtime_error(
          std::string("USAGE: ") + argv[0] +
          " <in_file> <out_file> <hdr_file> <symbol_name>");
    const char *in_filename = argv[1];
    const char *out_filename = argv[2];
    const char *hdr_filename = argv[3];
    const char *symbol_name = argv[4];

    // open input and output files
    FILE *in_file;
    if (!(in_file = fopen(in_filename, "r")))
      throw_error("Failed to open input file", in_filename);
    {
      FILE *out_file;
      if (!(out_file = fopen(out_filename, "w")))
        throw_error("Failed to open output file", out_filename);

      // write commend and 1st part of the array definition
      if (!fprintf(
              out_file,
              "// This file was auto-generated during CMake build process, "
              "using command:\n"
              "//\n"
              "//   %s %s %s %s %s\n"
              "//\n"
              "// (see " __FILE__ ")\n"
              "#include \"%s\"\n"
              "\n"
              "constexpr const char %s::data_[];\n",
              argv[0], in_filename, out_filename, hdr_filename, symbol_name,
              hdr_filename, symbol_name))
        throw_error("Failed writing output file", out_filename);
      if (fclose(out_file))
        throw_error("Failed closing output file", out_filename);
    }

    FILE *hdr_file;
    if (!(hdr_file = fopen(hdr_filename, "w")))
      throw_error("Failed to open output file", hdr_filename);

    if (!fprintf(hdr_file,
                 "#ifndef %s_INCLUDED\n"
                 "#define %s_INCLUDED\n"
                 "\n"
                 "#include <cstddef>\n"
                 "\n"
                 "// string-view of %s\n"
                 "class %s {\n"
                 " private:\n"
                 "  static constexpr const char data_[]{\n    ",
                 symbol_name, symbol_name, in_filename, symbol_name))
      throw_error("Failed writing output file", hdr_filename);

    // write array elements
    int cnt = 0;
    while (true) {
      // read char and write it as array element; break loop on EOF or throw on
      // I/O error
      char c;
      if (fread(&c, 1, 1, in_file)) {
        if (!fprintf(hdr_file, "0x%02x, ", c))
          throw_error("Failed writing output file", out_filename);
      } else {
        if (feof(in_file))
          break;
        else
          throw_error("Failed reading input file", in_filename);
      }

      // line break every 16th element
      if ((cnt++ & 0xf) == 0xf)
        if (!fprintf(hdr_file, "\n    "))
          throw_error("Failed writing output file", out_filename);
    }

    // write last part of array definition
    if (!fprintf(hdr_file,
                 "  };\n"
                 " public:\n"
                 "  static constexpr const char * data() { return data_; }\n"
                 "  static constexpr std::size_t size() { return "
                 "sizeof(data_); }\n"
                 "};\n"
                 "\n"
                 "#endif\n"))
      throw_error("Failed writing output file", out_filename);

    if (fclose(hdr_file))
      throw_error("Failed closing header file", hdr_filename);
    // close files
    if (fclose(in_file)) throw_error("Failed closing input file", in_filename);

    return 0;

  } catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
