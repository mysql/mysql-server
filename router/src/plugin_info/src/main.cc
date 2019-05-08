/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin_info_app.h"

#include <iostream>
#include <string>

#include "mysql/harness/tty.h"
#include "mysql/harness/vt100.h"
#include "mysql/harness/vt100_filter.h"

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
static void display_error(std::ostream &cerr, const std::string program_name,
                          const std::string &errmsg, bool with_help) {
  cerr << Vt100::foreground(Vt100::Color::Red) << "[ERROR] "
       << Vt100::render(Vt100::Render::ForegroundDefault) << errmsg << "\n";

  if (with_help) {
    cerr << "\n"
         << Vt100::foreground(Vt100::Color::Red) << "[NOTE]"
         << Vt100::render(Vt100::Render::ForegroundDefault) << " Use '"
         << program_name << " --help' to show the help."
         << "\n";
  }
  cerr << std::endl;  // flush
}

int main(int argc, char **argv) {
  Tty cout_tty(Tty::fd_from_stream(std::cout));
  Vt100Filter filtered_out_streambuf(
      std::cout.rdbuf(), !(cout_tty.is_tty() && cout_tty.ensure_vt100()));
  std::ostream filtered_out_stream(&filtered_out_streambuf);

  Tty cerr_tty(Tty::fd_from_stream(std::cerr));
  Vt100Filter filtered_err_streambuf(
      std::cerr.rdbuf(), !(cerr_tty.is_tty() && cerr_tty.ensure_vt100()));
  std::ostream filtered_err_stream(&filtered_err_streambuf);
  try {
    std::vector<std::string> args;
    if (argc > 1) {
      // if there are more args, place them in a vector
      args.reserve(argc - 1);
      for (int n = 1; n < argc; ++n) {
        args.emplace_back(argv[n]);
      }
    }
    // cout is a tty?
    PluginInfoFrontend frontend(argv[0], args, filtered_out_stream,
                                filtered_err_stream);
    return frontend.run();
  } catch (const UsageError &e) {
    display_error(filtered_err_stream, argv[0], e.what(), true);
    return EXIT_FAILURE;
  } catch (const FrontendError &e) {
    display_error(filtered_err_stream, argv[0], e.what(), false);
    return EXIT_FAILURE;
  }
}
