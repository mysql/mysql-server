/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * patch_file <input-file> <patch-file#1> ....
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
 */

#include<fstream>
#include<iostream>
#include<string>

int
main(int argc, char *argv[])
{
  const std::string input_file(argv[1]);
  std::ifstream input(input_file.c_str());
  for (int argi = 2; argi < argc; argi ++)
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
      std::cout << line << std::endl;
    }
    if (!input.good())
      return 2; // Bad read from input file or delimiter line not found

    // Copy all lines from patch file
    std::cout << delim << std::endl;
    delim.clear();
    for (std::string line; std::getline(patch, line); )
    {
      std::cout << line << std::endl;
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
    std::cout << line << std::endl;
  }
  if (!input.eof())
    return 6; // Bad read from input file

  return 0;
}
