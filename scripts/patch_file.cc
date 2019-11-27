/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * patch_file <input-file> <output-file> <patch-file#1> ....
 *
 * Sections in input file are replaced by content of patch-files.
 *
 * Each section are delimited by the first and last line of corresponding
 * patch file.
 *
 * Sections must occure in same order in input file as patch files are
 * given on command line.
 *
 * Command succeeds if each patch file are used once.
 *
 * Note, currently it is assumed that files uses only LF for new line.
 *
 * When reading files native new line are accepted but LF is used on output.
 */

#include<fstream>
#include<iostream>
#include<string>

int
main(int argc, char *argv[])
{
  const std::string input_file(argv[1]);
  std::ifstream input(input_file.c_str());
  const std::string output_file(argv[2]);
  std::ofstream output(output_file.c_str(), std::ofstream::binary);
  for (int argi = 3; argi < argc; argi ++)
  {
    const std::string patch_file(argv[argi]);
    std::ifstream patch(patch_file.c_str());
    std::string delim;

    // First line in patch file is delimiter line
    if (!std::getline(patch, delim))
      return 1; // Missing initial delimiter line in patch file

    // Copy all lines before delimiter line from input file
    for (std::string line; std::getline(input, line) && line != delim; )
    {
      output << line << std::endl;
    }
    if (!input.good())
      return 2; // Bad read from input file or delimiter line not found

    // Copy all lines from patch file
    output << delim << std::endl;
    delim.clear();
    for (std::string line; std::getline(patch, line); )
    {
      output << line << std::endl;
      // Remember last line as terminating delimiter
      delim = line;
    }
    if (!patch.eof())
      return 3; // Bad read from patch file
    if (delim.empty())
      return 4; // No or empty terminating delimiter line

    // Skip all lines in input file up to the terminating delimiter line
    for (std::string line; std::getline(input, line) && line != delim; )
    {
      ;
    }
    if (!input.good())
      return 5; // Terminating delimiter not found
  }
  // Copy all lines from input file to the end
  for (std::string line; std::getline(input, line); )
  {
    output << line << std::endl;
  }
  if (!input.eof())
    return 6; // Bad read from input file

  input.close();
  output.close();

  return 0;
}
