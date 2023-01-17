/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "keyring_frontend.h"

#include <ostream>
#include <string>

/**
 * display error.
 *
 * in case the frontend failed to parse arguments, show errormsg
 * and a hint for help.
 * If the frontend failed for another reason, just show the errormsg.
 *
 * @param cerr output stream
 * @param program_name name of executable that was started (argv[0])
 * @param errmsg error message to display
 * @param with_help true, if "hint for help" shall be displayed
 */
static void display_error(std::ostream &cerr, const std::string &program_name,
                          const std::string &errmsg, bool with_help) {
  cerr << "[Error] " << errmsg << std::endl;

  if (with_help) {
    cerr << std::endl
         << "[Note] Use '" << program_name << " --help' to show the help."
         << std::endl;
  }
}

int main(int argc, char **argv) {
  try {
    std::vector<std::string> args;
    if (argc > 1) {
      // if there are more args, place them in a vector
      args.reserve(argc - 1);
      for (int n = 1; n < argc; ++n) {
        args.emplace_back(argv[n]);
      }
    }
    KeyringFrontend frontend(argv[0], args);
    return frontend.run();
  } catch (const UsageError &e) {
    display_error(std::cerr, argv[0], e.what(), true);
    return EXIT_FAILURE;
  } catch (const FrontendError &e) {
    display_error(std::cerr, argv[0], e.what(), false);
    return EXIT_FAILURE;
  }
}
